#ifndef MIP_DOWNSAMPLE_SHARED_H
#define MIP_DOWNSAMPLE_SHARED_H

#define RN_MIP_DOWNSAMPLE_GROUP_DIM 8

Reflect(RN_MipDownsampleRoot, {
    RB_GpuPtr(u32) src;
    RB_GpuPtr(u32) dst;
    u32            src_width;
    u32            src_height;
    u32            dst_width;
    u32            dst_height;
    b32            srgb;
    u32            pad;
});

#endif // !MIP_DOWNSAMPLE_SHARED_H
