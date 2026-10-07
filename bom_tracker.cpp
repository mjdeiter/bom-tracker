// bom_tracker.cpp — Bill of Materials Tracker
// Dear ImGui + SQLite3, single-file build
// Build: see build.sh

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <sqlite3.h>

#include <algorithm>
#include <unordered_map>
#include <set>
#include <filesystem>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>

static const char* APP_VERSION = "1.7.0";
#ifndef BUILD_HASH
#define BUILD_HASH "dev"
#endif
static const char* BUILD_HASH_STR = BUILD_HASH;

// ─── Palette ────────────────────────────────────────────────────────────────
static const ImVec4 COL_BG          = {0.10f, 0.11f, 0.13f, 1.00f};
static const ImVec4 COL_PANEL       = {0.13f, 0.14f, 0.17f, 1.00f};
static const ImVec4 COL_BORDER      = {0.22f, 0.24f, 0.28f, 1.00f};
static const ImVec4 COL_ACCENT      = {0.95f, 0.65f, 0.15f, 1.00f};
static const ImVec4 COL_ACCENT_DIM  = {0.60f, 0.42f, 0.10f, 1.00f};
static const ImVec4 COL_TEXT        = {0.88f, 0.88f, 0.85f, 1.00f};
static const ImVec4 COL_TEXT_DIM    = {0.55f, 0.57f, 0.60f, 1.00f};
static const ImVec4 COL_RED         = {0.85f, 0.28f, 0.28f, 1.00f};
static const ImVec4 COL_GREEN       = {0.35f, 0.78f, 0.42f, 1.00f};
static const ImVec4 COL_YELLOW      = {0.90f, 0.80f, 0.20f, 1.00f};
static const ImVec4 COL_BLUE        = {0.35f, 0.60f, 0.90f, 1.00f};
static const ImVec4 COL_HEADER_BG   = {0.17f, 0.19f, 0.22f, 1.00f};
static const ImVec4 COL_ROW_ALT     = {0.15f, 0.16f, 0.19f, 1.00f};
static const ImVec4 COL_SEL_BG      = {0.25f, 0.20f, 0.05f, 1.00f};

// ─── Embedded 32×32 RGBA Window Icon ────────────────────────────────────────
static const int ICON_W = 32;
static const int ICON_H = 32;
static const unsigned char ICON_DATA[] = {
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,242,166,38,255,
    242,166,38,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,242,166,38,255,
    242,166,38,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,242,166,38,255,
    242,166,38,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,242,166,38,255,
    242,166,38,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,40,45,50,255,40,45,50,255,40,45,50,255,
    40,45,50,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,40,45,50,255,242,166,38,255,242,166,38,255,
    40,45,50,255,15,16,20,255,15,16,20,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,80,85,90,255,
    80,85,90,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,40,45,50,255,242,166,38,255,242,166,38,255,
    40,45,50,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,40,45,50,255,40,45,50,255,40,45,50,255,
    40,45,50,255,15,16,20,255,15,16,20,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,80,85,90,255,
    80,85,90,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,40,45,50,255,40,45,50,255,40,45,50,255,
    40,45,50,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,40,45,50,255,242,166,38,180,242,166,38,180,
    40,45,50,255,15,16,20,255,15,16,20,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,40,45,50,255,242,166,38,180,242,166,38,180,
    40,45,50,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,40,45,50,255,40,45,50,255,40,45,50,255,
    40,45,50,255,15,16,20,255,15,16,20,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,40,45,50,255,40,45,50,255,40,45,50,255,
    40,45,50,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,40,45,50,255,80,200,100,255,80,200,100,255,
    40,45,50,255,15,16,20,255,15,16,20,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,80,85,90,255,
    80,85,90,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,40,45,50,255,80,200,100,255,80,200,100,255,
    40,45,50,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,40,45,50,255,40,45,50,255,40,45,50,255,
    40,45,50,255,15,16,20,255,15,16,20,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,80,85,90,255,
    80,85,90,255,80,85,90,255,80,85,90,255,80,85,90,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,242,166,38,255,
    242,166,38,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,242,166,38,255,
    242,166,38,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,242,166,38,255,
    242,166,38,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,15,16,20,255,
    15,16,20,255,15,16,20,255,15,16,20,255,242,166,38,255,
    242,166,38,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,242,166,38,255,242,166,38,255,
    242,166,38,255,242,166,38,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
    26,28,33,255,26,28,33,255,26,28,33,255,26,28,33,255,
};

// ─── Data Model ─────────────────────────────────────────────────────────────
enum PartStatus {
    STATUS_NEEDED   = 0,
    STATUS_ORDERED  = 1,
    STATUS_IN_STOCK = 2,
    STATUS_INSTALLED= 3,
    STATUS_COUNT
};
static const char* STATUS_LABELS[STATUS_COUNT] = {
    "Needed", "Ordered", "In Stock", "Installed"
};
static ImVec4 statusColor(PartStatus s){
    switch(s){
        case STATUS_NEEDED:   return COL_TEXT_DIM;
        case STATUS_ORDERED:  return COL_YELLOW;
        case STATUS_IN_STOCK: return COL_GREEN;
        case STATUS_INSTALLED:return COL_BLUE;
        default:              return COL_TEXT;
    }
}

struct Part {
    int         id          = 0;
    int         project_id  = 0;
    std::string name;
    std::string url;
    std::string part_number;
    std::string vendor;
    std::string notes;
    int         quantity    = 1;
    double      unit_price  = 0.0;
    PartStatus  status      = STATUS_NEEDED;
    std::string section;
};

struct Attachment {
    int         id      = 0;
    int         note_id = 0;
    std::string filename;
    std::string mime;
    long long   size    = 0;
};

// A shop note / blueprint / wiring sketch / safety sheet that belongs to a project.
// `kind` is free text (the UI offers the NOTE_KINDS presets). Attached files are stored in
// the database itself, so they travel with everything else when machines sync.
struct Note {
    int         id         = 0;
    int         project_id = 0;
    std::string title;
    std::string kind;
    std::string body;
    std::vector<Attachment> files;
};

struct Project {
    int         id = 0;
    std::string name;
    std::string description;
    std::vector<Part> parts;
    std::vector<Note> notes;
};

static const char* NOTE_KINDS[] = { "Shop Note", "Blueprint", "Wiring", "Assembly", "Safety", "Reference" };
static const int   NOTE_KIND_COUNT = 6;
static ImVec4 noteKindColor(const std::string& k){
    if(k == "Blueprint") return COL_BLUE;
    if(k == "Wiring")    return COL_YELLOW;
    if(k == "Assembly")  return COL_GREEN;
    if(k == "Safety")    return COL_RED;
    if(k == "Reference") return COL_TEXT_DIM;
    return COL_ACCENT;
}
static const long long MAX_ATTACH_BYTES = 15LL * 1024 * 1024;   // per attached file

// ─── DB ─────────────────────────────────────────────────────────────────────
static sqlite3*    g_db = nullptr;
static std::string g_db_path;
static std::string g_status_msg;
static double      g_status_time = 0.0;  // glfwGetTime() when message was set

// Safely convert sqlite3_column_text (which can return NULL) to std::string
static std::string col_str(sqlite3_stmt* s, int col){
    const unsigned char* p = sqlite3_column_text(s, col);
    return p ? reinterpret_cast<const char*>(p) : "";
}

// Execute a single SQL statement, log errors to status bar
static void db_exec_one(const char* sql){
    char* err = nullptr;
    sqlite3_exec(g_db, sql, nullptr, nullptr, &err);
    if(err){
        g_status_msg = std::string("DB error: ") + err;
        g_status_time = glfwGetTime();
        sqlite3_free(err);
    }
}

static void db_init(){
    const char* home = getenv("HOME");
    if(!home) home = "/tmp";
    std::string dir = std::string(home) + "/.local/share/bom-tracker";
    std::filesystem::create_directories(dir);
    g_db_path = dir + "/bom.db";
    if(sqlite3_open(g_db_path.c_str(), &g_db) != SQLITE_OK){
        g_status_msg = std::string("Cannot open DB: ") + sqlite3_errmsg(g_db);
        g_status_time = glfwGetTime();
        return;
    }
    // Set pragmas separately — sqlite3_exec handles multi-statement but PRAGMA
    // must be applied to this connection before any schema work.
    db_exec_one("PRAGMA foreign_keys = ON;");
    db_exec_one("PRAGMA journal_mode = WAL;");
    db_exec_one("PRAGMA synchronous = NORMAL;");
    // Other writers (web app, sync) share this file; wait briefly instead of failing
    sqlite3_busy_timeout(g_db, 3000);
    db_exec_one(R"(
        CREATE TABLE IF NOT EXISTS projects (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT NOT NULL,
            description TEXT NOT NULL DEFAULT ''
        );
    )");
    db_exec_one(R"(
        CREATE TABLE IF NOT EXISTS parts (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            project_id  INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
            name        TEXT    NOT NULL DEFAULT '',
            url         TEXT    NOT NULL DEFAULT '',
            part_number TEXT    NOT NULL DEFAULT '',
            vendor      TEXT    NOT NULL DEFAULT '',
            notes       TEXT    NOT NULL DEFAULT '',
            quantity    INTEGER NOT NULL DEFAULT 1,
            unit_price  REAL    NOT NULL DEFAULT 0.0,
            status      INTEGER NOT NULL DEFAULT 0
        );
    )");
    // Migrate: add section column to existing DBs (no-op if already present)
    sqlite3_exec(g_db,
        "ALTER TABLE parts ADD COLUMN section TEXT NOT NULL DEFAULT '';",
        nullptr, nullptr, nullptr);

    // Shop notes / blueprints and their attached files. The sync layer (bom_web.py)
    // adds uuid / updated_at columns and triggers to these later, same as parts.
    db_exec_one(R"(
        CREATE TABLE IF NOT EXISTS notes (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
            title      TEXT    NOT NULL DEFAULT '',
            kind       TEXT    NOT NULL DEFAULT 'Shop Note',
            body       TEXT    NOT NULL DEFAULT ''
        );
    )");
    db_exec_one(R"(
        CREATE TABLE IF NOT EXISTS attachments (
            id       INTEGER PRIMARY KEY AUTOINCREMENT,
            note_id  INTEGER NOT NULL REFERENCES notes(id) ON DELETE CASCADE,
            filename TEXT    NOT NULL DEFAULT '',
            mime     TEXT    NOT NULL DEFAULT 'application/octet-stream',
            size     INTEGER NOT NULL DEFAULT 0,
            data     BLOB    NOT NULL
        );
    )");
}

static std::vector<Project> g_projects;

static void db_load(){
    g_projects.clear();
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(g_db,
        "SELECT id,name,description FROM projects ORDER BY name COLLATE NOCASE",
        -1, &stmt, nullptr);
    while(sqlite3_step(stmt) == SQLITE_ROW){
        Project p;
        p.id          = sqlite3_column_int(stmt, 0);
        p.name        = col_str(stmt, 1);
        p.description = col_str(stmt, 2);
        g_projects.push_back(std::move(p));
    }
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(g_db,
        "SELECT id,project_id,name,url,part_number,vendor,notes,quantity,unit_price,status,section "
        "FROM parts ORDER BY section COLLATE NOCASE, name COLLATE NOCASE",
        -1, &stmt, nullptr);
    while(sqlite3_step(stmt) == SQLITE_ROW){
        Part pt;
        pt.id          = sqlite3_column_int(stmt, 0);
        pt.project_id  = sqlite3_column_int(stmt, 1);
        pt.name        = col_str(stmt, 2);
        pt.url         = col_str(stmt, 3);
        pt.part_number = col_str(stmt, 4);
        pt.vendor      = col_str(stmt, 5);
        pt.notes       = col_str(stmt, 6);
        pt.quantity    = sqlite3_column_int(stmt, 7);
        pt.unit_price  = sqlite3_column_double(stmt, 8);
        pt.status      = static_cast<PartStatus>(sqlite3_column_int(stmt, 9));
        pt.section     = col_str(stmt, 10);
        for(auto& proj : g_projects)
            if(proj.id == pt.project_id){ proj.parts.push_back(std::move(pt)); break; }
    }
    sqlite3_finalize(stmt);

    // Notes (file contents are NOT loaded here, only their names and sizes)
    sqlite3_prepare_v2(g_db,
        "SELECT id,project_id,title,kind,body FROM notes "
        "ORDER BY kind COLLATE NOCASE, title COLLATE NOCASE, id",
        -1, &stmt, nullptr);
    while(sqlite3_step(stmt) == SQLITE_ROW){
        Note n;
        n.id         = sqlite3_column_int(stmt, 0);
        n.project_id = sqlite3_column_int(stmt, 1);
        n.title      = col_str(stmt, 2);
        n.kind       = col_str(stmt, 3);
        n.body       = col_str(stmt, 4);
        for(auto& proj : g_projects)
            if(proj.id == n.project_id){ proj.notes.push_back(std::move(n)); break; }
    }
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(g_db,
        "SELECT id,note_id,filename,mime,size FROM attachments "
        "ORDER BY filename COLLATE NOCASE, id",
        -1, &stmt, nullptr);
    while(sqlite3_step(stmt) == SQLITE_ROW){
        Attachment a;
        a.id       = sqlite3_column_int(stmt, 0);
        a.note_id  = sqlite3_column_int(stmt, 1);
        a.filename = col_str(stmt, 2);
        a.mime     = col_str(stmt, 3);
        a.size     = sqlite3_column_int64(stmt, 4);
        bool placed = false;
        for(auto& proj : g_projects){
            for(auto& n : proj.notes)
                if(n.id == a.note_id){ n.files.push_back(a); placed = true; break; }
            if(placed) break;
        }
    }
    sqlite3_finalize(stmt);
}

