#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

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

/// @brief 
typedef void (*ParseFn)();

/// @brief 解析ルール
typedef struct {
    /// @brief 与えられた型のトークンで始まる前置式を解析する関数
    ParseFn prefix;
    /// @brief 与えられた型のトークンが左オペランドの次に来るような中置式を解析する関数
    ParseFn infix;
    /// @brief 与えられた型のトークンを演算子として使う中置式の優先度
    Precedence precedence;
} ParseRule;

/// @brief 唯一のパーサ
Parser parser;
/// @brief 現在のチャンク
Chunk* compiling_chunk;

/// @brief 現在のチャンクを返す
/// @return 現在のチャンク
static Chunk* current_chunk() {
    return compiling_chunk;
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

/// @brief OP_RETURNをチャンクに加える
static void emit_return() {
    emit_byte(OP_RETURN);
}

/// @brief コンパイルを終わる
static void end_compiler() {
    emit_return();
}

/// @brief 中置式を解析する
static void binary() {
    TokenType operator_type = parser.previous.type;
    ParseRule* rule = get_rule(operator_type);
    parse_precedence((Precedence)rule->precedence + 1);
    switch (operator_type) {
        case TOKEN_PLUS: emit_byte(OP_ADD); break;
        case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR: emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
        default:
            return; // Unreachable
    }
}

/// @brief 丸括弧で囲まれた式を解析する
static void grouping() {
    expression();
    consume(TOKEN_LEFT_PAREN, "Expect ')' after expression.");
}

/// @brief 値を定数表に追加する
/// @param value 定数表に追加される値
/// @return 追加した値のインデックス
static uint8_t make_constant(Value value) { 
    int constant = add_constant(value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

/// @brief 定数をロードするコードをチャンクに加える
/// @param value 定数
static void emit_constant(Value value) {
    emit_bytes(OP_CONSTANT, make_constant(value))
}

/// @brief 数値リテラルを解析する
static void number() {
    double value = strtod(parser.previous.start, NULL);
    emit_constant(value);
}

static void unary() {
    TokenType operator_type = parser.current.type;

    // 被演算子をコンパイルする
    parse_precedence(PREC_UNARY);

    switch (operator_type) {
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default: return; // Unreachable
    }
}

/// @brief 現在のトークンから，指定した優先度より上位の式を解析する
/// @param precedence 
static void parse_precedence(Precedence precedence) {

}

/// @brief 式を解析する
static void expression() {
    // 式全体を解析する
    parse_precedence(PREC_ASSIGNMENT)
}

bool compile(const char* source, Chunk* chunk) {
    init_scanner(source);
    compiling_chunk = chunk;

    parser.had_error = false;
    parser.panic_mode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");

    end_compiler();

    return !parser.had_error;
}