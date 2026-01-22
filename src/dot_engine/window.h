#ifndef DOT_WINDOW_H
#define DOT_WINDOW_H

#define DOT_VK_SURFACE RGFW_VK_SURFACE

typedef struct DOT_Window{
        RGFW_window *window;
}DOT_Window;

struct RendererBackendBase;
internal void DOT_Window_MousePosCallback(RGFW_window *window, i32 x, i32 y, float vecX, float vecY);
internal void DOT_Window_Init(DOT_Window* window);
internal void DOT_Window_Destroy(DOT_Window* window);
internal void DOT_Window_CreateSurface(DOT_Window* window, struct RendererBackendBase* backend);
internal b8   DOT_Window_GetFrameBufferSize(DOT_Window* window, i32* w, i32* h);
internal b8   DOT_Window_GetSize(DOT_Window* window, i32* w, i32* h);
internal b8   DOT_Window_ShouldClose(DOT_Window* window);

#endif // !DOT_WINDOW_H
