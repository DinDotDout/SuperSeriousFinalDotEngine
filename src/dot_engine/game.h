#ifndef DOT_GAME_H
#define DOT_GAME_H

typedef struct DOT_Game{
    u8* permanent_memory;
    usize permanent_memory_size;
    u8* transient_memory;
    usize transient_memory_size;
}DOT_Game;

#ifdef DOT_HOT_RELOAD
typedef struct DOT_Game_Vtable DOT_Game_Vtable;
struct DOT_Game_Vtable{
    b8(*Game_Init)(DOT_Game* game, DOT_Game_Vtable *vtable);
    void(*Game_Shutdown)(void);
};

DOT_Game_Vtable game_vtable;
#else
#include "renderer/renderer.h"
#endif

void DOT_Game_Bootstrap();
b8 DOT_Game_Init(DOT_Game* game);
void DOT_Game_Shutdown(DOT_Game* game);
#endif
