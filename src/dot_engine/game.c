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
b8 dot_game_init(DOT_Game* game){
    (void)game;
    return true;
}

void dot_game_shutdown(DOT_Game* game){
    (void)game;
}
#endif
