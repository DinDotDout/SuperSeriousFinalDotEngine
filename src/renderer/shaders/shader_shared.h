#ifndef SHADER_SHARED_H
#define SHADER_SHARED_H

#ifdef __SLANG__
#define rfl_color
#define rfl_skip

#define Reflect(T, ...)          struct T __VA_ARGS__
#define ReflectEnum(B, T, ...)   typedef B T; enum __VA_ARGS__
#define ReflectConst(name)

typedef float    f32;
typedef uint     u32;
typedef float2   f32x2;
typedef float3   f32x3;
typedef float4   f32x4;
typedef float4x4 f32x4x4;
typedef uint64_t RB_GpuAddress;

// The only push constant: the address of this draw's root (see rb_draw*).
struct RootPointer { RB_GpuAddress addr; };
[[vk::push_constant]] RootPointer g_root;
#endif

#endif // !SHADER_SHARED_H
