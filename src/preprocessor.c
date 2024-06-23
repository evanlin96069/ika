#include "preprocessor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "str.h"

char* pp_expand(const char* filename, int depth) {
    if (depth > MAX_INCLUDE_DEPTH) {
        ika_log(LOG_ERROR, "#include nested too deeply\n");
        return NULL;
    }

    char* src = read_entire_file(filename);
    if (!src) {
        return NULL;
    }

    int src_len = strlen(src);

    char* result = malloc(src_len + 1);
    size_t r_off = 0;
    if (!result) {
        ika_log(LOG_ERROR, "failed to allocate memory\n");
        return NULL;
    }

    size_t pos = 0;
    char c = *(src + pos);
    while (c != '\0') {
        while (c == ' ' || c == '\t') {
            pos++;
            result[r_off++] = c;
            c = *(src + pos);
        }

        if (strncmp("#include", src + pos, 8) == 0) {
            pos += 8;
            c = *(src + pos);
            while (c == ' ' || c == '\t') {
                pos++;
                c = *(src + pos);
            }

            if (c != '"') {
                ika_log(LOG_ERROR, "#include expects \"FILENAME\"\n");
                return NULL;
            }

            pos++;
            c = *(src + pos);
            Str s = {
                .ptr = src + pos,
                .len = 0,
            };

            while (c != '"' && c != '\n' && c != '\0') {
                s.len++;
                pos++;
                c = *(src + pos);
            }

            if (c != '"') {
                ika_log(LOG_ERROR, "missing terminating \" character\n");
                return NULL;
            }

            pos++;
            c = *(src + pos);

            char* include_filename = malloc(s.len + 1);
            snprintf(include_filename, s.len + 1, "%.*s", s.len, s.ptr);
            char* include_src = pp_expand(include_filename, depth + 1);
            if (!include_src) {
                return NULL;
            }

            int include_len = strlen(include_src);
            src_len += include_len;
            result = realloc(result, src_len);
            if (!result) {
                ika_log(LOG_ERROR, "failed to allocate memory\n");
                return NULL;
            }
            memcpy(result + r_off, include_src, include_len);
            r_off += include_len;
            free(include_src);
        }

        while (c != '\n' && c != '\0') {
            pos++;
            result[r_off++] = c;
            c = *(src + pos);
        }

        if (c == '\n') {
            pos++;
            result[r_off++] = c;
            c = *(src + pos);
        }
    }

    result[r_off] = '\0';
    return result;
}
