#version 430 core
// This shader is also based on the rasterizer from the original gaussian splatting paper "3D Gaussian Splatting for Real-Time Radiance Field Rendering"
// The original code can be found here: https://github.com/graphdeco-inria/diff-gaussian-rasterization/blob/main/cuda_rasterizer 
// The calculations are based on the function renderCUDA() in the forward.cu file

in vec3 vColor;
in float vOpacity;
in vec3 vConic;
in vec2 vCoord;
flat in int fragIsPointCloud;

out vec4 FragColor;

void main()
{
    // FragColor = vec4(vColor, 1.0);
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