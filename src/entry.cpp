#include <Windows.h>

#include <Nexus.h>

#include <imgui.h>

#include <atomic>
#include <filesystem>



#include <algorithm>



#include <chrono>

#include <cmath>

#include <cstdint>

#include <cstdio>

#include <cstring>

#include <ctime>



#include <iterator>

#include <map>

#include <string>

#include <tuple>

#include <unordered_map>

#include <utility>

#include <vector>



#include "events_generated.h"



static AddonAPI_t* g_API = nullptr;

static AddonDefinition_t g_AddonDef{};





static bool g_ShowWindow = true;

static bool g_ShowCategoryHeaders = false;

static bool g_ShowTrackLabels = false;

static bool g_ShowHiddenTracks = false;

static int g_RangeIndex = 1; // 4h

static float g_CurrentPosition = 0.5f;



static std::atomic<ImFont*> g_ChineseFont{nullptr};
static ImFontConfig g_ChineseFontConfig{};
static ImVector<ImWchar> g_ChineseGlyphRanges;
static std::string g_ChineseFontPath;



struct Occurrence {

    int track_index = 0;

    const EventSchedule* schedule = nullptr;

    std::int64_t start = 0;

    std::int64_t end = 0;

};



struct TrackInfo {

    int track_index = 0;

    std::string category;

    std::string name;

    bool visible = true;

    float height = 20.0f;

    std::vector<const EventSchedule*> schedules;

};



static std::int64_t UnixNow() {

    return std::chrono::duration_cast<std::chrono::seconds>(

        std::chrono::system_clock::now().time_since_epoch()).count();

}



static std::int64_t FloorMod(std::int64_t a, std::int64_t b) {

    const std::int64_t r = a % b;

    return r < 0 ? r + b : r;

}



// Same base used by the source Event Timers schedule data.

static std::int64_t LocalDayStartBase(std::int64_t now) {

    constexpr std::int64_t kDay = 86400;

    constexpr std::int64_t kOffset = -3 * 3600;

    return now - FloorMod(now + kOffset, kDay);

}



static std::int64_t TyriaCycleBase(std::int64_t now) {

    constexpr std::int64_t kReference = 1759262400;

    constexpr std::int64_t kCycle = 7200;

    const std::int64_t elapsed = now - kReference;

    const std::int64_t cycles = elapsed >= 0 ? elapsed / kCycle : -((-elapsed + kCycle - 1) / kCycle);

    return kReference + cycles * kCycle;

}



static std::int64_t BaseFor(const char* calculator, std::int64_t now) {

    if (std::strcmp(calculator, "tyria_cycle") == 0 || std::strcmp(calculator, "cantha_cycle") == 0)

        return TyriaCycleBase(now);

    return LocalDayStartBase(now);

}



static int CycleMinutesFor(const char* calculator) {

    if (std::strcmp(calculator, "tyria_cycle") == 0 || std::strcmp(calculator, "cantha_cycle") == 0)

        return 120;

    return 1440;

}



static const std::vector<TrackInfo>& Tracks() {

    static const std::vector<TrackInfo> tracks = [] {

        std::map<int, TrackInfo> byId;

        for (std::size_t i = 0; i < kEventScheduleCount; ++i) {

            const auto& s = kEventSchedules[i];

            auto& t = byId[s.track_index];

            t.track_index = s.track_index;

            t.category = s.category;

            t.name = s.track;

            t.visible = s.visible;

            t.height = s.height;

            t.schedules.push_back(&s);

        }

        std::vector<TrackInfo> out;

        out.reserve(byId.size());

        for (auto& [_, t] : byId) out.push_back(std::move(t));

        return out;

    }();

    return tracks;

}



