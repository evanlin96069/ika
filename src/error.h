#ifndef ERROR_H
#define ERROR_H

#include "source.h"

#define ERROR_MAX_LENGTH 255

typedef struct Error {
    SourcePos pos;
    char msg[ERROR_MAX_LENGTH];
} Error;

void print_message(LogType level, const SourceState* src, Error* err);

static inline void print_err(const SourceState* src, Error* err) {
    print_message(LOG_ERROR, src, err);
}

static inline void print_warn(const SourceState* src, Error* err) {
    print_message(LOG_WARNING, src, err);
}

#endif
