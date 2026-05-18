cmake_minimum_required(VERSION 3.22)
project(SponzaDemo)

set(CMAKE_CXX_STANDARD 23)

set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
FetchContent_Declare(WolfEngineContent
        GIT_REPOSITORY https://github.com/arthur-monteiro/WolfEngine-2.0
        GIT_TAG 0231b578f4e68a9ea34937c3f3bdfff642970799
        SOURCE_SUBDIR "__skip_subdirectory__")
FetchContent_MakeAvailable(WolfEngineContent)

if(ANDROID)
	find_package(game-activity REQUIRED CONFIG)

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

elseif(WIN32)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/wolfenginecontent-src/ThirdParty/Tracy/tracy)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/wolfenginecontent-src/ThirdParty/UltraLight/windows/include)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/wolfenginecontent-src/ThirdParty/vulkan/Include)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/wolfenginecontent-src/ThirdParty/GLFW/include)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/wolfenginecontent-src/ThirdParty/xxh64)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/wolfenginecontent-src/ThirdParty/glm)
endif()

if(ANDROID)
	add_library(${PROJECT_NAME} SHARED
		main.cpp
		UniquePass.cpp
		SystemManager.cpp
	)
else()
	add_executable(${PROJECT_NAME}
		main.cpp
		UniquePass.cpp
		SystemManager.cpp
	)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/wolfenginecontent-src)

if(ANDROID)
	target_link_libraries(${PROJECT_NAME} PUBLIC
		game-activity::game-activity_static
		android
		log
		vulkan
		WolfEngine
		Common
		GraphicAPIBroker
		shaderc_combined
	)
elseif(WIN32)
	set(THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/wolfenginecontent-src/ThirdParty")

	set(VULKAN_LIB_DIR     "${THIRDPARTY_DIR}/vulkan/Lib")
	set(GLFW_LIB_DIR       "${THIRDPARTY_DIR}/GLFW/lib-vc2022")
	set(ULTRALIGHT_LIB_DIR "${THIRDPARTY_DIR}/UltraLight/windows/lib")

	target_link_libraries(${PROJECT_NAME} PUBLIC
			"${VULKAN_LIB_DIR}/vulkan-1.lib"
			"${GLFW_LIB_DIR}/glfw3.lib"
			"${ULTRALIGHT_LIB_DIR}/Ultralight.lib"
			"${ULTRALIGHT_LIB_DIR}/UltralightCore.lib"
			"${ULTRALIGHT_LIB_DIR}/WebCore.lib"
			"${ULTRALIGHT_LIB_DIR}/AppCore.lib"

			WolfEngine
			Common
			GraphicAPIBroker
	)
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC
	WOLF_VULKAN
	GLM_FORCE_DEPTH_ZERO_TO_ONE
	GLM_FORCE_RADIANS
	GLM_ENABLE_EXPERIMENTAL
	TRACY_TIMER_FALLBACK=1
)

if(ANDROID)
	target_compile_definitions(${PROJECT_NAME} PUBLIC
		VK_USE_PLATFORM_ANDROID_KHR=1
		GLM_FORCE_CXX14)
elseif(WIN32)
	target_compile_definitions(${PROJECT_NAME} PUBLIC
		TRACY_ENABLE
		TRACY_ON_DEMAND)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
		"dependencies/wolfenginecontent-src/ThirdParty/glm"
		"dependencies/wolfenginecontent-src/Common"
		"dependencies/wolfenginecontent-src/GraphicAPIBroker/Public"
		"dependencies/wolfenginecontent-src/Wolf-Engine-2.0"
)

add_dependencies(${PROJECT_NAME} Common)
add_dependencies(${PROJECT_NAME} GraphicAPIBroker)
add_dependencies(${PROJECT_NAME} WolfEngine)

if(ANDROID)
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS
			"-u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")
endif()