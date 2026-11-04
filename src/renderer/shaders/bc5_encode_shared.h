#ifndef BC5_ENCODE_SHARED_H
#define BC5_ENCODE_SHARED_H

Reflect(RN_BC5EncodeRoot, {
    RB_GpuPtr(u32)   pixels;
    RB_GpuPtr(u32x4) out_buffer;
    u32              tex_width;
    u32              tex_height;
    u32              num_block_x;
    u32              start_block_id;
    u32              num_total_blocks;
    u32              pad;
});

#endif // !BC5_ENCODE_SHARED_H
