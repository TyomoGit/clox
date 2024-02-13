#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

/// @brief パーサ
typedef struct {
    /// @brief 現在の字句
    Token current;
    /// @brief 一つ前の字句
    Token previous;
    /// @brief エラーが発生したか
    bool had_error;
    /// @brief パニックモードかどうか
    bool panic_mode;
} Parser;

/// @brief 優先度
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR, // or
    PREC_AND, // and
    PREC_EQUALITY, // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM, // + -
    PREC_FACTOR, // * /
    PREC_UNARY, // ! -
    PREC_CALL, // . ()
    PREC_PRIMARY
} Precedence;

/// @brief 解析関数
typedef void (*ParseFn)(bool can_assign);

/// @brief 解析ルール
typedef struct {
    /// @brief 与えられた型のトークンで始まる前置式を解析する関数
    ParseFn prefix;
    /// @brief 与えられた型のトークンが左オペランドの次に来るような中置式を解析する関数
    ParseFn infix;
    /// @brief 与えられた型のトークンを演算子として使う中置式の優先度
    Precedence precedence;
} ParseRule;

/// @brief ローカル変数．スタック上に保持．
typedef struct {
    /// @brief 変数名
    Token name;
    /// @brief スコープの深さ（0はグローバルスコープ）
    int depth;
    /// @brief キャプチャされているかどうか(スコープから抜けるときにヒープに移すかどうか判定用)
    bool is_captured;
} Local;

/// @brief 上位値．ヒープ上に保持．
typedef struct {
    /// @brief 上位値インデックス
    uint8_t index;
    /// @brief ローカル変数かどうか
    bool is_local;
} Upvalue;

/// @brief 関数の種類
typedef enum {
    /// @brief 関数
    TYPE_FUNCTION,
    /// @brief トップレベル
    TYPE_SCRIPT,
} FunctionType;

/// @brief 関数コンパイラの状態
typedef struct Compiler {
    /// @brief 呼び出し元
    struct Compiler* enclosing;
    /// @brief 現在解析している関数
    ObjFunction* function;
    /// @brief 関数の種類
    FunctionType type;
    /// @brief スコープに入るローカル変数を管理する
    Local locals[UINT8_COUNT];
    /// @brief スコープ内のローカル変数の数
    int local_count;
    /// @brief 上位値の配列
    Upvalue upvalues[UINT8_COUNT];
    /// @brief スコープの深さ（0はグローバルスコープ）
    int scope_depth;
} Compiler;

/// @brief 唯一のパーサ
Parser parser;
Compiler* current = NULL;
/// @brief 現在のチャンク
Chunk* compiling_chunk;

/// @brief 現在のチャンクを返す
/// @return 現在のチャンク
static Chunk* current_chunk() {
    return &current->function->chunk;
}

/// @brief エラーを報告する
/// @param token エラー箇所のトークン
/// @param message エラーメッセージ
static void error_at(Token* token, const char* message) {
    if (parser.panic_mode) {
        return;
    }
    parser.panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        ;
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void error(const char* message) {
    error_at(&parser.previous, message);
}

/// @brief 現在の位置でエラーを報告する
/// @param message エラーメッセージ
static void error_at_current(const char* message) {
    error_at(&parser.current, message);
}

/// @brief 次のトークンを取得し，parserを更新する
static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scan_token();
        if (parser.current.type != TOKEN_ERROR) {
            break;
        }

        error_at_current(parser.current.start);
    }
}

/// @brief 現在のトークンを指定したタイプか確認して消費する
/// @param type 期待されるタイプ
/// @param message エラーメッセージ
static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    error_at_current(message);
}

/// @brief 現在のトークンが指定された型かどうかを返す
/// @param type トークンの型
/// @return 現在のトークンが指定された型かどうか
static bool check(TokenType type) {
    return parser.current.type == type;
}

/// @brief 現在のトークンが指定された型なら消費する
/// @param type トークンの型
/// @return 現在のトークンが指定された型かどうか
static bool match(TokenType type) {
    if (!check(type)) {
        return false;
    }

    advance();
    return true;
}

/// @brief バイトをチャンクに加える
/// @param byte 加えるバイト
static void emit_byte(uint8_t byte) {
    write_chunk(current_chunk(), byte, parser.previous.line);
}

