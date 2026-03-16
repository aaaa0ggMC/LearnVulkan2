#version 460

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec4 frag_color;
void main(){
    uint index = gl_VertexIndex % 3;
    gl_Position = vec4(positions[index],0.0 , 1.0);
    frag_color = vec4(colors[index] , 1.0 );
}