# Gaussian Splatting 3D Renderer

A real-time 3D Gaussian Splatting renderer built in C++ and OpenGL. This project is mostly based on the [3D Gaussian Splatting rasterizer](https://github.com/graphdeco-inria/diff-gaussian-rasterization.git) from the paper [3D Gaussian Splatting for Real-Time Rendering of Radiance Fields](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/) , loading trained scene representations from `.ply` point cloud files and rendering them interactively using OpenGL and GLSL shaders.

Built as part of the NTNU course **TDT4230 – Graphics and Visualization**, extending the [TDT4230 base framework](https://github.com/bartvbl/TDT4230-Assignment-1).

---

## Features

- Real-time rendering of 3D Gaussian Splats from `.ply` files
- GLSL-based splatting pipeline with vertex and fragment shaders
- Interactive camera navigation 
	- W, A, S, D for moving forward, left, backward and right
	- Q, E for moving up and down
	- Left click on mouse and drag to rotate camera

---

## Requirements

### Linux

- GCC or Clang (C++14 or newer)
- CMake 3.6+
- Git
- [PCL (Point Cloud Library)](https://pointclouds.org/) — for `.ply` file loading
- Python 3 — used during the build process to generate GLAD bindings

Install PCL on Ubuntu/Debian:

```bash
sudo apt install libpcl-dev
```

### Windows (Not tested and based on the base code)

- Microsoft Visual Studio (with C++ workload)
- CMake (GUI or command-line)
- PCL for Windows — see [PCL releases](https://github.com/PointCloudLibrary/pcl/releases)

---

## Building and Running

### Linux (recommended)

Clone the repository with submodules:

```bash
git clone --recursive https://github.com/paulHMorud/Gaussian-Splatting-3D-Renderer.git
cd Gaussian-Splatting-3D-Renderer
```

If you forgot `--recursive`, initialize submodules manually:

```bash
git submodule update --init
```

Then build and run:

```bash
make run
```

This is equivalent to:

```bash
git submodule update --init
cd build
cmake ..
make
./glowbox
```


#### Other build targets

| Target | Description |
|--------|-------------|
| `make run` | Build and run (release mode) |
| `make run-debug` | Build and run in debug mode (requires GDB) |
| `make run-prof` | Build and run with profiling info |
| `make build` | Build only |
| `make clean` | Remove build artifacts |
| `make help` | List all available targets |

### Windows (Not tested, instructions from claude)

1. Install Visual Studio and CMake.
2. Run CMake (GUI or CLI) pointing at the repository root.
3. Open the generated `.sln` solution and build from Visual Studio.

---


## Dependencies

All C++ dependencies are included as git submodules under `lib/`, except PCL which must be installed separately on your system.

| Library | Purpose |
|---------|---------|
| [GLFW](https://github.com/glfw/glfw) | Window creation and input |
| [GLAD](https://github.com/Dav1dde/glad) | OpenGL function loader |
| [GLM](https://github.com/g-truc/glm) | OpenGL mathematics |
| [STB](https://github.com/nothings/stb) | Image loading utilities |
| [lodepng](https://github.com/lvandeve/lodepng) | PNG encoding/decoding |
| [arrrgh](https://github.com/ElectricToy/arrrgh) | Argument parsing |
| [SFML](https://github.com/SFML/SFML) | Audio playback |
| [fmt](https://github.com/fmtlib/fmt) | String formatting |
| [PCL](https://pointclouds.org/) | Point cloud / `.ply` file I/O |

---

## Background

3D Gaussian Splatting is a scene representation technique introduced in the SIGGRAPH 2023 paper:

> Kerbl et al., *"3D Gaussian Splatting for Real-Time Radiance Field Rendering"*, SIGGRAPH 2023.

Scenes are represented as a collection of 3D Gaussians, each with a position, scale, rotation, opacity, and color (encoded via spherical harmonics). Rendering is done by projecting and splatting each Gaussian onto the 2D screen using a fast rasterization pipeline.

---
