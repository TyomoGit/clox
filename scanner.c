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
                } else {
                    return;
                }
            default:
                return;
        }
    }
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
    }

    return error_token("Unexpected character.");
}