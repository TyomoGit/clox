#ifndef CLOX_DEBUG_h
#define CLOX_DEBUG_h

#include "chunk.h"

// チャンクの配列を逆アセンブルする
void disassemble_chunk(Chunk* chunk, const char* name);

// 一つの命令を逆アセンブルする
int disassemble_instruction(Chunk* chunk, int offset);

#endif