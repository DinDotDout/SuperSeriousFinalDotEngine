#ifndef DEBUG_ALLOC_H
#define DEBUG_ALLOC_H

// (jd) This is mainly used to instrospect into library allocation patterns
#if DOT_DEBUG_ALLOCATORS
internal void *malloc_debug(u64 size, char *file, u32 line);
internal void *realloc_debug(void *ptr, u64 new_size, char *file, u32 line);
internal void  free_debug(void *ptr, char *file, u32 line);
#endif

// (jd) Add debug allocators
//

#endif // !DEBUG_ALLOC_H
