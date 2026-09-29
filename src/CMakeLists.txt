set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(SOURCE_DIR "${ROOT_DIR}/src")

set(SOURCE_FILES
	"${SOURCE_DIR}/main.cpp"
	"${SOURCE_DIR}/PCH.h"
	"${SOURCE_DIR}/InputHandler.h"
	"${SOURCE_DIR}/InputHandler.cpp"
	"${SOURCE_DIR}/TetherController.h"
	"${SOURCE_DIR}/TetherController.cpp"
	"${SOURCE_DIR}/UpdateHook.h"
	"${SOURCE_DIR}/UpdateHook.cpp"
	"${SOURCE_DIR}/SpeedHook.h"
	"${SOURCE_DIR}/SpeedHook.cpp"
	"${SOURCE_DIR}/Settings.h"
	"${SOURCE_DIR}/Settings.cpp"
	"${SOURCE_DIR}/OARConditions.h"
	"${SOURCE_DIR}/OARConditions.cpp"
	"${SOURCE_DIR}/EquipEventSink.h"
	"${SOURCE_DIR}/EquipEventSink.cpp"
	"${SOURCE_DIR}/NetImmerseUtils.h"
	"${SOURCE_DIR}/Prompt.h"
	"${SOURCE_DIR}/Prompt.cpp"
	"${SOURCE_DIR}/UI.h"
	"${SOURCE_DIR}/UI.cpp"
	"${SOURCE_DIR}/API/SkyPrompt/API.hpp"
	"${SOURCE_DIR}/API/SKSEMenuFramework/SKSEMenuFramework.h"
	"${SOURCE_DIR}/API/OpenAnimationReplacer-ConditionTypes.h"
	"${SOURCE_DIR}/API/OpenAnimationReplacer-ConditionTypes.cpp"
	"${SOURCE_DIR}/API/OpenAnimationReplacer-FunctionTypes.h"
	"${SOURCE_DIR}/API/OpenAnimationReplacer-FunctionTypes.cpp"
	"${SOURCE_DIR}/API/OpenAnimationReplacer-SharedTypes.h"
	"${SOURCE_DIR}/API/OpenAnimationReplacerAPI-Animations.h"
	"${SOURCE_DIR}/API/OpenAnimationReplacerAPI-Animations.cpp"
	"${SOURCE_DIR}/API/OpenAnimationReplacerAPI-Conditions.h"
	"${SOURCE_DIR}/API/OpenAnimationReplacerAPI-Conditions.cpp"
	"${SOURCE_DIR}/API/OpenAnimationReplacerAPI-Functions.h"
	"${SOURCE_DIR}/API/OpenAnimationReplacerAPI-Functions.cpp"
	# NOTE: OpenAnimationReplacerAPI-UI.* requires imgui; we don't use OAR UI hooks.
)

source_group(TREE "${ROOT_DIR}" FILES ${SOURCE_FILES})

# Generated version header (Plugin::NAME / Plugin::VERSION)
set(VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/Plugin.h")
configure_file("${ROOT_DIR}/cmake/Plugin.h.in" "${VERSION_HEADER}" @ONLY)
source_group("src" FILES "${VERSION_HEADER}")

configure_file("${ROOT_DIR}/cmake/version.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

add_library(
	"${PROJECT_NAME}"
	SHARED
	${SOURCE_FILES}
	"${VERSION_HEADER}"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
	"${ROOT_DIR}/.clang-format"
)

# CommonLibSSE-NG (recent) requires C++23.
target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_23)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(
		"${PROJECT_NAME}"
		PRIVATE
			"/sdl"              # additional security checks
			"/utf-8"            # source + exec charset UTF-8
			"/permissive-"      # standards conformance
			"/Zc:preprocessor"  # conformant preprocessor
			"/wd4200"           # zero-sized array in struct/union
			"$<$<CONFIG:DEBUG>:/ZI>"
			"$<$<CONFIG:RELEASE>:/Zi;/Zc:inline;/Ob3>"
	)
	target_link_options(
		"${PROJECT_NAME}"
		PRIVATE
			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif()

target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE
		"${CMAKE_CURRENT_BINARY_DIR}/src"
		"${SOURCE_DIR}"
)

find_path(SIMPLEINI_INCLUDE_DIRS "SimpleIni.h" REQUIRED)
target_include_directories("${PROJECT_NAME}" PRIVATE ${SIMPLEINI_INCLUDE_DIRS})

add_subdirectory("$ENV{CommonLibSSEPath_NG}" CommonLibSSE EXCLUDE_FROM_ALL)

target_link_libraries(
	"${PROJECT_NAME}"
	PRIVATE
		CommonLibSSE::CommonLibSSE
)

target_precompile_headers("${PROJECT_NAME}" PRIVATE "${SOURCE_DIR}/PCH.h")

install(
	FILES "$<TARGET_FILE:${PROJECT_NAME}>"
	DESTINATION "SKSE/Plugins"
	COMPONENT "main"
)
install(
	FILES "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
	DESTINATION "/"
	COMPONENT "pdbs"
)

if(COPY_OUTPUT AND DEFINED CompiledPluginsPath)
	add_custom_command(
		TARGET "${PROJECT_NAME}"
		POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${CompiledPluginsPath}/SKSE/Plugins/"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${CompiledPluginsPath}/SKSE/Plugins/"
		VERBATIM
	)
endif()
