#ifndef TONEMAP_SHARED_H
#define TONEMAP_SHARED_H

Reflect(RN_TonemapRoot, {
    RN_TextureSlot hdr_slot;
    f32            exposure;
    u32            bypass;
});

#endif // !TONEMAP_SHARED_H
