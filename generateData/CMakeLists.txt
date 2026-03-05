cmake_minimum_required(VERSION 3.22)
include(FetchContent)
project(GenerateData)

set(CMAKE_CXX_STANDARD 23)

set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
FetchContent_Declare(WolfEngineContent
        GIT_REPOSITORY https://github.com/arthur-monteiro/WolfEngine-2.0
        GIT_TAG dfd8710891575d77db2778876f95ba7812ca5031
        SOURCE_SUBDIR "__skip_subdirectory__")
FetchContent_MakeAvailable(WolfEngineContent)

file(GLOB SRC
        "*.cpp"
)

# Includes Wolf libs
include_directories("dependencies/wolfenginecontent-src/Common")
include_directories("dependencies/wolfenginecontent-src/GraphicAPIBroker/Public")
include_directories("dependencies/wolfenginecontent-src/Wolf-Engine-2.0")

include_directories("dependencies/wolfenginecontent-src/ThirdParty/glm")
include_directories(dependencies/wolfenginecontent-src/ThirdParty/vulkan/Include)

add_subdirectory("dependencies/wolfenginecontent-src")

add_executable(GenerateData ${SRC})

link_directories("${CMAKE_CURRENT_SOURCE_DIR}/dependencies/wolfenginecontent-src/x64/Debug/lib")

add_dependencies(GenerateData Common)
add_dependencies(GenerateData GraphicAPIBroker)
add_dependencies(GenerateData WolfEngine)

target_link_libraries(GenerateData PRIVATE
        Common
        GraphicAPIBroker
        WolfEngine
)

set_target_properties(GenerateData
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/x64/${CMAKE_BUILD_TYPE}/exe"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/x64/Debug/exe"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release/exe")