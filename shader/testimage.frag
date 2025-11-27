#version 300 es

precision mediump float;

in vec2 tex_coord;
out vec4 frag_color;

uniform sampler2D tex;

void main()
{
	frag_color = texture(tex, tex_coord);
};
