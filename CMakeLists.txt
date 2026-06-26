cmake_minimum_required(VERSION 3.20)
project(CursorRing CXX)

set(CMAKE_CXX_STANDARD 17)

# ── Sources ────────────────────────────────────────────────────────────────
add_library(CursorRing SHARED
    src/main.cpp
    src/options.cpp
    src/cursor_hook.cpp
    src/cursor_compose.cpp
    src/settings.cpp
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
)

# ── Includes ───────────────────────────────────────────────────────────────
# Expects the following laid out in the repo root:
#   nexus/        ← Nexus SDK headers     (https://raidcore.gg/Nexus)
#   mumble/       ← Mumble link headers   (comes with Nexus SDK)
#   imgui/        ← ImGui headers/sources (comes with Nexus SDK)
target_include_directories(CursorRing PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# ── Export only GetAddonDef ────────────────────────────────────────────────
set_target_properties(CursorRing PROPERTIES
    LINK_FLAGS "/DEF:\"${CMAKE_SOURCE_DIR}/CursorRing.def\""
)

# ── Output name Nexus expects ──────────────────────────────────────────────
# Drop the resulting CursorRing.dll into:
#   %USERPROFILE%\Documents\Guild Wars 2\addons\CursorRing\CursorRing.dll
set_target_properties(CursorRing PROPERTIES
    OUTPUT_NAME "CursorRing"
    PREFIX ""
)
