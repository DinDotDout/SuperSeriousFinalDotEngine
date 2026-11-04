#ifndef SHADER_SHARED_H
#define SHADER_SHARED_H

// (jd) Bindless layout when compiling without descriptor heaps (DOT_MUTABLE_DESCRIPTOR_SET).
// Textures stay where Slang's VkMutableBindlessBindings puts ResourceDescriptorHeap[i]
// (set = -bindless-space-index, binding 2); samplers are moved to their own set by the
// getDescriptorFromHandle override below so each heap owns an independent descriptor set
#define RN_BINDLESS_TEXTURE_SET     0
#define RN_BINDLESS_SAMPLER_SET     1
#define RN_BINDLESS_SAMPLER_BINDING 0

// (jd) Slang emits ResourceDescriptorHeap[i] at this binding.
#define RN_SLANG_RESOURCE_HEAP_BINDING 2

#ifndef __SLANG__
DOT_STATIC_ASSERT(RN_SLANG_RESOURCE_HEAP_BINDING == 2, "Fixed by Slang (VkMutableBindlessBindings), not configurable");
DOT_STATIC_ASSERT(RN_BINDLESS_TEXTURE_SET != RN_BINDLESS_SAMPLER_SET, "Textures and samplers need separate descriptor sets");

typedef RB_GpuAddress RB_GpuPtr;
typedef u32           RN_TextureSlot;
#define RB_GpuPtr(T) RB_GpuPtr

ReflectAlias(RB_GpuAddress,  u64);
ReflectAlias(RB_GpuPtr,      u64);
ReflectAlias(RN_TextureSlot, u32);
#endif

#ifdef __SLANG__
#define rfl_skip
#define rfl(...)

#define Reflect(T, ...)         struct T __VA_ARGS__
#define ReflectEnum(B, T, ...)  typedef B T; enum __VA_ARGS__
#define ReflectAlias(T, B)
#define ReflectConst(name)

typedef int      i32;
typedef int64_t  i64;
typedef uint     u32;
typedef uint64_t u64;
typedef uint     b32;
typedef float    f32;
typedef double   f64;
typedef int2     i32x2;
typedef int3     i32x3;
typedef int4     i32x4;
typedef uint2    u32x2;
typedef uint3    u32x3;
typedef uint4    u32x4;
typedef float2   f32x2;
typedef float3   f32x3;
typedef float4   f32x4;
typedef float4x4 f32x4x4;
typedef float3   f32rgb;
typedef float4   f32rgba;
typedef uint64_t RB_GpuAddress;
typedef uint     RN_TextureSlot;
#define RB_GpuPtr(T) T*

// The only push constant: the address of this draw's root (see rb_draw*).
struct RootPointer { RB_GpuAddress addr; };
[[vk::push_constant]] RootPointer g_root;

#if DOT_MUTABLE_DESCRIPTOR_SET
[[vk::binding(RN_BINDLESS_SAMPLER_BINDING, RN_BINDLESS_SAMPLER_SET)]] SamplerState g_sampler_heap[];
export T getDescriptorFromHandle<T:IOpaqueDescriptor>(DescriptorHandle<T> handle)
{
    if(T.kind == DescriptorKind.Sampler){
        return reinterpret<T>(g_sampler_heap[((uint2)handle).x]);
    }
    return defaultGetDescriptorFromHandle(handle);
}
#endif // !DOT_MUTABLE_DESCRIPTOR_SET

#endif // !__SLANG__
#endif // !SHADER_SHARED_H
