#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#define MAX_INCLUDE_DEPTH 15

char* pp_expand(const char* filename, int depth);

#endif
