#include "error.h"

#include <stdio.h>

void print_message(LogType level, const SourceState* src, Error* err) {
#ifdef NDEBUG
    if (level == LOG_DEBUG) {
        return;
    }
#endif

    const SourceFile* file = &src->files.data[err->pos.line.file_index];
    const char* filename = file->filename;
    const char* line = err->pos.line.content;
    int lineno = err->pos.line.lineno;
    int pos = err->pos.index;
    int included_by = file->pos.line.file_index;

    if (file->is_open == 0) {
        fprintf(stderr, "%s: ", filename);
        ika_log(level, "%s\n", err->msg);
        return;
    }

    if (err->pos.line.file_index != 0) {
        const SourceFile* included_file = &src->files.data[included_by];
        fprintf(stderr, "In file included from %s:%d%c\n",
                included_file->filename, file->pos.line.lineno,
                included_by == 0 ? ':' : ',');
        file = included_file;
        while (included_by != 0) {
            included_by = file->pos.line.file_index;
            included_file = &src->files.data[included_by];
            fprintf(stderr, "                 from %s:%d%c\n",
                    included_file->filename, file->pos.line.lineno,
                    included_by == 0 ? ':' : ',');
            file = included_file;
        }
    }

    fprintf(stderr, "%s:%d:%d: ", filename, lineno, pos);
    ika_log(level, "%s\n", err->msg);

    fprintf(stderr, "%5d | %s\n", lineno, line);

    if (pos > 0) {
        fprintf(stderr, "      | %*c^\n", pos, ' ');
    } else {
        fprintf(stderr, "      | ^\n");
    }
}
