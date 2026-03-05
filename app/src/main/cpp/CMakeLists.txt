cmake_minimum_required(VERSION 3.22)
project(SponzaDemo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(game-activity REQUIRED CONFIG)

set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
FetchContent_Declare(WolfEngineContent
        GIT_REPOSITORY https://github.com/arthur-monteiro/WolfEngine-2.0
        GIT_TAG 825b17c6714271c51fee20b9f3297e2aaf613207
        SOURCE_SUBDIR "__skip_subdirectory__")
FetchContent_MakeAvailable(WolfEngineContent)

# Configure shaderc
add_library(shaderc_combined INTERFACE)
set(SHADERC_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/wolfenginecontent-src/ThirdParty/android/libshaderc/libs/${ANDROID_ABI}")

target_link_libraries(shaderc_combined INTERFACE
		"${SHADERC_LIB_DIR}/libshaderc.a"
		"${SHADERC_LIB_DIR}/libshaderc_util.a"
		"${SHADERC_LIB_DIR}/libglslang.a"
		"${SHADERC_LIB_DIR}/libSPIRV.a"
		"${SHADERC_LIB_DIR}/libSPIRV-Tools.a"
		"${SHADERC_LIB_DIR}/libSPIRV-Tools-opt.a"
		"${SHADERC_LIB_DIR}/libOSDependent.a"
		"${SHADERC_LIB_DIR}/libOGLCompiler.a"
)
target_include_directories(shaderc_combined INTERFACE "${ANDROID_NDK}/sources/third_party/shaderc/libshaderc/include")

add_library(${PROJECT_NAME} SHARED
		main.cpp
		UniquePass.cpp
		SystemManager.cpp
)

add_subdirectory("dependencies/wolfenginecontent-src")

target_link_libraries(${PROJECT_NAME} PUBLIC
		# Android NDK libs
		game-activity::game-activity_static
		android
		log
		vulkan

		# Internal engine targets
		WolfEngine
		Common
		GraphicAPIBroker

		# Third party
		shaderc_combined
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
		WOLF_VULKAN
		VK_USE_PLATFORM_ANDROID_KHR=1
		GLM_FORCE_CXX14
		GLM_FORCE_DEPTH_ZERO_TO_ONE
		GLM_FORCE_RADIANS
		GLM_ENABLE_EXPERIMENTAL
		TRACY_TIMER_FALLBACK=1  # Fixes the invariant TSC error
)

target_include_directories(${PROJECT_NAME} PRIVATE
		"dependencies/wolfenginecontent-src/ThirdParty/glm"
		"dependencies/wolfenginecontent-src/Common"
		"dependencies/wolfenginecontent-src/GraphicAPIBroker/Public"
		"dependencies/wolfenginecontent-src/Wolf-Engine-2.0"
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS
		"-u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")