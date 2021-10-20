LearnPortableGL
===============

This is a repository to contain ports of the tutorial code from Joey de Vries's
[https://learnopengl.com](https://learnopengl.com).  I've created a new repo
rather than forking and modifying, because the changes will be significant and
I won't be porting every single program.

Some of the obvious changes are moving from GLFW to SDL2, not needing or using GLAD
since we're not using real OpenGL, not needing the shader class, and using Premake
instead of CMake.  Also I'll be working and building on Linux and removed the
windows libraries included but the code itself will still be portable.

Other than that I'll try to generally keep the organization of the repo the same
as [his official one](https://github.com/JoeyDeVries/LearnOpenGL).

