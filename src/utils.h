#ifndef UTILS_H
#define UTILS_H

#include "str.h"

struct UtlAllocator;

extern struct UtlAllocator never_fail_allocator;

char* read_entire_file(struct UtlAllocator* allocator, const char* path);

Str get_dir_name(Str path);

int file_is_readable(const char* path);

#endif
