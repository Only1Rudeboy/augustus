#ifndef GAME_PROFILER_H
#define GAME_PROFILER_H

#include "core/time.h"

/**
 * Lightweight frame-budget profiler for Milestone A.
 * Tracks average milliseconds spent in simulation and draw phases.
 */

void game_profiler_begin_frame(void);
void game_profiler_mark_sim_end(void);
void game_profiler_mark_draw_end(void);

/** Average sim time over the last second, in whole milliseconds. */
int game_profiler_sim_ms(void);

/** Average draw time over the last second, in whole milliseconds. */
int game_profiler_draw_ms(void);

/** Average total frame time (sim + draw) over the last second, in whole milliseconds. */
int game_profiler_frame_ms(void);

#endif // GAME_PROFILER_H
