/*
仮想マシン
*/

#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "table.h"
#include "value.h"

#define STACK_MAX (UINT8_MAX+1)

/// @brief 仮想マシン
typedef struct {
    Chunk* chunk;

    // 次の命令の位置
    // instruction pointer
    uint8_t* ip;

    Value stack[STACK_MAX];
    Value* stack_top;

    // グローバルのハッシュ表
    Table globals;

    /// @brief インターン化された文字列の集合
    Table strings;

    //GC用の連結リスト
    Obj* objects;

} Vm;

/// @brief 実行結果
typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern Vm vm;

/// @brief 仮想マシンを初期化する
void init_vm();

/// @brief 仮想マシンを解放する
void free_vm();

/// @brief 命令の配列を実行する
/// @param chunk 命令の配列
/// @return 結果
InterpretResult interpret(const char* source);

/// @brief スタックにValueをプッシュする
/// @param value プッシュするValue
void push(Value value);

/// @brief スタックからValueをポップする
/// @return ポップした値
Value pop();

#endif