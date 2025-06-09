#ifndef ERROR_H
#define ERROR_H

#include "preprocessor.h"

#define ERROR_MAX_LENGTH 255

typedef struct Error {
    SourcePos pos;
    char msg[ERROR_MAX_LENGTH];
} Error;

void print_err(const SourceState* src, Error* err);

#endif
