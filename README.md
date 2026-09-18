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

Programs print an averaged FPS line every 3 seconds (`3000 12 FPS`). Define
`FPS_EVERY_N_SECS` before including `fps_log.h` to change the interval. They
use SDL's software renderer with no vsync; the log is there so you can see
whether a demo is actually outrunning the display.

Status: 7/8 chapters finished, ~81/97 programs ported

