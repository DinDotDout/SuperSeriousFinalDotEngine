#ifndef RB_BACKEND_UTILITY_H
#define RB_BACKEND_UTILITY_H

enum { RB_GPU_ARENA_ALIGN = 16 };

///////////////////////////////////////////////////////////
/// Write combined upload
///

typedef struct RB_CpuGpuArena
{
    RB_GpuHeap    heap;
    RB_GpuAddress gpu_base;
    i64           used;
    i64           size;
}RB_CpuGpuArena;

internal RB_CpuGpuArena rb_cpu_gpu_arena_create(RB_MemoryKind kind, i64 size);
internal void           rb_cpu_gpu_arena_destroy(RB_CpuGpuArena *arena);
internal RB_GpuRange    rb_cpu_gpu_arena_push_atomic_(RB_CpuGpuArena *arena, void *data, i64 size);
internal bool           rb_cpu_gpu_arena_fits(RB_CpuGpuArena *arena, i64 size);
internal void           rb_cpu_gpu_arena_reset(RB_CpuGpuArena *arena);

#define rb_cpu_gpu_arena_push_atomic(arena, value) rb_cpu_gpu_arena_push_atomic_((arena), &(value), sizeof(value))

///////////////////////////////////////////////////////////
///
/// Gpu Only memory

typedef struct RB_GpuArena
{
    RB_GpuHeap    heap;
    RB_GpuAddress gpu_base;
    i64           used;
    i64           size;
}RB_GpuArena;

internal RB_GpuArena rb_gpu_arena_create(i64 size);
internal void        rb_gpu_arena_destroy(RB_GpuArena *arena);

internal RB_GpuRange rb_gpu_arena_alloc_atomic(RB_GpuArena *arena, i64 size);

typedef struct RB_TextureHeapArena
{
    RB_TextureHeap heap;
    i64            used;
    i64            size;
}RB_TextureHeapArena;

internal RB_TextureHeapArena rb_texture_heap_arena_create(i64 size);
internal i64                 rb_texture_heap_arena_place(RB_TextureHeapArena *arena, RB_SizeAlign size_align);
internal void                rb_texture_heap_arena_destroy(RB_TextureHeapArena *arena);

///////////////////////////////////////////////////////////
///
/// Deletion Queue

typedef enum RB_DeleteQueueEntryKind
{
    RB_DeleteQueueEntryKind_Texture,
    RB_DeleteQueueEntryKind_Pipeline,
    RB_DeleteQueueEntryKind_GpuHeap,
    RB_DeleteQueueEntryKind_TextureHeap,
    RB_DeleteQueueEntryKind_RenderView,
}RB_DeleteQueueEntryKind;

typedef struct RB_DeleteQueueEntry
{
    u64 retire_value;
    RB_DeleteQueueEntryKind kind;
    union{
        RB_Texture texture;
        RB_Pipeline pipeline;
        RB_GpuHeap        gpu_heap;
        RB_TextureHeap    texture_heap;
        RB_RenderView     render_view;
    };
}RB_DeleteQueueEntry;

typedef struct RB_DeleteQueue
{
    RB_TimelineSemaphore  timeline;
    RB_DeleteQueueEntry  *entries;
    u32 capacity;
    u32 head;
    u32 tail;
    u32 count;
}RB_DeleteQueue;

internal RB_DeleteQueue rb_delete_queue_create(Arena *arena, RB_TimelineSemaphore timeline, u32 capacity);

internal void rb_delete_queue_defer_texture(RB_DeleteQueue *q, u64 retire_value, RB_Texture handle);
internal void rb_delete_queue_defer_pipeline(RB_DeleteQueue *q, u64 retire_value, RB_Pipeline handle);
internal void rb_delete_queue_defer_gpu_heap(RB_DeleteQueue *q, u64 retire_value, RB_GpuHeap heap);
internal void rb_delete_queue_defer_texture_heap(RB_DeleteQueue *q, u64 retire_value, RB_TextureHeap heap);
internal void rb_delete_queue_defer_render_view(RB_DeleteQueue *q, u64 retire_value, RB_RenderView view);

internal void rb_delete_queue_tick(RB_DeleteQueue *q);
internal void rb_delete_queue_drain(RB_DeleteQueue *q);

#endif // !RB_BACKEND_UTILITY_H