/// @brief 2つのバイトをチャンクに加える
/// @param byte1 一つ目のバイト
/// @param byte2 二つ目のバイト
static void emit_bytes(uint8_t byte1, uint8_t byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

static void emit_loop(int loop_start) {
    emit_byte(OP_LOOP);

    int offset = current_chunk()->count - loop_start + 2;
    if (offset > UINT16_MAX) {
        error("Loop body too large.");
    }
    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

/// @brief 命令を加え，その後に仮のオペランドを加える．仮のオペランドは，後でジャンプ命令を当てはめるため．
/// @param instruction 
/// @return 出力した仮のオペランドのオフセット
static int emit_jump(uint8_t instruction) {
    emit_byte(instruction);
    emit_byte(0xff);
    emit_byte(0xff);
    return current_chunk()->count - 2;
}

/// @brief OP_RETURNをチャンクに加える
static void emit_return() {
    emit_byte(OP_NIL);
    emit_byte(OP_RETURN);
}

/// @brief コンパイルを終わる
static ObjFunction* end_compiler() {
    emit_return();
    ObjFunction* function = current->function;

    #ifdef DEBUG_PRINT_CODE
    if (!parser.had_error) {
        disassemble_chunk(current_chunk(), function->name != NULL ? function->name->chars : "<script>");
    }
    #endif

    current = current->enclosing;

    return function;
}

/// @brief スコープに入る
static void begin_scope() {
    current->scope_depth += 1;
}

/// @brief スコープを抜ける
static void end_scope() {
    current->scope_depth -= 1;

    // ローカル変数を削除する
    while (
        current->local_count > 0 
        && current->locals[current->local_count - 1].depth > current->scope_depth
    ) {
        if (current->locals[current->local_count - 1].is_captured) {
            emit_byte(OP_CLOSE_UPVALUE);
        } else {
            emit_byte(OP_POP);
        }
        current->local_count -= 1;
    }
}

static void expression();
static void statement();
static void declaration();
static ParseRule* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);
static uint8_t make_constant(Value value);

/// @brief 定数部にトークンの字句を追加する
/// @param name 定数部に追加する字句
/// @return 定数部のインデックス
static uint8_t identifier_constant(Token* name) {
    return make_constant(OBJ_VAL(copy_string(name->start, name->length)));
}

/// @brief 識別子が同じであるかどうかを確かめる
/// @param a 
/// @param b 
/// @return 識別子が同じであるかどうか
static bool identifiers_equal(Token* a, Token* b) {
    if (a->length != b->length) {
        return false;
    }

    return memcmp(a->start, b->start, a->length) == 0;
}

/// @brief ローカル変数を解決する
/// @param compiler 
/// @param name 
/// @return ローカル変数のインデックスまたは-1（見つからなかったとき）
static int resolve_local(Compiler* compiler, Token* name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

/// @brief 上位値を追加する
/// @param compiler 関数のコンパイラ
/// @param index 上位値インデックス
/// @param is_local ローカル変数かどうか（ローカル変数ならtrue，他の関数からの上位値ならfalse）
/// @return 上位値インデックス
static int add_upvalue(Compiler* compiler, uint8_t index, bool is_local) {
    int upvalue_count = compiler->function->upvalue_count;

    // すでに同じインデックスのものあれば，そのインデックスを返す
    for (int i = 0; i < upvalue_count; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }

    if (upvalue_count >= UINT8_COUNT) {
        error("Too many closure variable in function.");
        return 0;
    }

    compiler->upvalues[upvalue_count].is_local = is_local;
    compiler->upvalues[upvalue_count].index = index;
    return compiler->function->upvalue_count++;
}

/// @brief 上位値を解決する
/// @param compiler 現在のコンパイラ
/// @param name 識別子
/// @return 上位値インデックス
static int resolve_upvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) {
        // 最も外側の関数に到達した
        return -1;
    }

    // すぐ外側の関数に探しているローカル変数があれば，上位値を追加する
    int local = resolve_local(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(compiler, (uint8_t)local, true);
    }

    // すぐ外側の関数の上位値を調べて，あればコンパイラに追加する
    int upvalue = resolve_upvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

/// @brief 現在のスコープに新しい変数を追加する
/// @param name 変数名
static void add_local(Token name) {
    if (current->local_count == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }

    Local* local = &current->locals[current->local_count];
    current->local_count += 1;

    local->name = name;
    local->depth = -1;
    local->is_captured = false;
}

/// @brief 変数を宣言する
static void declare_variable() {
    if (current->scope_depth == 0) {
        return;
    }

    Token* name = &parser.previous;

    for (int i = current->local_count - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scope_depth) {
            break;
        }

        if (identifiers_equal(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }

    add_local(*name);
}

/// @brief 変数を解析する
/// @param error_message 
/// @return 定数部のインデックス
static uint8_t parse_variable(const char* error_message) {
    consume(TOKEN_IDENTIFIER, error_message);

    declare_variable();
    // ローカル変数のときはダミーのインデックスを返す
    if (current->scope_depth > 0) return 0;

    return identifier_constant(&parser.previous);
}

/// @brief 変数を初期化した印に，スコープの深さを設定する
static void mark_initialized() {
    if (current->scope_depth == 0) {
        return;
    }

    current->locals[current->local_count - 1].depth = current->scope_depth;
}

/// @brief 変数を定義する命令を登録する
/// @param global 定数表における，グローバル変数の名前のインデックス 
static void define_variable(uint8_t global) {
    if (current->scope_depth > 0) {
        mark_initialized();
        return;
    }

    emit_bytes(OP_DEFINE_GLOBAL, global);
}

static uint8_t argument_list() {
    uint8_t arg_count = 0;

    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (arg_count >= 255) {
                error("Can't have more than 255 arguments.");
            }
            arg_count += 1;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return arg_count;
}

/// @brief 中置式を解析する
static void binary(bool can_assign) {
    TokenType operator_type = parser.previous.type;
    ParseRule* rule = get_rule(operator_type);
    parse_precedence((Precedence)rule->precedence + 1);
    switch (operator_type) {
        case TOKEN_BANG_EQUAL: emit_bytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL: emit_byte(OP_EQUAL); break;
        case TOKEN_GREATER: emit_byte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS: emit_byte(OP_LESS); break;
        case TOKEN_LESS_EQUAL: emit_bytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS: emit_byte(OP_ADD); break;
        case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR: emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
        default:
            return; // Unreachable
    }
}

/// @brief 関数呼び出しを解析する
/// @param canAssign 
static void call(bool canAssign) {
    uint8_t arg_count = argument_list();
    emit_bytes(OP_CALL, arg_count);
}

/// @brief リテラルを解析する
static void literal(bool can_assign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        case TOKEN_NIL: emit_byte(OP_NIL); break;
        case TOKEN_TRUE: emit_byte(OP_TRUE); break;
        default: return; // unreachable
    }
}

/// @brief 丸括弧で囲まれた式を解析する
static void grouping(bool can_assign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

/// @brief 値を定数表に追加する
/// @param value 定数表に追加される値
/// @return 追加した値のインデックス
static uint8_t make_constant(Value value) { 
    int constant = add_constant(current_chunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

/// @brief 定数をロードするコードをチャンクに加える
/// @param value 定数
static void emit_constant(Value value) {
    emit_bytes(OP_CONSTANT, make_constant(value));
}

/// @brief ジャンプ先を当てはめる
/// @param offset 
static void patch_jump(int offset) {
    // ジャンプ先（相対）
    int jump = current_chunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too match code to jump over.");
    }

    current_chunk()->code[offset] = (jump >> 8) & 0xff;
    current_chunk()->code[offset + 1] = jump & 0xff;
}

/// @brief コンパイラを初期化する
/// @param compiler 
/// @param type 
static void init_compiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function = new_function();
    current = compiler;
    if (type != TYPE_SCRIPT) {
        // 関数宣言なら関数名を解析する
        current->function->name = copy_string(parser.previous.start, parser.previous.length);
    }

    Local* local = &current->locals[current->local_count];
    current->local_count += 1;
    local->depth = 0;
    local->is_captured = false;
    local->name.start = "";
    local->name.length = 0;
}

/// @brief 数値リテラルを解析する
static void number(bool can_assign) {
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

/// @brief or演算子を解析する
/// @param can_assign 
static void or_(bool can_assign) {
    // 偽性なら OP_JUMP を飛び越える
    int else_jump = emit_jump(OP_JUMP_IF_FALSE);
    // そうでないなら条件式を飛び越える
    int end_jump = emit_jump(OP_JUMP);

    patch_jump(else_jump);
    emit_byte(OP_POP);

    parse_precedence(PREC_OR);

    patch_jump(end_jump);
}

/// @brief and演算子を解析する
/// @param can_assign 
static void and_(bool can_assign) {
    int end_jump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    parse_precedence(PREC_AND);

    patch_jump(end_jump);
}

static void string(bool can_assign) {
    emit_constant(OBJ_VAL(copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

/// @brief 変数名を解析する
/// @param name 
/// @param can_assign 
static void named_variable(Token name, bool can_assign) {
    uint8_t get_op, set_op;
    int arg = resolve_local(current, &name);

    if (arg != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    } else if ((arg = resolve_upvalue(current, &name)) != -1) {
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    } else {
        arg = identifier_constant(&name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }
    
    if (can_assign && match(TOKEN_EQUAL)) {
        // set式（代入式）
        expression();
        emit_bytes(set_op, (uint8_t)arg);
    } else {
        // get式
        emit_bytes(get_op, (uint8_t)arg);
    }
}

/// @brief 識別子を解析する
static void variable(bool can_assign) {
    named_variable(parser.previous, can_assign);
}

static void unary(bool can_assign) {
    TokenType operator_type = parser.previous.type;

    // 被演算子をコンパイルする
    parse_precedence(PREC_UNARY);

    switch (operator_type) {
        case TOKEN_BANG: emit_byte(OP_NOT); break;
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default: return; // Unreachable
    }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
    [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

/// @brief 現在のトークンから，指定した優先度より上位の式を解析する
/// @param precedence 
static void parse_precedence(Precedence precedence) {
    advance();
    // 現在のトークンに対応する前置パーサを探索する
    ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == NULL) {
        error("Expect expression.");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    // 後置パーサの探索
    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        ParseFn infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule(can_assign);
    }
}

/// @brief TokenTypeに対応するルールを返却する
/// @param type ルールに対応するTokenType
/// @return TokenTypeに対応するルール
static ParseRule* get_rule(TokenType type) {
    return &rules[type];
}

/// @brief 式を解析する
static void expression() {
    // 式全体を解析する
    parse_precedence(PREC_ASSIGNMENT);
}

/// @brief ブロックを解析する
static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

/// @brief 関数を解析する
/// @param type 
static void function(FunctionType type) {
    Compiler compiler;
    init_compiler(&compiler, type);
    begin_scope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity += 1;
            if (current->function->arity > 255) {
                error_at_current("Can't have more than 255 parameters.");
            }
            uint8_t constant = parse_variable("Expect parameter name.");
            define_variable(constant);
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = end_compiler();
    emit_bytes(OP_CLOSURE, make_constant(OBJ_VAL(function)));

    // 各上位値は2byteのオペランドを持つ
    for (int i = 0; i < function->upvalue_count; i++) {
        // オペランド1: 1ならすぐ外側の関数にあるローカル変数をキャプチャする
        // 0なら上位値をキャプチャする
        emit_byte(compiler.upvalues[i].is_local ? 1 : 0);
        // オペランド2: 上位値のインデックス
        emit_byte(compiler.upvalues[i].index);
    }
}

/// @brief 関数宣言を解析する
static void fun_declaration() {
    uint8_t global = parse_variable("Expect function name");
    mark_initialized();
    function(TYPE_FUNCTION);
    define_variable(global);
}

/// @brief 変数宣言を解析する
static void var_declaration() {
    uint8_t global = parse_variable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        // 初期化式がなければnilで初期化したものとする
        emit_byte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "Expect \';\' after declaration");

    define_variable(global);
}

/// @brief 式文を解析する
static void expression_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect \';\' after expression");
    emit_byte(OP_POP);
}

/// @brief for文を解析する
static void for_statement() {
    begin_scope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    
    // 初期化部
    if (match(TOKEN_SEMICOLON)) {
        ;
    } else if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        expression_statement();
    }

    int loop_start = current_chunk()->count;

    // 条件部
    int exit_jump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // 条件が偽ならループを抜ける
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP);
    }

    // インクリメント部
    if (!match(TOKEN_RIGHT_PAREN)) {
        int body_jump = emit_jump(OP_JUMP);
        int increment_start = current_chunk()->count;
        expression();
        emit_byte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emit_loop(loop_start);
        loop_start = increment_start;
        patch_jump(body_jump);
    }

    statement();

    emit_loop(loop_start);

    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP);
    }

    end_scope();
}

/// @brief if文を解析する
static void if_statement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();

    int else_jump = emit_jump(OP_JUMP);

    patch_jump(then_jump);
    emit_byte(OP_POP);

    if (match(TOKEN_ELSE)) {
        statement();
    }

    patch_jump(else_jump);
}

