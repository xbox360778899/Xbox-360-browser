#version 450
layout(push_constant) uniform Pc { vec4 color; };
layout(location = 0) out vec4 out_color;
void main() { out_color = color; }
