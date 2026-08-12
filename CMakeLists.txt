cmake_minimum_required(VERSION 3.22)

project(
	TETHER
	VERSION 1.0.0
	LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

macro(set_from_environment VARIABLE)
	if(NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
		set(${VARIABLE} $ENV{${VARIABLE}})
	endif()
endmacro()

# --- CommonLibSSE-NG ---------------------------------------------------------
# TETHER consumes CommonLibSSE-NG via add_subdirectory (same as Precision).
# Point the CommonLibSSEPath_NG environment variable at your CommonLibSSE-NG
# checkout (you already have one from your OStim build). See docs/build/BUILD.md
if(NOT DEFINED ENV{CommonLibSSEPath_NG})
	message(FATAL_ERROR
		"Set the CommonLibSSEPath_NG environment variable to your CommonLibSSE-NG checkout.\n"
		"See docs/build/BUILD.md")
endif()

# --- Optional: copy the built DLL into the game -----------------------------
option(COPY_OUTPUT "Copy the built DLL/PDB into CompiledPluginsPath (game Data folder)" OFF)
set_from_environment(CompiledPluginsPath)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# --- CommonLibSSE-NG build options (must be set before it is add_subdirectory'd) ---
# Don't build CommonLibSSE's own unit tests (avoids the catch2 dependency + build time).
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(src)
