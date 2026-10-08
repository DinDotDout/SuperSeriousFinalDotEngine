#ifndef UI_INPUT_H
#define UI_INPUT_H

typedef struct UI_Input{
    f64 last_button_click;
    b32 is_double_click_down;
    struct nk_vec2 double_click_pos;
}UI_Input;

internal void ui_input_init(UI_Input *input, struct nk_context *ctx);
internal void ui_input_update(UI_Input *input, struct nk_context *ctx, DOT_Window *window);

#endif // !UI_INPUT_H
