#if DOT_DEBUG_ALLOCATORS

internal void *
malloc_debug(u64 size, char *file, u32 line)
{
    void *mem = malloc(size);
    DOT_PRINT_FL(file, line, "malloc %p %M", mem, size);
    return(mem);
}

internal void *
realloc_debug(void *ptr, u64 new_size, char *file, u32 line)
{
    void *mem = realloc(ptr, new_size);
    DOT_PRINT_FL(file, line, "realloc %p -> %p %M", ptr, mem, new_size);
    return(mem);
}

internal void
free_debug(void *ptr, char *file, u32 line)
{
    DOT_PRINT_FL(file, line, "free %p", ptr);
    free(ptr);
}

#endif
