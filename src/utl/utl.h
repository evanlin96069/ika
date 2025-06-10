#ifndef UTL_H
#define UTL_H

#ifdef UTL_IMPLEMENTATION
#define UTLALLOCATOR_IMPLEMENTATION
#define UTLARENA_IMPLEMENTATION
#define UTLFIXEDBUF_IMPLEMENTATION
#define UTLSTACKFALLBACK_IMPLEMENTATION
#define UTLVECTOR_IMPLEMENTATION
#endif  // UTL_IMPLEMENTATION

#include "utldef.h"
#include "allocator/utlallocator.h"
#include "allocator/utlarena.h"
#include "allocator/utlfixedbuf.h"
#include "allocator/utlstackfallback.h"
#include "utlvector.h"

#endif  // UTL_H
