/* This file is dedicated to the public domain. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static _Noreturn void _opt_bad(const char *s, char c) {
    fprintf(stderr, "\x1b[31merror:\x1b[0m %s%c\n", s, c);
    exit(1);
}

/*
 * Parses POSIX-compliant command-line options. Takes the names of argc and argv
 * plus a braced code block containing switch cases matching each option
 * character. The default case is already provided and causes an error message
 * to be printed and the program to exit.
 */
#define FOR_OPTS(argc, argv, CODE)                                        \
    do {                                                                  \
        --(argc);                                                         \
        ++(argv);                                                         \
        for (const char *_p = *(argv), *_p1; *(argv);                     \
             _p = *++(argv), --(argc)) {                                  \
            (void)(_p1); /* avoid unused warnings if OPTARG isn't used */ \
            if (*_p != '-' || !*++_p)                                     \
                break;                                                    \
            if (*_p == '-' && !*(_p + 1)) {                               \
                ++(argv);                                                 \
                --(argc);                                                 \
                break;                                                    \
            }                                                             \
            while (*_p) {                                                 \
                switch (*_p++) {                                          \
                    default:                                              \
                        _opt_bad("invalid option: -", *(_p - 1));         \
                        CODE                                              \
                }                                                         \
            }                                                             \
        }                                                                 \
    } while (0)

/*
 * Produces the string given as a parameter to the current option - must only be
 * used inside one of the cases given to FOR_OPTS. If there is no parameter
 * given, prints an error message and exits.
 */
#define OPTARG(argc, argv)                                               \
    (*_p ? (_p1 = _p, _p = "", _p1)                                      \
         : (*(--(argc), ++(argv))                                        \
                ? *(argv)                                                \
                : (_opt_bad("missing argument for option -", *(_p - 1)), \
                   (char *)0)))
