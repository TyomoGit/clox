#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "object.h"
#include "vm.h"

/// @brief ソースコードをバイトコードに変換する
/// @param source ソースコード
/// @return エラーがなければ関数を返し，エラーならNULL
ObjFunction* compile(const char* source);

#endif