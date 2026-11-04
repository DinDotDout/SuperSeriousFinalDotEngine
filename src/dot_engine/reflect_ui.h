#ifndef REFLECT_UI_H
#define REFLECT_UI_H

#define rfl_skip

typedef f32x3 f32rgb;
typedef f32x4 f32rgba;

ReflectExtern(f32rgb,  { f32 r, g, b; });
ReflectExtern(f32rgba, { f32 r, g, b, a; });

enum
{
    RFL_UI_ROW_HEIGHT       = 22,
    RFL_UI_VIEW_BUCKETS_MAX = 64,
};

typedef void RFL_UI_ViewFn(struct nk_context *ctx, char *label, RFL_Any v);

typedef struct alignas(16) RFL_UI_View
{
    RFL_Type      *type;
    RFL_UI_ViewFn *fn;
    char          *file;
    u32            line;
}RFL_UI_View;

SECTION_DECLARE(RFL_UI_View, ReflectUIViewSection);

#define ReflectUIView(T, view_fn) \
    SECTION_ITEM(ReflectUIViewSection) static RFL_UI_View rfl_ui_g_view_##T = { \
        .type = rfl_type_def(T), \
        .fn   = (view_fn), \
        .file = __FILE__, \
        .line = __LINE__, \
    }

internal void rfl_ui_init(Arena *arena);
internal void rfl_ui_inspect(struct nk_context *ctx, RFL_Any v);
internal void rfl_ui_any(struct nk_context *ctx, char *label, RFL_Any v);
internal char *rfl_ui_label_from_string8(Arena *arena, String8 name);

#endif // !REFLECT_UI_H
