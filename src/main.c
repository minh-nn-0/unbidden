#include "glad/gles2.h"
#include <GLES3/gl32.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <stdlib.h>
#include <stdio.h>

struct EScontext
{
	GLuint _program;
	GLuint _vertshader;
	GLuint _fragshader;
};

char* read_file(const char* filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file) {perror("fopen");return NULL;};
    if (fseek(file,0,SEEK_END) != 0) {
        perror("fseek");
        fclose(file);
        return nullptr;
    };
    long size = ftell(file);
    if (size < 0)
    {
        perror("ftell");
        fclose(file);
        return nullptr;
    };
    rewind(file);

    char *buffer = malloc(size + 1);
    if (!buffer)
    {
        perror("malloc");
        fclose(file);
        return nullptr;
    };
    
    size_t read_size = fread(buffer, 1, size, file);
    if (read_size != size)
    {
        perror("fread");
		free(buffer);
        fclose(file);
        return nullptr;
    };
    buffer[size] = '\0';

    fclose(file);
    return buffer;
}

static GLuint load_shader(GLuint type, const char *src)
{
	GLuint shader;
	int compiled;

	shader = glCreateShader(type);
	if (shader == 0) {return 0;};

	glShaderSource(shader, 1, &src, nullptr);

	glCompileShader(shader);

	glGetProgramiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		GLint infolength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infolength);
		
		if (infolength > 1)
		{
			char *info = malloc(infolength * sizeof(char));
			glGetShaderInfoLog(shader, infolength, nullptr, info);
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error compiling shader: %s\n", info);
			free(info);
		};
		glDeleteShader(shader);
		return 0;
	};

	return shader;
};

static void load_es_program(struct EScontext *context, const char *vshader_src, const char *fshader_src)
{
	
	GLuint vertshader = load_shader(GL_VERTEX_SHADER, vshader_src);
	GLuint fragshader = load_shader(GL_FRAGMENT_SHADER, fshader_src);

	GLuint program = glCreateProgram();

	glAttachShader(program, vertshader);
	glAttachShader(program, fragshader);

	glLinkProgram(program);

	GLint link_status;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	
	if (!link_status)
	{
		GLint infolength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infolength);
		
		if (infolength > 1)
		{
			char *info = malloc(infolength * sizeof(char));
			glGetProgramInfoLog(program, infolength, nullptr, info);
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error linking opengl program: %s\n", info);
			free(info);
		};
		glDeleteProgram(program);
		return;
	};
	
	context->_vertshader = vertshader;
	context->_fragshader = fragshader;
	context->_program = program;
};

struct Game
{
	struct EScontext _es;
	SDL_Window *_window;
};

enum SDL_AppResult SDL_AppInit(void **appstate, int argc, char** argv)
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

	if(!SDL_Init(SDL_INIT_VIDEO))
	{
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to init SDL\n");
		return SDL_APP_FAILURE;
	};
	
	struct SDL_Window *window = SDL_CreateWindow("home invasion", 800, 600, SDL_WINDOWPOS_UNDEFINED | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	
	if (!window)
	{
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create Window\n");
		return SDL_APP_FAILURE;
	};

	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!gladLoadGLES2(SDL_GL_GetProcAddress))
	{
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to get proc address for glad\n");
		return SDL_APP_FAILURE;
	};

	struct Game *game = malloc(sizeof(struct Game));

	char path[512];

	snprintf(path, sizeof(path), "%sshader/test.vert", GAMEPATH);
	SDL_Log("vertex shader path: %s\n", path);
	char *vshader_src = read_file(path);
	snprintf(path, sizeof(path), "%sshader/test.frag", GAMEPATH);
	SDL_Log("fragment shader path: %s\n", path);
	char *fshader_src = read_file(path);

	load_es_program(&game->_es, vshader_src, fshader_src);

	free(vshader_src);
	free(fshader_src);

	game->_window = window;

	*appstate = game;
	return SDL_APP_CONTINUE;
};

SDL_AppResult SDL_AppIterate(void *appstate)
{
	struct Game *game = (struct Game *)appstate;

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	float vertices[] = 
		{	0.0f, 0.5f, 0.0f,
		  	-0.5f, -0.5f, 0.0f,
			0.5f, -0.5f, 0.0f	};
	
	glViewport(0, 0, 800, 600);
	glClear(GL_COLOR_BUFFER_BIT);
	
	glUseProgram(game->_es._program);

	//Load vertex data
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	glEnableVertexAttribArray(0);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	SDL_GL_SwapWindow(game->_window);

	return SDL_APP_CONTINUE;
};

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	return SDL_APP_CONTINUE;
};

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	free(appstate);
};