static void GenerateOccurrences(const EventSchedule& s,

                                std::int64_t now,

                                std::int64_t windowStart,

                                std::int64_t windowEnd,

                                std::vector<Occurrence>& out) {

    const int cycleMinutes = CycleMinutesFor(s.calculator);

    const std::int64_t cycleSeconds = static_cast<std::int64_t>(cycleMinutes) * 60;

    const std::int64_t base0 = BaseFor(s.calculator, now);

    const std::int64_t duration = std::max(1, s.duration_minutes) * 60LL;



    // Three neighboring cycles are enough for the supported <= 8h timeline windows.

    for (int delta = -2; delta <= 2; ++delta) {

        const std::int64_t base = base0 + static_cast<std::int64_t>(delta) * cycleSeconds;

        if (s.interval_minutes <= 0) {

            const std::int64_t start = base + static_cast<std::int64_t>(s.offset_minutes) * 60;

            const std::int64_t end = start + duration;

            if (end > windowStart && start < windowEnd)

                out.push_back({s.track_index, &s, start, end});

            continue;

        }



        const int phase = static_cast<int>(FloorMod(s.offset_minutes, s.interval_minutes));

        for (int minute = phase; minute < cycleMinutes; minute += s.interval_minutes) {

            const std::int64_t start = base + static_cast<std::int64_t>(minute) * 60;

            const std::int64_t end = start + duration;

            if (end > windowStart && start < windowEnd)

                out.push_back({s.track_index, &s, start, end});

        }

    }

}



static std::string FormatTime(std::int64_t ts) {

    std::time_t t = static_cast<std::time_t>(ts);

    std::tm tm{};

    localtime_s(&tm, &t);

    char buf[16]{};

    std::strftime(buf, sizeof(buf), "%H:%M", &tm);

    return buf;

}



static std::string FormatCountdown(std::int64_t seconds) {

    if (seconds < 0) seconds = 0;

    const long long h = seconds / 3600;

    const long long m = (seconds % 3600) / 60;

    char buf[64]{};

    if (h > 0) std::snprintf(buf, sizeof(buf), "%lld小时%02lld分", h, m);

    else std::snprintf(buf, sizeof(buf), "%lld分钟", m);

    return buf;

}



static ImU32 ToU32(float r, float g, float b, float a) {

    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));

}



static ImU32 TextColorFor(float r, float g, float b) {

    const float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;

    return lum > 0.58f ? IM_COL32(25, 25, 25, 255) : IM_COL32(245, 245, 245, 255);

}













static void OnChineseFontReceived(const char*, void* font)
{
    g_ChineseFont.store(static_cast<ImFont*>(font), std::memory_order_release);
}

static std::filesystem::path GameRoot()
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return {};
    return std::filesystem::path(buffer).parent_path();
}

static void BuildChineseGlyphRanges()
{
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesDefault());

    // UI text used by this addon.
    builder.AddText(u8"事件计时器显示范围分类标题轨道名称显示隐藏轨道未加载中文字体进行中剩余距离开始小时分钟当前时间界面按时间轴样式重做顶部时间刻度横向彩色事件条红色当前时间线数据内置自中文字体");

    // Event data: add every character actually used by the generated schedule.
    for (std::size_t i = 0; i < kEventScheduleCount; ++i) {
        const auto& e = kEventSchedules[i];
        if (e.name)     builder.AddText(e.name);
        if (e.category) builder.AddText(e.category);
        if (e.track)    builder.AddText(e.track);
    }

    g_ChineseGlyphRanges.clear();
    builder.BuildRanges(&g_ChineseGlyphRanges);
}

static void LoadChineseFont()
{
    if (!g_API || !g_API->Fonts_AddFromFile)
        return;

    const auto fontPath = GameRoot() / L"addons" / L"Nexus" / L"Fonts" / L"SarasaUiSC-Regular.ttf";
    if (!std::filesystem::exists(fontPath))
        return;

    BuildChineseGlyphRanges();

    g_ChineseFontConfig = ImFontConfig();
    g_ChineseFontConfig.OversampleH = 1;
    g_ChineseFontConfig.OversampleV = 1;
    g_ChineseFontConfig.GlyphRanges = g_ChineseGlyphRanges.Data;

    g_ChineseFontPath = fontPath.u8string();

    g_API->Fonts_AddFromFile(
        "EventTimerCN_Sarasa",
        18.0f,
        g_ChineseFontPath.c_str(),
        OnChineseFontReceived,
        &g_ChineseFontConfig
    );
}

