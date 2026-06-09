// SPDX-License-Identifier: MIT
#include "ui/UiTheme.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <system_error>

namespace UiTheme {

ImVec4 Gain = ImVec4(0.0f, 0.886f, 0.541f, 1.0f);
ImVec4 Loss = ImVec4(1.0f, 0.302f, 0.490f, 1.0f);
ImVec4 Amber = ImVec4(0.961f, 0.722f, 0.294f, 1.0f);
ImVec4 MutedText = ImVec4(0.557f, 0.639f, 0.722f, 1.0f);
ImVec4 AppBackground = ImVec4(0.020f, 0.039f, 0.071f, 1.0f);
ImVec4 SurfaceMain = ImVec4(0.027f, 0.067f, 0.122f, 1.0f);
ImVec4 SurfaceElevated = ImVec4(0.043f, 0.086f, 0.149f, 1.0f);
ImVec4 GlassPanel = ImVec4(0.035f, 0.071f, 0.125f, 0.78f);
ImVec4 BorderSubtle = ImVec4(0.471f, 0.706f, 1.0f, 0.14f);
ImVec4 BorderActiveCyan = ImVec4(0.0f, 0.761f, 1.0f, 0.55f);
ImVec4 BorderActiveMagenta = ImVec4(1.0f, 0.176f, 0.651f, 0.55f);
ImVec4 NeonMagenta = ImVec4(1.0f, 0.176f, 0.651f, 1.0f);
ImVec4 ElectricCyan = ImVec4(0.0f, 0.761f, 1.0f, 1.0f);
ImVec4 SoftBlue = ImVec4(0.231f, 0.510f, 0.965f, 1.0f);
ImVec4 CardBackground = ImVec4(0.043f, 0.086f, 0.149f, 0.90f);
ImVec4 PanelBackground = ImVec4(0.035f, 0.071f, 0.125f, 0.88f);
ImVec4 PanelHeader = ImVec4(0.027f, 0.067f, 0.122f, 0.92f);
ImVec4 Accent = ImVec4(0.0f, 0.761f, 1.0f, 1.0f);
ImVec4 ChartCyan = ImVec4(0.0f, 0.761f, 1.0f, 1.0f);
ImVec4 ChartMagenta = ImVec4(1.0f, 0.176f, 0.651f, 1.0f);
ImVec4 ChartGreen = ImVec4(0.0f, 0.886f, 0.541f, 1.0f);
ImVec4 ChartAmber = ImVec4(0.961f, 0.722f, 0.294f, 1.0f);
ImVec4 PlotGrid = ImVec4(0.471f, 0.706f, 1.0f, 0.11f);
ImVec4 FocusRing = ImVec4(0.0f, 0.761f, 1.0f, 0.72f);
ImVec4 TextPrimary = ImVec4(0.953f, 0.969f, 0.984f, 1.0f);
ImVec4 TextSecondary = ImVec4(0.722f, 0.780f, 0.851f, 1.0f);
ImVec4 TextMuted = ImVec4(0.557f, 0.639f, 0.722f, 1.0f);
ImVec4 TextWarning = ImVec4(0.961f, 0.722f, 0.294f, 1.0f);
ImVec4 TextSuccess = ImVec4(0.0f, 0.886f, 0.541f, 1.0f);
ImVec4 TextDanger = ImVec4(1.0f, 0.302f, 0.490f, 1.0f);

namespace {

ThemeMode ActiveTheme = ThemeMode::DarkCommandCenter;
ImVec4 Clear = AppBackground;
ImFont* BodyFont = nullptr;
ImFont* NumericFont = nullptr;

ImVec4 rgba(int red, int green, int blue, float alpha = 1.0f)
{
    return ImVec4(
        static_cast<float>(red) / 255.0f,
        static_cast<float>(green) / 255.0f,
        static_cast<float>(blue) / 255.0f,
        alpha);
}

ImVec4 mix(ImVec4 left, ImVec4 right, float amount)
{
    const float t = std::clamp(amount, 0.0f, 1.0f);
    return ImVec4(
        left.x + (right.x - left.x) * t,
        left.y + (right.y - left.y) * t,
        left.z + (right.z - left.z) * t,
        left.w + (right.w - left.w) * t);
}

std::string uppercaseCopy(const char* value)
{
    std::string result = value == nullptr ? std::string() : std::string(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return result;
}

const char* firstExistingFont(std::initializer_list<const char*> paths)
{
    for (const char* path : paths) {
        std::error_code error;
        if (path != nullptr && std::filesystem::exists(path, error)) {
            return path;
        }
    }
    return nullptr;
}

void setPalette(ThemeMode theme)
{
    ActiveTheme = theme;

    if (theme == ThemeMode::LightTradingTerminal) {
        AppBackground = rgba(232, 238, 246);
        SurfaceMain = rgba(245, 248, 252);
        SurfaceElevated = rgba(255, 255, 255);
        GlassPanel = rgba(255, 255, 255, 0.86f);
        BorderSubtle = rgba(60, 92, 130, 0.24f);
        BorderActiveCyan = rgba(0, 128, 184, 0.52f);
        BorderActiveMagenta = rgba(190, 32, 118, 0.42f);
        NeonMagenta = rgba(206, 36, 132);
        ElectricCyan = rgba(0, 126, 184);
        SoftBlue = rgba(41, 105, 209);
        Gain = rgba(0, 126, 76);
        Loss = rgba(199, 38, 78);
        Amber = rgba(173, 117, 22);
        MutedText = rgba(83, 98, 115);
        CardBackground = rgba(255, 255, 255, 0.96f);
        PanelBackground = rgba(245, 248, 252, 0.95f);
        PanelHeader = rgba(226, 235, 247, 0.98f);
        Accent = SoftBlue;
        ChartCyan = ElectricCyan;
        ChartMagenta = NeonMagenta;
        ChartGreen = Gain;
        ChartAmber = Amber;
        PlotGrid = rgba(60, 92, 130, 0.16f);
        FocusRing = rgba(0, 126, 184, 0.60f);
        TextPrimary = rgba(15, 23, 42);
        TextSecondary = rgba(51, 65, 85);
        TextMuted = MutedText;
        TextWarning = Amber;
        TextSuccess = Gain;
        TextDanger = Loss;
        Clear = AppBackground;
        return;
    }

    AppBackground = rgba(5, 10, 18);
    SurfaceMain = rgba(7, 17, 31);
    SurfaceElevated = rgba(11, 22, 38);
    GlassPanel = rgba(9, 18, 32, 0.82f);
    BorderSubtle = rgba(120, 180, 255, 0.14f);
    BorderActiveCyan = rgba(0, 194, 255, 0.42f);
    BorderActiveMagenta = rgba(255, 45, 166, 0.42f);
    NeonMagenta = rgba(255, 45, 166);
    ElectricCyan = rgba(0, 194, 255);
    SoftBlue = rgba(59, 130, 246);
    Gain = rgba(0, 226, 138);
    Loss = rgba(255, 77, 125);
    Amber = rgba(245, 184, 75);
    MutedText = rgba(142, 163, 184);
    CardBackground = rgba(11, 22, 38, 0.92f);
    PanelBackground = rgba(9, 18, 32, 0.88f);
    PanelHeader = rgba(7, 17, 31, 0.94f);
    Accent = ElectricCyan;
    ChartCyan = ElectricCyan;
    ChartMagenta = NeonMagenta;
    ChartGreen = Gain;
    ChartAmber = Amber;
    PlotGrid = rgba(120, 180, 255, 0.11f);
    FocusRing = rgba(0, 194, 255, 0.72f);
    TextPrimary = rgba(243, 247, 251);
    TextSecondary = rgba(184, 199, 217);
    TextMuted = MutedText;
    TextWarning = Amber;
    TextSuccess = Gain;
    TextDanger = Loss;
    Clear = AppBackground;
}

void applyFinanceGeometry(ImGuiStyle& style)
{
    style.WindowPadding = ImVec2(14.0f, 12.0f);
    style.FramePadding = ImVec2(10.0f, 7.0f);
    style.CellPadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(11.0f, 9.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 14.0f;
    style.WindowRounding = 0.0f;
    style.ChildRounding = 16.0f;
    style.FrameRounding = 10.0f;
    style.PopupRounding = 14.0f;
    style.GrabRounding = 10.0f;
    style.TabRounding = 10.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
}

void applyLightColors(ImVec4* colors)
{
    colors[ImGuiCol_Text] = TextPrimary;
    colors[ImGuiCol_TextDisabled] = TextMuted;
    colors[ImGuiCol_WindowBg] = AppBackground;
    colors[ImGuiCol_ChildBg] = PanelBackground;
    colors[ImGuiCol_PopupBg] = SurfaceElevated;
    colors[ImGuiCol_Border] = BorderSubtle;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = rgba(255, 255, 255, 0.92f);
    colors[ImGuiCol_FrameBgHovered] = mix(rgba(255, 255, 255), SoftBlue, 0.12f);
    colors[ImGuiCol_FrameBgActive] = mix(rgba(255, 255, 255), ElectricCyan, 0.16f);
    colors[ImGuiCol_TitleBg] = PanelHeader;
    colors[ImGuiCol_TitleBgActive] = PanelHeader;
    colors[ImGuiCol_MenuBarBg] = rgba(236, 243, 252, 0.94f);
    colors[ImGuiCol_ScrollbarBg] = rgba(219, 228, 240, 0.74f);
    colors[ImGuiCol_ScrollbarGrab] = rgba(132, 153, 178, 0.70f);
    colors[ImGuiCol_ScrollbarGrabHovered] = rgba(85, 111, 141, 0.84f);
    colors[ImGuiCol_ScrollbarGrabActive] = SoftBlue;
    colors[ImGuiCol_CheckMark] = ElectricCyan;
    colors[ImGuiCol_SliderGrab] = SoftBlue;
    colors[ImGuiCol_SliderGrabActive] = ElectricCyan;
    colors[ImGuiCol_Button] = rgba(226, 235, 247, 0.92f);
    colors[ImGuiCol_ButtonHovered] = mix(rgba(226, 235, 247), ElectricCyan, 0.16f);
    colors[ImGuiCol_ButtonActive] = mix(rgba(226, 235, 247), ElectricCyan, 0.24f);
    colors[ImGuiCol_Header] = rgba(218, 230, 247, 0.92f);
    colors[ImGuiCol_HeaderHovered] = mix(rgba(218, 230, 247), ElectricCyan, 0.14f);
    colors[ImGuiCol_HeaderActive] = mix(rgba(218, 230, 247), ElectricCyan, 0.22f);
    colors[ImGuiCol_Separator] = BorderSubtle;
    colors[ImGuiCol_ResizeGrip] = withAlpha(SoftBlue, 0.28f);
    colors[ImGuiCol_ResizeGripHovered] = withAlpha(ElectricCyan, 0.50f);
    colors[ImGuiCol_Tab] = rgba(226, 235, 247, 0.84f);
    colors[ImGuiCol_TabHovered] = mix(rgba(226, 235, 247), ElectricCyan, 0.20f);
    colors[ImGuiCol_TabActive] = rgba(255, 255, 255, 0.96f);
    colors[ImGuiCol_TableHeaderBg] = rgba(221, 232, 247, 0.96f);
    colors[ImGuiCol_TableBorderStrong] = rgba(60, 92, 130, 0.30f);
    colors[ImGuiCol_TableBorderLight] = rgba(60, 92, 130, 0.18f);
    colors[ImGuiCol_TableRowBg] = rgba(255, 255, 255, 0.74f);
    colors[ImGuiCol_TableRowBgAlt] = rgba(232, 240, 250, 0.72f);
    colors[ImGuiCol_NavHighlight] = FocusRing;
}

void applyDarkColors(ImVec4* colors)
{
    colors[ImGuiCol_Text] = TextPrimary;
    colors[ImGuiCol_TextDisabled] = TextMuted;
    colors[ImGuiCol_WindowBg] = AppBackground;
    colors[ImGuiCol_ChildBg] = PanelBackground;
    colors[ImGuiCol_PopupBg] = rgba(9, 18, 32, 0.98f);
    colors[ImGuiCol_Border] = BorderSubtle;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = rgba(12, 25, 43, 0.94f);
    colors[ImGuiCol_FrameBgHovered] = rgba(14, 38, 62, 0.96f);
    colors[ImGuiCol_FrameBgActive] = rgba(16, 50, 78, 0.98f);
    colors[ImGuiCol_TitleBg] = SurfaceMain;
    colors[ImGuiCol_TitleBgActive] = SurfaceElevated;
    colors[ImGuiCol_MenuBarBg] = rgba(5, 10, 18, 0.96f);
    colors[ImGuiCol_ScrollbarBg] = rgba(5, 10, 18, 0.64f);
    colors[ImGuiCol_ScrollbarGrab] = rgba(66, 93, 125, 0.58f);
    colors[ImGuiCol_ScrollbarGrabHovered] = rgba(0, 194, 255, 0.42f);
    colors[ImGuiCol_ScrollbarGrabActive] = rgba(0, 194, 255, 0.62f);
    colors[ImGuiCol_CheckMark] = ElectricCyan;
    colors[ImGuiCol_SliderGrab] = ElectricCyan;
    colors[ImGuiCol_SliderGrabActive] = NeonMagenta;
    colors[ImGuiCol_Button] = rgba(11, 30, 52, 0.92f);
    colors[ImGuiCol_ButtonHovered] = rgba(0, 194, 255, 0.16f);
    colors[ImGuiCol_ButtonActive] = rgba(0, 194, 255, 0.24f);
    colors[ImGuiCol_Header] = rgba(0, 194, 255, 0.10f);
    colors[ImGuiCol_HeaderHovered] = rgba(0, 194, 255, 0.16f);
    colors[ImGuiCol_HeaderActive] = rgba(255, 45, 166, 0.14f);
    colors[ImGuiCol_Separator] = BorderSubtle;
    colors[ImGuiCol_ResizeGrip] = rgba(0, 194, 255, 0.22f);
    colors[ImGuiCol_ResizeGripHovered] = rgba(0, 194, 255, 0.44f);
    colors[ImGuiCol_Tab] = rgba(11, 22, 38, 0.88f);
    colors[ImGuiCol_TabHovered] = rgba(0, 194, 255, 0.20f);
    colors[ImGuiCol_TabActive] = rgba(15, 33, 57, 0.98f);
    colors[ImGuiCol_TableHeaderBg] = rgba(12, 25, 43, 0.96f);
    colors[ImGuiCol_TableBorderStrong] = rgba(120, 180, 255, 0.18f);
    colors[ImGuiCol_TableBorderLight] = rgba(120, 180, 255, 0.10f);
    colors[ImGuiCol_TableRowBg] = rgba(9, 18, 32, 0.54f);
    colors[ImGuiCol_TableRowBgAlt] = rgba(14, 28, 48, 0.58f);
    colors[ImGuiCol_NavHighlight] = FocusRing;
}

void drawRadialWash(ImDrawList* drawList, ImVec2 center, float radius, ImVec4 color, float alpha)
{
    constexpr int Steps = 18;
    for (int step = Steps; step >= 1; --step) {
        const float t = static_cast<float>(step) / static_cast<float>(Steps);
        ImVec4 wash = withAlpha(color, alpha * t * t * 0.12f);
        drawList->AddCircleFilled(center, radius * t, ImGui::GetColorU32(wash), 96);
    }
}

} // namespace

ThemeMode themeFromKey(const std::string& key)
{
    if (key == "light_trading_terminal") {
        return ThemeMode::LightTradingTerminal;
    }
    return ThemeMode::DarkCommandCenter;
}

std::string themeKey(ThemeMode theme)
{
    return theme == ThemeMode::LightTradingTerminal ? "light_trading_terminal" : "dark_command_center";
}

const char* themeName(ThemeMode theme)
{
    return theme == ThemeMode::LightTradingTerminal ? "Light Trading View" : "Dark Finance Command";
}

ThemeMode currentTheme()
{
    return ActiveTheme;
}

ImVec4 clearColor()
{
    return Clear;
}

void apply(ThemeMode theme)
{
    setPalette(theme);
    ImGuiStyle& style = ImGui::GetStyle();
    applyFinanceGeometry(style);

    if (theme == ThemeMode::LightTradingTerminal) {
        applyLightColors(style.Colors);
    } else {
        applyDarkColors(style.Colors);
    }
}

void apply()
{
    apply(ActiveTheme);
}

void configureFonts(ImGuiIO& io)
{
    io.Fonts->Clear();
    io.FontGlobalScale = 1.0f;

    ImFontConfig bodyConfig;
    bodyConfig.OversampleH = 3;
    bodyConfig.OversampleV = 2;
    bodyConfig.PixelSnapH = false;
    bodyConfig.GlyphOffset = ImVec2(0.0f, 0.0f);

    const char* bodyPath = firstExistingFont({
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\SegoeUI.ttf",
    });
    BodyFont = bodyPath == nullptr
        ? io.Fonts->AddFontDefault(&bodyConfig)
        : io.Fonts->AddFontFromFileTTF(bodyPath, 15.5f, &bodyConfig);
    if (BodyFont == nullptr) {
        BodyFont = io.Fonts->AddFontDefault(&bodyConfig);
    }

    ImFontConfig numericConfig;
    numericConfig.OversampleH = 3;
    numericConfig.OversampleV = 2;
    numericConfig.PixelSnapH = false;
    numericConfig.GlyphOffset = ImVec2(0.0f, 0.0f);
    const char* numericPath = firstExistingFont({
        "C:\\Windows\\Fonts\\CascadiaMono.ttf",
        "C:\\Windows\\Fonts\\CascadiaCode.ttf",
    });
    NumericFont = numericPath == nullptr
        ? BodyFont
        : io.Fonts->AddFontFromFileTTF(numericPath, 14.5f, &numericConfig);
    if (NumericFont == nullptr) {
        NumericFont = BodyFont;
    }

    io.FontDefault = BodyFont;
}

ImVec4 withAlpha(ImVec4 color, float alpha)
{
    color.w = std::clamp(alpha, 0.0f, 1.0f);
    return color;
}

void drawAppBackdrop(ImVec2 position, ImVec2 size)
{
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const ImVec2 max(position.x + size.x, position.y + size.y);
    drawList->AddRectFilled(position, max, ImGui::GetColorU32(AppBackground));

    if (ActiveTheme == ThemeMode::LightTradingTerminal) {
        drawRadialWash(drawList, ImVec2(position.x + size.x * 0.15f, position.y + size.y * 0.08f), size.x * 0.48f, SoftBlue, 0.20f);
        drawRadialWash(drawList, ImVec2(position.x + size.x * 0.88f, position.y + size.y * 0.10f), size.x * 0.40f, ElectricCyan, 0.12f);
        return;
    }

    drawRadialWash(drawList, ImVec2(position.x + size.x * 0.18f, position.y + size.y * 0.02f), size.x * 0.45f, ElectricCyan, 0.32f);
    drawRadialWash(drawList, ImVec2(position.x + size.x * 0.92f, position.y + size.y * 0.16f), size.x * 0.38f, NeonMagenta, 0.24f);
    drawRadialWash(drawList, ImVec2(position.x + size.x * 0.50f, position.y + size.y * 0.96f), size.x * 0.52f, SoftBlue, 0.16f);
}

void pushPanelStyle()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, GlassPanel);
    ImGui::PushStyleColor(ImGuiCol_Border, withAlpha(BorderSubtle, 0.10f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 18.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
}

void popPanelStyle()
{
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void pushTableStyle()
{
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, withAlpha(SurfaceElevated, 0.88f));
    ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, withAlpha(ElectricCyan, 0.12f));
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight, withAlpha(BorderSubtle, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, withAlpha(SurfaceMain, 0.50f));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, withAlpha(SurfaceElevated, 0.54f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(11.0f, 7.0f));
}

void popTableStyle()
{
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);
}

void pushButtonStyle(ImVec4 accent)
{
    ImGui::PushStyleColor(ImGuiCol_Button, withAlpha(accent, 0.11f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, withAlpha(accent, 0.18f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, withAlpha(accent, 0.24f));
    ImGui::PushStyleColor(ImGuiCol_Border, withAlpha(accent, 0.24f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
}

void popButtonStyle()
{
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

void pushBadgeStyle(ImVec4 color)
{
    ImGui::PushStyleColor(ImGuiCol_Button, withAlpha(color, 0.14f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, withAlpha(color, 0.20f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, withAlpha(color, 0.26f));
    ImGui::PushStyleColor(ImGuiCol_Border, withAlpha(color, 0.26f));
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
}

void popBadgeStyle()
{
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(5);
}

ImVec4 moneyColor(double value)
{
    if (value > 0.0) {
        return Gain;
    }
    if (value < 0.0) {
        return Loss;
    }
    return MutedText;
}

void pushNumericFont()
{
    if (NumericFont != nullptr) {
        ImGui::PushFont(NumericFont);
    }
}

void popNumericFont()
{
    if (NumericFont != nullptr) {
        ImGui::PopFont();
    }
}

void textMoney(double value, const char* text)
{
    pushNumericFont();
    ImGui::TextColored(moneyColor(value), "%s", text);
    popNumericFont();
}

void textRightAligned(const char* text)
{
    textRightAligned(text, TextPrimary);
}

void textRightAligned(const char* text, ImVec4 color)
{
    const char* displayText = text == nullptr ? "" : text;
    pushNumericFont();
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float textWidth = ImGui::CalcTextSize(displayText).x;
    if (textWidth < availableWidth) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - textWidth);
    }
    ImGui::TextColored(color, "%s", displayText);
    popNumericFont();
}

void sectionHeading(const char* title, const char* subtitle)
{
    ImGui::SetWindowFontScale(1.16f);
    ImGui::TextColored(TextPrimary, "%s", title);
    ImGui::SetWindowFontScale(1.0f);
    if (subtitle != nullptr && subtitle[0] != '\0') {
        ImGui::TextColored(TextMuted, "%s", subtitle);
    }

    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + 1.0f), ImGui::GetColorU32(withAlpha(BorderSubtle, 0.07f)));
    drawList->AddRectFilled(cursor, ImVec2(cursor.x + std::min(width, 118.0f), cursor.y + 1.0f), ImGui::GetColorU32(withAlpha(ElectricCyan, 0.20f)));
    ImGui::Dummy(ImVec2(width, 9.0f));
}

void metricCard(const char* title, const char* value, const char* caption, ImVec4 accent, ImVec2 size)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, CardBackground);
    ImGui::PushStyleColor(ImGuiCol_Border, withAlpha(accent, 0.14f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::BeginChild(title, size, true, ImGuiWindowFlags_NoScrollbar);

    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max(min.x + ImGui::GetWindowSize().x, min.y + ImGui::GetWindowSize().y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(min, ImVec2(max.x, min.y + 30.0f), ImGui::GetColorU32(withAlpha(accent, 0.025f)), 16.0f, ImDrawFlags_RoundCornersTop);
    drawList->AddRectFilled(ImVec2(min.x + 12.0f, min.y + 8.0f), ImVec2(min.x + 58.0f, min.y + 10.0f), ImGui::GetColorU32(withAlpha(accent, 0.44f)), 2.0f);

    const std::string label = uppercaseCopy(title);
    ImGui::TextColored(TextSecondary, "%s", label.c_str());
    ImGui::SetWindowFontScale(1.08f);
    pushNumericFont();
    ImGui::TextColored(TextPrimary, "%s", value);
    popNumericFont();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(accent, "%s", caption);
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void emptyState(const char* title, const char* body)
{
    pushPanelStyle();
    ImGui::BeginChild(title, ImVec2(0.0f, 108.0f), true);
    ImGui::TextColored(TextWarning, "%s", title);
    ImGui::TextColored(TextMuted, "%s", body);
    ImGui::EndChild();
    popPanelStyle();
}

void formError(const std::string& message)
{
    if (!message.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(TextDanger, "%s", message.c_str());
    }
}

void badge(const char* label, ImVec4 color)
{
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const ImVec2 padding(8.0f, 3.0f);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 size(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);
    const ImVec2 max(min.x + size.x, min.y + size.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(min, max, ImGui::GetColorU32(withAlpha(color, 0.12f)), 8.0f);
    drawList->AddRect(min, max, ImGui::GetColorU32(withAlpha(color, 0.24f)), 8.0f);
    drawList->AddText(ImVec2(min.x + padding.x, min.y + padding.y), ImGui::GetColorU32(color), label);
    ImGui::Dummy(size);
}

}
