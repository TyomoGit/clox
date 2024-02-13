#ifndef CLOX_TABLE_H
#define CLOX_TABLE_H

#include "common.h"
#include "value.h"

/// @brief キーと値のペア
typedef struct {
    ObjString* key;
    Value value;
} Entry;

/// @brief ハッシュ表
typedef struct {
    /// @brief エントリの個数
    int count;
    /// @brief 配列の容量
    int capacity;
    /// @brief 配列
    Entry* entries;
} Table;

/// @brief ハッシュ表を初期化する
/// @param table 初期化されるハッシュ表
void init_table(Table* table);

/// @brief ハッシュ表を開放する
/// @param table 解放されるハッシュ表
void free_table(Table* table);

/// @brief ハッシュ表のキーを探索する
/// @param table 探索するハッシュ表
/// @param key 探索するキー
/// @param value 見つかった場合は，その値を格納する
/// @return 
bool table_get(Table* table, ObjString* key, Value* value);

/// @brief キーと値のペアをハッシュ表に追加する
/// @param table ハッシュ表
/// @param key キー
/// @param value 値
/// @return 新規のエントリーかどうか
bool table_set(Table* table, ObjString* key, Value value);

/// @brief エントリを削除する
/// @param table 
/// @param key 
/// @return 
bool table_delete(Table* table, ObjString* key);

/// @brief ハッシュ表をコピーする
/// @param from コピー元
/// @param to コピー先
void table_add_all(Table* from, Table* to);

/// @brief ハッシュ表にある文字列を探索する
/// @param table 文字列を追加するテーブル
/// @param chars 追加されるC文字列
/// @param length 文字列の長さ
/// @param hash 文字列のハッシュ
/// @return 
ObjString* table_find_string(Table* table, const char* chars, int length, uint32_t hash);

/// @brief 文字列表から白色（到達不可能）のものを削除する
/// @param table 対象のテーブル
void table_remove_white(Table* table);

/// @brief 表にある全てのオブジェクトにマークをつける
/// @param table マークする表
void mark_table(Table* table);

#endif