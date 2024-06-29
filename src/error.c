#include "error.h"

#include <stdio.h>

#include "utils.h"

void print_err(SourceState* src, Error* err) {
    const SourceFile* file = &src->files[err->pos.line.file_index];
    const char* filename = file->filename;
    const char* line = err->pos.line.content;
    int lineno = err->pos.line.lineno;
    int pos = err->pos.index;
    int included_by = file->pos.line.file_index;

    if (err->pos.line.file_index != 0) {
        const SourceFile* included_file = &src->files[included_by];
        fprintf(stderr, "In file included from %s:%d%c\n",
                included_file->filename, file->pos.line.lineno,
                included_by == 0 ? ':' : ',');
        file = included_file;
        while (included_by != 0) {
            included_by = file->pos.line.file_index;
            included_file = &src->files[included_by];
            fprintf(stderr, "                 from %s:%d%c\n",
                    included_file->filename, file->pos.line.lineno,
                    included_by == 0 ? ':' : ',');
            file = included_file;
        }
    }

    fprintf(stderr, "%s:%d:%d: ", filename, lineno, pos);
    ika_log(LOG_ERROR, "%s\n", err->msg);

    fprintf(stderr, "%5d | %s\n", lineno, line);

    if (pos > 0) {
        fprintf(stderr, "      | %*c^\n", pos, ' ');
    } else {
        fprintf(stderr, "      | ^\n");
    }
}
