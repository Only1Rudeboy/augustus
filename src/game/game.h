#ifndef GAME_GAME_H
#define GAME_GAME_H

int game_pre_init(void);

int game_init(void);

int game_init_editor(void);

int game_reload_language(void);

void game_run(void);

void game_draw(void);

void game_display_fps(int fps);

/** Draw FPS and optional sim/draw profiler next to it. */
void game_display_profiler(int fps);

void game_exit_editor(void);

void game_exit(void);

#endif // GAME_GAME_H
