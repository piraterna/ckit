/*
 * ============================================================================
 * arena.h - Minimal, Portable Freestanding Arena Allocator
 * ============================================================================
 *
 * Supported C standards:
 *     arena.h aims to support anything from ANSI C to C23, though ANSI C support
 *     may be broken.
 *
 * Overview:
 *     This header provides a simple, drop-in arena allocator suitable for
 *     freestanding or embedded environments where standard malloc/free may not
 *     be available or desirable. It supports both dynamic memory allocation and
 *     a static buffer fallback for environments without dynamic memory.
 *
 * Features:
 *   - Dynamic arena allocation (default): allocates arena memory via
 *     configurable malloc/free macros.
 *   - Static arena allocation (#define ARENA_NOALLOC): uses fixed-size
 *     static buffer specified by #define ARENA_SIZE.
 *   - Optional thread safety with user-defined ARENA_LOCK() / ARENA_UNLOCK().
 *   - Simple API: init, alloc, alloc_aligned, reset, destroy, and error reporting.
 *   - No individual free calls; only whole-arena reset or destroy.
 *   - Optional namespace support for C++ via ARENA_NAMESPACE.
 *
 * Usage Examples:
 * -------------
 * // 1. Dynamic arena allocation (default mode):
 * #define ARENA_IMPLEMENTATION
 * #include "arena.h"
 *
 * arena_t *a = arena_init(4096);  // Create 4KB arena
 * if (!a) { puts(arena_error(NULL)); exit(1); }
 *
 * void *p = arena_alloc(a, 128);  // Allocate 128 bytes
 * if (!p) { puts(arena_error(a)); }
 *
 * arena_reset(a);  // Reset arena for reuse
 *
 * arena_destroy(a); // Free arena memory
 *
 * // 2. Static arena (no malloc/free):
 * #define ARENA_NOALLOC
 * #define ARENA_SIZE 8192
 * #define ARENA_IMPLEMENTATION
 * #include "arena.h"
 *
 * arena_t *a = arena_init(); // No size needed
 * if (!a) { puts(arena_error(NULL)); exit(1); }
 *
 * void *p = arena_alloc(a, 128);
 * if (!p) { puts(arena_error(a)); }
 *
 * arena_reset(a);  // Reset arena for reuse
 * // No destroy needed for static arena
 *
 * // 3. Thread safety (example with pthread mutex):
 * #define ARENA_LOCK() pthread_mutex_lock(&lock)
 * #define ARENA_UNLOCK() pthread_mutex_unlock(&lock)
 * pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
 *
 * Notes:
 * ------
 * - Only whole-arena reset or destroy; no per-allocation free.
 * - arena_alloc_aligned() provides aligned allocations (alignment must be
 *   power of two).
 * - arena_error() returns last error message for debugging.
 * - Configurable allocation functions via ARENA_MALLOC/ARENA_FREE macros.
 * - Compatible with C and C++ (with optional namespace support).
 *
 * License:
 * --------
 * Apache License 2.0 (see LICENSE file or
 * https://www.apache.org/licenses/LICENSE-2.0)
 *
 * ============================================================================
 */

#ifndef ARENA_H
#define ARENA_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* ============================================================================
     * Arena Structure
     * ============================================================================
     */
    typedef struct arena_t
    {
        unsigned char *data;    /* Pointer to arena memory */
        unsigned long capacity; /* Total size in bytes */
        unsigned long pos;      /* Current offset / allocation position */
        const char *error;      /* Last error string (per arena) */
    } arena_t;

/* ============================================================================
 * Function Prefixing
 * ============================================================================
 */
#ifdef ARENA_NAMESPACE
#define _ARENA_PREFIX(name) name
#else
#define _ARENA_PREFIX(name) arena_##name
#endif

/* ============================================================================
 * Thread-Safety Hooks (optional)
 * ============================================================================
 */
#ifndef ARENA_LOCK
#define ARENA_LOCK()
#endif

#ifndef ARENA_UNLOCK
#define ARENA_UNLOCK()
#endif

/* ============================================================================
 * Allocation Hooks (unless NOALLOC is defined)
 * ============================================================================
 */
#ifndef ARENA_NOALLOC
#ifndef ARENA_MALLOC
    extern void *_arena_alloc(unsigned long);
#define ARENA_MALLOC(size) _arena_alloc(size)
#endif

#ifndef ARENA_FREE
    extern void _arena_free(void *);
#define ARENA_FREE(ptr) _arena_free(ptr)
#endif
#endif /* ARENA_NOALLOC */

/* ============================================================================
 * Function Declarations
 * ============================================================================
 */
#ifdef ARENA_NOALLOC
    arena_t *_ARENA_PREFIX(init)(void); /* No size needed */
#else
    arena_t *_ARENA_PREFIX(init)(int size);
#endif
    void *_ARENA_PREFIX(alloc)(arena_t *arena, int size);
    void *_ARENA_PREFIX(alloc_aligned)(arena_t *arena, int size, int alignment);
    void _ARENA_PREFIX(reset)(arena_t *arena);
#ifndef ARENA_NOALLOC
    void _ARENA_PREFIX(destroy)(arena_t *arena);
#endif
    const char *_ARENA_PREFIX(error)(arena_t *arena);
#ifdef __cplusplus
} /* extern "C" */

#ifdef ARENA_NAMESPACE
namespace arena
{
    using ::alloc;
    using ::alloc_aligned;
    using ::arena_t;
    using ::error;
    using ::init;
    using ::reset;
#ifndef ARENA_NOALLOC
    using ::destroy;
#endif
}
#endif /* ARENA_NAMESPACE */
#endif /* __cplusplus */

