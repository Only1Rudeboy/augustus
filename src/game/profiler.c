#include "profiler.h"

#include "game/system.h"

static struct {
    time_millis frame_start;
    time_millis sim_end;
    time_millis draw_end;

    time_millis accum_sim;
    time_millis accum_draw;
    time_millis accum_frame;
    int sample_count;
    time_millis last_report_time;

    int avg_sim_ms;
    int avg_draw_ms;
    int avg_frame_ms;
} data;

void game_profiler_begin_frame(void)
{
    data.frame_start = system_get_ticks();
    data.sim_end = data.frame_start;
    data.draw_end = data.frame_start;
}

void game_profiler_mark_sim_end(void)
{
    data.sim_end = system_get_ticks();
}

void game_profiler_mark_draw_end(void)
{
    data.draw_end = system_get_ticks();

    time_millis sim = data.sim_end - data.frame_start;
    time_millis draw = data.draw_end - data.sim_end;
    time_millis frame = data.draw_end - data.frame_start;

    data.accum_sim += sim;
    data.accum_draw += draw;
    data.accum_frame += frame;
    data.sample_count++;

    if (data.draw_end - data.last_report_time >= 1000 && data.sample_count > 0) {
        data.avg_sim_ms = (int) (data.accum_sim / data.sample_count);
        data.avg_draw_ms = (int) (data.accum_draw / data.sample_count);
        data.avg_frame_ms = (int) (data.accum_frame / data.sample_count);
        data.accum_sim = 0;
        data.accum_draw = 0;
        data.accum_frame = 0;
        data.sample_count = 0;
        data.last_report_time = data.draw_end;
    }
}

int game_profiler_sim_ms(void)
{
    return data.avg_sim_ms;
}

int game_profiler_draw_ms(void)
{
    return data.avg_draw_ms;
}

int game_profiler_frame_ms(void)
{
    return data.avg_frame_ms;
}
