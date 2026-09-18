
project "7.1.debugging"
	kind "ConsoleApp"
	files "debugging.cpp"

project "7.2.text_rendering"
	kind "ConsoleApp"
	files "text_rendering.cpp"
	includedirs { "/usr/include/freetype2" }
	links { "freetype" }

project "7.3.2d_game"
	kind "ConsoleApp"
	files { "2d_game/*.cpp", "2d_game/*.h" }
	includedirs { "2d_game", "/usr/include/freetype2" }
	links { "freetype" }
