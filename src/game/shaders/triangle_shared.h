#ifndef TRIANGLE_SHARED_H
#define TRIANGLE_SHARED_H

Reflect(TriangleVertex, {
    f32x3 position;
    f32x3 color;
});

Reflect(TriangleRoot, {
    f32x4x4       mvp;
    RB_GpuAddress vertex_addr;
});

#endif // !TRIANGLE_SHARED_H
