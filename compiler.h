#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "vm.h"

/// @brief ソースコードをバイトコードに変換する
/// @param source ソースコード
/// @param chunk 初期化されたチャンク
/// @return 成功したかどうか
bool compile(const char* source, Chunk* chunk);

#endif