/*
仮想マシン
*/

#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "object.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

/// @brief 関数のローカル変数
typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    /// @brief VMのスタックでこの関数が利用できるスロット
    Value* slots;
} CallFrame;

/// @brief 仮想マシン
typedef struct {
    /// @brief コールフレーム
    CallFrame frames[FRAMES_MAX];
    /// @brief framesの長さ
    int frame_count;

    /// @brief スタック
    Value stack[STACK_MAX];
    /// @brief スタックの一番上
    Value* stack_top;

    /// @brief グローバルのハッシュ表
    Table globals;

    /// @brief インターン化された文字列の集合
    Table strings;

    /// @brief オープンな上位値の配列
    ObjUpvalue* open_upvalues;

    /// @brief GC用の連結リスト
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