
workspace "LearnPortableGL"
	configurations { "Debug", "Release" }
	location "bin"
	targetdir "bin"

	language "C++"
	links { "SDL2" }
	includedirs { "inc" }

	--configuration "gmake"
	--	buildoptions { "-fno-rtti", "-fno-exceptions", "-Wall" }

	configuration "Debug"
		defines { "DEBUG" }
		symbols "On"

	configuration "Release"
		defines { "NDEBUG" }
		optimize "On"

	printf("%s", _WORKING_DIR)

	-- generate stupid file for filesystem.h
	io.writefile("inc/root_directory.h", "const char* logl_root = \"".._WORKING_DIR.."\";")


	include "src/1.getting_started"
