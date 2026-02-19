#include "renderer/renderer.h"
#ifdef DOT_HOT_RELOAD
void dot_game_bootstrap(DOT_GameVtable *vtable){}
    game_vtable = *vtable;
}

b8 dot_game_init(DOT_Game* game){
    game_vtable.Game_Init(game);
}

void DOT_Game_Shutdown(DOT_Game* game){
    game_vtable.Game_Shutdown(game);
}
#else
void dot_game_bootstrap(){}

b8 dot_game_init(Arena *arena, DOT_Game *game, DOT_Renderer *renderer)
{
    (void) arena;
    game->renderer = renderer;
    (void)game;
    return true;
}

// void dot_game_run(DOT_Game *game)
// {
    // game->run(game);
// }

void dot_game_run(DOT_Game *game)
{
    // game->run(game);
    DOT_Renderer *renderer = game->renderer;
    // f64 flash = fabs(sin(renderer->current_frame / 60.f));
    f64 flash = ((sin(renderer->current_frame / 30.f)+1.f)/2.0f);
    renderer_clear_background(renderer, v3(0,0,flash));
}

void dot_game_shutdown(DOT_Game* game)
{
    (void)game;
}

#endif
