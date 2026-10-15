// Atomic 16-byte bump shared by both arenas; returns the offset, or U64_MAX when exhausted.
internal i64
rb_gpu_bump_atomic_(i64 *used, i64 capacity, i64 size)
{
    i64 reserved = ALIGN_POW2(size, cast(i64)RB_GPU_ARENA_ALIGN);
    i64 end = ins_atomic_i64_add_eval(used, reserved);
    return(end <= capacity ? end - reserved : -1);
}

internal RB_CpuGpuArena
rb_cpu_gpu_arena_create(i64 size)
{
    DOT_ASSERT(size > 0);
    RB_CpuGpuArena arena = {0};
    arena.size     = size;
    arena.heap     = rb_gpu_heap_create(RB_MemoryKind_CpuToGpu, size);
    arena.gpu_base = rb_gpu_heap_address(arena.heap);
    return(arena);
}

internal bool
rb_cpu_gpu_arena_fits(RB_CpuGpuArena *arena, i64 size)
{
    return(arena->used + ALIGN_POW2(size, cast(i64)RB_GPU_ARENA_ALIGN) <= arena->size);
}

internal RB_GpuRange
rb_cpu_gpu_arena_push_atomic_(RB_CpuGpuArena *arena, void *data, i64 size)
{
    DOT_ASSERT(data && size > 0);
    RB_GpuRange range = {0};
    i64 offset = rb_gpu_bump_atomic_(&arena->used, arena->size, size);
    if(offset >= 0){
        rb_gpu_heap_write(arena->heap, offset, data, size);
        range.gpu  = arena->gpu_base + cast(u64)offset;
        range.size = size;
    }
    return(range);
}

internal void
rb_cpu_gpu_arena_reset(RB_CpuGpuArena *arena)
{
    arena->used = 0;
}

internal void
rb_cpu_gpu_arena_destroy(RB_CpuGpuArena *arena)
{
    rb_gpu_heap_destroy(arena->heap);
    MEMORY_ZERO_STRUCT(arena);
}

internal RB_GpuArena
rb_gpu_arena_create(i64 size)
{
    DOT_ASSERT(size > 0);
    RB_GpuArena arena = {0};
    arena.size     = size;
    arena.heap     = rb_gpu_heap_create(RB_MemoryKind_GpuOnly, size);
    arena.gpu_base = rb_gpu_heap_address(arena.heap);
    return(arena);
}

internal RB_GpuRange
rb_gpu_arena_alloc_atomic(RB_GpuArena *arena, i64 size)
{
    DOT_ASSERT(size > 0);
    RB_GpuRange range = {0};
    i64 offset = rb_gpu_bump_atomic_(&arena->used, arena->size, size);
    if(offset >= 0){
        range.gpu  = arena->gpu_base + cast(u64)offset;
        range.size = size;
    }
    return(range);
}

internal void
rb_gpu_arena_destroy(RB_GpuArena *arena)
{
    rb_gpu_heap_destroy(arena->heap);
    MEMORY_ZERO_STRUCT(arena);
}

internal RB_TextureHeapArena
rb_texture_heap_arena_create(i64 size)
{
    RB_TextureHeapArena arena = {0};
    arena.size = size;
    arena.heap = rb_texture_heap_create(size);
    return(arena);
}

internal i64
rb_texture_heap_arena_place(RB_TextureHeapArena *arena, RB_SizeAlign size_align)
{
    i64 offset = ALIGN_POW2(arena->used, size_align.align);
    DOT_ASSERT(offset + size_align.size <= arena->size, "Texture heap exhausted");
    arena->used = offset + size_align.size;
    return(offset);
}

internal void
rb_texture_heap_arena_destroy(RB_TextureHeapArena *arena)
{
    rb_texture_heap_destroy(arena->heap);
    MEMORY_ZERO_STRUCT(arena);
}

internal RB_DeleteQueue
rb_delete_queue_create(Arena *arena, RB_TimelineSemaphore timeline, u32 capacity)
{
    DOT_ASSERT(RB_HANDLE_VALID(timeline) && capacity != 0);

    RB_DeleteQueue q = {0};
    q.timeline = timeline;
    q.capacity = capacity;
    q.entries  = PUSH_ARRAY(arena, RB_DeleteQueueEntry, capacity);
    return(q);
}

