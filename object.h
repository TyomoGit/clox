#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

// valueを*ObjStringとする
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
// valueをchar*にする
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    // GC用の，オブジェクトの連結リスト
    struct Obj* next;
};

// 先頭の数バイトはobjと一致する（ポインタのキャスト可能）
struct ObjString {
    Obj obj;
    int length;
    char* chars;
};

/// @brief 文字列を所有する
/// @param chars 
/// @param length 
/// @return 
ObjString* take_string(char* chars, int length);

ObjString* copy_string(const char* chars, int length);

/// @brief ヒープに割り当てたオブジェクトをプリントする
/// @param value 
void print_object(Value value);

/// @brief Valueの型が指定したものと一致するかどうかを判定する
/// @param value 
/// @param type 
/// @return 
static inline bool is_obj_type(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif