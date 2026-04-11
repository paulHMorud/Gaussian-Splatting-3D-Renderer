// Local headers
#include "program.hpp"
#include "utilities/window.hpp"
#include "gamelogic.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <SFML/Audio.hpp>
#include <SFML/System/Time.hpp>
#include <utilities/glutils.h>
#include <utilities/shader.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <utilities/timeutils.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// --- Recording dependencies ---
// Drop stb_image_write.h into your project root (single-header, no build step needed).
// Download from: https://github.com/nothings/stb/blob/master/stb_image_write.h
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cstdlib>   // system()
#include <cstdio>    // snprintf
#include <string>

// ---------------------------------------------------------------------------
// Orbit recorder configuration – tweak these to taste
// ---------------------------------------------------------------------------
namespace OrbitConfig {
    // Centre of the bicycle scene (adjust to taste after a first test run)
    constexpr glm::vec3 kCenter      = glm::vec3(0.0f, 0.0f, 0.0f);
    // How far the camera sits from the centre
    constexpr float     kRadius      = 3.0f;
    // Height of the camera above the centre point
    constexpr float     kHeight      = 0.1f;
    // Total number of frames in the 360° orbit
    constexpr int       kTotalFrames = 300;        // 300 frames @ 30 fps = 10 s
    // Output framerate passed to ffmpeg
    constexpr int       kFps         = 30;
    // Number of frames to render (and discard) before recording starts.
    // This lets the radix sort warm up so the first real frame is clean.
    constexpr int       kWarmupFrames = 60;
    // Folder that frame PNGs are written into
    const std::string   kFrameDir    = "orbit_frames";
    // Final output file
    const std::string   kOutputMp4   = "orbit.mp4";
}

// ---------------------------------------------------------------------------
// Capture the current OpenGL front-buffer and save it as a PNG.
// ---------------------------------------------------------------------------
static void captureFrame(int frameIndex, int width, int height)
{
    std::vector<unsigned char> pixels(width * height * 3);

    // Ensure all GPU commands are done before reading
    glFinish();
    // Avoid row-padding issues (default alignment is 4, but width*3 may not be a multiple of 4)
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    // Read from the front buffer (the fully composited frame after swap)
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glReadBuffer(GL_BACK); // restore default

    // OpenGL has (0,0) at bottom-left; PNG expects top-left – flip rows.
    std::vector<unsigned char> flipped(pixels.size());
    const int rowBytes = width * 3;
    for (int row = 0; row < height; ++row) {
        std::memcpy(
            flipped.data() + row * rowBytes,
            pixels.data()  + (height - 1 - row) * rowBytes,
            rowBytes
        );
    }

    char path[512];
    std::snprintf(path, sizeof(path),
                  "%s/frame_%05d.png",
                  OrbitConfig::kFrameDir.c_str(), frameIndex);

    if (!stbi_write_png(path, width, height, 3, flipped.data(), rowBytes)) {
        std::cerr << "[Recorder] Failed to write " << path << "\n";
    }
}

// ---------------------------------------------------------------------------
// After all frames are written, call ffmpeg to produce orbit.mp4.
// Requires ffmpeg to be on PATH (apt install ffmpeg / brew install ffmpeg).
// ---------------------------------------------------------------------------
static void assembleMp4()
{
    std::string cmd =
        "ffmpeg -y"
        " -framerate " + std::to_string(OrbitConfig::kFps) +
        " -i "         + OrbitConfig::kFrameDir + "/frame_%05d.png"
        " -c:v libx264"
        " -crf 18"           // quality: 0=lossless, 51=worst; 18 looks great
        " -preset slow"      // better compression, still fast enough
        " -pix_fmt yuv420p"  // widest player compatibility
        " "                  + OrbitConfig::kOutputMp4;

    std::cout << "[Recorder] Running: " << cmd << "\n";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "[Recorder] ffmpeg returned non-zero exit code: " << ret << "\n";
        std::cerr << "[Recorder] Make sure ffmpeg is installed and on PATH.\n";
    } else {
        std::cout << "[Recorder] Saved → " << OrbitConfig::kOutputMp4 << "\n";
    }
}

