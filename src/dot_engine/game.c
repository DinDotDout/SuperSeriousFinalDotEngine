#include "base/arena.h"
#include "base/string.h"
#include "renderer/renderer.h"
global DOT_Game *g_game = NULL;

#ifdef DOT_HOT_RELOAD
void dot_game_init(DOT_Game *game, DOT_Renderer *renderer,
    u8 permanent_memory[], usize permanent_memory_size,
    u8 transient_memory[], usize transient_memory_size) {
    g_game_api.init(game);
}

void dot_game_shutdown(DOT_Game *game) {
    g_game_api.shutdown(game);
}

void dot_game_run(DOT_Game *game) {
    g_game_api.run(game);
}

#else
b8 dot_game_init(DOT_Game *game, DOT_Renderer *renderer,
    u8 permanent_memory[], usize permanent_memory_size,
    u8 transient_memory[], usize transient_memory_size)
{
    g_game = game;
    g_game->renderer = renderer;
    g_game->permanent_arena = ARENA_ALLOC(.buffer = permanent_memory, .reserve_size = permanent_memory_size);
    g_game->transient_arena = ARENA_ALLOC(.buffer = transient_memory, .reserve_size = transient_memory_size);

    test_shader_module = renderer_load_shader_module_from_path(g_game->permanent_arena, game->renderer, String8Lit(DOT_GAME_SHADER_PATH"compute.glsl"));
    return true;
}

void dot_game_run(DOT_Game *game)
{
    DOT_Renderer *renderer = game->renderer;
    // f64 flash = fabs(sin(renderer->current_frame / 60.f));
    f64 flash = ((sin(renderer->current_frame / 1200.f)+1.f)/2.0f);
    renderer_clear_background(renderer, v3(0,0,flash));
}

void dot_game_shutdown(DOT_Game* game)
{
    (void)game;
}

#endif
