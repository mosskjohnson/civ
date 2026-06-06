#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 color_old;
uniform vec4 color_highlight_old;
uniform vec4 color_new;
uniform vec4 color_highlight_new;

out vec4 final_color;

void main() {
    vec4 tex = texture(texture0, fragTexCoord);
    vec4 result = tex;
    
    if (distance(tex.rgb, color_old.rgb) < 0.01) {
        result = vec4(color_new.rgb, tex.a);
    } else if (distance(tex.rgb, color_highlight_old.rgb) < 0.01) {
        result = vec4(color_highlight_new.rgb, tex.a);
    }
    
    final_color = result;
}
