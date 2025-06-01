#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define ARENA_IMPLEMENTATION
#define ARENA_NOALLOC
#define ARENA_SIZE 1024

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#define ARENA_LOCK() pthread_mutex_lock(&lock)
#define ARENA_UNLOCK() pthread_mutex_unlock(&lock)

#include "../arena.h"

int main(void)
{
    arena_t *arena = arena_init();
    if (!arena)
    {
        fprintf(stderr, "Arena init error: %s\n", arena_error(NULL));
        return 1;
    }

    // Example: allocate 128 bytes (no alignment grantees)
    void *p = arena_alloc(arena, 128);
    if (!p)
    {
        fprintf(stderr, "Arena alloc error: %s\n", arena_error(arena));
        return 1;
    }
    printf("Allocated 128 bytes at %p\n", p);
    return 0;
}
