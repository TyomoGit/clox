#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include "common.h"

/// @brief あらゆるオブジェクト
typedef struct Obj Obj;

/// 文字列オブジェクト
typedef struct ObjString ObjString;

/// @brief VMが組み込みでサポートする型の種類
typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;

/// @brief VMが組み込みでサポートする型
typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

// Valueがnilかどうかを判定する
#define IS_NIL(value) ((value).type == VAL_NIL)
// ValueがObjかどうかを判定する
#define IS_OBJ(value) ((value).type == VAL_OBJ)
// Valueがnumberかどうかを判定する
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
// Valueがbooleanかどうかを判定する
#define IS_BOOL(value) ((value).type == VAL_BOOL)

// ValueからObjへのポインタを生成する
#define AS_OBJ(value) ((value).as.obj)
// ValueからC言語のboolを生成する
#define AS_BOOL(value) ((value).as.boolean)
// ValueからC言語のdoubleを生成する
#define AS_NUMBER(value) ((value).as.number)


// Cの値をLoxのbooleanに変換する
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
// Loxのnil
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
// Cの値をLoxのnumberに変換する
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
// ObjへのポインタをLoxオブジェクトに変換する
#define OBJ_VAL(object) ((Value) {VAL_OBJ, {.obj = (Obj*) object}})

/// @brief Valueの配列をもつ構造体
typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

/// @brief 等価を判定する
/// @param a 判定される値
/// @param b 判定される値
/// @return 等価かどうか
bool values_equal(Value a, Value b);

/// @brief ValueArrayを初期化する
/// @param array 
void init_value_array(ValueArray* array);

/// @brief ValueArrayにValueを追加する
/// @param array 対象のValueArray
/// @param value 追加する値
void write_value_array(ValueArray* array, Value value);
void free_value_array(ValueArray* array);

/// @brief Valueを表示する
/// @param value 表示するValue
void print_value(Value value);

#endif