#include <math.h>

typedef union vec2{
    struct{
        f32 x,y;
    };
    struct{
        f32 r,g;
    };
    struct{
        f32 u,v;
    };
    f32 e[2];
}vec2;

typedef union vec3{
    struct{
        f32 x,y,z;
    };
    struct{
        f32 r,g,b;
    };
    struct{
        vec2 xy;
        f32 _z;
    };
    struct{
        f32 _x;
        vec2 yz;
    };
    struct{
        vec2 rg;
        f32 _b;
    };
    struct{
        f32 _r;
        vec2 gb;
    };

    struct{
        f32 u,v,w;
    };
    struct{
        vec2 uv;
        f32 _w;
    };
    struct{
        f32 _u;
        vec2 vw;
    };
    f32 e[3];
}vec3;

typedef union vec4{
    struct{
        f32 x,y,z,w;
    };
    struct{
        f32 r,g,b,a;
    };
    struct{
        vec3 xyz;
        f32 _w;
    };
    struct{
        vec3 rgb;
        f32 _a;
    };
    f32 e[4];
}vec4;

internal inline
vec2 v2(f32 x, f32 y)
{
    vec2 res = {.x = x, .y = y};
    return res;
}

internal inline
vec3 v3(f32 x, f32 y, f32 z)
{
    vec3 res = {.x = x, .y = y, .z = z};
    return res;
}

internal inline
vec4 v4(f32 x, f32 y, f32 z, f32 w)
{
    vec4 res = {.x = x, .y = y, .z = z, .w = w};
    return res;
}

#define VEC2(v, a,b) (vec2){.x = v.a, .y = v.b}
#define VEC3(v, a,b,c) (vec3){.x = v.a, .y = v.b, .z = v.c}
#define VEC4(v, a,b,c,d) (vec4){.x = v.a, .y = v.b, .z = v.c, .w = v.d}
