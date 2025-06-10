#ifndef UTILS_H
#define UTILS_H

#include "str.h"

void ika_log(LogType level, const char* fmt, ...);

struct UtlAllocator;

char* read_entire_file(struct UtlAllocator* allocator, const char* path);

Str get_dir_name(Str path);

#endif
