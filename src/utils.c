#include "utils.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ika_log(int level, const char* fmt, ...) {
    switch (level) {
        case LOG_DEBUG:
#ifdef _DEBUG
            fprintf(stderr, "\x1b[1mdebug:\x1b[0m ");
#else
            return;
#endif
            break;
        case LOG_NOTE:
            fprintf(stderr, "\x1b[1;96mnote:\x1b[0m ");
            break;
        case LOG_WARNING:
            fprintf(stderr, "\x1b[1;95mwarning:\x1b[0m ");
            break;
        case LOG_ERROR:
            fprintf(stderr, "\x1b[1;91merror:\x1b[0m ");
            break;
        default:
            break;
    }

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

char* read_entire_file(const char* path) {
    FILE* fp = fopen(path, "r");
    if (!fp) {
        return NULL;
    }

    long size;
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    char* buf = malloc(size + 1);
    if (!buf) {
        fclose(fp);
        errno = ENOMEM;
        return NULL;
    }

    if (size > 0) {
        int n_read = fread(buf, size, 1, fp);
        fclose(fp);

        if (n_read != 1) {
            free(buf);
            errno = EIO;
            return NULL;
        }
    }

    buf[size] = '\0';

    return buf;
}

Str get_dir_name(Str path) {
    if (path.len == 0) {
        return str(".");
    }

    int end_index = path.len - 1;
    while (path.ptr[end_index] == '/'
#ifdef _WIN32
           || path.ptr[end_index] == '\\'
#endif
    ) {
        if (end_index == 0) {
            return str("/");
        }
        end_index--;
    }

    while (path.ptr[end_index] != '/'
#ifdef _WIN32
           && path.ptr[end_index] != '\\'
#endif
    ) {
        if (end_index == 0) {
            return str(".");
        }
        end_index--;
    }

    if (end_index == 0 && (path.ptr[0] == '/'
#ifdef _WIN32
                           || path.ptr[0] == '\\'
#endif
                           )) {
        return str("/");
    }

    if (end_index == 0) {
        return str(".");
    }

    path.len = end_index + 1;
    return path;
}
