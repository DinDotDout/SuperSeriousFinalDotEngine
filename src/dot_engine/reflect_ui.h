#ifndef REFLECT_UI_H
#define REFLECT_UI_H

#define rfl_skip

typedef f32x3 f32rgb;
typedef f32x4 f32rgba;

ReflectExtern(f32rgb,  { f32 r, g, b; });
ReflectExtern(f32rgba, { f32 r, g, b, a; });

enum
{
    RFL_UI_ROW_HEIGHT = 22,
};

internal void rfl_ui_inspect(struct nk_context *ctx, RFL_Any v);
internal void rfl_ui_any(struct nk_context *ctx, char *label, RFL_Any v);

#endif // !REFLECT_UI_H
