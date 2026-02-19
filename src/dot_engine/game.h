#ifndef DOT_GAME_H
#define DOT_GAME_H


// typedef struct DOT_GameAPI {
//     void (*run)(DOT_Game *game);
//     void (*init)(DOT_Game *game);
//     void (*shutdown)(DOT_Game *game);
// } DOT_GameAPI;

// global DOT_GameAPI g_game_api;


typedef struct DOT_Game{
    DOT_Renderer *renderer;
    u8* permanent_memory;
    usize permanent_memory_size;
    u8* transient_memory;
    usize transient_memory_size;
}DOT_Game;

#ifdef DOT_HOT_RELOAD
typedef struct DOT_GameVtable DOT_GameVtable;

void reload_game_dll(String8 dll_path) {
    unload_library(game_lib);
    game_lib = load_library(dll_path);

    g_game_api.run      = get_symbol(game_lib, "dot_game_run");
    g_game_api.init     = get_symbol(game_lib, "dot_game_init");
    g_game_api.shutdown = get_symbol(game_lib, "dot_game_shutdown");
}

struct DOT_GameVtable{
    b8(*Game_Init)(DOT_Game* game, DOT_GameVtable *vtable);
    void(*Game_Shutdown)(void);
};

void dot_game_run(DOT_Game *game) {
    g_game_api.run(game);
}

void dot_game_init(DOT_Game *game) {
    g_game_api.init(game);
}

void dot_game_shutdown(DOT_Game *game) {
    g_game_api.shutdown(game);
}

DOT_GameVtable game_vtable;
#else
#include "renderer/renderer.h"
#endif

void dot_game_bootstrap();
b8 dot_game_init(Arena *arena, DOT_Game *game, DOT_Renderer *renderer);
void dot_game_shutdown(DOT_Game *game);
void dot_game_run(DOT_Game *game);

void dot_game_draw(DOT_Game *game, DOT_Renderer *renderer);
#endif // !DOT_GAME_H
