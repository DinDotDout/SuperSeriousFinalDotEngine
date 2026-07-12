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

global const vec3 vec3_zero     = {0};
global const vec3 vec3_one      = { .x = 1.0f, .y = 1.0f, .z = 1.0f};
global const vec3 vec3_right    = { .x = 1.0f, .y = 0.0f, .z = 0.0f};
global const vec3 vec3_up       = { .x = 0.0f, .y = 1.0f, .z = 0.0f};

typedef union alignas(16) vec4{
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

// #ifdef __AVX__
// #  define alignmat CGLM_ALIGN(32)
// #else
// #  define alignmat CGLM_ALIGN(16)
// #endif
typedef union alignas(16) mat4{
    vec4 mat4[4];
    struct {
        f32 m00, m01, m02, m03;
        f32 m10, m11, m12, m13;
        f32 m20, m21, m22, m23;
        f32 m30, m31, m32, m33;
    };
}mat4;

typedef vec4 quat;
// typedef struct quat{
//     vec4 quat;
// }quat;
//

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

internal inline
vec4 v4_v3_f(vec3 v3, f32 w)
{
    vec4 res = {.x = v3.x, .y = v3.y, .z = v3.z, .w = w};
    return res;
}

internal inline mat4
mat4_make(vec4 a, vec4 b,vec4 c, vec4 d)
{
    mat4 m = {{a,b,c,d }};
    return m;
}

internal inline mat4
mat4_identity()
{
    mat4 id = mat4_make(
        v4(1,0,0,0),
        v4(0,1,0,0),
        v4(0,0,1,0),
        v4(0,0,0,1)
    );
    return(id);
}

internal inline quat
quat_make(f32 x, f32 y, f32 z, f32 w)
{
    quat res = v4(x,y,z,w);
    return(res);
}

#define VEC2(v, a,b) (vec2){.x = v.a, .y = v.b}
// #define VEC3(v, a,b,c) (vec3){.x = v.a, .y = v.b, .z = v.c}
#define VEC3(v, a,b,c) {.x = v.a, .y = v.b, .z = v.c}

#define VEC3_UNWRAP(v) (v).x, (v).y, (v).z

#define VEC4(v, a,b,c,d) (vec4){.x = v.a, .y = v.b, .z = v.c, .w = v.d}

internal mat4
mat4_mul(mat4 a, mat4 b)
{
    mat4 r;

    // Column 0
    r.mat4[0].x = a.m00*b.m00 + a.m01*b.m10 + a.m02*b.m20 + a.m03*b.m30;
    r.mat4[0].y = a.m10*b.m00 + a.m11*b.m10 + a.m12*b.m20 + a.m13*b.m30;
    r.mat4[0].z = a.m20*b.m00 + a.m21*b.m10 + a.m22*b.m20 + a.m23*b.m30;
    r.mat4[0].w = a.m30*b.m00 + a.m31*b.m10 + a.m32*b.m20 + a.m33*b.m30;

    // Column 1
    r.mat4[1].x = a.m00*b.m01 + a.m01*b.m11 + a.m02*b.m21 + a.m03*b.m31;
    r.mat4[1].y = a.m10*b.m01 + a.m11*b.m11 + a.m12*b.m21 + a.m13*b.m31;
    r.mat4[1].z = a.m20*b.m01 + a.m21*b.m11 + a.m22*b.m21 + a.m23*b.m31;
    r.mat4[1].w = a.m30*b.m01 + a.m31*b.m11 + a.m32*b.m21 + a.m33*b.m31;

    // Column 2
    r.mat4[2].x = a.m00*b.m02 + a.m01*b.m12 + a.m02*b.m22 + a.m03*b.m32;
    r.mat4[2].y = a.m10*b.m02 + a.m11*b.m12 + a.m12*b.m22 + a.m13*b.m32;
    r.mat4[2].z = a.m20*b.m02 + a.m21*b.m12 + a.m22*b.m22 + a.m23*b.m32;
    r.mat4[2].w = a.m30*b.m02 + a.m31*b.m12 + a.m32*b.m22 + a.m33*b.m32;

    // Column 3
    r.mat4[3].x = a.m00*b.m03 + a.m01*b.m13 + a.m02*b.m23 + a.m03*b.m33;
    r.mat4[3].y = a.m10*b.m03 + a.m11*b.m13 + a.m12*b.m23 + a.m13*b.m33;
    r.mat4[3].z = a.m20*b.m03 + a.m21*b.m13 + a.m22*b.m23 + a.m23*b.m33;
    r.mat4[3].w = a.m30*b.m03 + a.m31*b.m13 + a.m32*b.m23 + a.m33*b.m33;

    return r;
}

internal mat4
mat4_from_quat(quat q)
{
    float x = q.x, y = q.y, z = q.z, w = q.w;

    float xx = x * x;
    float yy = y * y;
    float zz = z * z;
    float xy = x * y;
    float xz = x * z;
    float yz = y * z;
    float wx = w * x;
    float wy = w * y;
    float wz = w * z;
    mat4 ret = mat4_make(quat_make(1.0f - 2.0f*(yy + zz),
            2.0f*(xy + wz),
            2.0f*(xz - wy),
            0.0f),

        quat_make(2.0f*(xy - wz),
            1.0f - 2.0f*(xx + zz),
            2.0f*(yz + wx),
            0.0f),

        quat_make(2.0f*(xz + wy),
            2.0f*(yz - wx),
            1.0f - 2.0f*(xx + yy),
            0.0f),
        quat_make(0.0f, 0.0f, 0.0f, 1.0f));
    return(ret);
}

internal mat4
mat4_from_trs(vec3 translation, quat r, vec3 s)
{
    mat4 R = mat4_from_quat(r);

    // scale columns
    R.mat4[0].x *= s.x; R.mat4[0].y *= s.x; R.mat4[0].z *= s.x;
    R.mat4[1].x *= s.y; R.mat4[1].y *= s.y; R.mat4[1].z *= s.y;
    R.mat4[2].x *= s.z; R.mat4[2].y *= s.z; R.mat4[2].z *= s.z;

    R.mat4[3] = v4_v3_f(translation, 1.0f);
    return R;
}

internal inline quat
quat_identity(void)
{
    return v4(0.0f, 0.0f, 0.0f, 1.0f);
}

