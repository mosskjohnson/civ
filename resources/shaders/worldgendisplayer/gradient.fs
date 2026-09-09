#version 330

in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform int channel; // 0, 1, 2, or 3 for rgba

out vec4 final_color;

void main() {
    vec4 data = texture(texture0, fragTexCoord);
    float v;
    if (channel == 0) v = data.r;
    if (channel == 1) v = data.g;
    if (channel == 2) v = data.b;
    if (channel == 3) v = data.a;
    
    final_color = vec4(v, v, v, 1.0);
}