internal void
rb_delete_queue_defer_(RB_DeleteQueue *q, u64 retire_value, RB_DeleteQueueEntryKind kind, RB_DeleteQueueEntry payload)
{
    DOT_ASSERT(q->count < q->capacity, "delete queue capacity exhausted");
    DOT_ASSERT(q->count == 0 || q->entries[q->tail == 0 ? q->capacity - 1 : q->tail - 1].retire_value <= retire_value,
        "delete queue retire values must be nondecreasing");

    RB_DeleteQueueEntry *entry = &q->entries[q->tail];
    *entry = payload;
    entry->retire_value = retire_value;
    entry->kind          = kind;

    q->tail = q->tail + 1 == q->capacity ? 0 : q->tail + 1;
    q->count += 1;
}

internal void
rb_delete_queue_defer_texture(RB_DeleteQueue *q, u64 retire_value, RB_Texture handle)
{
    rb_delete_queue_defer_(q, retire_value, RB_DeleteQueueEntryKind_Texture, (RB_DeleteQueueEntry){.texture = handle});
}

internal void
rb_delete_queue_defer_pipeline(RB_DeleteQueue *q, u64 retire_value, RB_Pipeline handle)
{
    rb_delete_queue_defer_(q, retire_value, RB_DeleteQueueEntryKind_Pipeline, (RB_DeleteQueueEntry){.pipeline = handle});
}

internal void
rb_delete_queue_defer_gpu_heap(RB_DeleteQueue *q, u64 retire_value, RB_GpuHeap heap)
{
    rb_delete_queue_defer_(q, retire_value, RB_DeleteQueueEntryKind_GpuHeap, (RB_DeleteQueueEntry){.gpu_heap = heap});
}

internal void
rb_delete_queue_defer_texture_heap(RB_DeleteQueue *q, u64 retire_value, RB_TextureHeap heap)
{
    rb_delete_queue_defer_(q, retire_value, RB_DeleteQueueEntryKind_TextureHeap, (RB_DeleteQueueEntry){.texture_heap = heap});
}

internal void
rb_delete_queue_defer_render_view(RB_DeleteQueue *q, u64 retire_value, RB_RenderView view)
{
    rb_delete_queue_defer_(q, retire_value, RB_DeleteQueueEntryKind_RenderView, (RB_DeleteQueueEntry){.render_view = view});
}

internal void
rb_delete_queue_run_entry_(RB_DeleteQueueEntry *entry)
{
    switch(entry->kind){
    case RB_DeleteQueueEntryKind_Texture:     rb_texture_destroy(entry->texture);          break;
    case RB_DeleteQueueEntryKind_Pipeline:    rb_pipeline_destroy(entry->pipeline);        break;
    case RB_DeleteQueueEntryKind_GpuHeap:     rb_gpu_heap_destroy(entry->gpu_heap);        break;
    case RB_DeleteQueueEntryKind_TextureHeap: rb_texture_heap_destroy(entry->texture_heap); break;
    case RB_DeleteQueueEntryKind_RenderView:  rb_render_view_destroy(entry->render_view);   break;
    default: DOT_ERROR("Invalid delete queue entry kind %d", entry->kind);
    }
}

internal void
rb_delete_queue_collect_(RB_DeleteQueue *q, u64 completed_value)
{
    while(q->count != 0){
        RB_DeleteQueueEntry *entry = &q->entries[q->head];
        if(entry->retire_value > completed_value){
            break;
        }
        rb_delete_queue_run_entry_(entry);
        q->head  = q->head + 1 == q->capacity ? 0 : q->head + 1;
        q->count -= 1;
    }
}

internal void
rb_delete_queue_tick(RB_DeleteQueue *q)
{
    if(q->count == 0){ return; }
    rb_delete_queue_collect_(q, rb_timeline_completed_value(q->timeline));
}

// (claude) Shutdown only: ignores the timeline.
internal void
rb_delete_queue_drain(RB_DeleteQueue *q)
{
    rb_delete_queue_collect_(q, U64_MAX);
}
