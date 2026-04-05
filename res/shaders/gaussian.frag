#version 430 core

in vec3 vColor;
in float vOpacity;
in vec3 vConic;
in vec2 vCoord;
flat in int fragIsPointCloud;

out vec4 FragColor;

void main()
{
    if (fragIsPointCloud > 0) {
        FragColor = vec4(vColor, 1.0);
        return;
    }

    float power = -0.5 * (vConic.x * vCoord.x * vCoord.x +
                          2.0 * vConic.y * vCoord.x * vCoord.y +
                          vConic.z * vCoord.y * vCoord.y);

    if (power > 0.0) {
        discard;
    }

    float alpha = min(1.0f, vOpacity * exp(power));

    if (alpha < (1.0 / 255.0)) {
        discard;
    }

    FragColor = vec4(vColor, alpha);
}