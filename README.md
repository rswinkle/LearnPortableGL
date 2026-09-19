LearnPortableGL
===============

This repository to contains [PortableGL](http://portablegl.com) ports of the
tutorial code from Joey de Vries's
[https://learnopengl.com](https://learnopengl.com). I've created a new repo
rather than forking and modifying, because the changes will be significant and
I won't be porting every single program.

Some of the obvious changes are moving from GLFW to SDL2, not needing or using GLAD
since we're not using real OpenGL, not needing the shader class, and using Premake
instead of CMake. We also removed the vendored libraries so you'll have to have things
like SDL2 and assimp installed. We also updated stb_image.h to the latest to get
rid of a few compile warnings.

Also, I'll be working and building on Linux and removed the
windows libraries included but the code itself will still be portable. I may add
it back in the future.

Other than that I'll try to generally keep the organization of the repo the same
as [his official one](https://github.com/JoeyDeVries/LearnOpenGL) except with
a flattened bin directory (no chapter subdirectories) for convenience.

System packages
---------------

Every program needs SDL2. Model loading and skeletal animation also need Assimp,
text rendering and Breakout need FreeType, and Breakout needs SDL2_mixer. glm
and stb_image ship in `inc/`. You also need a C++ compiler, `make`, and
**Premake 5** (`premake5` on your PATH). Debian/Ubuntu's `premake`/`premake4`
packages are still Premake 4 and will not work; grab a 5.x binary from
https://premake.github.io/download (Arch and recent Fedora ship Premake 5 as
`premake`).

Debian / Ubuntu:

```
sudo apt install build-essential libsdl2-dev libsdl2-mixer-dev \
    libassimp-dev libfreetype-dev
```

Fedora:

```
sudo dnf install gcc-c++ make premake SDL2-devel SDL2_mixer-devel \
    assimp-devel freetype-devel
```

Arch:

```
sudo pacman -S --needed base-devel premake sdl2 sdl2_mixer assimp freetype2
```

Then from the repo root: `premake5 gmake` and `make -C bin` (or a single
target such as `make -C bin 1.3.3.shaders_class`).

Programs print an averaged FPS line every 3 seconds (`53.86 FPS`). Define
`FPS_EVERY_N_SECS` before including `fps_log.h` to change the interval. They
use SDL's software renderer with no vsync; the log is there so you can see
whether a demo is actually outrunning the display.

Status: 8/8 chapters finished, ~93/97 programs ported

