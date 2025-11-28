#include "glad/gles2.h"
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <stdlib.h>
#include <stdio.h>

static void check_gl_error(const char* where) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "OpenGL error at %s: 0x%x\n", where, err);
    }
}

uint32_t VAO, VAO2, VBO, VBO2, Texcoord_VBO, Texcoord_VBO2, EBO;
static uint32_t texture;
// Vertex data: position (x, y) + texture coordinates (u, v)
float vertices[] = {
    -0.5f,  0.5f,   // top left
    -0.5f, -0.5f,   // bottom left
     0.5f, -0.5f,   // bottom right
     0.5f,  0.5f   // top right
};
float texture_coords[] =
{
	0.0f, 0.0f,  
	0.0f, 1.0f,  
	1.0f, 1.0f,  
	1.0f, 0.0f   	
};
float vertices2[] = {
    -1.0f,  1.0f,  // top left
    -1.0f, -1.0f,  // bottom left
     1.0f, -1.0f,  // bottom right
     1.0f,  1.0f,  // top right
};

uint32_t indices[] = {
    0, 1, 2,  // first triangle
    0, 2, 3   // second triangle
};
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

	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
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
	if (vertshader == 0) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to compile vertex shader\n");
		return;
	}
	SDL_Log("Vertex shader compiled: %u\n", vertshader);
	
	GLuint fragshader = load_shader(GL_FRAGMENT_SHADER, fshader_src);
	if (fragshader == 0) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to compile fragment shader\n");
		glDeleteShader(vertshader);
		return;
	}
	SDL_Log("Fragment shader compiled: %u\n", fragshader);

	GLuint program = glCreateProgram();
	SDL_Log("Program created: %u\n", program);

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
		glDeleteShader(vertshader);
		glDeleteShader(fragshader);
		return;
	};
	
	SDL_Log("Program linked successfully: %u\n", program);
	
	context->_vertshader = vertshader;
	context->_fragshader = fragshader;
	context->_program = program;
}

static uint32_t load_image()
{
	uint32_t texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	char path[512];
	snprintf(path, sizeof(path), "%s/Sample_interior.png", GAMEPATH);
	SDL_Surface *sf = IMG_Load(path);

	if (!sf) {SDL_Log("Error loading %s\n", path); return 0;};

	SDL_Log("sf=%p pixels=%p w=%d h=%d format=%s",
       (void*)sf,
       sf->pixels,
       sf->w, sf->h,
       SDL_GetPixelFormatName(sf->format));

	sf = SDL_ConvertSurface(sf, SDL_PIXELFORMAT_ABGR8888);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sf->w, sf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, sf->pixels);
	SDL_Log("hihihi\n");

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	SDL_DestroySurface(sf);

	return texture;
};
struct Game
{
	struct EScontext _es;
	SDL_Window *_window;
};

enum SDL_AppResult SDL_AppInit(void **appstate, int argc, char** argv)
{
	if(!SDL_Init(SDL_INIT_VIDEO))
	{
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to init SDL\n");
		return SDL_APP_FAILURE;
	};
	
	struct SDL_Window *window = SDL_CreateWindow("home invasion", 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	
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
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

	;
	struct Game *game = calloc(1, sizeof(struct Game));

	char path[512];

	snprintf(path, sizeof(path), "%s/shader/testimage.vert", GAMEPATH);
	SDL_Log("vertex shader path: %s\n", path);
	char *vshader_src = read_file(path);
	snprintf(path, sizeof(path), "%s/shader/testimage.frag", GAMEPATH);
	SDL_Log("fragment shader path: %s\n", path);
	char *fshader_src = read_file(path);

	load_es_program(&game->_es, vshader_src, fshader_src);

	free(vshader_src);
	free(fshader_src);

	game->_window = window;

	*appstate = game;

	glUseProgram(game->_es._program);
	
	// Set the texture uniform
	GLint texLocation = glGetUniformLocation(game->_es._program, "tex");
	if (texLocation != -1) {
		glUniform1i(texLocation, 0);  // Use texture unit 0
		SDL_Log("Texture uniform location: %d\n", texLocation);
	} else {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not find 'tex' uniform\n");
	}


	texture = load_image();
	glBindTexture(GL_TEXTURE_2D, texture);

	glGenBuffers(1, &VBO);
	glGenBuffers(1, &VBO2);
	glGenBuffers(1, &Texcoord_VBO);
	glGenBuffers(1, &Texcoord_VBO2);
	glGenBuffers(1, &EBO);

	printf("%d %d\n", VBO, VBO2);
	glGenVertexArrays(1, &VAO2);
	
	glBindVertexArray(VAO2);
	
	glBindBuffer(GL_ARRAY_BUFFER, VBO2);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices2), vertices2, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, Texcoord_VBO2);
	glBufferData(GL_ARRAY_BUFFER, sizeof(texture_coords), texture_coords, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

	// Position attribute
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Texture coordinate attribute
	glBindBuffer(GL_ARRAY_BUFFER, Texcoord_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(texture_coords), texture_coords, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);


	return SDL_APP_CONTINUE;
};


SDL_AppResult SDL_AppIterate(void *appstate)
{
	struct Game *game = (struct Game *)appstate;

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	int winw, winh;
	SDL_GetWindowSize(game->_window, &winw, &winh);
	
	glViewport(0, 0, winw, winh);
	glClear(GL_COLOR_BUFFER_BIT);
	
	glUseProgram(game->_es._program);

	GLint currentVBO;
	glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &currentVBO);
	
	printf("%d\n", currentVBO);
	glBindBuffer(GL_ARRAY_BUFFER, currentVBO);
	vertices[0] += .001;
	vertices[4] -= 0.001;
	glBufferSubData(GL_ARRAY_BUFFER, 0 * sizeof(float), sizeof(float), &vertices[0]);
	glBufferSubData(GL_ARRAY_BUFFER, 4 * sizeof(float), sizeof(float), &vertices[4]);

	//glBindVertexArray(VAO);
	//glBindTexture(GL_TEXTURE_2D, texture);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	SDL_GL_SwapWindow(game->_window);

	return SDL_APP_CONTINUE;
};

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
	if (event->type == SDL_EVENT_KEY_DOWN)
	{
		switch (event->key.key)
		{
			case SDLK_A:
				glBindVertexArray(VAO2);
				break;
			case SDLK_B:
				glBindVertexArray(VAO);
				break;
			default:
				break;
		};
	};
	return SDL_APP_CONTINUE;
};

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	free(appstate);
};
