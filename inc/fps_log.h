#ifndef FPS_LOG_H
#define FPS_LOG_H

#include <stdio.h>

#ifndef FPS_EVERY_N_SECS
#define FPS_EVERY_N_SECS 3
#endif

#define FPS_DELAY (FPS_EVERY_N_SECS * 1000)

static inline void fps_log_tick(void)
{
	static int old_time = 0;
	static int counter = 0;
	int new_time = SDL_GetTicks();
	counter++;
	int ms = new_time - old_time;
	if (ms >= FPS_DELAY) {
		printf("%.2f FPS\n", counter * 1000.0f / ms);
		old_time = new_time;
		counter = 0;
	}
}

#endif