static bool PushChineseFont()
{
    ImFont* font = g_ChineseFont.load(std::memory_order_acquire);
    if (!font)
        return false;

    ImGui::PushFont(font);
    return true;
}

static void RenderTimeRuler(ImDrawList* dl, ImVec2 min, ImVec2 max,

                            std::int64_t windowStart, std::int64_t windowEnd) {

    const float width = max.x - min.x;

    const std::int64_t span = windowEnd - windowStart;

    dl->AddRectFilled(min, max, IM_COL32(67, 67, 67, 255));

    dl->AddRect(min, max, IM_COL32(15, 15, 15, 255));



    constexpr std::int64_t step = 5 * 60;

    std::int64_t tick = ((windowStart + step - 1) / step) * step;

    for (; tick <= windowEnd; tick += step) {

        const float x = min.x + static_cast<float>(tick - windowStart) / static_cast<float>(span) * width;

        dl->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), IM_COL32(95, 95, 95, 255), 1.0f);

        const std::string label = FormatTime(tick);

        const ImVec2 ts = ImGui::CalcTextSize(label.c_str());

        dl->AddText(ImVec2(x - ts.x * 0.5f, min.y + 2.0f), IM_COL32_WHITE, label.c_str());

    }

}



static void RenderTrackRow(const TrackInfo& track,

                           const std::vector<Occurrence>& occs,

                           std::int64_t now,

                           std::int64_t windowStart,

                           std::int64_t windowEnd,

                           float labelWidth) {

    const float rowHeight = std::max(20.0f, track.height);

    const ImVec2 rowPos = ImGui::GetCursorScreenPos();

    const float fullWidth = ImGui::GetContentRegionAvail().x;

    const float timelineX = rowPos.x + labelWidth;

    const float timelineWidth = std::max(1.0f, fullWidth - labelWidth);

    const float span = static_cast<float>(windowEnd - windowStart);

    ImDrawList* dl = ImGui::GetWindowDrawList();



    if (g_ShowTrackLabels) {

        dl->AddText(ImVec2(rowPos.x + 4.0f, rowPos.y + 2.0f), IM_COL32(225,225,225,255), track.name.c_str());

        dl->AddLine(ImVec2(timelineX - 4.0f, rowPos.y), ImVec2(timelineX - 4.0f, rowPos.y + rowHeight), IM_COL32(85,85,85,255));

    }



    dl->AddRectFilled(ImVec2(timelineX, rowPos.y), ImVec2(timelineX + timelineWidth, rowPos.y + rowHeight), IM_COL32(39,44,50,255));



    for (const auto& o : occs) {

        if (o.track_index != track.track_index) continue;

        const auto& s = *o.schedule;

        float x1 = timelineX + static_cast<float>(o.start - windowStart) / span * timelineWidth;

        float x2 = timelineX + static_cast<float>(o.end - windowStart) / span * timelineWidth;

        x1 = std::clamp(x1, timelineX, timelineX + timelineWidth);

        x2 = std::clamp(x2, timelineX, timelineX + timelineWidth);

        if (x2 <= x1) continue;



        const bool active = now >= o.start && now < o.end;

        const bool past = o.end <= now;

        float mul = active ? 1.0f : (past ? 0.42f : 0.78f);

        const float r = std::clamp(s.r * mul, 0.0f, 1.0f);

        const float g = std::clamp(s.g * mul, 0.0f, 1.0f);

        const float b = std::clamp(s.b * mul, 0.0f, 1.0f);

        const ImVec2 bmin(x1, rowPos.y + 1.0f);

        const ImVec2 bmax(x2, rowPos.y + rowHeight - 1.0f);

        dl->AddRectFilled(bmin, bmax, ToU32(r,g,b,s.a));

        dl->AddRect(bmin, bmax, IM_COL32(10,10,10,220));



        const char* name = s.name;

        const float innerWidth = std::max(0.0f, x2 - x1 - 6.0f);

        if (innerWidth >= 16.0f) {

            std::string text = name;

            while (!text.empty() && ImGui::CalcTextSize(text.c_str()).x > innerWidth) {

                // UTF-8 safe enough for ellipsis: remove a complete trailing code point.

                do { text.pop_back(); } while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80);

            }

            if (!text.empty()) dl->AddText(ImVec2(x1 + 3.0f, rowPos.y + 2.0f), TextColorFor(r,g,b), text.c_str());

        }



        ImGui::SetCursorScreenPos(bmin);

        ImGui::InvisibleButton((std::string("##evt_") + std::to_string(track.track_index) + "_" + std::to_string(o.start) + "_" + name).c_str(), ImVec2(std::max(1.0f,x2-x1), rowHeight-2.0f));

        if (ImGui::IsItemHovered()) {

            ImGui::BeginTooltip();

            ImGui::TextUnformatted(name);

            ImGui::TextDisabled("%s · %s", track.category.c_str(), track.name.c_str());

            ImGui::Text("%s - %s", FormatTime(o.start).c_str(), FormatTime(o.end).c_str());

            if (active) ImGui::TextColored(ImVec4(0.35f,1.0f,0.35f,1.0f), "进行中 · 剩余 %s", FormatCountdown(o.end-now).c_str());

            else if (o.start > now) ImGui::Text("距离开始 %s", FormatCountdown(o.start-now).c_str());

            ImGui::EndTooltip();

        }

    }



    ImGui::SetCursorScreenPos(rowPos);

    ImGui::Dummy(ImVec2(fullWidth, rowHeight + 2.0f));

}



