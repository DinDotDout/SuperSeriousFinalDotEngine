#include <math.h>

typedef union f32x2{
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
}f32x2;

typedef f32x2 f32x2;

typedef struct u32x3{
    u32 x,y,z;
}u32x3;

typedef struct u32x2{
    u32 x,y;
}u32x2;

typedef struct i32x3{
    i32 x,y,z;
}i32x3;

typedef struct i32x2{
    i32 x,y;
}i32x2;

typedef union f32x3
{
    struct{
        f32 x,y,z;
    };
    struct{
        f32 r,g,b;
    };
    struct{
        f32x2 xy;
        f32 _z;
    };
    struct{
        f32 _x;
        f32x2 yz;
    };
    struct{
        f32x2 rg;
        f32 _b;
    };
    struct{
        f32 _r;
        f32x2 gb;
    };

    struct{
        f32 u,v,w;
    };
    struct{
        f32x2 uv;
        f32 _w;
    };
    struct{
        f32 _u;
        f32x2 vw;
    };
    f32 e[3];
}f32x3;
typedef f32x3 f32x3;

global const f32x3 f32x3_zero     = {0};
global const f32x3 f32x3_one      = { .x = 1.0f, .y = 1.0f, .z = 1.0f};
global const f32x3 f32x3_right    = { .x = 1.0f, .y = 0.0f, .z = 0.0f};
global const f32x3 f32x3_up       = { .x = 0.0f, .y = 1.0f, .z = 0.0f};

typedef union f32x4
{
    struct{
        f32 x,y,z,w;
    };
    struct{
        f32 r,g,b,a;
    };
    struct{
        f32x3 xyz;
        f32 _w;
    };
    struct{
        f32x3 rgb;
        f32 _a;
    };
    f32 e[4];
}f32x4;

// #ifdef __AVX__
// #  define alignmat CGLM_ALIGN(32)
// #else
// #  define alignmat CGLM_ALIGN(16)
// #endif
typedef union alignas(16) mat4{
    f32x4 mat4[4];
    struct {
        f32 m00, m01, m02, m03;
        f32 m10, m11, m12, m13;
        f32 m20, m21, m22, m23;
        f32 m30, m31, m32, m33;
    };
}mat4;

typedef f32x4 quat;

internal inline
f32x2 v2(f32 x, f32 y)
{
    f32x2 res = {.x = x, .y = y};
    return res;
}

internal inline
f32x3 v3(f32 x, f32 y, f32 z)
{
    f32x3 res = {.x = x, .y = y, .z = z};
    return res;
}

internal inline f32x4
v4(f32 x, f32 y, f32 z, f32 w)
{
    f32x4 res = {.x = x, .y = y, .z = z, .w = w};
    return res;
}

internal inline
f32x4 v4_v3_f(f32x3 v3, f32 w)
{
    f32x4 res = {.x = v3.x, .y = v3.y, .z = v3.z, .w = w};
    return res;
}

internal inline f32x3
f32x3_add(f32x3 a, f32x3 b)
{
    return v3(a.x+b.x, a.y+b.y, a.z+b.z);
}

internal inline f32x3
f32x3_sub(f32x3 a, f32x3 b)
{
    return v3(a.x-b.x, a.y-b.y, a.z-b.z);
}

internal inline f32x3
f32x3_scale(f32x3 a, f32 s)
{
    return v3(a.x*s, a.y*s, a.z*s);
}

internal inline f32
f32x3_dot(f32x3 a, f32x3 b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

internal inline f32x3
f32x3_cross(f32x3 a, f32x3 b)
{
    return v3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}

internal inline f32
f32x3_length(f32x3 a)
{
    return sqrtf(f32x3_dot(a, a));
}

internal inline f32x3
f32x3_normalize(f32x3 a)
{
    f32 len = f32x3_length(a);
    return len > 0.0f ? f32x3_scale(a, 1.0f/len) : a;
}

internal inline mat4
mat4_make(f32x4 a, f32x4 b,f32x4 c, f32x4 d)
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

#define f32x2(v, a,b) (f32x2){.x = v.a, .y = v.b}
// #define f32x3(v, a,b,c) (f32x3){.x = v.a, .y = v.b, .z = v.c}
#define f32x3(v, a,b,c) {.x = v.a, .y = v.b, .z = v.c}

#define f32x3_UNWRAP(v) (v).x, (v).y, (v).z

#define F32x4(v, a,b,c,d) (f32x4){.x = v.a, .y = v.b, .z = v.c, .w = v.d}

#define DOT_PI 3.14159265358979323846f

internal mat4
mat4_mul(mat4 a, mat4 b)
{
    // Column-major: r column c = a * (b column c).
    mat4 r;
    f32 *ra = cast(f32*)&r;
    f32 *aa = cast(f32*)&a;
    f32 *ba = cast(f32*)&b;
    for(u32 c = 0; c < 4; ++c){
        for(u32 row = 0; row < 4; ++row){
            ra[c*4 + row] = aa[0*4 + row]*ba[c*4 + 0] + aa[1*4 + row]*ba[c*4 + 1]
                          + aa[2*4 + row]*ba[c*4 + 2] + aa[3*4 + row]*ba[c*4 + 3];
        }
    }
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
mat4_from_trs(f32x3 translation, quat r, f32x3 s)
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

internal inline mat4
mat4_look_at(f32x3 eye, f32x3 center, f32x3 up)
{
    f32x3 f = f32x3_normalize(f32x3_sub(center, eye));
    f32x3 s = f32x3_normalize(f32x3_cross(f, up));
    f32x3 u = f32x3_cross(s, f);

    mat4 r = mat4_make(
        v4(s.x, u.x, -f.x, 0.0f),
        v4(s.y, u.y, -f.y, 0.0f),
        v4(s.z, u.z, -f.z, 0.0f),
        v4(-f32x3_dot(s, eye), -f32x3_dot(u, eye), f32x3_dot(f, eye), 1.0f)
    );
    return r;
}

internal inline mat4
mat4_perspective(f32 fov_y_radians, f32 aspect, f32 z_near, f32 z_far)
{
    f32 tan_half_fov = tanf(fov_y_radians * 0.5f);

    mat4 r = mat4_make(
        v4(1.0f/(aspect*tan_half_fov), 0.0f, 0.0f, 0.0f),
        v4(0.0f, -1.0f/tan_half_fov, 0.0f, 0.0f),
        v4(0.0f, 0.0f, z_far/(z_near-z_far), -1.0f),
        v4(0.0f, 0.0f, (z_far*z_near)/(z_near-z_far), 0.0f)
    );
    return r;
}

