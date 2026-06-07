cmake_minimum_required(VERSION 3.20)

include(FetchContent)
set(PROTOCOL_ENABLE_FORMATTING ON CACHE BOOL "Enable formatting support in Protocol")
FetchContent_Declare(
	Protocol
	GIT_REPOSITORY https://github.com/SculkCatalystMC/Protocol.git
	GIT_TAG main
)
FetchContent_Declare(
	reflection
	GIT_REPOSITORY https://github.com/SculkCatalystMC/Reflection.git
	GIT_TAG main
)
FetchContent_MakeAvailable(Protocol reflection)

project(ProxyPass LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE
	MAGIC_ENUM_RANGE_MIN=0
	MAGIC_ENUM_RANGE_MAX=512
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	Protocol::Protocol
	reflection::reflection
)

target_include_directories(${PROJECT_NAME}
	PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${PROJECT_SOURCES})
