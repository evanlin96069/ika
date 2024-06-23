#include "utils.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ika_log(int level, const char* fmt, ...) {
    switch (level) {
        case LOG_INFO:
            fprintf(stderr, "info: ");
            break;
        case LOG_NOTE:
            fprintf(stderr, "\x1b[36mnote:\x1b[0m ");
            break;
        case LOG_WARNING:
            fprintf(stderr, "\x1b[33mwarning:\x1b[0m ");
            break;
        case LOG_ERROR:
            fprintf(stderr, "\x1b[31merror:\x1b[0m ");
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
        ika_log(LOG_ERROR, "cannot open file %s: %s\n", path, strerror(errno));
        return NULL;
    }

    long size;
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    char* buf = malloc(size + 1);
    if (!buf) {
        ika_log(LOG_ERROR, "failed to allocate memory\n");
        fclose(fp);
        return NULL;
    }

    if (size > 0) {
        int n_read = fread(buf, size, 1, fp);
        fclose(fp);

        if (n_read != 1) {
            ika_log(LOG_ERROR, "failed to read file: %s\n", path);
            free(buf);
            return NULL;
        }
    }

    buf[size] = '\0';

    return buf;
}
