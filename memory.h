#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include "common.h"

// 現在の容量か新しい容量を算出する
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

// 指定したサイズに合わせて配列を作成または拡大する
#define GROW_ARRAY(type, pointer, old_count, new_count) \
    (type*)reallocate(pointer, sizeof(type) * (old_count), \
        sizeof(type) * (new_count))

// 配列を解放する
#define FREE_ARRAY(type, pointer, old_count) \
    reallocate(pointer, sizeof(type) * (old_count), 0)

// メモリの再割り当てを行う
//
// old_size, new_size   , 行う演算
// --------+------------+------------------------
// 0       , 0以外       , 新しいブロックを割り当てる
// 0以外    , 0          , 割り当てを解放する
// 0以外    , < old_size , 既存の割り当てを縮小する
// 0以外    , > old_size , 既存の割り当てを拡大する
void* reallocate(void* pointer, size_t old_size, size_t new_size);

#endif