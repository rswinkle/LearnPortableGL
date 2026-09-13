
project "5.1.advanced_lighting"
	kind "ConsoleApp"
	files "advanced_lighting.cpp"

project "5.2.gamma_correction"
	kind "ConsoleApp"
	files "gamma_correction.cpp"

project "5.3.1.1.shadow_mapping_depth"
	kind "ConsoleApp"
	files "shadow_mapping_depth.cpp"

project "5.3.1.2.shadow_mapping_base"
	kind "ConsoleApp"
	files "shadow_mapping_base.cpp"

project "5.3.1.3.shadow_mapping"
	kind "ConsoleApp"
	files "shadow_mapping.cpp"

project "5.3.2.1.point_shadows"
	kind "ConsoleApp"
	files "point_shadows.cpp"

project "5.3.2.2.point_shadows_soft"
	kind "ConsoleApp"
	files "point_shadows_soft.cpp"

project "5.4.normal_mapping"
	kind "ConsoleApp"
	files "normal_mapping.cpp"

project "5.5.1.parallax_mapping"
	kind "ConsoleApp"
	files "parallax_mapping.cpp"

project "5.5.2.steep_parallax_mapping"
	kind "ConsoleApp"
	files "steep_parallax_mapping.cpp"

project "5.5.3.parallax_occlusion_mapping"
	kind "ConsoleApp"
	files "parallax_occlusion_mapping.cpp"

project "5.6.hdr"
	kind "ConsoleApp"
	files "hdr.cpp"

project "5.7.bloom"
	kind "ConsoleApp"
	files "bloom.cpp"

project "5.8.1.deferred_shading"
	kind "ConsoleApp"
	files "deferred_shading.cpp"
	links { "assimp" }

project "5.8.2.deferred_shading_volumes"
	kind "ConsoleApp"
	files "deferred_shading_volumes.cpp"
	links { "assimp" }

project "5.9.ssao"
	kind "ConsoleApp"
	files "ssao.cpp"
	links { "assimp" }
