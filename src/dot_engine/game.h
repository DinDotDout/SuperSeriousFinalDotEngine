#ifndef DOT_GAME_H
#define DOT_GAME_H

typedef struct DOT_Game{
    u8* permanent_memory;
    usize permanent_memory_size;
    u8* transient_memory;
    usize transient_memory_size;
}DOT_Game;

#ifdef DOT_HOT_RELOAD
typedef struct DOT_GameVtable DOT_GameVtable;
struct DOT_GameVtable{
    b8(*Game_Init)(DOT_Game* game, DOT_GameVtable *vtable);
    void(*Game_Shutdown)(void);
};

DOT_GameVtable game_vtable;
#else
#include "renderer/renderer.h"
#endif

void dot_game_bootstrap();
b8 dot_game_init(DOT_Game* game);
void dot_game_shutdown(DOT_Game* game);
#endif
