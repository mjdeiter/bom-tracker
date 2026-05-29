// bom_tracker.cpp — Bill of Materials Tracker
// Dear ImGui + SQLite3, single-file build (like temp-watch / AeroMCP)
// Build: see build.sh

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <sqlite3.h>

#include <algorithm>
#include <filesystem>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

static const char* APP_VERSION = "1.0.0";
#ifndef BUILD_HASH
#define BUILD_HASH "dev"
#endif
static const char* BUILD_HASH_STR = BUILD_HASH;

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

enum PartStatus { STATUS_NEEDED=0, STATUS_ORDERED, STATUS_IN_STOCK, STATUS_INSTALLED, STATUS_COUNT };
static const char* STATUS_LABELS[STATUS_COUNT] = { "Needed", "Ordered", "In Stock", "Installed" };
static ImVec4 statusColor(PartStatus s){
    switch(s){
        case STATUS_NEEDED:    return COL_TEXT_DIM;
        case STATUS_ORDERED:   return COL_YELLOW;
        case STATUS_IN_STOCK:  return COL_GREEN;
        case STATUS_INSTALLED: return COL_BLUE;
        default:               return COL_TEXT;
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
};

struct Project {
    int         id = 0;
    std::string name;
    std::string description;
    std::vector<Part> parts;
};

static sqlite3* g_db = nullptr;
static std::string g_db_path;
static std::string g_status_msg;

static void db_exec(const char* sql){
    char* err = nullptr;
    sqlite3_exec(g_db, sql, nullptr, nullptr, &err);
    if(err){ g_status_msg = std::string("DB error: ") + err; sqlite3_free(err); }
}

static void db_init(){
    const char* home = getenv("HOME");
    std::string dir  = std::string(home) + "/.local/share/bom-tracker";
    std::filesystem::create_directories(dir);
    g_db_path = dir + "/bom.db";
    sqlite3_open(g_db_path.c_str(), &g_db);
    db_exec(R"(
        CREATE TABLE IF NOT EXISTS projects (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT NOT NULL,
            description TEXT NOT NULL DEFAULT ''
        );
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
        PRAGMA foreign_keys = ON;
    )");
}

static std::vector<Project> g_projects;

static void db_load(){
    g_projects.clear();
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(g_db, "SELECT id,name,description FROM projects ORDER BY name", -1, &stmt, nullptr);
    while(sqlite3_step(stmt) == SQLITE_ROW){
        Project p;
        p.id          = sqlite3_column_int(stmt, 0);
        p.name        = (const char*)sqlite3_column_text(stmt, 1);
        p.description = (const char*)sqlite3_column_text(stmt, 2);
        g_projects.push_back(std::move(p));
    }
    sqlite3_finalize(stmt);
    sqlite3_prepare_v2(g_db,
        "SELECT id,project_id,name,url,part_number,vendor,notes,quantity,unit_price,status "
        "FROM parts ORDER BY name", -1, &stmt, nullptr);
    while(sqlite3_step(stmt) == SQLITE_ROW){
        Part pt;
        pt.id          = sqlite3_column_int(stmt, 0);
        pt.project_id  = sqlite3_column_int(stmt, 1);
        pt.name        = (const char*)sqlite3_column_text(stmt, 2);
        pt.url         = (const char*)sqlite3_column_text(stmt, 3);
        pt.part_number = (const char*)sqlite3_column_text(stmt, 4);
        pt.vendor      = (const char*)sqlite3_column_text(stmt, 5);
        pt.notes       = (const char*)sqlite3_column_text(stmt, 6);
        pt.quantity    = sqlite3_column_int(stmt, 7);
        pt.unit_price  = sqlite3_column_double(stmt, 8);
        pt.status      = (PartStatus)sqlite3_column_int(stmt, 9);
        for(auto& proj : g_projects)
            if(proj.id == pt.project_id){ proj.parts.push_back(std::move(pt)); break; }
    }
    sqlite3_finalize(stmt);
}

static int db_insert_project(const std::string& name, const std::string& desc){
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(g_db, "INSERT INTO projects(name,description) VALUES(?,?)", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, desc.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
    return (int)sqlite3_last_insert_rowid(g_db);
}

static void db_update_project(int id, const std::string& name, const std::string& desc){
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(g_db, "UPDATE projects SET name=?,description=? WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, desc.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, id);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
}

static void db_delete_project(int id){
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(g_db, "DELETE FROM projects WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
}

static int db_insert_part(const Part& pt){
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(g_db,
        "INSERT INTO parts(project_id,name,url,part_number,vendor,notes,quantity,unit_price,status)"
        " VALUES(?,?,?,?,?,?,?,?,?)", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, pt.project_id);
    sqlite3_bind_text(stmt, 2, pt.name.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, pt.url.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, pt.part_number.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, pt.vendor.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, pt.notes.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, pt.quantity);
    sqlite3_bind_double(stmt, 8, pt.unit_price);
    sqlite3_bind_int(stmt, 9, (int)pt.status);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
    return (int)sqlite3_last_insert_rowid(g_db);
}

static void db_update_part(const Part& pt){
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(g_db,
        "UPDATE parts SET name=?,url=?,part_number=?,vendor=?,notes=?,quantity=?,unit_price=?,status=?"
        " WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, pt.name.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, pt.url.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, pt.part_number.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, pt.vendor.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, pt.notes.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, pt.quantity);
    sqlite3_bind_double(stmt, 7, pt.unit_price);
    sqlite3_bind_int(stmt, 8, (int)pt.status);
    sqlite3_bind_int(stmt, 9, pt.id);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
}

static void db_delete_part(int id){
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(g_db, "DELETE FROM parts WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt); sqlite3_finalize(stmt);
}

static void open_url(const std::string& url){
    if(url.empty()) return;
    std::string cmd = "xdg-open \"" + url + "\" &";
    system(cmd.c_str());
}

static std::string format_price(double p){
    if(p <= 0.0) return "\xe2\x80\x94";
    std::ostringstream ss;
    ss << "$" << std::fixed << std::setprecision(2) << p;
    return ss.str();
}

static int    g_sel_project      = -1;
static int    g_sel_part         = -1;
static int    g_sel_part_id      = -1;  // tracks selection across db_load()
static void resolve_sel_part(){
    g_sel_part = -1;
    if(g_sel_part_id < 0 || g_sel_project < 0 || g_sel_project >= (int)g_projects.size()) return;
    auto& parts = g_projects[g_sel_project].parts;
    for(int i=0;i<(int)parts.size();i++)
        if(parts[i].id == g_sel_part_id){ g_sel_part = i; return; }
}
static char   g_proj_name[256]   = {};
static char   g_proj_desc[512]   = {};
static char   g_part_name[256]   = {};
static char   g_part_url[1024]   = {};
static char   g_part_pn[256]     = {};
static char   g_part_vendor[256] = {};
static char   g_part_notes[1024] = {};
static int    g_part_qty         = 1;
static float  g_part_price       = 0.0f;
static int    g_part_status      = 0;
static bool   g_show_add_project  = false;
static bool   g_show_edit_project = false;
static bool   g_show_del_project  = false;
static bool   g_show_add_part     = false;
static bool   g_show_edit_part    = false;
static bool   g_show_del_part     = false;
static bool   g_show_about        = false;
static char   g_search[256] = {};

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

static bool begin_modal(const char* id, ImVec2 size={420,0}){
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f,0.5f});
    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    return ImGui::BeginPopupModal(id, nullptr, 0);
}

static double project_total(const Project& p){
    double t = 0;
    for(auto& pt : p.parts) t += pt.unit_price * pt.quantity;
    return t;
}

static void draw_project_list(){
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(COL_PANEL.x, COL_PANEL.y, COL_PANEL.z, 1.0f));
    ImGui::BeginChild("##proj_panel", {220, 0}, true);
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
    for(int i = 0; i < (int)g_projects.size(); i++){
        auto& proj = g_projects[i];
        bool selected = (g_sel_project == i);
        if(selected) ImGui::PushStyleColor(ImGuiCol_Header,        COL_SEL_BG);
        if(selected) ImGui::PushStyleColor(ImGuiCol_HeaderHovered, COL_SEL_BG);
        std::string label = proj.name + " (" + std::to_string(proj.parts.size()) + ")##proj" + std::to_string(i);
        if(ImGui::Selectable(label.c_str(), selected)){
            g_sel_project = i;
            g_sel_part    = -1;
            g_sel_part_id = -1;
        }
        if(selected){ ImGui::PopStyleColor(2); }
        if(ImGui::BeginPopupContextItem(("##ctx_proj" + std::to_string(i)).c_str())){
            g_sel_project = i;
            if(ImGui::MenuItem("Edit Project")){
                strncpy(g_proj_name, proj.name.c_str(), sizeof(g_proj_name)-1);
                strncpy(g_proj_desc, proj.description.c_str(), sizeof(g_proj_desc)-1);
                g_show_edit_project = true;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, COL_RED);
            if(ImGui::MenuItem("Delete Project")){
                g_show_del_project = true;
            }
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void draw_parts_panel(){
    if(g_sel_project < 0 || g_sel_project >= (int)g_projects.size()){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 60);
        float w = ImGui::GetContentRegionAvail().x;
        const char* msg = "<- Select a project";
        ImGui::SetCursorPosX((w - ImGui::CalcTextSize(msg).x) * 0.5f);
        ImGui::TextUnformatted(msg);
        ImGui::PopStyleColor();
        return;
    }
    Project& proj = g_projects[g_sel_project];
    ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
    ImGui::Text("%s", proj.name.c_str());
    ImGui::PopStyleColor();
    if(!proj.description.empty()){
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::Text("  --  %s", proj.description.c_str());
        ImGui::PopStyleColor();
    }
    double total = project_total(proj);
    std::string cost_str = "Total: " + format_price(total) +
                           "   Parts: " + std::to_string(proj.parts.size());
    float rw = ImGui::CalcTextSize(cost_str.c_str()).x;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - rw + ImGui::GetCursorPosX() - 8);
    ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN);
    ImGui::TextUnformatted(cost_str.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

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
    bool has_sel = (g_sel_part >= 0 && g_sel_part < (int)proj.parts.size());
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
        g_part_price  = (float)p.unit_price;
        g_part_status = (int)p.status;
        g_show_edit_part = true;
    }
    pop_accent_style();
    ImGui::SameLine();
    push_danger_style();
    if(ImGui::Button("Delete") && has_sel){
        g_show_del_part = true;
    }
    pop_danger_style();
    if(!has_sel) ImGui::EndDisabled();
    ImGui::SameLine();
    bool has_url = has_sel && !proj.parts[g_sel_part].url.empty();
    if(!has_url) ImGui::BeginDisabled();
    if(ImGui::Button("Open URL") && has_url) open_url(proj.parts[g_sel_part].url);
    if(!has_url) ImGui::EndDisabled();
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200 + ImGui::GetCursorPosX() - 8);
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##search", "Search parts...", g_search, sizeof(g_search));
    ImGui::Spacing();

    static ImGuiTableFlags tflags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_SizingStretchProp;
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,    COL_HEADER_BG);
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight, COL_BORDER);
    float table_height = ImGui::GetContentRegionAvail().y - 6;
    if(ImGui::BeginTable("##parts", 8, tflags, {0, table_height})){
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Part Name",  ImGuiTableColumnFlags_DefaultSort, 160);
        ImGui::TableSetupColumn("Part #",     0, 90);
        ImGui::TableSetupColumn("Vendor",     0, 90);
        ImGui::TableSetupColumn("Qty",        0, 40);
        ImGui::TableSetupColumn("Unit Price", 0, 80);
        ImGui::TableSetupColumn("Total",      0, 80);
        ImGui::TableSetupColumn("Status",     0, 80);
        ImGui::TableSetupColumn("Notes",      0, 140);
        ImGui::TableHeadersRow();

        // --- Sorting ---
        static int s_sort_col = 0;
        static bool s_sort_asc = true;
        if(ImGuiTableSortSpecs* ss = ImGui::TableGetSortSpecs()){
            if(ss->SpecsDirty && ss->SpecsCount > 0){
                s_sort_col = ss->Specs[0].ColumnIndex;
                s_sort_asc = (ss->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
                ss->SpecsDirty = false;
            }
        }
        std::vector<int> order(proj.parts.size());
        for(int i=0;i<(int)order.size();i++) order[i]=i;
        std::stable_sort(order.begin(), order.end(), [&](int a, int b){
            const Part& pa = proj.parts[a];
            const Part& pb = proj.parts[b];
            int cmp = 0;
            switch(s_sort_col){
                case 0: cmp = pa.name.compare(pb.name); break;
                case 1: cmp = pa.part_number.compare(pb.part_number); break;
                case 2: cmp = pa.vendor.compare(pb.vendor); break;
                case 3: cmp = (pa.quantity - pb.quantity); break;
                case 4: cmp = (pa.unit_price < pb.unit_price) ? -1 : (pa.unit_price > pb.unit_price) ? 1 : 0; break;
                case 5: { double ta=pa.unit_price*pa.quantity, tb=pb.unit_price*pb.quantity; cmp=(ta<tb)?-1:(ta>tb)?1:0; break; }
                case 6: cmp = ((int)pa.status - (int)pb.status); break;
                case 7: cmp = pa.notes.compare(pb.notes); break;
                default: break;
            }
            return s_sort_asc ? cmp < 0 : cmp > 0;
        });

        std::string search_lower(g_search);
        std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
        for(int oi = 0; oi < (int)order.size(); oi++){
            int i = order[oi];
            Part& pt = proj.parts[i];
            if(!search_lower.empty()){
                std::string hay = pt.name + " " + pt.part_number + " " + pt.vendor + " " + pt.notes;
                std::transform(hay.begin(), hay.end(), hay.begin(), ::tolower);
                if(hay.find(search_lower) == std::string::npos) continue;
            }
            ImGui::TableNextRow();
            bool row_sel = (g_sel_part == i);
            if(row_sel) ImGui::PushStyleColor(ImGuiCol_TableRowBg, COL_SEL_BG);
            ImGui::TableSetColumnIndex(0);
            bool has_link = !pt.url.empty();
            if(has_link) ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
            std::string sl = pt.name + "##row" + std::to_string(i);
            if(ImGui::Selectable(sl.c_str(), row_sel, ImGuiSelectableFlags_SpanAllColumns))
                g_sel_part = (g_sel_part == i) ? -1 : i; g_sel_part_id = (g_sel_part < 0) ? -1 : proj.parts[i].id;
            if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && has_link)
                open_url(pt.url);
            if(has_link){
                ImGui::PopStyleColor();
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("Double-click to open:\n%s", pt.url.c_str());
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
            ImGui::TextUnformatted(pt.part_number.empty() ? "-" : pt.part_number.c_str());
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(pt.vendor.empty() ? "-" : pt.vendor.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", pt.quantity);
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(format_price(pt.unit_price).c_str());
            ImGui::TableSetColumnIndex(5);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN);
            ImGui::TextUnformatted(format_price(pt.unit_price * pt.quantity).c_str());
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(6);
            ImGui::PushStyleColor(ImGuiCol_Text, statusColor(pt.status));
            ImGui::TextUnformatted(STATUS_LABELS[pt.status]);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(7);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
            if(!pt.notes.empty()){
                std::string disp = pt.notes.size() > 60 ? pt.notes.substr(0,57) + "..." : pt.notes;
                ImGui::TextUnformatted(disp.c_str());
                if(pt.notes.size() > 60 && ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", pt.notes.c_str());
            } else { ImGui::TextUnformatted("-"); }
            ImGui::PopStyleColor();
            if(row_sel) ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleColor(2);
}

static void draw_part_form(){
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM); ImGui::TextUnformatted("Part Name *"); ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1); ImGui::InputText("##pname", g_part_name, sizeof(g_part_name));
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM); ImGui::TextUnformatted("Web Link"); ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1); ImGui::InputText("##purl", g_part_url, sizeof(g_part_url));
    float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM); ImGui::TextUnformatted("Part Number / SKU"); ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(half); ImGui::InputText("##ppn", g_part_pn, sizeof(g_part_pn));
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM); ImGui::TextUnformatted("Vendor / Supplier"); ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1); ImGui::InputText("##pvendor", g_part_vendor, sizeof(g_part_vendor));
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM); ImGui::TextUnformatted("Quantity"); ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(half); ImGui::InputInt("##pqty", &g_part_qty);
    if(g_part_qty < 1) g_part_qty = 1;
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM); ImGui::TextUnformatted("Unit Price ($)"); ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1); ImGui::InputFloat("##pprice", &g_part_price, 0.01f, 1.0f, "%.2f");
    if(g_part_price < 0) g_part_price = 0;
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM); ImGui::TextUnformatted("Status"); ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1); ImGui::Combo("##pstatus", &g_part_status, STATUS_LABELS, STATUS_COUNT);
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM); ImGui::TextUnformatted("Notes"); ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1); ImGui::InputTextMultiline("##pnotes", g_part_notes, sizeof(g_part_notes), {0, 60});
}