/// @brief print文を解析する
static void print_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect \';\' after value");
    emit_byte(OP_PRINT);
}

static void return_statement() {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON)) {
        emit_return();
    } else {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emit_byte(OP_RETURN);
    }
}

/// @brief while文を解析する
static void while_statement() {
    int loop_start = current_chunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after while.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit_byte(OP_POP);
}

/// @brief 同期を開始し，文の境界に達するまでトークンを捨てる
static void synchronize() {
    parser.panic_mode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) {
            return;
        }

        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                ;
        }
    }
}

/// @brief 宣言を解析する
static void declaration() {
    if (match(TOKEN_FUN)) {
        fun_declaration();
    } else if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        statement();
    }

    if (parser.panic_mode) {
        synchronize();
    }
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        print_statement();
    } else if (match(TOKEN_FOR)) {
        for_statement();
    } else if (match(TOKEN_IF)) {
        if_statement();
    } else if (match(TOKEN_RETURN)) {
        return_statement();
    } else if (match(TOKEN_WHILE)) {
        while_statement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        begin_scope();
        block();
        end_scope();
    } else {
        expression_statement();
    }
}

ObjFunction* compile(const char* source) {
    init_scanner(source);

    Compiler compiler;
    init_compiler(&compiler, TYPE_SCRIPT);

    parser.had_error = false;
    parser.panic_mode = false;

    advance();
    
    while (!match(TOKEN_EOF)) {
        declaration();
    }

    ObjFunction* function = end_compiler();
    return parser.had_error ? NULL : function;
}

void mark_compiler_roots() {
    Compiler* compiler = current;
    while (compiler != NULL) {
        mark_object((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}