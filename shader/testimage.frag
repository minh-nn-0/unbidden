#version 300 es
precision mediump float;

in vec2 tex_coord;
out vec4 frag_color;

uniform sampler2D tex;

void main()
{
    vec4 tex_color = texture(tex, tex_coord);
    //frag_color = vec4(tex_color.rgb * vec3(0.2,0.2,0.2), tex_color.a);
}