#ifndef REFLECT_UI_H
#define REFLECT_UI_H

enum
{
    RFL_UI_ROW_HEIGHT = 22,
};

internal void rfl_ui_inspect(struct nk_context *ctx, RFL_Any v);
internal void rfl_ui_any(struct nk_context *ctx, char *label, RFL_Any v);

#endif // !REFLECT_UI_H
