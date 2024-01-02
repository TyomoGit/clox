#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

// 関数かどうか
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
// 文字列かどうか
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

// valueをObjFunction*とする
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
// valueをObjString*とする
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
// valueをchar*にする
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

/// @brief Loxオブジェクトの種類
typedef enum {
    /// @brief 関数
    OBJ_FUNCTION,
    /// @brief 文字列
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    // GC用の，オブジェクトの連結リスト
    struct Obj* next;
};

/// @brief 関数オブジェクト
typedef struct {
    /// @brief Loxオブジェクトしてのヘッダ
    Obj obj;
    /// @brief パラメータの数
    int arity;
    /// @brief コード
    Chunk chunk;
    /// @brief 関数名
    ObjString* name;
} ObjFunction;

// 先頭の数バイトはobjと一致する（ポインタのキャスト可能）
struct ObjString {
    Obj obj;
    int length;
    char* chars;
    // 文字列のハッシュ
    uint32_t hash;
};

/// @brief 新しい関数を作る
/// @return 新しい関数
ObjFunction* new_function();
/// @brief 文字列を所有する
/// @param chars 
/// @param length 
/// @return 
ObjString* take_string(char* chars, int length);

/// @brief 文字列をコピーする
/// @param chars 
/// @param length 
/// @return 
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