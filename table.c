#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void init_table(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void free_table(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    init_table(table);
}

/// @brief ハッシュ表でエントリを探す
/// @param entries エントリの配列
/// @param capacity エントリの配列の容量
/// @param key 対象のキー
/// @return 
static Entry* find_entry(Entry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL; // 墓標

    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // 空
                return tombstone != NULL ? tombstone : entry;
            } else if (tombstone == NULL) {
                // 墓標
                tombstone = entry;
            }
        } else if (entry->key == key) {
            // キー
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

bool table_get(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) {
        return false;
    }

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) {
        return false;
    }

    *value = entry->value;
    return true;
}

/// @brief ハッシュ表のバケット配列を作成して初期化する
/// @param table 
/// @param capacity 
static void adjust_capacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);

    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    //古い配列で，空でないパケットを新しい配列に入れる
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) {
            continue;
        }

        Entry* dest = find_entry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count += 1;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);

    table->entries = entries;
    table->capacity = capacity;
}

bool table_set(Table* table, ObjString* key, Value value) {
    // 容量がないときは拡大する
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    Entry* entry = find_entry(table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;
    if (is_new_key || IS_NIL(entry->value)) {
        // 新しいキーで墓標でない
        table->count += 1;
    }

    entry->key = key;
    entry->value = value;

    return is_new_key;
}

bool table_delete(Table* table, ObjString* key) {
    if (table->count == 0) {
        return false;
    }

    Entry* entry = find_entry(table->entries, table->capacity, key);
    // 見つからない場合は終わり
    if (entry->key == NULL) {
        return false;
    }

    // 削除したという印（墓標）
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void table_add_all(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            table_set(to, entry->key, entry->value);
        }
    }
}

ObjString* table_find_string(Table* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0) {
        return NULL;
    }

    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // 空を見つけたら終わり
                return NULL;
            }
        } else if (
            entry->key->length == length
            && entry->key->hash == hash
            && memcmp(entry->key->chars, chars, length) == 0
        ) {
            // ハッシュだけではなく，厳密に文字列を比較する
            // 文字列を見つけた
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}

void table_remove_white(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.is_marked) {
            table_delete(table, entry->key);
        }
    }
}

void mark_table(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        mark_object((Obj*)entry->key);
        mark_value(entry->value);
    }
}