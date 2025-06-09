#ifndef UTILS_H
#define UTILS_H

#include "str.h"

void ika_log(LogType level, const char* fmt, ...);

char* read_entire_file(const char* path);

Str get_dir_name(Str path);

#endif
