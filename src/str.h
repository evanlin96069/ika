#include <string.h>

typedef struct Str {
    const char* ptr;
    size_t len;
} Str;

static inline Str str(const char* s) { return (Str){s, strlen(s)}; }

static inline int str_eql(Str s1, Str s2) {
    if (s1.len != s2.len)
        return 0;
    if (s1.ptr == s2.ptr)
        return 1;
    for (size_t i = 0; i < s1.len; i++) {
        if (s1.ptr[i] != s2.ptr[i])
            return 0;
    }
    return 1;
}
