#ifndef UTILS_H
#define UTILS_H

#define LOG_DEBUG 0
#define LOG_NOTE 1
#define LOG_WARNING 2
#define LOG_ERROR 3

void ika_log(int level, const char* fmt, ...);

char* read_entire_file(const char* path);

char* dirname(const char* path);

char* strdup(const char* str);

#endif