static void draw_modals(){
    // Fire OpenPopup on the frame the flag goes true, then clear it.
    // This prevents reopening every frame while the popup is open.
    if(g_show_add_project)  { ImGui::OpenPopup("Add Project");     g_show_add_project  = false; }
    if(g_show_edit_project) { ImGui::OpenPopup("Edit Project");    g_show_edit_project = false; }
    if(g_show_del_project)  { ImGui::OpenPopup("Delete Project?"); g_show_del_project  = false; }
    if(g_show_add_part)     { ImGui::OpenPopup("Add Part");        g_show_add_part     = false; }
    if(g_show_edit_part)    { ImGui::OpenPopup("Edit Part");       g_show_edit_part    = false; }
    if(g_show_del_part)     { ImGui::OpenPopup("Delete Part?");    g_show_del_part     = false; }
    if(g_show_about)        { ImGui::OpenPopup("About##dlg");      g_show_about        = false; }

    if(begin_modal("Add Project")){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT); ImGui::TextUnformatted("NEW PROJECT"); ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::TextUnformatted("Name *"); ImGui::SetNextItemWidth(-1); ImGui::InputText("##aname", g_proj_name, sizeof(g_proj_name));
        ImGui::TextUnformatted("Description"); ImGui::SetNextItemWidth(-1); ImGui::InputText("##adesc", g_proj_desc, sizeof(g_proj_desc));
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        push_accent_style();
        if(ImGui::Button("Create", {120,0}) && g_proj_name[0]){
            int new_id = db_insert_project(g_proj_name, g_proj_desc);
            db_load(); resolve_sel_part();
            for(int i=0;i<(int)g_projects.size();i++)
                if(g_projects[i].id==new_id){ g_sel_project=i; break; }
            ImGui::CloseCurrentPopup();
        }
        pop_accent_style(); ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if(begin_modal("Edit Project")){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT); ImGui::TextUnformatted("EDIT PROJECT"); ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::TextUnformatted("Name *"); ImGui::SetNextItemWidth(-1); ImGui::InputText("##ename", g_proj_name, sizeof(g_proj_name));
        ImGui::TextUnformatted("Description"); ImGui::SetNextItemWidth(-1); ImGui::InputText("##edesc", g_proj_desc, sizeof(g_proj_desc));
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        push_accent_style();
        if(ImGui::Button("Save", {120,0}) && g_proj_name[0] && g_sel_project >= 0){
            db_update_project(g_projects[g_sel_project].id, g_proj_name, g_proj_desc);
            db_load(); resolve_sel_part(); g_show_edit_project = false; ImGui::CloseCurrentPopup();
        }
        pop_accent_style(); ImGui::SameLine();
        if(ImGui::Button("Cancel", {80,0})){ g_show_edit_project=false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if(begin_modal("Delete Project?", {380,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_RED); ImGui::TextUnformatted("DELETE PROJECT"); ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        if(g_sel_project >= 0 && g_sel_project < (int)g_projects.size())
            ImGui::TextWrapped("Delete \"%s\" and ALL its parts? This cannot be undone.", g_projects[g_sel_project].name.c_str());
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        push_danger_style();
        if(ImGui::Button("Delete", {120,0}) && g_sel_project >= 0){
            db_delete_project(g_projects[g_sel_project].id);
            g_sel_project = -1; g_sel_part = -1; g_sel_part_id = -1; db_load(); resolve_sel_part();
            g_show_del_project = false; ImGui::CloseCurrentPopup();
        }
        pop_danger_style(); ImGui::SameLine();
        if(ImGui::Button("Cancel",{80,0})){ g_show_del_project=false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if(begin_modal("Add Part", {460,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT); ImGui::TextUnformatted("ADD PART"); ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        draw_part_form();
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        push_accent_style();
        if(ImGui::Button("Add", {120,0}) && g_part_name[0] && g_sel_project >= 0){
            Part pt;
            pt.project_id  = g_projects[g_sel_project].id;
            pt.name        = g_part_name; pt.url         = g_part_url;
            pt.part_number = g_part_pn;   pt.vendor      = g_part_vendor;
            pt.notes       = g_part_notes; pt.quantity   = g_part_qty;
            pt.unit_price  = (double)g_part_price; pt.status = (PartStatus)g_part_status;
            db_insert_part(pt); db_load(); resolve_sel_part();
            g_show_add_part = false; ImGui::CloseCurrentPopup();
        }
        pop_accent_style(); ImGui::SameLine();
        if(ImGui::Button("Cancel",{80,0})){ g_show_add_part=false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if(begin_modal("Edit Part", {460,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT); ImGui::TextUnformatted("EDIT PART"); ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        draw_part_form();
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        push_accent_style();
        if(ImGui::Button("Save", {120,0}) && g_part_name[0] && g_sel_project >= 0 && g_sel_part >= 0){
            Part& ex = g_projects[g_sel_project].parts[g_sel_part];
            ex.name=g_part_name; ex.url=g_part_url; ex.part_number=g_part_pn;
            ex.vendor=g_part_vendor; ex.notes=g_part_notes; ex.quantity=g_part_qty;
            ex.unit_price=(double)g_part_price; ex.status=(PartStatus)g_part_status;
            db_update_part(ex); db_load(); resolve_sel_part();
            g_show_edit_part = false; ImGui::CloseCurrentPopup();
        }
        pop_accent_style(); ImGui::SameLine();
        if(ImGui::Button("Cancel",{80,0})){ g_show_edit_part=false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if(begin_modal("Delete Part?", {380,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_RED); ImGui::TextUnformatted("DELETE PART"); ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        if(g_sel_project >= 0 && g_sel_part >= 0 && g_sel_part < (int)g_projects[g_sel_project].parts.size())
            ImGui::Text("Delete \"%s\"? This cannot be undone.", g_projects[g_sel_project].parts[g_sel_part].name.c_str());
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        push_danger_style();
        if(ImGui::Button("Delete",{120,0}) && g_sel_project >= 0 && g_sel_part >= 0){
            db_delete_part(g_projects[g_sel_project].parts[g_sel_part].id);
            g_sel_part = -1; g_sel_part_id = -1; db_load(); resolve_sel_part();
            g_show_del_part = false; ImGui::CloseCurrentPopup();
        }
        pop_danger_style(); ImGui::SameLine();
        if(ImGui::Button("Cancel",{80,0})){ g_show_del_part=false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if(begin_modal("About##dlg", {420,0})){
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT); ImGui::TextUnformatted("BOM TRACKER"); ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::Text("Version: %s (%s)", APP_VERSION, BUILD_HASH_STR);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::TextUnformatted("Source / releases:");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        static const char* repo_url = "https://github.com/mjdeiter/bom-tracker";
        ImGui::TextUnformatted(repo_url);
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Click to copy");
        if(ImGui::IsItemClicked()) ImGui::SetClipboardText(repo_url);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::TextUnformatted("Database:");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextUnformatted(g_db_path.c_str());
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Click to copy");
        if(ImGui::IsItemClicked()) ImGui::SetClipboardText(g_db_path.c_str());
        ImGui::Spacing();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::TextWrapped("Bill of Materials tracker built with Dear ImGui + SQLite3. Changes save automatically. Double-click a part row to open its URL.");
        ImGui::PopStyleColor();
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        push_accent_style();
        if(ImGui::Button("Close",{80,0})){ g_show_about=false; ImGui::CloseCurrentPopup(); }
        pop_accent_style();
        ImGui::EndPopup();
    }
}

static void apply_theme(){
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding=4; s.ChildRounding=4; s.FrameRounding=3; s.PopupRounding=4;
    s.ScrollbarRounding=4; s.GrabRounding=3; s.TabRounding=3;
    s.FramePadding={6,4}; s.ItemSpacing={8,5}; s.WindowPadding={10,10};
    s.ScrollbarSize=12; s.IndentSpacing=18; s.SeparatorTextBorderSize=1;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]          = COL_BG;
    c[ImGuiCol_ChildBg]           = COL_PANEL;
    c[ImGuiCol_PopupBg]           = {0.12f,0.13f,0.16f,0.98f};
    c[ImGuiCol_Border]            = COL_BORDER;
    c[ImGuiCol_FrameBg]           = {0.16f,0.18f,0.21f,1.0f};
    c[ImGuiCol_FrameBgHovered]    = {0.20f,0.22f,0.26f,1.0f};
    c[ImGuiCol_FrameBgActive]     = {0.22f,0.24f,0.28f,1.0f};
    c[ImGuiCol_TitleBg]           = {0.09f,0.10f,0.12f,1.0f};
    c[ImGuiCol_TitleBgActive]     = {0.12f,0.13f,0.16f,1.0f};
    c[ImGuiCol_MenuBarBg]         = {0.11f,0.12f,0.14f,1.0f};
    c[ImGuiCol_ScrollbarBg]       = {0.10f,0.11f,0.13f,1.0f};
    c[ImGuiCol_ScrollbarGrab]     = {0.30f,0.32f,0.36f,1.0f};
    c[ImGuiCol_ScrollbarGrabHovered] = COL_ACCENT_DIM;
    c[ImGuiCol_ScrollbarGrabActive]  = COL_ACCENT;
    c[ImGuiCol_CheckMark]         = COL_ACCENT;
    c[ImGuiCol_SliderGrab]        = COL_ACCENT_DIM;
    c[ImGuiCol_SliderGrabActive]  = COL_ACCENT;
    c[ImGuiCol_Button]            = {0.20f,0.22f,0.26f,1.0f};
    c[ImGuiCol_ButtonHovered]     = {0.26f,0.28f,0.33f,1.0f};
    c[ImGuiCol_ButtonActive]      = COL_ACCENT_DIM;
    c[ImGuiCol_Header]            = COL_SEL_BG;
    c[ImGuiCol_HeaderHovered]     = {0.22f,0.18f,0.04f,1.0f};
    c[ImGuiCol_HeaderActive]      = COL_SEL_BG;
    c[ImGuiCol_Tab]               = {0.14f,0.16f,0.19f,1.0f};
    c[ImGuiCol_TabHovered]        = COL_ACCENT_DIM;
    c[ImGuiCol_TabActive]         = COL_ACCENT_DIM;
    c[ImGuiCol_TableHeaderBg]     = COL_HEADER_BG;
    c[ImGuiCol_TableBorderStrong] = COL_BORDER;
    c[ImGuiCol_TableBorderLight]  = {0.18f,0.20f,0.23f,1.0f};
    c[ImGuiCol_TableRowBg]        = COL_BG;
    c[ImGuiCol_TableRowBgAlt]     = COL_ROW_ALT;
    c[ImGuiCol_Separator]         = COL_BORDER;
    c[ImGuiCol_Text]              = COL_TEXT;
    c[ImGuiCol_TextDisabled]      = COL_TEXT_DIM;
    c[ImGuiCol_ModalWindowDimBg]  = {0.0f,0.0f,0.0f,0.55f};
}

int main(){
    db_init();
    db_load(); resolve_sel_part();
    if(!glfwInit()) return 1;
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    std::string win_title = std::string("BOM Tracker v") + APP_VERSION;
    GLFWwindow* window = glfwCreateWindow(1100, 680, win_title.c_str(), nullptr, nullptr);
    if(!window){ glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    {
        const char* fonts[] = {
            "/usr/share/fonts/TTF/MesloLGMNerdFontMono-Regular.ttf",
            "/usr/share/fonts/TTF/MesloLGSNerdFontMono-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            nullptr
        };
        for(int fi = 0; fonts[fi]; fi++){
            if(access(fonts[fi], R_OK) == 0){
                io.Fonts->AddFontFromFileTTF(fonts[fi], 14.0f);
                break;
            }
        }
    }
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    apply_theme();
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("##main", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);
        if(ImGui::BeginMenuBar()){
            ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
            ImGui::TextUnformatted("BOM TRACKER");
            ImGui::PopStyleColor();
            ImGui::Separator();
            if(ImGui::BeginMenu("Project")){
                if(ImGui::MenuItem("New Project","Ctrl+N")){
                    memset(g_proj_name,0,sizeof(g_proj_name));
                    memset(g_proj_desc,0,sizeof(g_proj_desc));
                    ImGui::OpenPopup("Add Project");
                }
                if(g_sel_project>=0 && ImGui::MenuItem("Edit Project")){
                    auto& p=g_projects[g_sel_project];
                    strncpy(g_proj_name,p.name.c_str(),sizeof(g_proj_name)-1);
                    strncpy(g_proj_desc,p.description.c_str(),sizeof(g_proj_desc)-1);
                    g_show_edit_project=true;
                }
                if(g_sel_project>=0){
                    ImGui::PushStyleColor(ImGuiCol_Text, COL_RED);
                    if(ImGui::MenuItem("Delete Project")){
                        g_show_del_project=true;
                    }
                    ImGui::PopStyleColor();
                }
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Help")){
                if(ImGui::MenuItem("About")){ g_show_about=true; ImGui::OpenPopup("About##dlg"); }
                ImGui::EndMenu();
            }
            if(!g_status_msg.empty()){
                float sw = ImGui::CalcTextSize(g_status_msg.c_str()).x;
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - sw + ImGui::GetCursorPosX() - 8);
                ImGui::PushStyleColor(ImGuiCol_Text, COL_RED);
                ImGui::TextUnformatted(g_status_msg.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::EndMenuBar();
        }
        draw_project_list();
        ImGui::SameLine();
        ImGui::BeginGroup();
        draw_parts_panel();
        ImGui::EndGroup();
        draw_modals();
        ImGui::End();
        ImGui::Render();
        int dw,dh; glfwGetFramebufferSize(window,&dw,&dh);
        glViewport(0,0,dw,dh);
        glClearColor(COL_BG.x,COL_BG.y,COL_BG.z,1.0f);
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
