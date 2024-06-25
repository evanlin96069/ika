#include "preprocessor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"
#include "utils.h"

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
        size_t start_pos = pos;
        while (c == ' ' || c == '\t') {
            pos++;
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

            char* dir = dirname(filename);
            if (!dir) {
                ika_log(LOG_ERROR, "failed to allocate memory\n");
                return NULL;
            }

            size_t inc_path_len = strlen(dir) + 1 + s.len;
            char* inc_path = malloc(inc_path_len + 1);
            if (!inc_path) {
                ika_log(LOG_ERROR, "failed to allocate memory\n");
                free(dir);
                return NULL;
            }
            snprintf(inc_path, inc_path_len + 1, "%s/%.*s", dir, s.len, s.ptr);
            free(dir);

            char* inc_src = pp_expand(inc_path, depth + 1);
            free(inc_path);
            if (!inc_src) {
                ika_log(LOG_ERROR, "failed to include file \"%.*s\"\n", s.len,
                        s.ptr);
                return NULL;
            }

            int inc_src_len = strlen(inc_src);
            src_len += inc_src_len;
            result = realloc(result, src_len);
            if (!result) {
                free(inc_src);
                ika_log(LOG_ERROR, "failed to allocate memory\n");
                return NULL;
            }
            memcpy(result + r_off, inc_src, inc_src_len);
            r_off += inc_src_len;
            free(inc_src);
        } else {
            pos = start_pos;
            c = *(src + pos);
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
