#include "utils.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ika_log(LogType level, const char* fmt, ...) {
    switch (level) {
        case LOG_DEBUG:
#ifdef NDEBUG
            return;
#else
            fprintf(stderr, "\x1b[1mdebug:\x1b[0m ");
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
            UNREACHABLE();
    }

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static void* never_fail_alloc(void* context, size_t size) {
    UNUSED(context);

    void* ptr = malloc(size);
    if (!ptr && size != 0) {
        ika_log(LOG_ERROR, "malloc: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static void* never_fail_remap(void* context, void* buf, size_t new_size) {
    UNUSED(context);

    void* ptr = realloc(buf, new_size);
    if (!ptr && new_size != 0) {
        ika_log(LOG_ERROR, "realloc: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static void never_fail_free(void* context, void* buf) {
    UNUSED(context);
    free(buf);
}

UtlAllocator never_fail_allocator = {
    .alloc = never_fail_alloc,
    .remap = never_fail_remap,
    .free = never_fail_free,
};

char* read_entire_file(UtlAllocator* allocator, const char* path) {
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }

    long size;
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    char* buf = allocator->alloc(allocator, size + 1);
    if (!buf) {
        fclose(fp);
        errno = ENOMEM;
        return NULL;
    }

    if (size > 0) {
        int n_read = fread(buf, size, 1, fp);
        fclose(fp);

        if (n_read != 1) {
            allocator->free(allocator, buf);
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

int file_is_readable(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}
