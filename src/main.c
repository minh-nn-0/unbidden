#include "app.h"
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <stdlib.h>
#include <stdio.h>


SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
	AppState *app = malloc(sizeof(AppState));
	app_init(app);
	*appstate = app;
	
	return SDL_APP_CONTINUE;
};

SDL_AppResult SDL_AppIterate(void *appstate)
{
	app_mainloop((AppState *)appstate);
	return SDL_APP_CONTINUE;
};

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
	
	return SDL_APP_CONTINUE;
};

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	app_quit((AppState *)appstate);
};