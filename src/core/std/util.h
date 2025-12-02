// Returns pointer to element if found, NULL otherwise
#define ArrayHas(array, target, predicate)                                                         \
ArrayHas_(array, ArrayCount(array), sizeof(*(array)), predicate, &(target))

#define ArrayHasCount(array, count, target, predicate)                                             \
ArrayHas_(array, count, sizeof(*(array)), predicate, &(target))

int MatchString(const void *elem, void *ctx) {
    const char *candidate = *(const char *const *)elem; // array element
    const char *target = (const char *)ctx;             // string to find
    printf("%s\n", candidate);
    printf("%s\n", target);
    return strcmp(candidate, target) == 0;
}

bool ArrayHas_(void *array, usize count, usize elem_size,
               int (*predicate)(const void *elem, void *ctx), void *ctx) {
    printf("%zu\n", count);
    for (usize i = 0; i < count; i++) {
        void *elem = (char *)array + i * elem_size;
        if (predicate(elem, ctx)) {
            printf("found!!!!!!!!!\n");
            return true;
            // return elem;
        }
        printf("found!!!!!!!!!\n");
    }
    return false;
    // return NULL;
}

bool StringArrayHas(char **array, usize count, char *word) {
    for EachIndex(i, count) {
        if (strcmp(array[i], word) == 0) {
            return true;
        }
    }
    return false;
}
