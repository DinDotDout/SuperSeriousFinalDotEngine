#ifndef UI_SHARED_H
#define UI_SHARED_H

#ifndef __SLANG__
typedef struct UI_RenderRoot UI_RenderRoot;
#endif

struct UI_RenderRoot
{
    f32x2         scale;
    f32x2         translate;
    RB_GpuAddress vertex_addr;
    u32           texture_slot;
    u32           sampler_slot;
};

#endif // !UI_SHARED_H
