#ifdef DOT_HOT_RELOAD
void DOT_Game_Bootstrap(DOT_Game_Vtable *vtable){}
    game_vtable = *vtable;
}

b8 DOT_Game_Init(DOT_Game* game){
    game_vtable.Game_Init(game);
}

void DOT_Game_Shutdown(DOT_Game* game){
    game_vtable.Game_Shutdown(game);
}
#else
void DOT_Game_Bootstrap(){}
b8 DOT_Game_Init(DOT_Game* game){
    (void)game;
    return true;
}

void DOT_Game_Shutdown(DOT_Game* game){
    (void)game;
}
#endif
