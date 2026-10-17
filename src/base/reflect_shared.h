#ifndef REFLECT_SHARED_H
#define REFLECT_SHARED_H

#define rfl_color
#define rfl_skip

#ifdef __SLANG__
#   define Reflect(T, ...)          struct T __VA_ARGS__
#   define ReflectEnum(B, T, ...)   typedef B T; enum __VA_ARGS__
#   define ReflectConst(name)
#endif

#endif // !REFLECT_SHARED_H
