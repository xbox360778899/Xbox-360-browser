#version 450
// Filled-rectangle helper for the UI: unit-square VB scaled by push constants.
// Pixel coordinates, origin at the viewport's top left; D3D clip space is y-up.
layout(location = 0) in vec2 a_pos;
layout(push_constant) uniform Pc {
    vec4 rect;       // x, y, w, h in pixels
    vec4 inv_view;   // 1/width, 1/height, 0, 0
};
void main() {
    vec2 px = rect.xy + a_pos * rect.zw;
    vec2 clip = vec2(px.x * inv_view.x * 2.0 - 1.0,
                     1.0 - px.y * inv_view.y * 2.0);
    gl_Position = vec4(clip, 0.0, 1.0);
}
