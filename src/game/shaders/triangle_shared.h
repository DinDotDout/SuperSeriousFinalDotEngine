#ifndef TRIANGLE_SHARED_H
#define TRIANGLE_SHARED_H

#ifndef __SLANG__
typedef struct TriangleVertex TriangleVertex;
typedef struct TriangleRoot TriangleRoot;
#endif

struct TriangleVertex
{
    f32x3 position;
    f32x3 color;
};

struct TriangleRoot
{
    mat4          mvp;
    RB_GpuAddress vertex_addr;
};

#endif // !TRIANGLE_SHARED_H
