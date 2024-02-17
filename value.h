#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include <string.h>

#include "common.h"

/// @brief あらゆるオブジェクト
typedef struct Obj Obj;

/// 文字列オブジェクト
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

#define SIGN_BIT ((uint64_t)0x8000000000000000)
// quiet NaN
#define QNAN ((uint64_t)0x7ffc000000000000)

#define TAG_NIL 1
#define TAG_FALSE 2
#define TAG_TRUE 3

typedef uint64_t Value;

#define IS_BOOL(value) (((value) | 1) == TRUE_VAL)
#define IS_NIL(value) ((value) == NIL_VAL)
#define IS_NUMBER(value) (((value) & QNAN) != QNAN)
#define IS_OBJ(value) (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value) ((value) == TRUE_VAL)
#define AS_NUMBER(value) value_to_num(value)
#define AS_OBJ(value) ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b) ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL ((Value)(uint64_t)(QNAN | TAG_NIL))
#define NUMBER_VAL(num) num_to_value(num)
#define OBJ_VAL(obj) (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline double value_to_num(Value value) {
    // double num;
    // memcpy(&num, &value, sizeof(Value));
    // return num;

    union {
        uint64_t bits;
        double num;
    } data;

    data.bits = value;
    return data.num;
}

/// @brief 
/// @param num 
/// @return 
static inline Value num_to_value(double num) {
    // Value value;
    // memcpy(&value, &num, sizeof(double));
    // return value;

    union {
        uint64_t bits;
        double num;
    } data;

    data.num = num;
    return data.bits;
}

#else

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

#endif

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