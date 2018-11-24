workspace "rtxON"
    configurations { "Debug", "Release" }
    platforms { "Win64" }

project "rtxON"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++11"
    includedirs {
        "external/glfw/include",
        "external/glm",
        "external/stb",
        "external/tinyobjloader",
        "external/volk",
        "external/vulkan/include",
        "src"
    }
    files {
        "src/**.h",
        "src/**.cpp"
    }

    libdirs { "external/glfw/lib-vc2015" }
    links { "glfw3" }

    filter "platforms:Win64"
        system "Windows"
        architecture "x64"

    filter "configurations:Debug"
        targetsuffix "_D"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

