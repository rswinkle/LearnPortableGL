
project "4.1.1.depth_testing"
	kind "ConsoleApp"
	files "depth_testing.cpp"

project "4.1.2.depth_testing_view"
	kind "ConsoleApp"
	files "depth_testing_view.cpp"

project "4.2.stencil_testing"
	kind "ConsoleApp"
	files "stencil_testing.cpp"

project "4.3.1.blending_discard"
	kind "ConsoleApp"
	files "blending_discard.cpp"

project "4.3.2.blending_sorted"
	kind "ConsoleApp"
	files "blending_sorted.cpp"

project "4.5.1.framebuffers"
	kind "ConsoleApp"
	files "framebuffers.cpp"

project "4.5.2.framebuffers_exercise1"
	kind "ConsoleApp"
	files "framebuffers_exercise1.cpp"

project "4.6.1.cubemaps_skybox"
	kind "ConsoleApp"
	files "cubemaps_skybox.cpp"

project "4.6.2.cubemaps_environment_mapping"
	kind "ConsoleApp"
	files "cubemaps_environment_mapping.cpp"

project "4.10.1.instancing_quads"
	kind "ConsoleApp"
	files "instancing_quads.cpp"

project "4.10.2.asteroids"
	kind "ConsoleApp"
	files "asteroids.cpp"
	links { "assimp" }

project "4.10.3.asteroids_instanced"
	kind "ConsoleApp"
	files "asteroids_instanced.cpp"
	links { "assimp" }