// ---------------------------------------------------------------------------
// Main program entry point
// ---------------------------------------------------------------------------
void runProgram(GLFWwindow* window, CommandLineOptions options)
{
    initGame(window, options);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    // ------------------------------------------------------------------
    // Determine whether we are in orbit-recording mode.
    // Pass  --record  on the command line to activate.
    // (CommandLineOptions already parses argv; add the flag there, or
    //  just hard-code `bool recording = true;` for a quick test.)
    // ------------------------------------------------------------------
    const bool recording = options.record; // see program.hpp / main.cpp note below

    if (recording) {
        system(("mkdir -p " + OrbitConfig::kFrameDir).c_str());
        std::cout << "[Recorder] Orbit recording mode ON\n";
        std::cout << "[Recorder] Warmup frames : " << OrbitConfig::kWarmupFrames << "\n";
        std::cout << "[Recorder] Capture frames: " << OrbitConfig::kTotalFrames  << "\n";
        std::cout << "[Recorder] Output dir    : " << OrbitConfig::kFrameDir     << "\n";
    }

    double lastTime      = glfwGetTime();
    double fpsUpdateTimer = 0.0;
    int    fpsFrameCount  = 0;

    // Counters used only in recording mode
    int warmupLeft    = OrbitConfig::kWarmupFrames;
    int capturedFrames = 0;

    // -----------------------------------------------------------------------
    // Rendering loop
    // -----------------------------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        double currentTime = glfwGetTime();
        float  deltaTime   = float(currentTime - lastTime);
        lastTime = currentTime;

        // FPS counter
        fpsUpdateTimer += deltaTime;
        fpsFrameCount++;
        if (fpsUpdateTimer >= 0.25) {
            gCurrentFps = float(fpsFrameCount / fpsUpdateTimer);
            fpsUpdateTimer = 0.0;
            fpsFrameCount  = 0;
        }

        // ------------------------------------------------------------------
        // Orbit camera override (recording mode)
        // ------------------------------------------------------------------
        if (recording) {
            // Total frames processed so far (warmup + captured)
            const int totalProcessed = (OrbitConfig::kWarmupFrames - warmupLeft) + capturedFrames;

            // The angle advances each frame whether we are warming up or not,
            // so the scene geometry is sorted correctly from the start.
            float angle = glm::two_pi<float>()
                          * float(capturedFrames) / float(OrbitConfig::kTotalFrames);

            // Position on a horizontal circle around kCenter
            glm::vec3 camPos = OrbitConfig::kCenter + glm::vec3(
                OrbitConfig::kRadius * std::cos(angle),
                OrbitConfig::kHeight,
                OrbitConfig::kRadius * std::sin(angle)
            );

            // Yaw so the camera always faces the centre
            // atan2 gives the angle from +X toward +Z; we negate because the
            // camera's forward is -Z in view space.
            float yaw = std::atan2(
                camPos.x - OrbitConfig::kCenter.x,
                camPos.z - OrbitConfig::kCenter.z
            );

            // Pitch slightly downward to frame the scene nicely
            float pitch = 0.05f; // radians 

            camera->setOrbitPose(camPos, yaw, pitch);
        } else {
            // Normal interactive camera update
            camera->updateCamera(deltaTime);
        }

        // ------------------------------------------------------------------
        // ImGui
        // ------------------------------------------------------------------
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (!recording) {
            renderDebugUI();
        }

        // ------------------------------------------------------------------
        // Render scene
        // ------------------------------------------------------------------
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderFrame(window);

        // ------------------------------------------------------------------
        // UI + swap
        // ------------------------------------------------------------------
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwPollEvents();

        if (!recording) {
            handleKeyboardInput(window);
        }

        glfwSwapBuffers(window);

        // ------------------------------------------------------------------
        // Capture AFTER swap so we read the fully composited front buffer
        // ------------------------------------------------------------------
        int fbWidth = 0, fbHeight = 0;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        if (recording) {
            if (warmupLeft > 0) {
                --warmupLeft;
                // Skip capture – just let the sort/render pipeline stabilise
            } else if (capturedFrames < OrbitConfig::kTotalFrames) {
                captureFrame(capturedFrames, fbWidth, fbHeight);
                ++capturedFrames;
                std::cout << "\r[Recorder] Captured " << capturedFrames
                          << " / " << OrbitConfig::kTotalFrames << std::flush;
            } else {
                // All frames captured – assemble and exit
                std::cout << "\n[Recorder] All frames captured. Assembling MP4...\n";
                assembleMp4();
                glfwSetWindowShouldClose(window, GL_TRUE);
            }
        }
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void handleKeyboardInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
}