#ifdef ARENA_IMPLEMENTATION

/* ============================================================================
 * Internal default error string when arena pointer is NULL
 * ============================================================================
 */
static const char *_arena_error_global = "no error";

/* ============================================================================
 * arena_error - returns the last error string for the arena, or global if NULL
 * ============================================================================
 */
const char *_ARENA_PREFIX(error)(arena_t *arena)
{
    if (arena)
        return arena->error ? arena->error : "no error";
    return _arena_error_global;
}

/* ============================================================================
 * Static Arena (if NOALLOC is enabled)
 * ============================================================================
 */
#ifdef ARENA_NOALLOC
#ifndef ARENA_SIZE
#error "ARENA_SIZE must be defined when using ARENA_NOALLOC"
#endif

static unsigned char _arena_static_data[ARENA_SIZE];
static arena_t _arena_static = {_arena_static_data, ARENA_SIZE, 0, NULL};
static int _arena_static_used = 0;
#endif

/* ============================================================================
 * arena_init
 * ============================================================================
 */
#ifdef ARENA_NOALLOC
arena_t *_ARENA_PREFIX(init)(void)
{
    ARENA_LOCK();

    if (_arena_static_used)
    {
        _arena_static.error = "static arena already used";
        ARENA_UNLOCK();
        return NULL;
    }

    _arena_static.pos = 0;
    _arena_static.error = "no error";
    _arena_static_used = 1;

    ARENA_UNLOCK();
    return &_arena_static;
}
#else
arena_t *_ARENA_PREFIX(init)(int size)
{
    ARENA_LOCK();

    arena_t *arena = (arena_t *)ARENA_MALLOC(sizeof(arena_t));
    if (!arena)
    {
        _arena_error_global = "out of memory (arena struct)";
        ARENA_UNLOCK();
        return NULL;
    }

    arena->data = (unsigned char *)ARENA_MALLOC(size);
    if (!arena->data)
    {
        ARENA_FREE(arena);
        _arena_error_global = "out of memory (arena data)";
        ARENA_UNLOCK();
        return NULL;
    }

    arena->capacity = size;
    arena->pos = 0;
    arena->error = "no error";

    ARENA_UNLOCK();
    return arena;
}
#endif

/* ============================================================================
 * arena_alloc
 * Allocates memory without alignment guarantees.
 * ============================================================================
 */
void *_ARENA_PREFIX(alloc)(arena_t *arena, int size)
{
    if (!arena)
    {
        _arena_error_global = "null arena";
        return NULL;
    }

    if (size <= 0)
    {
        arena->error = "invalid allocation size";
        return NULL;
    }

    ARENA_LOCK();

    if (arena->pos + (unsigned long)size > arena->capacity)
    {
        arena->error = "arena overflow";
        ARENA_UNLOCK();
        return NULL;
    }

    void *ptr = arena->data + arena->pos;
    arena->pos += size;
    arena->error = "no error";

    ARENA_UNLOCK();
    return ptr;
}

/* ============================================================================
 * arena_alloc_aligned
 * Allocates memory with specified alignment (must be power of two).
 * ============================================================================
 */
void *_ARENA_PREFIX(alloc_aligned)(arena_t *arena, int size, int alignment)
{
    if (!arena)
    {
        _arena_error_global = "null arena";
        return NULL;
    }

    if (size <= 0)
    {
        arena->error = "invalid allocation size";
        return NULL;
    }

    if (alignment <= 0 || (alignment & (alignment - 1)) != 0)
    {
        arena->error = "alignment must be power of two";
        return NULL;
    }

    ARENA_LOCK();

    unsigned long current_addr = (unsigned long)(arena->data + arena->pos);
    unsigned long offset = (alignment - (current_addr % alignment)) % alignment;
    unsigned long new_pos = arena->pos + offset;

    if (new_pos + (unsigned long)size > arena->capacity)
    {
        arena->error = "arena overflow (aligned)";
        ARENA_UNLOCK();
        return NULL;
    }

    void *ptr = arena->data + new_pos;
    arena->pos = new_pos + size;
    arena->error = "no error";

    ARENA_UNLOCK();
    return ptr;
}

/* ============================================================================
 * arena_reset - resets arena to reuse memory (pos = 0)
 * ============================================================================
 */
void _ARENA_PREFIX(reset)(arena_t *arena)
{
    if (!arena)
        return;

    ARENA_LOCK();

    arena->pos = 0;
    arena->error = "no error";

    ARENA_UNLOCK();
}

/* ============================================================================
 * arena_destroy - frees arena memory (only for dynamic arena)
 * ============================================================================
 */
#ifndef ARENA_NOALLOC
void _ARENA_PREFIX(destroy)(arena_t *arena)
{
    if (!arena)
        return;

    ARENA_LOCK();

    ARENA_FREE(arena->data);
    ARENA_FREE(arena);
    _arena_error_global = "no error";

    ARENA_UNLOCK();
}
#endif

/* ============================================================================
 * arena_used - returns number of bytes currently allocated (internal)
 * ============================================================================
 */
static inline unsigned long _ARENA_PREFIX(used)(arena_t *arena)
{
    if (!arena)
        return 0;
    return arena->pos;
}

/* ============================================================================
 * arena_capacity - returns total capacity of arena (internal)
 * ============================================================================
 */
static inline unsigned long _ARENA_PREFIX(capacity)(arena_t *arena)
{
    if (!arena)
        return 0;
    return arena->capacity;
}

#endif /* ARENA_IMPLEMENTATION */
#endif /* ARENA_H */