static void RenderMainWindow() {

    if (!g_ShowWindow) return;

    const bool pushed = PushChineseFont();



    ImGui::SetNextWindowSize(ImVec2(920.0f, 720.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("事件计时器###EventTimerCN", &g_ShowWindow)) {

        if (!g_ChineseFont.load(std::memory_order_acquire)) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.45f, 0.3f, 1.0f),
                "未加载中文字体：请确认 addons/Nexus/Fonts/SarasaUiSC-Regular.ttf 已存在。"
            );
            ImGui::Separator();
        }



        const char* ranges[] = {"2 小时", "4 小时", "6 小时", "8 小时"};

        const int hours[] = {2,4,6,8};

        ImGui::SetNextItemWidth(110.0f);

        ImGui::Combo("显示范围", &g_RangeIndex, ranges, IM_ARRAYSIZE(ranges));

        ImGui::SameLine();

        ImGui::Checkbox("分类标题", &g_ShowCategoryHeaders);

        ImGui::SameLine();

        ImGui::Checkbox("轨道名称", &g_ShowTrackLabels);

        ImGui::SameLine();

        ImGui::Checkbox("显示隐藏轨道", &g_ShowHiddenTracks);



        const std::int64_t now = UnixNow();

        const std::int64_t total = static_cast<std::int64_t>(hours[g_RangeIndex]) * 3600;

        const std::int64_t before = static_cast<std::int64_t>(static_cast<double>(total) * g_CurrentPosition);

        const std::int64_t windowStart = now - before;

        const std::int64_t windowEnd = windowStart + total;



        std::vector<Occurrence> occs;

        occs.reserve(512);

        for (std::size_t i = 0; i < kEventScheduleCount; ++i)

            GenerateOccurrences(kEventSchedules[i], now, windowStart, windowEnd, occs);

        std::sort(occs.begin(), occs.end(), []\(const Occurrence& a, const Occurrence& b) { return a.start < b.start; });



        const float labelWidth = g_ShowTrackLabels ? 150.0f : 0.0f;

        const ImVec2 rulerPos = ImGui::GetCursorScreenPos();

        const float avail = ImGui::GetContentRegionAvail().x;

        const ImVec2 rmin(rulerPos.x + labelWidth, rulerPos.y);

        const ImVec2 rmax(rulerPos.x + avail, rulerPos.y + 24.0f);

        RenderTimeRuler(ImGui::GetWindowDrawList(), rmin, rmax, windowStart, windowEnd);

        ImGui::Dummy(ImVec2(avail, 26.0f));



        ImGui::BeginChild("##timeline_scroll", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar);

        std::string lastCategory;

        for (const auto& track : Tracks()) {

            if (!track.visible && !g_ShowHiddenTracks) continue;

            if (g_ShowCategoryHeaders && track.category != lastCategory) {

                if (!lastCategory.empty()) ImGui::Dummy(ImVec2(0,4));

                ImGui::TextDisabled("%s", track.category.c_str());

                ImGui::Separator();

                lastCategory = track.category;

            }

            RenderTrackRow(track, occs, now, windowStart, windowEnd, labelWidth);

        }



        // Draw the current-time marker over the scrolling timeline content.

        const ImVec2 childMin = ImGui::GetWindowPos();

        const ImVec2 childMax(childMin.x + ImGui::GetWindowSize().x, childMin.y + ImGui::GetWindowSize().y);

        const float xNow = childMin.x + labelWidth + (childMax.x - childMin.x - labelWidth) * g_CurrentPosition;

        ImGui::GetWindowDrawList()->AddLine(ImVec2(xNow, childMin.y), ImVec2(xNow, childMax.y), IM_COL32(255, 40, 55, 255), 2.0f);

        ImGui::EndChild();

    }

    ImGui::End();



    if (pushed) ImGui::PopFont();

}



