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
        vec2 xy;
        f32 _z;
    };
    struct{
        f32 _x;
        vec2 yz;
    };

    struct{
        f32 r,g,b;
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

vec3 v3(f32 x, f32 y, f32 z)
{
    vec3 res = {.x = x, .y = y, .z = z};
    return res;
}
