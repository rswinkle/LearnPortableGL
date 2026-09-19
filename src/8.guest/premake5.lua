
project "8.guest.2020.oit"
	kind "ConsoleApp"
	files "2020/oit/weighted_blended.cpp"

project "8.guest.2020.skeletal_animation"
	kind "ConsoleApp"
	files "2020/skeletal_animation/skeletal_animation.cpp"
	links { "assimp" }

project "8.guest.2021.scene_graph"
	kind "ConsoleApp"
	files "2021/1.scene/1.scene_graph/scene_graph.cpp"
	links { "assimp" }

project "8.guest.2021.frustum_culling"
	kind "ConsoleApp"
	files "2021/1.scene/2.frustum_culling/frustum_culling.cpp"
	links { "assimp" }

project "8.guest.2021.csm"
	kind "ConsoleApp"
	files "2021/2.csm/shadow_mapping.cpp"

project "8.guest.2021.terrain_gpu_dist"
	kind "ConsoleApp"
	files "2021/3.tessellation/terrain_gpu_dist/main.cpp"

project "8.guest.2021.terrain_cpu_src"
	kind "ConsoleApp"
	files "2021/3.tessellation/terrain_cpu_src/main.cpp"

project "8.guest.2021.dsa"
	kind "ConsoleApp"
	files "2021/4.dsa/hello_triangle_dsa.cpp"

project "8.guest.2022.computeshader_helloworld"
	kind "ConsoleApp"
	files "2022/5.computeshader_helloworld/compute_shader_hello_world.cpp"

project "8.guest.2022.physically_based_bloom"
	kind "ConsoleApp"
	files "2022/6.physically_based_bloom/physically_based_bloom.cpp"

project "8.guest.2022.area_light"
	kind "ConsoleApp"
	files { "2022/7.area_lights/1.area_light/area_lights.cpp", "2022/7.area_lights/*.hpp" }

project "8.guest.2022.multiple_area_lights"
	kind "ConsoleApp"
	files { "2022/7.area_lights/2.multiple_area_lights/multiple_area_lights.cpp", "2022/7.area_lights/*.hpp" }
