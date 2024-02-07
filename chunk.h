// コードの実行単位であるチャンクを表す構造体を定義する

#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
    // 定数を生成する
    OP_CONSTANT,
    // nilをプッシュする
    OP_NIL,
    // trueをプッシュする
    OP_TRUE,
    // falseプッシュする
    OP_FALSE,
    // スタックのトップから値をポップする
    OP_POP,
    // ローカル変数を取得する
    OP_GET_LOCAL,
    // ローカル変数を代入する
    OP_SET_LOCAL,
    // グローバル変数を取得する
    OP_GET_GLOBAL,
    // グローバル変数を定義する
    OP_DEFINE_GLOBAL,
    // グローバル変数を代入する
    OP_SET_GLOBAL,
    // ==
    OP_EQUAL,
    // >
    OP_GREATER,
    // <
    OP_LESS,
    // 加算
    OP_ADD,
    // 減算
    OP_SUBTRACT,
    // 乗算
    OP_MULTIPLY,
    // 除算
    OP_DIVIDE,
    // 否定の !
    OP_NOT,
    // 単項-
    OP_NEGATE,
    // プリントする
    OP_PRINT,
    // ジャンプする
    OP_JUMP,
    // falseならジャンプする 
    OP_JUMP_IF_FALSE,
    // ループ命令
    OP_LOOP,
    // コール
    OP_CALL,
    // 関数を抜ける
    OP_RETURN,
} OpCode;

// 動的配列
typedef struct {
    // 実際に格納されている要素の数
    int count;
    // 配列の容量
    int capacity;
    // バイトコードの配列
    uint8_t* code;
    // 行番号の配列
    int* lines;
    // 定数を格納する配列
    ValueArray constants;
} Chunk;

// チャンクを初期化する
void init_chunk(Chunk* chunk);

// チャンクの末尾に1バイトを追加する
void write_chunk(Chunk* chunk, uint8_t byte, int line);

// チャンクの定数部に新しい定数を書き込む
int add_constant(Chunk* chunk, Value value);

// チャンクを解放する
void free_chunk(Chunk* chunk);

#endif //CLOX_CHUNK_H