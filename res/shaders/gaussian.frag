#version 430 core

in vec3 vColor;
in float vOpacity;
in vec3 vConic;
in vec2 vCoord;

flat in int fragIsPointCloud;

in float debugRadius;

out vec4 FragColor;

void main()
{
    // FragColor = vec4(vColor, 1.0);
    // return;
    if (fragIsPointCloud > 0) {
        FragColor = vec4(vColor, 1.0);
        return;
    }
    float power =
        -0.5 * (vConic.x * vCoord.x * vCoord.x +
                vConic.z * vCoord.y * vCoord.y)
        -       vConic.y * vCoord.x * vCoord.y;

    if (power > 0.0) {
        discard;
    }

    float alpha = min(0.99, vOpacity * exp(power));

    if (alpha < (1.0 / 255.0)) {
        discard;
    }

    FragColor = vec4(vColor, alpha);
}