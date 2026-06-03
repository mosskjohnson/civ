#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D noise;
uniform float time;

out vec4 finalColor;

void main() {
    vec4 fog_mask = texture(texture0, fragTexCoord);
    
    vec2 uv1 = fragTexCoord + vec2(time * 0.015, time * 0.01);
    vec2 uv2 = fragTexCoord - vec2(time * 0.008, time * 0.015);
    float n = texture(noise, uv1).r * 0.5 + texture(noise, uv2).r * 0.5;
    float noise_multiplier = 2.0;
    float noise_strength = -(abs(fog_mask.a-0.5)*2)+1;
    float alpha = fog_mask.a + noise_strength*noise_multiplier*(n-0.5);

    finalColor = vec4(0.0, 0.0, 0.0, clamp(alpha, 0.0, 1.0));
}
