#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

Scanner scanner;

void init_scanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static bool is_alpha(char c) {
    return ('a' <= c && c <= 'z')
        || ('A' <= c && c <= 'Z')
        || c == '_';
}

static bool is_digit(char c) {
    return '0' <= c && c <= '9';
}

/// @brief 文字列が終了したかを判定する
/// @return 文字列が終了したか
static bool isAtEnd() {
    return *scanner.current == '\0';
}

/// @brief 現在の文字を消費して返す
/// @return 現在の文字
static char advance() {
    scanner.current += 1;
    return scanner.current[-1];
}

/// @brief 現在の文字が指定された文字の場合は消費する
/// @param expected 確認する文字
/// @return 現在の文字が確認する文字と一致しているか
static bool match(char expected) {
    if (isAtEnd()) {
        return false;
    }

    if (*scanner.current != expected) {
        return false;
    }

    scanner.current += 1;
    return true;
}

/// @brief トークンの字句をキャプチャする
/// @param type トークンの種類
/// @return トークン
static Token make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;

    return token;
}

/// @brief エラートークンを生成する
/// @param message エラーメッセージ
/// @return エラートークン
static Token error_token(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

/// @brief 現在の文字を返す
/// @return 現在の文字
static char peek() {
    return *scanner.current;
}

/// @brief 現在位置の次の文字を返す
/// @return 現在位置の次の文字
static char peek_next() {
    if (isAtEnd()) {
        return '\0';
    }

    return scanner.current[1];
}

/// @brief 空白文字を飛ばす
static void skip_whitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line += 1;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    while (peek() != '\n' && !isAtEnd()) {
                        advance();
                    }
                    break;
                } else {
                    return;
                }
            default:
                return;
        }
    }
}

/// @brief 候補のキーワードと字句の残りの部分を比較する
/// @param start 開始位置
/// @param length 長さ
/// @param rest 候補のキーワード
/// @param type 候補のキーワードのTokenType
/// @return 候補のキーワードのTokenTypeまたはTOKEN_IDENTIFIER
static TokenType check_keyword(int start, int length, const char* rest, TokenType type) {
    if (scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

/// @brief 識別子を判別する
/// @return 結果
static TokenType identifier_type() {
    switch (scanner.start[0]) {
        case 'a': return check_keyword(1, 2, "nd", TOKEN_AND);
        case 'c': return check_keyword(1, 4, "lass", TOKEN_CLASS);
        case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i': return check_keyword(1, 1, "f", TOKEN_IF);
        case 'n': return check_keyword(1, 2, "il", TOKEN_NIL);
        case 'o': return check_keyword(1, 1, "r", TOKEN_OR);
        case 'p': return check_keyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return check_keyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h': return check_keyword(2, 2, "is", TOKEN_THIS);
                    case 'r': return check_keyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);
    }

    return TOKEN_IDENTIFIER;
}

static Token identifier() {
    while (is_alpha(peek()) || is_digit(peek())) {
        advance();
    }

    return make_token(identifier_type());
}

static Token number() {
    while (is_digit(peek())) {
        advance();
    }

    if (peek() == '.' && is_digit(peek_next())) {
        advance();

        while (is_digit(peek())) advance();
    }

    return make_token(TOKEN_NUMBER);
}

static Token string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') {
            scanner.line += 1;
        }
        advance();
    }

    if (isAtEnd()) {
        return error_token("Unterminated string.");
    }
    
    // 最後のダブルクオーテーション
    advance();
    return make_token(TOKEN_STRING);
}

/// @brief トークンを1つ読み込む
/// @return トークン
Token scan_token() {
    skip_whitespace();
    scanner.start = scanner.current;

    if (isAtEnd()) {
        return make_token(TOKEN_EOF);
    }

    char c = advance();

    if (is_alpha(c)) {
        return identifier();
    }
    if (is_digit(c)) {
        return number();
    }

    switch (c) {
        case '(': return make_token(TOKEN_LEFT_PAREN);
        case ')': return make_token(TOKEN_RIGHT_PAREN);
        case '{': return make_token(TOKEN_LEFT_BRACE);
        case '}': return make_token(TOKEN_RIGHT_BRACE);
        case ';': return make_token(TOKEN_SEMICOLON);
        case ',': return make_token(TOKEN_COMMA);
        case '.': return make_token(TOKEN_DOT);
        case '-': return make_token(TOKEN_MINUS);
        case '+': return make_token(TOKEN_PLUS);
        case '/': return make_token(TOKEN_SLASH);
        case '*': return make_token(TOKEN_STAR);
        case '!': return make_token(
            match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG
        );
        case '=': return make_token(
            match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL
        );
        case '<': return make_token(
            match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS
        );
        case '>': return make_token(
            match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER
        );
        case '"':
            return string();
    }

    return error_token("Unexpected character.");
}