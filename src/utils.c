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

char* dirname(const char* path) {
    if (path == NULL || *path == '\0') {
        return strdup(".");
    }

    char* path_copy = strdup(path);
    if (path_copy == NULL) {
        return NULL;
    }

    size_t len = strlen(path_copy);
    while (len > 1 && path_copy[len - 1] == '/') {
        path_copy[--len] = '\0';
    }

    char* last_slash = strrchr(path_copy, '/');
    if (last_slash != NULL) {
        if (last_slash == path_copy) {
            last_slash[1] = '\0';
        } else {
            *last_slash = '\0';
        }
    } else {
        strcpy(path_copy, ".");
    }

    return path_copy;
}

char* strdup(const char* str) {
    if (str == NULL) {
        return NULL;
    }

    size_t len = strlen(str) + 1;
    char* copy = (char*)malloc(len);
    if (copy == NULL) {
        return NULL;
    }

    strcpy(copy, str);

    return copy;
}