static void RenderOptions() {

    const bool pushed = PushChineseFont();

    ImGui::TextUnformatted("事件计时器（中文）");

    ImGui::Checkbox("显示主窗口", &g_ShowWindow);

    ImGui::TextWrapped("界面按 Event Timers 的时间轴样式重做：顶部时间刻度、横向彩色事件条、红色当前时间线。\n中文字体：SarasaUiSC-Regular.ttf（插件独立加载）。");

    if (pushed) ImGui::PopFont();

}



static void AddonLoad(AddonAPI_t* api)

{

    g_API = api;



    ImGui::SetCurrentContext(

        static_cast<ImGuiContext*>(g_API->ImguiContext)

    );



    ImGui::SetAllocatorFunctions(

        reinterpret_cast<void* (*)(size_t, void*)>(g_API->ImguiMalloc),

        reinterpret_cast<void (*)(void*, void*)>(g_API->ImguiFree)

    );

    LoadChineseFont();

    g_API->GUI_Register(RT_Render, RenderMainWindow);

    g_API->GUI_Register(RT_OptionsRender, RenderOptions);

}



static void AddonUnload()

{

    if (g_API) {

        g_API->GUI_Deregister(RenderMainWindow);

        g_API->GUI_Deregister(RenderOptions);

    }



    g_ChineseFont.store(nullptr, std::memory_order_release);
    g_API = nullptr;

}



BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }



extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {

    g_AddonDef.Signature = static_cast<uint32_t>(-20260930);

    g_AddonDef.APIVersion = NEXUS_API_VERSION;

    g_AddonDef.Name = "Event Timer CN";

    g_AddonDef.Version.Major = 1;

    g_AddonDef.Version.Minor = 1;

    g_AddonDef.Version.Build = 0;

    g_AddonDef.Version.Revision = 0;

    g_AddonDef.Author = "736838681";

    g_AddonDef.Description = "Chinese GW2 Event Timers-style timeline for Nexus with dedicated Sarasa UI font.";

    g_AddonDef.Load = AddonLoad;

    g_AddonDef.Unload = AddonUnload;

    g_AddonDef.Flags = AF_None;

    return &g_AddonDef;

}