static int db_insert_project(const std::string& name, const std::string& desc){
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(g_db,
        "INSERT INTO projects(name,description) VALUES(?,?)", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, desc.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(g_db));
}

static void db_update_project(int id, const std::string& name, const std::string& desc){
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(g_db,
        "UPDATE projects SET name=?,description=? WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, desc.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void db_delete_project(int id){
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(g_db, "DELETE FROM projects WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static int db_insert_part(const Part& pt){
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(g_db,
        "INSERT INTO parts(project_id,name,url,part_number,vendor,notes,quantity,unit_price,status,section)"
        " VALUES(?,?,?,?,?,?,?,?,?,?)", -1, &stmt, nullptr);
    sqlite3_bind_int   (stmt, 1, pt.project_id);
    sqlite3_bind_text  (stmt, 2, pt.name.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 3, pt.url.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 4, pt.part_number.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 5, pt.vendor.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 6, pt.notes.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_int   (stmt, 7, pt.quantity);
    sqlite3_bind_double(stmt, 8, pt.unit_price);
    sqlite3_bind_int   (stmt, 9, static_cast<int>(pt.status));
    sqlite3_bind_text  (stmt,10, pt.section.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(g_db));
}

static void db_update_part(const Part& pt){
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(g_db,
        "UPDATE parts SET name=?,url=?,part_number=?,vendor=?,notes=?,quantity=?,unit_price=?,status=?,section=?"
        " WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_text  (stmt, 1, pt.name.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 2, pt.url.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 3, pt.part_number.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 4, pt.vendor.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 5, pt.notes.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_int   (stmt, 6, pt.quantity);
    sqlite3_bind_double(stmt, 7, pt.unit_price);
    sqlite3_bind_int   (stmt, 8, static_cast<int>(pt.status));
    sqlite3_bind_text  (stmt, 9, pt.section.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_bind_int   (stmt,10, pt.id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void db_delete_part(int id){
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(g_db, "DELETE FROM parts WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ─── Notes & attachments ─────────────────────────────────────────────────────
static int db_insert_note(int project_id, const std::string& title,
                          const std::string& kind, const std::string& body){
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(g_db,
        "INSERT INTO notes(project_id,title,kind,body) VALUES(?,?,?,?)", -1, &stmt, nullptr);
    sqlite3_bind_int (stmt, 1, project_id);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, kind.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, body.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(g_db));
}

static void db_update_note(int id, const std::string& title,
                           const std::string& kind, const std::string& body){
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(g_db,
        "UPDATE notes SET title=?,kind=?,body=? WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, kind.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, body.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 4, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void db_delete_note(int id){   // attached files go with it (ON DELETE CASCADE)
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(g_db, "DELETE FROM notes WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void db_delete_attachment(int id){
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(g_db, "DELETE FROM attachments WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static std::string human_size(long long n){
    char b[32];
    if(n < 1024)             snprintf(b, sizeof(b), "%lld B", n);
    else if(n < 1024 * 1024) snprintf(b, sizeof(b), "%.0f KB", n / 1024.0);
    else                     snprintf(b, sizeof(b), "%.1f MB", n / (1024.0 * 1024.0));
    return b;
}

static std::string guess_mime(const std::string& filename){
    std::string ext = std::filesystem::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
    if(ext == ".png")                   return "image/png";
    if(ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if(ext == ".gif")                   return "image/gif";
    if(ext == ".webp")                  return "image/webp";
    if(ext == ".svg")                   return "image/svg+xml";
    if(ext == ".pdf")                   return "application/pdf";
    if(ext == ".txt" || ext == ".md")   return "text/plain";
    if(ext == ".csv")                   return "text/csv";
    return "application/octet-stream";
}

// Read a file from disk into the attachments table. Returns false and sets err on failure.
static bool db_add_attachment(int note_id, std::string path, std::string& err){
    namespace fs = std::filesystem;
    // tolerate pasted paths: surrounding spaces / quotes, and a leading ~/
    while(!path.empty() && (path.back() == ' ' || path.back() == '\n' || path.back() == '\r')) path.pop_back();
    while(!path.empty() && path.front() == ' ') path.erase(path.begin());
    if(path.size() >= 2 && (path.front() == '"' || path.front() == '\'') && path.back() == path.front())
        path = path.substr(1, path.size() - 2);
    if(path.size() >= 2 && path[0] == '~' && path[1] == '/'){
        const char* home = getenv("HOME");
        if(home) path = std::string(home) + path.substr(1);
    }
    std::error_code ec;
    if(path.empty() || !fs::is_regular_file(path, ec)){ err = "Not a file: " + path; return false; }
    uintmax_t sz = fs::file_size(path, ec);
    if(ec){ err = "Cannot read file size"; return false; }
    if(sz == 0){ err = "File is empty"; return false; }
    if(sz > static_cast<uintmax_t>(MAX_ATTACH_BYTES)){
        err = "File is " + human_size(static_cast<long long>(sz)) + " (limit " + human_size(MAX_ATTACH_BYTES) + ")";
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if(!in){ err = "Cannot open file"; return false; }
    std::string data(static_cast<size_t>(sz), '\0');
    in.read(&data[0], static_cast<std::streamsize>(sz));
    if(static_cast<uintmax_t>(in.gcount()) != sz){ err = "Short read"; return false; }

    std::string name = fs::path(path).filename().string();
    std::string mime = guess_mime(name);
    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(g_db,
        "INSERT INTO attachments(note_id,filename,mime,size,data) VALUES(?,?,?,?,?)",
        -1, &stmt, nullptr) != SQLITE_OK){ err = sqlite3_errmsg(g_db); return false; }
    sqlite3_bind_int  (stmt, 1, note_id);
    sqlite3_bind_text (stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 3, mime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(sz));
    sqlite3_bind_blob (stmt, 5, data.data(), static_cast<int>(data.size()), SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if(!ok) err = sqlite3_errmsg(g_db);
    sqlite3_finalize(stmt);
    return ok;
}

// Write an attachment to a private temp folder and return its path ("" + err on failure),
// so it can be handed to the system's default viewer.
static std::string db_extract_attachment(int id, std::string& err){
    namespace fs = std::filesystem;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(g_db, "SELECT filename,data FROM attachments WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    std::string name, data, out;
    if(sqlite3_step(stmt) == SQLITE_ROW){
        name = col_str(stmt, 0);
        const void* blob = sqlite3_column_blob(stmt, 1);
        int len = sqlite3_column_bytes(stmt, 1);
        if(blob && len > 0) data.assign(static_cast<const char*>(blob), static_cast<size_t>(len));
    }
    sqlite3_finalize(stmt);
    if(data.empty()){ err = "Attachment not found"; return ""; }

    for(auto& c : name)   // keep it a plain file name: no separators or odd characters
        if(!isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-' && c != '_' && c != ' ') c = '_';
    while(!name.empty() && name.front() == '.') name.erase(name.begin());
    if(name.empty()) name = "attachment";

    std::error_code ec;
    fs::path dir = fs::temp_directory_path(ec) / ("bom-tracker-" + std::to_string(getuid()));
    fs::create_directories(dir, ec);
    fs::permissions(dir, fs::perms::owner_all, fs::perm_options::replace, ec);
    fs::path p = dir / (std::to_string(id) + "_" + name);
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    if(!o){ err = "Cannot write " + p.string(); return ""; }
    o.write(data.data(), static_cast<std::streamsize>(data.size()));
    o.close();
    return p.string();
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
static void open_url(const std::string& url){
    if(url.empty()) return;
    // Use fork+execvp to avoid shell injection via system()
    pid_t pid = fork();
    if(pid == 0){
        // Child: exec the platform URL opener, detach from parent
        setsid();
#ifdef __APPLE__
        execl("/usr/bin/open", "open", url.c_str(), nullptr);
#else
        execl("/usr/bin/xdg-open", "xdg-open", url.c_str(), nullptr);
#endif
        _exit(1);  // exec failed
    }
    // Parent: don't wait — fire and forget
}

static std::string format_price(double p, bool zero_as_dash = true){
    if(p <= 0.0 && zero_as_dash) return "\xe2\x80\x94"; // em-dash
    std::ostringstream ss;
    ss << "$" << std::fixed << std::setprecision(2) << p;
    return ss.str();
}

static double project_total(const Project& p){
    double t = 0;
    for(auto& pt : p.parts) t += pt.unit_price * pt.quantity;
    return t;
}



// ─── Markdown BOM Import ─────────────────────────────────────────────────────

static std::string trim_str(const std::string& s){
    size_t a = s.find_first_not_of(" \t\r\n");
    if(a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string to_lower_str(std::string s){
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// Strip inline markdown markers: **bold**, *italic*, `code`
static std::string strip_md_inline(const std::string& s){
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while(i < s.size()){
        char c = s[i];
        if(c=='*' || c=='_' || c=='`'){
            while(i < s.size() && (s[i]=='*'||s[i]=='_'||s[i]=='`')) i++;
        } else { out += c; i++; }
    }
    return trim_str(out);
}

// Strip common BOM document title prefixes before using as project name
static std::string strip_bom_prefix(const std::string& s){
    const char* prefixes[] = {
        "bill of materials (bom):",
        "bill of materials:",
        "bom:",
        "bom -",
        "bill of materials -",
        nullptr
    };
    std::string sl = to_lower_str(s);
    for(int i = 0; prefixes[i]; i++){
        std::string p(prefixes[i]);
        if(sl.rfind(p, 0) == 0)
            return trim_str(s.substr(p.size()));
    }
    return s;
}

// Split "| a | b | c |" → {"a","b","c"}
static std::vector<std::string> split_md_row(const std::string& line){
    std::vector<std::string> cells;
    std::stringstream ss(line);
    std::string cell;
    bool first = true;
    while(std::getline(ss, cell, '|')){
        if(first){ first = false; continue; }
        cells.push_back(trim_str(cell));
    }
    if(!cells.empty() && cells.back().empty()) cells.pop_back();
    return cells;
}

// True if line is a table separator row (|---|---|)
static bool is_md_separator(const std::string& line){
    for(char c : line)
        if(c!='|' && c!='-' && c!=' ' && c!=':' && c!='\t') return false;
    return line.find('-') != std::string::npos;
}

// True if line is a horizontal rule (---, ***, ___)
static bool is_md_hr(const std::string& line){
    std::string t = trim_str(line);
    if(t.size() < 3) return false;
    char c = t[0];
    if(c!='-' && c!='*' && c!='_') return false;
    for(char ch : t) if(ch!=c && ch!=' ') return false;
    return true;
}

static PartStatus parse_status_str(const std::string& s){
    std::string sl = to_lower_str(trim_str(s));
    if(sl=="ordered")                                          return STATUS_ORDERED;
    if(sl=="in stock"||sl=="instock"||sl=="have"||sl=="stock") return STATUS_IN_STOCK;
    if(sl=="installed"||sl=="done"||sl=="complete")            return STATUS_INSTALLED;
    return STATUS_NEEDED;
}

// Parse "1", "2", "1 spool", "As needed", "2–4"
// Returns integer qty; sets qty_note if value was non-standard
static int parse_qty_str(const std::string& raw, std::string& qty_note){
    std::string s = trim_str(raw);
    if(s.empty()) return 1;
    try {
        size_t pos;
        int q = std::stoi(s, &pos);
        std::string rem = trim_str(s.substr(pos));
        if(!rem.empty()) qty_note = s;   // e.g. "1 spool"
        return std::max(1, q);
    } catch(...) {}
    // Non-numeric or range ("As needed", "2–4")
    qty_note = s;
    for(size_t j = 0; j < s.size(); j++){
        if(isdigit((unsigned char)s[j])){
            try { return std::max(1, std::stoi(s.substr(j))); } catch(...) {}
        }
    }
    return 1;
}

struct ImportResult {
    std::string       project_name;
    std::string       project_desc;
    std::vector<Part> parts;
    std::string       error;
    bool ok() const { return error.empty(); }
};

static ImportResult parse_bom_markdown(const std::string& path){
    ImportResult r;
    std::ifstream mdf(path);
    if(!mdf){ r.error = "Cannot open: " + path; return r; }

    std::vector<std::string> lines;
    std::string line;
    while(std::getline(mdf, line)) lines.push_back(line);
    mdf.close();

    // --- Find project name: first # heading ---
    size_t idx = 0;
    for(; idx < lines.size(); idx++){
        std::string t = trim_str(lines[idx]);
        if(t.rfind("# ", 0)==0 && (t.size()<3 || t[1]!='#')){
            r.project_name = strip_bom_prefix(strip_md_inline(trim_str(t.substr(2))));
            idx++; break;
        }
    }
    if(r.project_name.empty()){ r.error = "No '# Heading' found for project name"; return r; }

    // --- Collect description: text before first table or ## section ---
    std::string desc_acc;
    for(size_t j = idx; j < lines.size(); j++){
        std::string t = trim_str(lines[j]);
        if(t.empty() || is_md_hr(t)) continue;
        if(t[0]=='|') break;
        if(t.rfind("##",0)==0) break;
        if(t[0]=='>') t = trim_str(t.substr(1));
        if(!t.empty()){
            if(!desc_acc.empty()) desc_acc += " ";
            desc_acc += strip_md_inline(t);
        }
    }
    r.project_desc = desc_acc;

    // --- Scan ALL tables in the file ---
    enum ColField { CF_NAME, CF_PN, CF_VENDOR, CF_QTY, CF_PRICE, CF_STATUS, CF_NOTES, CF_DESC, CF_URL, CF_COUNT };

    size_t pos = 0;
    std::string current_section;
    while(pos < lines.size()){
        // Track ## section headings between tables
        while(pos < lines.size()){
            std::string t = trim_str(lines[pos]);
            if(!t.empty() && t[0]=='|') break;
            if(t.rfind("## ",0)==0)
                current_section = strip_md_inline(trim_str(t.substr(3)));
            else if(t.rfind("##",0)==0 && t.size()>2)
                current_section = strip_md_inline(trim_str(t.substr(2)));
            pos++;
        }
        if(pos >= lines.size()) break;

        // Parse header
        auto headers = split_md_row(lines[pos]); pos++;
        if(pos < lines.size() && is_md_separator(lines[pos])) pos++;
        if(headers.empty()) continue;

        // Map header names → column indices
        int col_map[CF_COUNT];
        for(int k = 0; k < CF_COUNT; k++) col_map[k] = -1;
        for(int hi = 0; hi < (int)headers.size(); hi++){
            std::string h = to_lower_str(strip_md_inline(headers[hi]));
            if(h=="part name"||h=="name"||h=="part"||h=="item"||h=="component"
               ||h=="material"||h=="materials")
                { if(col_map[CF_NAME]<0) col_map[CF_NAME]=hi; }
            else if(h=="part #"||h=="part number"||h=="part no"||h=="pn"||h=="sku"||h=="mpn")
                col_map[CF_PN]=hi;
            else if(h=="vendor"||h=="supplier"||h=="source"||h=="store"||h=="retailer")
                col_map[CF_VENDOR]=hi;
            else if(h=="qty"||h=="quantity"||h=="count"||h=="amount")
                col_map[CF_QTY]=hi;
            else if(h=="unit price"||h=="price"||h=="unit cost"||h=="cost"||h=="each"||h=="unit")
                col_map[CF_PRICE]=hi;
            else if(h=="status")
                col_map[CF_STATUS]=hi;
            else if(h=="notes"||h=="note"||h=="comments"||h=="comment")
                col_map[CF_NOTES]=hi;
            else if(h=="description"||h=="description / specification"||h=="spec"||h=="specification"||h=="details")
                col_map[CF_DESC]=hi;
            else if(h=="url"||h=="link"||h=="web"||h=="website"||h=="purchase link"||h=="buy")
                col_map[CF_URL]=hi;
        }
        auto col_taken = [&](int hi) -> bool {
            for(int k = 0; k < CF_COUNT; k++) if(col_map[k]==hi) return true;
            return false;
        };

        // No recognizable name column: if the table still looks like a parts table
        // (it has part #, vendor, qty, price or URL columns), use its first free column.
        if(col_map[CF_NAME]<0){
            bool bom_like = col_map[CF_PN]>=0 || col_map[CF_VENDOR]>=0 || col_map[CF_QTY]>=0
                         || col_map[CF_PRICE]>=0 || col_map[CF_URL]>=0;
            if(bom_like)
                for(int hi = 0; hi < (int)headers.size(); hi++)
                    if(!col_taken(hi)){ col_map[CF_NAME] = hi; break; }
        }
        // Otherwise it's not a parts table (pinouts, specs, ...): skip it
        if(col_map[CF_NAME]<0){
            while(pos < lines.size() && !trim_str(lines[pos]).empty() && trim_str(lines[pos])[0]=='|') pos++;
            continue;
        }

        // Columns we don't recognise (Recommendation, Function, Requirement, ...)
        // are kept as notes instead of being silently dropped.
        std::vector<int> extra_cols;
        for(int hi = 0; hi < (int)headers.size(); hi++){
            if(col_taken(hi)) continue;
            std::string h = to_lower_str(strip_md_inline(headers[hi]));
            if(h.empty()||h=="#"||h=="no"||h=="no."||h=="id"||h=="line"||h=="ref") continue;  // row counters
            extra_cols.push_back(hi);
        }

        // Parse data rows
        while(pos < lines.size()){
            std::string t = trim_str(lines[pos]);
            if(t.empty() || t[0]!='|') break;
            auto cells = split_md_row(lines[pos]); pos++;

            auto cell = [&](int field) -> std::string {
                int i2 = col_map[field];
                if(i2<0 || i2>=(int)cells.size()) return "";
                return strip_md_inline(cells[i2]);
            };

            std::string name = cell(CF_NAME);
            if(name.empty() || name=="\xe2\x80\x94" || name=="-") continue;

            Part pt;
            pt.name        = name;
            pt.part_number = cell(CF_PN);
            pt.vendor      = cell(CF_VENDOR);
            pt.url         = cell(CF_URL);
            pt.status      = (col_map[CF_STATUS]>=0) ? parse_status_str(cell(CF_STATUS)) : STATUS_NEEDED;

            // Notes = unrecognised columns, then description column, then notes column
            {
                std::vector<std::string> pieces;
                auto add_piece = [&](std::string v){
                    v = trim_str(v);
                    std::string lv = to_lower_str(v);
                    if(v.empty()||v=="\xe2\x80\x94"||v=="\xe2\x80\x93"||v=="-"||lv=="n/a") return;
                    pieces.push_back(v);
                };
                for(int ec : extra_cols)
                    if(ec < (int)cells.size()) add_piece(strip_md_inline(cells[ec]));
                add_piece(cell(CF_DESC));
                add_piece(cell(CF_NOTES));
                for(size_t pi2 = 0; pi2 < pieces.size(); pi2++){
                    if(pi2) pt.notes += " \xe2\x80\x94 ";
                    pt.notes += pieces[pi2];
                }
            }

            // Quantity (handle "As needed", "1 spool", "2–4")
            std::string qty_note;
            pt.quantity = (col_map[CF_QTY]>=0) ? parse_qty_str(cell(CF_QTY), qty_note) : 1;
            if(!qty_note.empty()){
                if(!pt.notes.empty()) pt.notes += " [qty: " + qty_note + "]";
                else                  pt.notes  = "qty: " + qty_note;
            }

            // Price (strip $, commas)
            std::string ps = cell(CF_PRICE);
            pt.unit_price = 0.0;
            if(!ps.empty()){
                std::string pc;
                for(char c : ps) if(c!='$' && c!=',' && c!=' ') pc+=c;
                try { pt.unit_price = std::stod(pc); } catch(...) {}
            }

            pt.section = current_section;
            r.parts.push_back(std::move(pt));
        }
    }

    if(r.parts.empty()){ r.error = "No parts found in any table"; return r; }
    return r;
}

// ─── One-shot import ─────────────────────────────────────────────────────────
// Parse a Markdown BOM and create the project + parts in one step. If a project
// with the same name already exists it is *synced* instead of duplicated: new
// parts are added and empty fields (part #, vendor, URL, notes) are filled in,
// but quantity / price / status and anything you've typed are never overwritten.
struct ImportOutcome {
    bool        ok = false;
    std::string error;
    std::string project_name;
    int         project_id = 0;
    bool        merged = false;
    int         added = 0, filled = 0, unchanged = 0;
};

static std::string norm_key(const std::string& s){ return to_lower_str(trim_str(s)); }

static ImportOutcome import_markdown_file(const std::string& path){
    ImportOutcome o;
    ImportResult r = parse_bom_markdown(path);
    if(!r.ok()){ o.error = r.error; return o; }
    o.project_name = r.project_name;

    db_load();  // make sure we're comparing against what's really in the DB
    const Project* existing = nullptr;
    for(auto& p : g_projects)
        if(norm_key(p.name) == norm_key(r.project_name)){ existing = &p; break; }
    // Renamed the project since the last import? Fall back to the project that
    // already holds most of these parts (>= 60% of them, and at least 2).
    if(!existing){
        std::set<std::string> incoming;
        for(auto& pt : r.parts) incoming.insert(norm_key(pt.name));
        size_t best = 0;
        for(auto& p : g_projects){
            std::set<std::string> have;
            for(auto& ep : p.parts) have.insert(norm_key(ep.name));
            size_t hits = 0;
            for(auto& k : incoming) if(have.count(k)) hits++;
            if(hits > best){ best = hits; existing = &p; }
        }
        if(best < 2 || best * 10 < incoming.size() * 6) existing = nullptr;
    }
    if(existing) o.project_name = existing->name;

    db_exec_one("BEGIN");
    if(!existing){
        o.project_id = db_insert_project(r.project_name, r.project_desc);
        for(auto& pt : r.parts){
            Part p = pt; p.project_id = o.project_id;
            db_insert_part(p); o.added++;
        }
    } else {
        o.merged     = true;
        o.project_id = existing->id;
        if(existing->description.empty() && !r.project_desc.empty())
            db_update_project(existing->id, existing->name, r.project_desc);

        std::unordered_multimap<std::string, const Part*> by_key;
        for(auto& ep : existing->parts) by_key.emplace(norm_key(ep.name) + "\x1f" + norm_key(ep.section), &ep);
        std::unordered_multimap<std::string, const Part*> by_name;
        for(auto& ep : existing->parts) by_name.emplace(norm_key(ep.name), &ep);
        std::set<int> used;

        for(auto& pt : r.parts){
            const Part* match = nullptr;
            auto range = by_key.equal_range(norm_key(pt.name) + "\x1f" + norm_key(pt.section));
            for(auto it = range.first; it != range.second; ++it)
                if(!used.count(it->second->id)){ match = it->second; used.insert(match->id); break; }
            if(!match){   // same part moved to a different section
                auto r2 = by_name.equal_range(norm_key(pt.name));
                for(auto it = r2.first; it != r2.second; ++it)
                    if(!used.count(it->second->id)){ match = it->second; used.insert(match->id); break; }
            }

            if(!match){
                Part p = pt; p.project_id = existing->id;
                db_insert_part(p); o.added++;
                continue;
            }
            Part up = *match; bool changed = false;
            if(up.part_number.empty() && !pt.part_number.empty()){ up.part_number = pt.part_number; changed = true; }
            if(up.vendor.empty()      && !pt.vendor.empty())     { up.vendor      = pt.vendor;      changed = true; }
            if(up.url.empty()         && !pt.url.empty())        { up.url         = pt.url;         changed = true; }
            if(up.notes.empty()       && !pt.notes.empty())      { up.notes       = pt.notes;       changed = true; }
            if(changed){ db_update_part(up); o.filled++; } else o.unchanged++;
        }
    }
    db_exec_one("COMMIT");
    db_load();
    o.ok = true;
    return o;
}

static std::string describe_outcome(const ImportOutcome& o){
    if(!o.merged)
        return "Imported \"" + o.project_name + "\" (" + std::to_string(o.added) + " parts)";
    return "Synced \"" + o.project_name + "\": " + std::to_string(o.added) + " added, " +
           std::to_string(o.filled) + " filled in, " + std::to_string(o.unchanged) + " unchanged";
}

// Files dropped onto the window are queued here and handled in the main loop
static std::vector<std::string> g_dropped_files;
static void drop_callback(GLFWwindow*, int count, const char** paths){
    for(int i = 0; i < count; i++) g_dropped_files.push_back(paths[i]);
}

static bool looks_like_markdown(const std::string& path){
    std::string l = to_lower_str(path);
    auto ends = [&](const char* ext){ size_t n = strlen(ext); return l.size()>=n && l.compare(l.size()-n, n, ext)==0; };
    return ends(".md") || ends(".markdown") || ends(".txt");
}

// Open a native file picker via zenity; returns "" if cancelled / not available
static std::string pick_file_zenity(){
#ifdef __APPLE__
    // macOS: native picker via osascript
    {
        FILE* fp = popen("osascript -e 'POSIX path of (choose file with prompt \"Import BOM from Markdown\")' 2>/dev/null", "r");
        if(!fp) return "";
        char buf[1024]={};
        fgets(buf, sizeof(buf), fp);
        pclose(fp);
        std::string s(buf);
        if(!s.empty() && s.back()=='\n') s.pop_back();
        return s;
    }
#endif
    // Try kdialog (KDE/Plasma) first, then zenity (GNOME/GTK)
    struct { const char* bin; const char* cmd; } tools[] = {
        { "kdialog", "kdialog --getopenfilename \"$HOME\" \'Markdown Files (*.md *.markdown)\'" },
        { "zenity",  "zenity --file-selection --title=\'Import BOM from Markdown\' --file-filter=\'Markdown | *.md *.markdown\'" },
    };
    for(auto& t : tools){
        std::string chk = std::string("which ") + t.bin + " >/dev/null 2>&1";
        if(system(chk.c_str()) != 0) continue;
        FILE* fp = popen((std::string(t.cmd) + " 2>/dev/null").c_str(), "r");
        if(!fp) continue;
        char buf[1024]={};
        fgets(buf, sizeof(buf), fp);
        pclose(fp);
        std::string s(buf);
        if(!s.empty() && s.back()=='\n') s.pop_back();
        if(!s.empty()) return s;
    }
    return "";
}

// Import modal UI state
static bool         g_show_import_md   = false;
static char         g_import_path[1024]= {};
static ImportResult g_import_result;
static bool         g_import_previewed = false;

// ─── Print / Export ──────────────────────────────────────────────────────
static void export_bom_html(const Project& proj){
    std::string safe = proj.name;
    for(auto& c : safe) if(!isalnum((unsigned char)c) && c!='_' && c!='-') c='_';
    std::string path = "/tmp/bom_" + safe + ".html";
    FILE* f = fopen(path.c_str(), "w");
    if(!f){ g_status_msg="Print: cannot write "+path; g_status_time=glfwGetTime(); return; }

    double grand = 0.0;
    for(auto& pt : proj.parts) grand += pt.unit_price * pt.quantity;

    auto esc = [](const std::string& s) -> std::string {
        std::string o; o.reserve(s.size());
        for(char c : s){
            if(c=='&') o+="&amp;"; else if(c=='<') o+="&lt;";
            else if(c=='>') o+="&gt;"; else if(c=='"'||c=='\''||c=='`') o+="&quot;";
            else o+=c;
        }
        return o;
    };

    fprintf(f,
        "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
        "<meta charset=\"UTF-8\">\n<title>BOM - %s</title>\n"
        "<style>\n"
        "body{font-family:Arial,sans-serif;font-size:12px;margin:24px;color:#222}\n"
        "h1{font-size:18px;margin-bottom:4px}\n"
        ".desc{color:#666;font-size:12px;margin-bottom:16px}\n"
        "table{border-collapse:collapse;width:100%%}\n"
        "th{background:#222;color:#fff;text-align:left;padding:6px 8px}\n"
        "td{border-bottom:1px solid #ddd;padding:5px 8px;vertical-align:top}\n"
        "tr:nth-child(even) td{background:#f7f7f7}\n"
        ".num{text-align:right}\n"
        ".sn{color:#888}.so{color:#b8960a}.si{color:#2a7a35}.sa{color:#2255aa}\n"
        ".tr td{font-weight:bold;border-top:2px solid #222;background:#f0f0f0}\n"
        ".note{margin-top:14px;page-break-inside:avoid}\n"
        "h2{font-size:15px;margin:22px 0 4px;border-bottom:2px solid #222}\n"
        ".note h3{font-size:13px;margin:0 0 4px}.kind{color:#888;font-weight:normal;font-size:11px}\n"
        ".note pre{white-space:pre-wrap;font-family:ui-monospace,Menlo,Consolas,monospace;font-size:11px;"
        "background:#f7f7f7;border:1px solid #ddd;padding:8px;margin:0}\n"
        ".files{color:#666;font-size:11px;margin:4px 0 0}\n"
        "@media print{body{margin:12px}}\n"
        "</style>\n</head>\n<body>\n"
        "<h1>Bill of Materials \xe2\x80\x94 %s</h1>\n",
        proj.name.c_str(), proj.name.c_str());

    if(!proj.description.empty())
        fprintf(f, "<p class=\"desc\">%s</p>\n", esc(proj.description).c_str());

    fprintf(f, "<table>\n<thead><tr>"
        "<th>#</th><th>Part Name</th><th>Part Number</th><th>Vendor</th>"
        "<th class=\"num\">Qty</th><th class=\"num\">Unit Price</th>"
        "<th class=\"num\">Total</th><th>Status</th><th>Notes</th>"
        "</tr></thead>\n<tbody>\n");

    int row=1;
    for(auto& pt : proj.parts){
        const char* sc="sn";
        if(pt.status==STATUS_ORDERED)   sc="so";
        if(pt.status==STATUS_IN_STOCK)  sc="si";
        if(pt.status==STATUS_INSTALLED) sc="sa";
        double lt = pt.unit_price * pt.quantity;
        std::string nc = pt.url.empty()
            ? esc(pt.name)
            : "<a href=\"" + esc(pt.url) + "\" target=\"_blank\">" + esc(pt.name) + "</a>";
        fprintf(f,
            "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td>"
            "<td class=\"num\">%d</td>"
            "<td class=\"num\">%s</td>"
            "<td class=\"num\">%s</td>"
            "<td class=\"%s\">%s</td><td>%s</td></tr>\n",
            row++, nc.c_str(),
            esc(pt.part_number).c_str(), esc(pt.vendor).c_str(),
            pt.quantity,
            pt.unit_price>0 ? format_price(pt.unit_price,false).c_str() : "\xe2\x80\x94",
            lt>0            ? format_price(lt,false).c_str()             : "\xe2\x80\x94",
            sc, STATUS_LABELS[pt.status], esc(pt.notes).c_str());
    }

    fprintf(f,
        "</tbody>\n<tfoot>\n"
        "<tr class=\"tr\">"
        "<td colspan=\"6\" style=\"text-align:right\">Total (excl. tax &amp; shipping):</td>"
        "<td class=\"num\">%s</td><td colspan=\"2\"></td></tr>\n"
        "</tfoot>\n</table>\n",
        format_price(grand, false).c_str());

    // Shop notes / blueprints go after the parts so the shop copy carries safety rules and wiring too
    if(!proj.notes.empty()){
        auto esc_t = [](const std::string& s) -> std::string {   // text only: leave quotes alone
            std::string o; o.reserve(s.size());
            for(char c : s){
                if(c=='&') o+="&amp;"; else if(c=='<') o+="&lt;"; else if(c=='>') o+="&gt;"; else o+=c;
            }
            return o;
        };
        fprintf(f, "<h2>Notes</h2>\n");
        for(auto& n : proj.notes){
            fprintf(f, "<div class=\"note\"><h3>%s <span class=\"kind\">%s</span></h3>\n",
                    esc_t(n.title).c_str(), esc_t(n.kind).c_str());
            if(!n.body.empty()) fprintf(f, "<pre>%s</pre>\n", esc_t(n.body).c_str());
            if(!n.files.empty()){
                fprintf(f, "<p class=\"files\">Attached files: ");
                for(size_t i = 0; i < n.files.size(); i++)
                    fprintf(f, "%s%s", i ? ", " : "", esc_t(n.files[i].filename).c_str());
                fprintf(f, "</p>\n");
            }
            fprintf(f, "</div>\n");
        }
    }

    fprintf(f,
        "<p style=\"color:#888;font-size:10px;margin-top:12px\">"
        "Generated by BOM Tracker &mdash; %zu part(s)</p>\n"
        "<script>window.onload=function(){window.print()}</script>\n"
        "</body>\n</html>\n",
        proj.parts.size());

    fclose(f);
    open_url("file://" + path);
    g_status_msg = "Opened print view: " + path;
    g_status_time = glfwGetTime();
}

// ─── UI State ────────────────────────────────────────────────────────────────
static int  g_sel_project    = -1;
static int  g_sel_project_id  = -1;  // survives db_load() reorder
static int  g_sel_part     = -1;
static int  g_sel_part_id  = -1;  // survives db_load() reorder

static void resolve_sel_part(){
    // Re-resolve selected project by ID first
    if(g_sel_project_id >= 0){
        g_sel_project = -1;
        for(int i = 0; i < static_cast<int>(g_projects.size()); i++)
            if(g_projects[i].id == g_sel_project_id){ g_sel_project = i; break; }
    }
    // Then re-resolve selected part
    g_sel_part = -1;
    if(g_sel_part_id < 0 || g_sel_project < 0 ||
       g_sel_project >= static_cast<int>(g_projects.size())) return;
    auto& parts = g_projects[g_sel_project].parts;
    for(int i = 0; i < static_cast<int>(parts.size()); i++)
        if(parts[i].id == g_sel_part_id){ g_sel_part = i; return; }
}

// ─── Live refresh ───────────────────────────────────────────────────────────
// PRAGMA data_version changes only when ANOTHER connection commits (web app, sync
// agent, a second desktop instance) and never for this app's own writes, so it is a
// cheap "someone else changed the data" signal. The reload is deferred while any
// dialog is open so an in-progress edit never has its row moved from under it.
static int    g_last_data_version = -1;
static bool   g_external_pending  = false;
static double g_next_poll_time    = 0.0;

static int db_data_version(){
    sqlite3_stmt* st = nullptr;
    int v = -1;
    if(sqlite3_prepare_v2(g_db, "PRAGMA data_version;", -1, &st, nullptr) == SQLITE_OK
       && sqlite3_step(st) == SQLITE_ROW)
        v = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return v;
}

static void poll_external_changes(double now){
    if(now < g_next_poll_time) return;
    g_next_poll_time = now + 1.0;
    int v = db_data_version();
    if(g_last_data_version < 0) g_last_data_version = v;
    else if(v >= 0 && v != g_last_data_version){
        g_last_data_version = v;
        g_external_pending  = true;
    }
    if(g_external_pending &&
       !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)){
        db_load();
        resolve_sel_part();
        g_external_pending = false;
        g_status_msg  = "Updated from another device";
        g_status_time = now;
    }
}

static char  g_proj_name  [256]  = {};
static char  g_proj_desc  [512]  = {};
static char  g_part_name  [256]  = {};
static char  g_part_url   [1024] = {};
static char  g_part_pn    [256]  = {};
static char  g_part_vendor[256]  = {};
static char  g_part_notes [1024] = {};
static int   g_part_qty          = 1;
static float g_part_price        = 0.0f;
static int   g_part_status       = 0;

// Modal open flags — set true to trigger, cleared on OpenPopup call
static bool g_show_add_project  = false;
static bool g_show_edit_project = false;
static bool g_show_del_project  = false;
static bool g_show_add_part     = false;
static bool g_show_edit_part    = false;
static bool g_show_del_part     = false;
static bool g_show_about        = false;

static char  g_search[256]  = {};
static float g_sidebar_w    = 220.0f;   // draggable project-panel width

// ─── Style helpers ───────────────────────────────────────────────────────────
static void push_accent_style(){
    ImGui::PushStyleColor(ImGuiCol_Button,        COL_ACCENT_DIM);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {1.0f,0.75f,0.2f,1.0f});
}
static void pop_accent_style(){ ImGui::PopStyleColor(3); }

static void push_danger_style(){
    ImGui::PushStyleColor(ImGuiCol_Button,        {0.50f,0.10f,0.10f,1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_RED);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {1.0f,0.35f,0.35f,1.0f});
}
static void pop_danger_style(){ ImGui::PopStyleColor(3); }

// Center a modal; size.y=0 → auto-fit
static bool begin_modal(const char* id, ImVec2 size={420,0}){
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f,0.5f});
    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    return ImGui::BeginPopupModal(id, nullptr, 0);
}

// ─── Project list (left panel) ───────────────────────────────────────────────
static void draw_project_list(){
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
        ImVec4(COL_PANEL.x, COL_PANEL.y, COL_PANEL.z, 1.0f));
    ImGui::BeginChild("##proj_panel", {g_sidebar_w, 0}, true);

    ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
    ImGui::TextUnformatted("PROJECTS");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    push_accent_style();
    if(ImGui::Button("+ New Project", {-1, 0})){
        memset(g_proj_name, 0, sizeof(g_proj_name));
        memset(g_proj_desc, 0, sizeof(g_proj_desc));
        g_show_add_project = true;
    }
    pop_accent_style();
    ImGui::Spacing();

    for(int i = 0; i < static_cast<int>(g_projects.size()); i++){
        auto& proj    = g_projects[i];
        bool selected = (g_sel_project == i);
        if(selected){
            ImGui::PushStyleColor(ImGuiCol_Header,        COL_SEL_BG);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, COL_SEL_BG);
        }
        std::string label = proj.name + " (" +
            std::to_string(proj.parts.size()) + ")##proj" + std::to_string(i);
        if(ImGui::Selectable(label.c_str(), selected)){
            g_sel_project    = i;
            g_sel_project_id = proj.id;
            g_sel_part       = -1;
            g_sel_part_id    = -1;
        }
        if(selected) ImGui::PopStyleColor(2);

        // Context menu (right-click)
        if(ImGui::BeginPopupContextItem(("##ctx_proj" + std::to_string(i)).c_str())){
            g_sel_project    = i;
            g_sel_project_id = proj.id;
            if(ImGui::MenuItem("Edit Project")){
                strncpy(g_proj_name, proj.name.c_str(), sizeof(g_proj_name)-1);
                strncpy(g_proj_desc, proj.description.c_str(), sizeof(g_proj_desc)-1);
                g_show_edit_project = true;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, COL_RED);
            if(ImGui::MenuItem("Delete Project")) g_show_del_project = true;
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
        if(ImGui::IsItemHovered() && !proj.description.empty())
            ImGui::SetTooltip("%s", proj.description.c_str());
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ─── Notes tab: shop notes, blueprints, attached files ───────────────────────
static int         g_sel_note_id      = -1;
static char        g_note_title[256]  = {};
static int         g_note_kind        = 0;      // index into NOTE_KINDS (or NOTE_KIND_COUNT = custom)
static std::string g_note_kind_extra;           // a kind that isn't one of the presets
static std::string g_note_body;
static char        g_note_search[256] = {};
static char        g_attach_path[1024]= {};
static std::string g_attach_err;
static int         g_del_att_id       = -1;
static std::string g_del_att_name;
static bool g_show_add_note  = false;
static bool g_show_edit_note = false;
static bool g_show_del_note  = false;
static bool g_show_attach    = false;
static bool g_show_del_att   = false;

static const char* note_kind_name(int i){
    return i < NOTE_KIND_COUNT ? NOTE_KINDS[i] : g_note_kind_extra.c_str();
}

// Multiline input bound to a std::string (grows as you type; same approach as imgui_stdlib)
static int note_input_resize_cb(ImGuiInputTextCallbackData* d){
    if(d->EventFlag == ImGuiInputTextFlags_CallbackResize){
        std::string* s = static_cast<std::string*>(d->UserData);
        s->resize(static_cast<size_t>(d->BufTextLen));
        d->Buf = const_cast<char*>(s->c_str());
    }
    return 0;
}
static bool input_multiline_str(const char* label, std::string& s, ImVec2 size){
    return ImGui::InputTextMultiline(label, const_cast<char*>(s.c_str()), s.capacity() + 1, size,
        ImGuiInputTextFlags_CallbackResize, note_input_resize_cb, &s);
}

// Native "choose a file" dialog for attachments (any file type). "" if cancelled / unavailable.
static std::string pick_any_file(){
#ifdef __APPLE__
    {
        FILE* fp = popen("osascript -e 'POSIX path of (choose file with prompt \"Attach a file\")' 2>/dev/null", "r");
        if(!fp) return "";
        char buf[1024]={};
        fgets(buf, sizeof(buf), fp);
        pclose(fp);
        std::string s(buf);
        if(!s.empty() && s.back()=='\n') s.pop_back();
        return s;
    }
#endif
    struct { const char* bin; const char* cmd; } tools[] = {
        { "kdialog", "kdialog --getopenfilename \"$HOME\"" },
        { "zenity",  "zenity --file-selection --title='Attach a file'" },
    };
    for(auto& t : tools){
        std::string chk = std::string("which ") + t.bin + " >/dev/null 2>&1";
        if(system(chk.c_str()) != 0) continue;
        FILE* fp = popen((std::string(t.cmd) + " 2>/dev/null").c_str(), "r");
        if(!fp) continue;
        char buf[1024]={};
        fgets(buf, sizeof(buf), fp);
        pclose(fp);
        std::string s(buf);
        if(!s.empty() && s.back()=='\n') s.pop_back();
        if(!s.empty()) return s;
    }
    return "";
}

static void begin_note_edit(const Note* n){
    memset(g_note_title, 0, sizeof(g_note_title));
    g_note_kind_extra.clear();
    g_note_body.clear();
    g_note_kind = 0;
    if(!n) return;
    strncpy(g_note_title, n->title.c_str(), sizeof(g_note_title)-1);
    g_note_body = n->body;
    g_note_kind = -1;
    for(int i = 0; i < NOTE_KIND_COUNT; i++)
        if(n->kind == NOTE_KINDS[i]){ g_note_kind = i; break; }
    if(g_note_kind < 0){            // keep an unknown kind (e.g. typed on the phone) as it is
        g_note_kind_extra = n->kind;
        g_note_kind = NOTE_KIND_COUNT;
    }
}

static void draw_note_form(){
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
    ImGui::TextUnformatted("Title *");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##ntitle", g_note_title, sizeof(g_note_title));

    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
    ImGui::TextUnformatted("Type");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1);
    int n_items = NOTE_KIND_COUNT + (g_note_kind_extra.empty() ? 0 : 1);
    if(ImGui::BeginCombo("##nkind", note_kind_name(g_note_kind))){
        for(int i = 0; i < n_items; i++){
            bool is_sel = (i == g_note_kind);
            if(ImGui::Selectable(note_kind_name(i), is_sel)) g_note_kind = i;
            if(is_sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
    ImGui::TextUnformatted("Note  (plain text; spacing is kept, so ASCII sketches and wiring tables work)");
    ImGui::PopStyleColor();
    input_multiline_str("##nbody", g_note_body, {-1, 300});
}

// File types that run code when opened. An attachment can arrive by sync from any device on the
// tailnet, so these are saved to the temp folder but never handed to the system opener.
static bool is_runnable_type(const std::string& filename){
    std::string ext = std::filesystem::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
    static const char* RUN[] = { ".desktop", ".sh", ".bash", ".zsh", ".command", ".run", ".appimage",
        ".bin", ".exe", ".bat", ".cmd", ".com", ".msi", ".scr", ".ps1", ".vbs", ".jar", ".jnlp",
        ".scpt", ".app", ".workflow", ".terminal" };
    for(const char* r : RUN) if(ext == r) return true;
    return false;
}

static void open_attachment(const Attachment& f){
    std::string err;
    std::string path = db_extract_attachment(f.id, err);
    if(path.empty()) g_status_msg = "Open failed: " + err;
    else if(is_runnable_type(f.filename)) g_status_msg = "Not opened (program or script file). Saved to " + path;
    else { open_url(path); g_status_msg = "Opened " + f.filename; }
    g_status_time = glfwGetTime();
}

static void draw_notes_tab(Project& proj){
    std::string needle = to_lower_str(g_note_search);
    auto matches = [&](const Note& n){
        if(needle.empty()) return true;
        return to_lower_str(n.title + " " + n.kind + " " + n.body).find(needle) != std::string::npos;
    };

    // Selected note (by id, so it survives reloads); fall back to the first visible one
    Note* sel = nullptr;
    for(auto& n : proj.notes) if(n.id == g_sel_note_id && matches(n)){ sel = &n; break; }
    if(!sel)
        for(auto& n : proj.notes) if(matches(n)){ sel = &n; g_sel_note_id = n.id; break; }

    // ── Toolbar ──
    push_accent_style();
    if(ImGui::Button("+ Add Note")){
        begin_note_edit(nullptr);
        g_show_add_note = true;
    }
    pop_accent_style();
    ImGui::SameLine();
    if(!sel) ImGui::BeginDisabled();
    push_accent_style();
    if(ImGui::Button("Edit") && sel){
        begin_note_edit(sel);
        g_show_edit_note = true;
    }
    ImGui::SameLine();
    if(ImGui::Button("Attach File...") && sel){
        memset(g_attach_path, 0, sizeof(g_attach_path));
        g_attach_err.clear();
        g_show_attach = true;
    }
    pop_accent_style();
    ImGui::SameLine();
    if(ImGui::Button("Copy Text") && sel){
        ImGui::SetClipboardText(sel->body.c_str());
        g_status_msg = "Note text copied";
        g_status_time = glfwGetTime();
    }
    ImGui::SameLine();
    push_danger_style();
    if(ImGui::Button("Delete") && sel) g_show_del_note = true;
    pop_danger_style();
    if(!sel) ImGui::EndDisabled();

    float search_w = 220.0f;
    float avail    = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(ImGui::GetCursorPosX() + avail - search_w - 8);
    ImGui::SetNextItemWidth(search_w);
    ImGui::InputTextWithHint("##nsearch", "Search notes...", g_note_search, sizeof(g_note_search));
    ImGui::Spacing();

    // ── Note list (left) ──
    float list_w = std::clamp(ImGui::GetContentRegionAvail().x * 0.32f, 200.0f, 340.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, COL_PANEL);
    ImGui::BeginChild("##note_list", {list_w, 0}, true);
    int shown = 0;
    for(auto& n : proj.notes){
        if(!matches(n)) continue;
        shown++;
        bool is_sel = (sel && n.id == sel->id);
        ImGui::PushID(n.id);
        if(is_sel){
            ImGui::PushStyleColor(ImGuiCol_Header,        COL_SEL_BG);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, COL_SEL_BG);
        }
        float line_h = ImGui::GetTextLineHeight();
        if(ImGui::Selectable("##notesel", is_sel, 0, {0, line_h * 2 + 6}))
            g_sel_note_id = n.id;
        if(is_sel) ImGui::PopStyleColor(2);
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect({a.x, a.y}, {b.x - 2, b.y}, true);
        std::string tag = n.kind.empty() ? "Note" : n.kind;
        if(!n.files.empty()) tag += "  [" + std::to_string(n.files.size()) +
                                    (n.files.size() == 1 ? " file]" : " files]");
        dl->AddText({a.x + 6, a.y + 2}, ImGui::GetColorU32(noteKindColor(n.kind)), tag.c_str());
        dl->AddText({a.x + 6, a.y + 2 + line_h}, ImGui::GetColorU32(COL_TEXT), n.title.c_str());
        dl->PopClipRect();
        ImGui::PopID();
    }
    if(shown == 0){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::TextWrapped(proj.notes.empty() ? "No notes yet." : "No matches.");
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // ── Note viewer (right) ──
    ImGui::PushStyleColor(ImGuiCol_ChildBg, COL_PANEL);
    ImGui::BeginChild("##note_view", {0, 0}, true);
    if(!sel){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(proj.notes.empty()
            ? "No notes yet.\n\nUse + Add Note for shop notes, wiring sketches, assembly steps or safety rules, "
              "then Attach File... to keep a blueprint, photo or PDF with the note."
            : "Select a note.");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::TextUnformatted(sel->title.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, noteKindColor(sel->kind));
        ImGui::Text("  [%s]", sel->kind.empty() ? "Note" : sel->kind.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();

        float att_h = 0.0f;
        if(!sel->files.empty())
            att_h = std::min(160.0f, ImGui::GetFrameHeightWithSpacing() * static_cast<float>(sel->files.size())
                                     + ImGui::GetTextLineHeightWithSpacing() + 16.0f);
        ImGui::BeginChild("##note_body", {0, att_h > 0 ? -(att_h + 6.0f) : 0.0f}, false);
        if(sel->body.empty()){
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
            ImGui::TextUnformatted("(no text)");
            ImGui::PopStyleColor();
        } else {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(sel->body.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::EndChild();

        if(!sel->files.empty()){
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT_DIM);
            ImGui::TextUnformatted("ATTACHED FILES");
            ImGui::PopStyleColor();
            ImGui::BeginChild("##note_files", {0, 0}, false);
            for(auto& f : sel->files){
                ImGui::PushID(f.id);
                ImGui::TextUnformatted(f.filename.c_str());
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
                ImGui::Text("(%s)", human_size(f.size).c_str());
                ImGui::PopStyleColor();
                ImGui::SameLine();
                push_accent_style();
                if(ImGui::SmallButton("Open")) open_attachment(f);
                pop_accent_style();
                ImGui::SameLine();
                push_danger_style();
                if(ImGui::SmallButton("Remove")){
                    g_del_att_id   = f.id;
                    g_del_att_name = f.filename;
                    g_show_del_att = true;
                }
                pop_danger_style();
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void draw_parts_tab(Project& proj);

// ─── Parts panel (right area) ────────────────────────────────────────────────
static void draw_parts_panel(){
    if(g_sel_project < 0 || g_sel_project >= static_cast<int>(g_projects.size())){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 60);
        float w = ImGui::GetContentRegionAvail().x;
        const char* msg = "<-- Select a project";
        ImGui::SetCursorPosX((w - ImGui::CalcTextSize(msg).x) * 0.5f);
        ImGui::TextUnformatted(msg);
        ImGui::PopStyleColor();
        return;
    }
    Project& proj = g_projects[g_sel_project];

    // ── Header row ──
    ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
    ImGui::Text("%s", proj.name.c_str());
    ImGui::PopStyleColor();
    if(!proj.description.empty()){
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::Text("  —  %s", proj.description.c_str());
        ImGui::PopStyleColor();
    }
    {
        // Show part count + installed progress; cost lives exclusively in the footer
        int installed = 0;
        for(auto& pt : proj.parts)
            if(pt.status == STATUS_INSTALLED) installed++;
        int total_parts = static_cast<int>(proj.parts.size());
        std::string count_str = std::to_string(total_parts) +
                                (total_parts != 1 ? " parts" : " part");
        if(total_parts > 0)
            count_str += "   (" + std::to_string(installed) + "/" +
                         std::to_string(total_parts) + " installed)";
        float rw = ImGui::CalcTextSize(count_str.c_str()).x;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - rw + ImGui::GetCursorPosX() - 8);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::TextUnformatted(count_str.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Separator();
    ImGui::Spacing();

    if(ImGui::BeginTabBar("##proj_tabs")){
        if(ImGui::BeginTabItem("Parts")){
            draw_parts_tab(proj);
            ImGui::EndTabItem();
        }
        std::string notes_label = "Notes (" + std::to_string(proj.notes.size()) + ")###notes_tab";
        if(ImGui::BeginTabItem(notes_label.c_str())){
            draw_notes_tab(proj);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

// ─── Parts tab (toolbar, table, footer) ──────────────────────────────────────
static void draw_parts_tab(Project& proj){
    // ── Toolbar ──
    push_accent_style();
    if(ImGui::Button("+ Add Part")){
        memset(g_part_name,   0, sizeof(g_part_name));
        memset(g_part_url,    0, sizeof(g_part_url));
        memset(g_part_pn,     0, sizeof(g_part_pn));
        memset(g_part_vendor, 0, sizeof(g_part_vendor));
        memset(g_part_notes,  0, sizeof(g_part_notes));
        g_part_qty    = 1;
        g_part_price  = 0.0f;
        g_part_status = 0;
        g_show_add_part = true;
    }
    pop_accent_style();
    ImGui::SameLine();

    bool has_sel = (g_sel_part >= 0 &&
                    g_sel_part < static_cast<int>(proj.parts.size()));
    if(!has_sel) ImGui::BeginDisabled();
    push_accent_style();
    if(ImGui::Button("Edit") && has_sel){
        Part& p = proj.parts[g_sel_part];
        strncpy(g_part_name,   p.name.c_str(),        sizeof(g_part_name)-1);
        strncpy(g_part_url,    p.url.c_str(),          sizeof(g_part_url)-1);
        strncpy(g_part_pn,     p.part_number.c_str(),  sizeof(g_part_pn)-1);
        strncpy(g_part_vendor, p.vendor.c_str(),       sizeof(g_part_vendor)-1);
        strncpy(g_part_notes,  p.notes.c_str(),        sizeof(g_part_notes)-1);
        g_part_qty    = p.quantity;
        g_part_price  = static_cast<float>(p.unit_price);
        g_part_status = static_cast<int>(p.status);
        g_show_edit_part = true;
    }
    pop_accent_style();
    ImGui::SameLine();
    push_danger_style();
    if(ImGui::Button("Delete") && has_sel) g_show_del_part = true;
    pop_danger_style();
    if(!has_sel) ImGui::EndDisabled();

    ImGui::SameLine();
    bool has_url = has_sel && !proj.parts[g_sel_part].url.empty();
    if(!has_url) ImGui::BeginDisabled();
    if(ImGui::Button("Open URL") && has_url) open_url(proj.parts[g_sel_part].url);
    if(!has_url) ImGui::EndDisabled();

    ImGui::SameLine();
    bool has_parts = !proj.parts.empty();
    if(!has_parts) ImGui::BeginDisabled();
    push_accent_style();
    if(ImGui::Button("Print BOM") && has_parts) export_bom_html(proj);
    pop_accent_style();
    if(!has_parts) ImGui::EndDisabled();

    // Search box — right-aligned
    float search_w = 220.0f;
    float avail    = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(ImGui::GetCursorPosX() + avail - search_w - 8);
    ImGui::SetNextItemWidth(search_w);
    ImGui::InputTextWithHint("##search", "Search parts...", g_search, sizeof(g_search));
    ImGui::Spacing();

    // ── Parts table ──
    static ImGuiTableFlags tflags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_SizingFixedFit;

    // Sorting state — hoisted so footer can use same order/needle
    static int  s_sort_col = 0;
    static bool s_sort_asc = true;

    std::vector<int> order(proj.parts.size());
    for(int i = 0; i < static_cast<int>(order.size()); i++) order[i] = i;
    // Initial sort applied here; re-sorted below when SpecsDirty fires
    auto do_sort = [&](){
        std::stable_sort(order.begin(), order.end(), [&](int a, int b){
            const Part& pa = proj.parts[a];
            const Part& pb = proj.parts[b];
            // Section is always the primary sort key
            if(pa.section != pb.section)
                return pa.section < pb.section;
            int cmp = 0;
            switch(s_sort_col){
                case 0: cmp = pa.name.compare(pb.name); break;
                case 1: cmp = pa.part_number.compare(pb.part_number); break;
                case 2: cmp = pa.vendor.compare(pb.vendor); break;
                case 3: cmp = pa.quantity - pb.quantity; break;
                case 4: cmp = (pa.unit_price < pb.unit_price) ? -1 :
                               (pa.unit_price > pb.unit_price) ?  1 : 0; break;
                case 5: { double ta = pa.unit_price*pa.quantity,
                                 tb = pb.unit_price*pb.quantity;
                          cmp = (ta < tb) ? -1 : (ta > tb) ? 1 : 0; break; }
                case 6: cmp = static_cast<int>(pa.status) -
                               static_cast<int>(pb.status); break;
                case 7: cmp = pa.notes.compare(pb.notes); break;
                default: break;
            }
            return s_sort_asc ? cmp < 0 : cmp > 0;
        });
    };
    do_sort();

    // Build lowercase search needle once per frame
    std::string needle(g_search);
    std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);

    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,    COL_HEADER_BG);
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight, COL_BORDER);

    float table_h = ImGui::GetContentRegionAvail().y - 42;
    if(ImGui::BeginTable("##parts", 8, tflags, {0, table_h})){
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Part Name",  ImGuiTableColumnFlags_DefaultSort, 160);
        ImGui::TableSetupColumn("Part #",     0, 90);
        ImGui::TableSetupColumn("Vendor",     0, 90);
        ImGui::TableSetupColumn("Qty",        ImGuiTableColumnFlags_NoResize, 45);
        ImGui::TableSetupColumn("Unit Price", 0, 85);
        ImGui::TableSetupColumn("Total",      0, 85);
        ImGui::TableSetupColumn("Status",     0, 85);
        ImGui::TableSetupColumn("Notes",      ImGuiTableColumnFlags_WidthStretch, 0);
        ImGui::TableHeadersRow();

        if(ImGuiTableSortSpecs* ss = ImGui::TableGetSortSpecs()){
            if(ss->SpecsDirty && ss->SpecsCount > 0){
                s_sort_col = ss->Specs[0].ColumnIndex;
                s_sort_asc = (ss->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
                ss->SpecsDirty = false;
                do_sort();
            }
        }

        std::string prev_section_label;
        for(int oi = 0; oi < static_cast<int>(order.size()); oi++){
            int i    = order[oi];
            Part& pt = proj.parts[i];

            // Section header row
            if(!pt.section.empty() && pt.section != prev_section_label){
                prev_section_label = pt.section;
                ImGui::TableNextRow();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    ImGui::GetColorU32(COL_HEADER_BG));
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                    ImGui::GetColorU32(COL_HEADER_BG));
                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT_DIM);
                ImGui::TextUnformatted(("  >  " + pt.section).c_str());
                ImGui::PopStyleColor();
            }

            // Filter
            if(!needle.empty()){
                std::string hay = pt.name + " " + pt.part_number + " " +
                                  pt.vendor + " " + pt.notes + " " + pt.url;
                std::transform(hay.begin(), hay.end(), hay.begin(), ::tolower);
                if(hay.find(needle) == std::string::npos) continue;
            }

            ImGui::TableNextRow();
            bool row_sel = (g_sel_part == i);
            if(row_sel) ImGui::PushStyleColor(ImGuiCol_TableRowBg, COL_SEL_BG);

            // Col 0 — Part Name (selectable spans all columns)
            ImGui::TableSetColumnIndex(0);
            bool has_link = !pt.url.empty();
            if(has_link) ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
            std::string sl = pt.name + "##row" + std::to_string(i);
            // BUG FIX: use braces so g_sel_part_id assignment is inside the if
            if(ImGui::Selectable(sl.c_str(), row_sel, ImGuiSelectableFlags_SpanAllColumns)){
                g_sel_part    = (g_sel_part == i) ? -1 : i;
                g_sel_part_id = (g_sel_part < 0)  ? -1 : proj.parts[i].id;
            }
            if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && has_link)
                open_url(pt.url);
            if(has_link){
                ImGui::PopStyleColor();
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("Double-click to open:\n%s", pt.url.c_str());
            }

            // Col 1 — Part #
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
            ImGui::TextUnformatted(pt.part_number.empty() ? "—" : pt.part_number.c_str());
            ImGui::PopStyleColor();

            // Col 2 — Vendor
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(pt.vendor.empty() ? "—" : pt.vendor.c_str());

            // Col 3 — Qty
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", pt.quantity);

            // Col 4 — Unit price
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(format_price(pt.unit_price).c_str());

            // Col 5 — Line total (show $0.00 explicitly if qty>0)
            ImGui::TableSetColumnIndex(5);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN);
            ImGui::TextUnformatted(
                format_price(pt.unit_price * pt.quantity, pt.unit_price <= 0.0).c_str());
            ImGui::PopStyleColor();

            // Col 6 — Status
            ImGui::TableSetColumnIndex(6);
            ImGui::PushStyleColor(ImGuiCol_Text, statusColor(pt.status));
            ImGui::TextUnformatted(STATUS_LABELS[pt.status]);
            ImGui::PopStyleColor();

            // Col 7 — Notes (truncated)
            ImGui::TableSetColumnIndex(7);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
            if(!pt.notes.empty()){
                std::string disp = (pt.notes.size() > 60)
                    ? pt.notes.substr(0, 57) + "…" : pt.notes;
                ImGui::TextUnformatted(disp.c_str());
                if(pt.notes.size() > 60 && ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", pt.notes.c_str());
            } else {
                ImGui::TextUnformatted("—");
            }
            ImGui::PopStyleColor();

            if(row_sel) ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleColor(2);

    // ── Running total footer ──
    {
        // Compute totals for visible (filtered) rows only
        double vis_subtotal = 0.0;
        int    vis_count    = 0;
        int    vis_needed   = 0;
        for(int oi = 0; oi < static_cast<int>(order.size()); oi++){
            const Part& pt = proj.parts[order[oi]];
            if(!needle.empty()){
                std::string hay = pt.name + " " + pt.part_number + " " +
                                  pt.vendor + " " + pt.notes + " " + pt.url;
                std::transform(hay.begin(), hay.end(), hay.begin(), ::tolower);
                if(hay.find(needle) == std::string::npos) continue;
            }
            vis_subtotal += pt.unit_price * pt.quantity;
            vis_count++;
            if(pt.status != STATUS_INSTALLED) vis_needed++;
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(COL_PANEL.x, COL_PANEL.y, COL_PANEL.z, 1.0f));
        ImGui::BeginChild("##footer", {0, 30}, false);
        ImGui::Separator();
        ImGui::Spacing();

        // Left: part count
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        if(needle.empty())
            ImGui::Text("%d part%s", vis_count, vis_count != 1 ? "s" : "");
        else
            ImGui::Text("%d / %d part%s shown", vis_count,
                static_cast<int>(proj.parts.size()),
                static_cast<int>(proj.parts.size()) != 1 ? "s" : "");
        ImGui::PopStyleColor();

        // Right: subtotal label + value
        std::string sub_str = format_price(vis_subtotal, false);
        std::string label   = "Subtotal (excl. tax & shipping):  ";
        float right_w = ImGui::CalcTextSize((label + sub_str).c_str()).x + 16;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - right_w + ImGui::GetCursorPosX());
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::TextUnformatted(label.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::TextUnformatted(sub_str.c_str());
        ImGui::PopStyleColor();

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
}

// ─── Part form (shared by Add/Edit modals) ───────────────────────────────────
static void draw_part_form(){
    float half = (ImGui::GetContentRegionAvail().x -
                  ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
    ImGui::TextUnformatted("Part Name *");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##pname", g_part_name, sizeof(g_part_name));

    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
    ImGui::TextUnformatted("Web Link");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##purl", g_part_url, sizeof(g_part_url));

    // Part # and Vendor side by side
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
    ImGui::TextUnformatted("Part Number / SKU");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(half);
    ImGui::InputText("##ppn", g_part_pn, sizeof(g_part_pn));
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
    ImGui::TextUnformatted("Vendor / Supplier");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##pvendor", g_part_vendor, sizeof(g_part_vendor));

    // Qty and Price side by side
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
    ImGui::TextUnformatted("Quantity");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(half);
    ImGui::InputInt("##pqty", &g_part_qty);
    if(g_part_qty < 1) g_part_qty = 1;
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
    ImGui::TextUnformatted("Unit Price ($)");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputFloat("##pprice", &g_part_price, 0.01f, 1.0f, "%.2f");
    if(g_part_price < 0.0f) g_part_price = 0.0f;

    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
    ImGui::TextUnformatted("Status");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##pstatus", &g_part_status, STATUS_LABELS, STATUS_COUNT);

    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
    ImGui::TextUnformatted("Notes");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextMultiline("##pnotes", g_part_notes, sizeof(g_part_notes), {0, 64});
}


static void select_project_by_id(int id){
    resolve_sel_part();
    for(int k = 0; k < (int)g_projects.size(); k++)
        if(g_projects[k].id == id){ g_sel_project = k; g_sel_project_id = id; break; }
}

static void draw_import_md_modal(){
    if(begin_modal("Import from Markdown", {540, 0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::TextUnformatted("IMPORT BOM FROM MARKDOWN");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();

        // File path row
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::TextUnformatted("File:");
        ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(-98);
        bool changed = ImGui::InputText("##imp_path", g_import_path, sizeof(g_import_path));
        ImGui::SameLine();
        push_accent_style();
        if(ImGui::Button("Browse", {84,0})){
            std::string picked = pick_file_zenity();
            if(!picked.empty()){
                strncpy(g_import_path, picked.c_str(), sizeof(g_import_path)-1);
                changed = true;
            }
        }
        pop_accent_style();
        if(changed){ g_import_previewed = false; g_import_result = ImportResult{}; }

        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::TextWrapped(
            "Expects a Markdown file with a # Heading for the project name, then one "
            "or more tables. Columns matched by name: Part Name / Item, Part #, Vendor, "
            "Qty, Unit Price, Status, Description, Notes, URL; any other column "
            "(Recommendation, Function, ...) is kept in the part's notes. "
            "Multiple tables are merged into one project, and re-importing a project "
            "that already exists fills in missing details instead of duplicating it. "
            "Tip: you can also just drop a .md file onto the window.");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Preview button
        bool has_path = g_import_path[0] != '\0';
        if(!has_path) ImGui::BeginDisabled();
        push_accent_style();
        if(ImGui::Button("Preview", {100,0})){
            g_import_result   = parse_bom_markdown(std::string(g_import_path));
            g_import_previewed = true;
        }
        pop_accent_style();
        if(!has_path) ImGui::EndDisabled();

        // Results pane
        if(g_import_previewed){
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            if(!g_import_result.ok()){
                ImGui::PushStyleColor(ImGuiCol_Text, COL_RED);
                ImGui::TextWrapped("Error: %s", g_import_result.error.c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN);
                ImGui::TextUnformatted("Ready to import:");
                ImGui::PopStyleColor();
                ImGui::Spacing();
                ImGui::Text("Project : %s", g_import_result.project_name.c_str());
                if(!g_import_result.project_desc.empty()){
                    std::string preview_desc = g_import_result.project_desc.substr(0,120);
                    if(g_import_result.project_desc.size() > 120) preview_desc += "...";
                    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
                    ImGui::TextWrapped("Desc    : %s", preview_desc.c_str());
                    ImGui::PopStyleColor();
                }
                ImGui::Text("Parts   : %d", (int)g_import_result.parts.size());
                ImGui::Spacing();

                // Mini preview table
                ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,    COL_HEADER_BG);
                ImGui::PushStyleColor(ImGuiCol_TableBorderLight, COL_BORDER);
                if(ImGui::BeginTable("##imp_prev", 3,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp)){
                    ImGui::TableSetupColumn("Part Name", 0, 210);
                    ImGui::TableSetupColumn("Vendor",    0,  90);
                    ImGui::TableSetupColumn("Qty / $",   0,  80);
                    ImGui::TableHeadersRow();
                    int show = std::min((int)g_import_result.parts.size(), 8);
                    for(int pi = 0; pi < show; pi++){
                        auto& pt = g_import_result.parts[pi];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT);
                        ImGui::TextUnformatted(pt.name.c_str());
                        ImGui::PopStyleColor();
                        ImGui::TableSetColumnIndex(1);
                        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
                        ImGui::TextUnformatted(pt.vendor.empty() ? "\xe2\x80\x94" : pt.vendor.c_str());
                        ImGui::PopStyleColor();
                        ImGui::TableSetColumnIndex(2);
                        if(pt.unit_price > 0.0)
                            ImGui::Text("%d \xc3\x97 %s", pt.quantity, format_price(pt.unit_price, false).c_str());
                        else
                            ImGui::Text("qty %d", pt.quantity);
                    }
                    if((int)g_import_result.parts.size() > 8){
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
                        ImGui::Text("\xe2\x80\xa6 and %d more", (int)g_import_result.parts.size()-8);
                        ImGui::PopStyleColor();
                    }
                    ImGui::EndTable();
                }
                ImGui::PopStyleColor(2);
            }
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        bool can_import = g_import_previewed && g_import_result.ok();
        if(!can_import) ImGui::BeginDisabled();
        push_accent_style();
        if(ImGui::Button("Import", {120,0})){
            ImportOutcome oc = import_markdown_file(std::string(g_import_path));
            if(oc.ok) select_project_by_id(oc.project_id);
            g_status_msg  = oc.ok ? describe_outcome(oc) : "Import failed: " + oc.error;
            g_status_time = glfwGetTime();
            g_import_result   = ImportResult{};
            g_import_previewed = false;
            memset(g_import_path, 0, sizeof(g_import_path));
            ImGui::CloseCurrentPopup();
        }
        pop_accent_style();
        if(!can_import) ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})){
            g_import_result   = ImportResult{};
            g_import_previewed = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ─── Modals ──────────────────────────────────────────────────────────────────
static void draw_modals(){
    // All OpenPopup calls must happen OUTSIDE BeginMenuBar so they reach the
    // correct popup stack.  We use flag → OpenPopup → clear pattern.
    if(g_show_add_project)  { ImGui::OpenPopup("Add Project");     g_show_add_project  = false; }
    if(g_show_edit_project) { ImGui::OpenPopup("Edit Project");    g_show_edit_project = false; }
    if(g_show_del_project)  { ImGui::OpenPopup("Delete Project?"); g_show_del_project  = false; }
    if(g_show_add_part)     { ImGui::OpenPopup("Add Part");        g_show_add_part     = false; }
    if(g_show_edit_part)    { ImGui::OpenPopup("Edit Part");       g_show_edit_part    = false; }
    if(g_show_del_part)     { ImGui::OpenPopup("Delete Part?");    g_show_del_part     = false; }
    if(g_show_about)        { ImGui::OpenPopup("About##dlg");      g_show_about        = false; }
    if(g_show_import_md)    { ImGui::OpenPopup("Import from Markdown"); g_show_import_md = false; }
    if(g_show_add_note)     { ImGui::OpenPopup("Add Note");        g_show_add_note     = false; }
    if(g_show_edit_note)    { ImGui::OpenPopup("Edit Note");       g_show_edit_note    = false; }
    if(g_show_del_note)     { ImGui::OpenPopup("Delete Note?");    g_show_del_note     = false; }
    if(g_show_attach)       { ImGui::OpenPopup("Attach File");     g_show_attach       = false; }
    if(g_show_del_att)      { ImGui::OpenPopup("Remove File?");    g_show_del_att      = false; }

    // ── Add Project ──
    if(begin_modal("Add Project")){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::TextUnformatted("NEW PROJECT");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::TextUnformatted("Name *");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##aname", g_proj_name, sizeof(g_proj_name));
        ImGui::TextUnformatted("Description");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##adesc", g_proj_desc, sizeof(g_proj_desc));
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        bool can_create = g_proj_name[0] != '\0';
        if(!can_create) ImGui::BeginDisabled();
        push_accent_style();
        if(ImGui::Button("Create", {120,0})){
            int new_id = db_insert_project(g_proj_name, g_proj_desc);
            db_load(); resolve_sel_part();
            for(int i = 0; i < static_cast<int>(g_projects.size()); i++)
                if(g_projects[i].id == new_id){ g_sel_project = i; g_sel_project_id = new_id; break; }
            ImGui::CloseCurrentPopup();
        }
        pop_accent_style();
        if(!can_create) ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Edit Project ──
    if(begin_modal("Edit Project")){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::TextUnformatted("EDIT PROJECT");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::TextUnformatted("Name *");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##ename", g_proj_name, sizeof(g_proj_name));
        ImGui::TextUnformatted("Description");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##edesc", g_proj_desc, sizeof(g_proj_desc));
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        bool can_save = g_proj_name[0] != '\0' && g_sel_project >= 0;
        if(!can_save) ImGui::BeginDisabled();
        push_accent_style();
        if(ImGui::Button("Save", {120,0})){
            db_update_project(g_projects[g_sel_project].id, g_proj_name, g_proj_desc);
            db_load(); resolve_sel_part();
            ImGui::CloseCurrentPopup();
        }
        pop_accent_style();
        if(!can_save) ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Delete Project ──
    if(begin_modal("Delete Project?", {380,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_RED);
        ImGui::TextUnformatted("DELETE PROJECT");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        if(g_sel_project >= 0 && g_sel_project < static_cast<int>(g_projects.size()))
            ImGui::TextWrapped("Delete \"%s\" and ALL its parts and notes? This cannot be undone.",
                g_projects[g_sel_project].name.c_str());
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        push_danger_style();
        if(ImGui::Button("Delete", {120,0}) && g_sel_project >= 0){
            db_delete_project(g_projects[g_sel_project].id);
            g_sel_project = -1; g_sel_project_id = -1; g_sel_part = -1; g_sel_part_id = -1;
            db_load(); resolve_sel_part();
            ImGui::CloseCurrentPopup();
        }
        pop_danger_style();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Add Part ──
    if(begin_modal("Add Part", {480,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::TextUnformatted("ADD PART");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        draw_part_form();
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        bool can_add = g_part_name[0] != '\0' && g_sel_project >= 0;
        if(!can_add) ImGui::BeginDisabled();
        push_accent_style();
        if(ImGui::Button("Add", {120,0})){
            Part pt;
            pt.project_id  = g_projects[g_sel_project].id;
            pt.name        = g_part_name;
            pt.url         = g_part_url;
            pt.part_number = g_part_pn;
            pt.vendor      = g_part_vendor;
            pt.notes       = g_part_notes;
            pt.quantity    = g_part_qty;
            pt.unit_price  = static_cast<double>(g_part_price);
            pt.status      = static_cast<PartStatus>(g_part_status);
            int new_part_id = db_insert_part(pt);
            db_load(); resolve_sel_part();
            // Select the newly added part
            if(g_sel_project >= 0){
                auto& parts = g_projects[g_sel_project].parts;
                for(int i = 0; i < static_cast<int>(parts.size()); i++)
                    if(parts[i].id == new_part_id){ g_sel_part = i; g_sel_part_id = new_part_id; break; }
            }
            ImGui::CloseCurrentPopup();
        }
        pop_accent_style();
        if(!can_add) ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Edit Part ──
    if(begin_modal("Edit Part", {480,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::TextUnformatted("EDIT PART");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        draw_part_form();
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        bool can_save = g_part_name[0] != '\0' && g_sel_project >= 0 && g_sel_part >= 0;
        if(!can_save) ImGui::BeginDisabled();
        push_accent_style();
        if(ImGui::Button("Save", {120,0})){
            Part& ex      = g_projects[g_sel_project].parts[g_sel_part];
            ex.name        = g_part_name;
            ex.url         = g_part_url;
            ex.part_number = g_part_pn;
            ex.vendor      = g_part_vendor;
            ex.notes       = g_part_notes;
            ex.quantity    = g_part_qty;
            ex.unit_price  = static_cast<double>(g_part_price);
            ex.status      = static_cast<PartStatus>(g_part_status);
            db_update_part(ex);
            db_load(); resolve_sel_part();
            ImGui::CloseCurrentPopup();
        }
        pop_accent_style();
        if(!can_save) ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Delete Part ──
    if(begin_modal("Delete Part?", {380,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_RED);
        ImGui::TextUnformatted("DELETE PART");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        if(g_sel_project >= 0 && g_sel_part >= 0 &&
           g_sel_part < static_cast<int>(g_projects[g_sel_project].parts.size()))
            ImGui::Text("Delete \"%s\"? This cannot be undone.",
                g_projects[g_sel_project].parts[g_sel_part].name.c_str());
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        push_danger_style();
        if(ImGui::Button("Delete", {120,0}) && g_sel_project >= 0 && g_sel_part >= 0){
            db_delete_part(g_projects[g_sel_project].parts[g_sel_part].id);
            g_sel_part = -1; g_sel_part_id = -1;
            db_load(); resolve_sel_part();
            ImGui::CloseCurrentPopup();
        }
        pop_danger_style();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Add Note ──
    if(begin_modal("Add Note", {720,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::TextUnformatted("ADD NOTE");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        draw_note_form();
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        bool can_add = g_note_title[0] != '\0' && g_sel_project >= 0;
        if(!can_add) ImGui::BeginDisabled();
        push_accent_style();
        if(ImGui::Button("Add", {120,0})){
            int new_id = db_insert_note(g_projects[g_sel_project].id, g_note_title,
                                        note_kind_name(g_note_kind), g_note_body);
            g_sel_note_id = new_id;
            db_load(); resolve_sel_part();
            ImGui::CloseCurrentPopup();
        }
        pop_accent_style();
        if(!can_add) ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Edit Note ──
    if(begin_modal("Edit Note", {720,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::TextUnformatted("EDIT NOTE");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        draw_note_form();
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        bool can_save = g_note_title[0] != '\0' && g_sel_note_id >= 0;
        if(!can_save) ImGui::BeginDisabled();
        push_accent_style();
        if(ImGui::Button("Save", {120,0})){
            db_update_note(g_sel_note_id, g_note_title, note_kind_name(g_note_kind), g_note_body);
            db_load(); resolve_sel_part();
            ImGui::CloseCurrentPopup();
        }
        pop_accent_style();
        if(!can_save) ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Delete Note ──
    if(begin_modal("Delete Note?", {420,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_RED);
        ImGui::TextUnformatted("DELETE NOTE");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        const Note* dn = nullptr;
        if(g_sel_project >= 0 && g_sel_project < static_cast<int>(g_projects.size()))
            for(auto& n : g_projects[g_sel_project].notes) if(n.id == g_sel_note_id){ dn = &n; break; }
        if(dn){
            ImGui::TextWrapped("Delete \"%s\"%s? This cannot be undone.", dn->title.c_str(),
                dn->files.empty() ? "" : (" and its " + std::to_string(dn->files.size()) +
                                          (dn->files.size() == 1 ? " attached file" : " attached files")).c_str());
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        push_danger_style();
        if(ImGui::Button("Delete", {120,0}) && dn){
            db_delete_note(dn->id);
            g_sel_note_id = -1;
            db_load(); resolve_sel_part();
            ImGui::CloseCurrentPopup();
        }
        pop_danger_style();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Attach File ──
    if(begin_modal("Attach File", {600,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::TextUnformatted("ATTACH FILE");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::TextUnformatted("File path");
        ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(-100);
        ImGui::InputText("##attpath", g_attach_path, sizeof(g_attach_path));
        ImGui::SameLine();
        if(ImGui::Button("Browse...", {90,0})){
            std::string p = pick_any_file();
            if(!p.empty()){ strncpy(g_attach_path, p.c_str(), sizeof(g_attach_path)-1); g_attach_err.clear(); }
        }
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::TextWrapped("Photos, PDFs, drawings, CAD files: up to %s each. Files are stored in the "
                           "database and sync to your other machines.", human_size(MAX_ATTACH_BYTES).c_str());
        ImGui::PopStyleColor();
        if(!g_attach_err.empty()){
            ImGui::PushStyleColor(ImGuiCol_Text, COL_RED);
            ImGui::TextWrapped("%s", g_attach_err.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        bool can_attach = g_attach_path[0] != '\0' && g_sel_note_id >= 0;
        if(!can_attach) ImGui::BeginDisabled();
        push_accent_style();
        if(ImGui::Button("Attach", {120,0})){
            std::string err;
            if(db_add_attachment(g_sel_note_id, g_attach_path, err)){
                db_load(); resolve_sel_part();
                g_status_msg  = "File attached";
                g_status_time = glfwGetTime();
                ImGui::CloseCurrentPopup();
            } else {
                g_attach_err = err;
            }
        }
        pop_accent_style();
        if(!can_attach) ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Remove Attachment ──
    if(begin_modal("Remove File?", {400,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_RED);
        ImGui::TextUnformatted("REMOVE FILE");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::TextWrapped("Remove \"%s\" from this note? This cannot be undone.", g_del_att_name.c_str());
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        push_danger_style();
        if(ImGui::Button("Remove", {120,0}) && g_del_att_id >= 0){
            db_delete_attachment(g_del_att_id);
            g_del_att_id = -1;
            db_load(); resolve_sel_part();
            ImGui::CloseCurrentPopup();
        }
        pop_danger_style();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── About ──
    if(begin_modal("About##dlg", {420,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::TextUnformatted("BOM TRACKER");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::Text("Version %s  (build %s)", APP_VERSION, BUILD_HASH_STR);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::TextUnformatted("Source / releases:");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        static const char* REPO_URL = "https://github.com/mjdeiter/bom-tracker";
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::TextUnformatted(REPO_URL);
        ImGui::PopStyleColor();
        if(ImGui::IsItemHovered()){
            ImGui::SetTooltip("Click to copy");
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        if(ImGui::IsItemClicked()) ImGui::SetClipboardText(REPO_URL);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::TextUnformatted("Database:");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextUnformatted(g_db_path.c_str());
        if(ImGui::IsItemHovered()){
            ImGui::SetTooltip("Click to copy");
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        if(ImGui::IsItemClicked()) ImGui::SetClipboardText(g_db_path.c_str());
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::TextWrapped(
            "Bill of Materials tracker built with Dear ImGui + SQLite3.\n"
            "All changes save automatically to the database above.\n"
            "Double-click any part row to open its URL in your browser.\n"
            "Keyboard: Ctrl+N  New Project");
        ImGui::PopStyleColor();
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        push_accent_style();
        if(ImGui::Button("Close", {80,0})) ImGui::CloseCurrentPopup();
        pop_accent_style();
        ImGui::EndPopup();
    }
}

// ─── Theme ───────────────────────────────────────────────────────────────────
static void apply_theme(){
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding       = 4;
    s.ChildRounding        = 4;
    s.FrameRounding        = 3;
    s.PopupRounding        = 4;
    s.ScrollbarRounding    = 4;
    s.GrabRounding         = 3;
    s.TabRounding          = 3;
    s.FramePadding         = {6, 4};
    s.ItemSpacing          = {8, 5};
    s.WindowPadding        = {10, 10};
    s.ScrollbarSize        = 12;
    s.IndentSpacing        = 18;
    s.SeparatorTextBorderSize = 1;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = COL_BG;
    c[ImGuiCol_ChildBg]              = COL_PANEL;
    c[ImGuiCol_PopupBg]              = {0.12f,0.13f,0.16f,0.98f};
    c[ImGuiCol_Border]               = COL_BORDER;
    c[ImGuiCol_FrameBg]              = {0.16f,0.18f,0.21f,1.0f};
    c[ImGuiCol_FrameBgHovered]       = {0.20f,0.22f,0.26f,1.0f};
    c[ImGuiCol_FrameBgActive]        = {0.22f,0.24f,0.28f,1.0f};
    c[ImGuiCol_TitleBg]              = {0.09f,0.10f,0.12f,1.0f};
    c[ImGuiCol_TitleBgActive]        = {0.12f,0.13f,0.16f,1.0f};
    c[ImGuiCol_MenuBarBg]            = {0.11f,0.12f,0.14f,1.0f};
    c[ImGuiCol_ScrollbarBg]          = {0.10f,0.11f,0.13f,1.0f};
    c[ImGuiCol_ScrollbarGrab]        = {0.30f,0.32f,0.36f,1.0f};
    c[ImGuiCol_ScrollbarGrabHovered] = COL_ACCENT_DIM;
    c[ImGuiCol_ScrollbarGrabActive]  = COL_ACCENT;
    c[ImGuiCol_CheckMark]            = COL_ACCENT;
    c[ImGuiCol_SliderGrab]           = COL_ACCENT_DIM;
    c[ImGuiCol_SliderGrabActive]     = COL_ACCENT;
    c[ImGuiCol_Button]               = {0.20f,0.22f,0.26f,1.0f};
    c[ImGuiCol_ButtonHovered]        = {0.26f,0.28f,0.33f,1.0f};
    c[ImGuiCol_ButtonActive]         = COL_ACCENT_DIM;
    c[ImGuiCol_Header]               = COL_SEL_BG;
    c[ImGuiCol_HeaderHovered]        = {0.22f,0.18f,0.04f,1.0f};
    c[ImGuiCol_HeaderActive]         = COL_SEL_BG;
    c[ImGuiCol_Tab]                  = {0.14f,0.16f,0.19f,1.0f};
    c[ImGuiCol_TabHovered]           = COL_ACCENT_DIM;
    c[ImGuiCol_TabActive]            = COL_ACCENT_DIM;
    c[ImGuiCol_TableHeaderBg]        = COL_HEADER_BG;
    c[ImGuiCol_TableBorderStrong]    = COL_BORDER;
    c[ImGuiCol_TableBorderLight]     = {0.18f,0.20f,0.23f,1.0f};
    c[ImGuiCol_TableRowBg]           = COL_BG;
    c[ImGuiCol_TableRowBgAlt]        = COL_ROW_ALT;
    c[ImGuiCol_Separator]            = COL_BORDER;
    c[ImGuiCol_Text]                 = COL_TEXT;
    c[ImGuiCol_TextDisabled]         = COL_TEXT_DIM;
    c[ImGuiCol_ModalWindowDimBg]     = {0.0f,0.0f,0.0f,0.55f};
}

// ─── main ────────────────────────────────────────────────────────────────────
int main(int argc, char** argv){
    bool headless = false;
    std::vector<std::string> cli_files;
    for(int i = 1; i < argc; i++){
        std::string a = argv[i];
        if(a == "--import")                  headless = true;
        else if(a == "-h" || a == "--help"){
            printf("BOM Tracker v%s\n"
                   "Usage: bom-tracker [file.md ...]          open the app, importing any Markdown BOMs first\n"
                   "       bom-tracker --import file.md ...   import Markdown BOMs without opening a window\n",
                   APP_VERSION);
            return 0;
        }
        else cli_files.push_back(a);
    }

    db_init();
    db_load();

    // Markdown files given on the command line: create / sync the project(s)
    std::string pending_status;
    int last_imported_id = -1, cli_failures = 0;
    for(auto& f : cli_files){
        ImportOutcome oc = import_markdown_file(f);
        if(oc.ok){ last_imported_id = oc.project_id; pending_status = describe_outcome(oc); }
        else     { cli_failures++; pending_status = "Import failed: " + oc.error; }
        if(headless) printf("%s: %s\n", f.c_str(), oc.ok ? describe_outcome(oc).c_str() : ("ERROR: " + oc.error).c_str());
    }
    if(headless){
        if(cli_files.empty()){ fprintf(stderr, "bom-tracker --import: no files given\n"); return 2; }
        return cli_failures ? 1 : 0;
    }

    resolve_sel_part();
    if(last_imported_id >= 0) select_project_by_id(last_imported_id);

    if(!glfwInit()) return 1;
    glfwWindowHint(GLFW_RESIZABLE,              GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,  3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,  3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);  // required for core profile on macOS
#endif

    std::string win_title = std::string("BOM Tracker v") + APP_VERSION;
    GLFWwindow* window = glfwCreateWindow(1160, 700, win_title.c_str(), nullptr, nullptr);
    if(!window){ glfwTerminate(); return 1; }

    // Set window icon from embedded RGBA data
    {
        GLFWimage icon;
        icon.width  = ICON_W;
        icon.height = ICON_H;
        icon.pixels = const_cast<unsigned char*>(ICON_DATA);
        glfwSetWindowIcon(window, 1, &icon);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;  // Don't save imgui.ini clutter

    // Font: prefer Meslo Nerd Font → DejaVu Mono → ImGui default
    {
        const char* fonts[] = {
            "/usr/share/fonts/TTF/MesloLGMNerdFontMono-Regular.ttf",
            "/usr/share/fonts/TTF/MesloLGSNerdFontMono-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/System/Library/Fonts/Menlo.ttc",
            nullptr
        };
        for(int fi = 0; fonts[fi]; fi++){
            if(access(fonts[fi], R_OK) == 0){
#ifdef __APPLE__
                // Rasterize at Retina scale, then scale back down for crisp text
                float xs = 1.0f, ys = 1.0f;
                glfwGetWindowContentScale(window, &xs, &ys);
                io.Fonts->AddFontFromFileTTF(fonts[fi], 14.0f * xs);
                io.FontGlobalScale = 1.0f / xs;
#else
                io.Fonts->AddFontFromFileTTF(fonts[fi], 14.0f);
#endif
                break;
            }
        }
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    glfwSetDropCallback(window, drop_callback);
    if(!pending_status.empty()){ g_status_msg = pending_status; g_status_time = glfwGetTime(); }
    ImGui_ImplOpenGL3_Init("#version 330");
    apply_theme();

    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        poll_external_changes(glfwGetTime());

        // ── Markdown files dropped onto the window ──
        if(!g_dropped_files.empty()){
            std::vector<std::string> dropped; dropped.swap(g_dropped_files);
            for(auto& f : dropped){
                if(!looks_like_markdown(f)){
                    g_status_msg = "Not a Markdown file: " + std::filesystem::path(f).filename().string();
                } else {
                    ImportOutcome oc = import_markdown_file(f);
                    if(oc.ok){ select_project_by_id(oc.project_id); g_status_msg = describe_outcome(oc); }
                    else       g_status_msg = "Import failed: " + oc.error;
                }
                g_status_time = glfwGetTime();
            }
        }

        // ── Keyboard shortcuts ──
        ImGuiIO& kio = ImGui::GetIO();
        bool ctrl = kio.KeyCtrl;
        if(ctrl && ImGui::IsKeyPressed(ImGuiKey_N)){
            memset(g_proj_name, 0, sizeof(g_proj_name));
            memset(g_proj_desc, 0, sizeof(g_proj_desc));
            g_show_add_project = true;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Full-viewport window
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("##main", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);

        // ── Menu bar ──
        if(ImGui::BeginMenuBar()){
            ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
            ImGui::TextUnformatted("BOM TRACKER");
            ImGui::PopStyleColor();
            ImGui::Separator();

            if(ImGui::BeginMenu("Project")){
                if(ImGui::MenuItem("New Project", "Ctrl+N")){
                    memset(g_proj_name, 0, sizeof(g_proj_name));
                    memset(g_proj_desc, 0, sizeof(g_proj_desc));
                    g_show_add_project = true;
                }
                if(ImGui::MenuItem("Import from Markdown...")){
                    memset(g_import_path, 0, sizeof(g_import_path));
                    g_import_result   = ImportResult{};
                    g_import_previewed = false;
                    g_show_import_md  = true;
                }
                ImGui::Separator();
                if(g_sel_project >= 0 && ImGui::MenuItem("Edit Project")){
                    auto& p = g_projects[g_sel_project];
                    strncpy(g_proj_name, p.name.c_str(),        sizeof(g_proj_name)-1);
                    strncpy(g_proj_desc, p.description.c_str(), sizeof(g_proj_desc)-1);
                    g_show_edit_project = true;
                }
                if(g_sel_project >= 0 && !g_projects[g_sel_project].parts.empty())
                    if(ImGui::MenuItem("Print BOM")) export_bom_html(g_projects[g_sel_project]);
                if(g_sel_project < 0) ImGui::BeginDisabled();
                ImGui::PushStyleColor(ImGuiCol_Text, COL_RED);
                if(ImGui::MenuItem("Delete Project")) g_show_del_project = true;
                ImGui::PopStyleColor();
                if(g_sel_project < 0) ImGui::EndDisabled();
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Help")){
                if(ImGui::MenuItem("About")) g_show_about = true;
                ImGui::EndMenu();
            }

            // Status bar message (right-aligned)
            if(!g_status_msg.empty()){
                double age = glfwGetTime() - g_status_time;
                if(age > 8.0){
                    g_status_msg.clear();
                } else {
                    float alpha = (age > 5.0) ? static_cast<float>(1.0 - (age - 5.0) / 3.0) : 1.0f;
                    float sw = ImGui::CalcTextSize(g_status_msg.c_str()).x + 8;
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - sw + ImGui::GetCursorPosX());
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COL_RED.x, COL_RED.y, COL_RED.z, alpha));
                    ImGui::TextUnformatted(g_status_msg.c_str());
                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndMenuBar();
        }

        // ── Layout ──
        draw_project_list();
        ImGui::SameLine();

        // Draggable splitter handle
        {
            const float SPLITTER_W  = 4.0f;
            const float SIDEBAR_MIN = 140.0f;
            const float SIDEBAR_MAX = 480.0f;
            ImVec2 p = ImGui::GetCursorScreenPos();
            float  h = ImGui::GetContentRegionAvail().y;
            ImGui::InvisibleButton("##splitter", {SPLITTER_W, h});
            if(ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if(ImGui::IsItemActive())
                g_sidebar_w = std::clamp(g_sidebar_w + ImGui::GetIO().MouseDelta.x,
                                      SIDEBAR_MIN, SIDEBAR_MAX);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImU32 col = ImGui::IsItemHovered() || ImGui::IsItemActive()
                      ? IM_COL32(180,140,80,200) : IM_COL32(80,80,80,120);
            dl->AddRectFilled(p, {p.x + SPLITTER_W, p.y + h}, col);
            ImGui::SameLine();
        }

        ImGui::BeginGroup();
        draw_parts_panel();
        ImGui::EndGroup();

        // All modals live outside any child/menu context
        draw_modals();
        draw_import_md_modal();

        ImGui::End();

        ImGui::Render();
        int dw, dh;
        glfwGetFramebufferSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(COL_BG.x, COL_BG.y, COL_BG.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    sqlite3_close(g_db);
    return 0;
}
