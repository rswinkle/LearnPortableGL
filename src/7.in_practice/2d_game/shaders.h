#pragma once
#include "pgl_gl.h"

void sprite_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void sprite_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void particle_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void particle_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void post_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void post_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void text_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void text_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
