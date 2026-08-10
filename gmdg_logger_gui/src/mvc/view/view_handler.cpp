#include "pch.h"

#include "view_handler.hpp"

#define GLFW_EXPOSE_NATIVE_WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"
#include <commdlg.h>
#include "imgui.h"
#include "imgui_internal.h" // ImGui::TableSetColumnWidth() is internal-only in this ImGui version
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "gmdg_logger.h"

#include "application.hpp"
#include "mvc/controller/control_handler.hpp"
#include "mvc/view/table_layout.hpp"
#include "version.hpp"

#include <algorithm>
#include <cmath>

using namespace GMDGLoggerGUI;

namespace
{
    constexpr float IDsColumnMinWidth      = 70.0f;
    constexpr float TimeColumnMinWidth     = 170.0f;
    constexpr float ThreadColumnMinWidth   = 90.0f;
    constexpr float SeverityColumnMinWidth = 70.0f;
    constexpr float CategoryColumnMinWidth = 150.0f;
    constexpr float MessageColumnMinWidth  = 150.0f;

    constexpr const char* ColumnNames[6]     = { "IDs", "Time", "Thread", "Severity", "Category", "Message" };
    constexpr float       ColumnMinWidths[6] = { IDsColumnMinWidth,
                                                 TimeColumnMinWidth, 
                                                 ThreadColumnMinWidth,
                                                 SeverityColumnMinWidth, 
                                                 CategoryColumnMinWidth,
                                                 MessageColumnMinWidth };

    constexpr float RowVerticalPadding = 4.0f;
}

void ViewHandler::Initialize()
{
    // InitGLFW
    {
        glfwSetErrorCallback(GLFWErrorCallback);
        if (!glfwInit())
        {
            exit(-1);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_window = glfwCreateWindow(1280, 720, APP_NAME, nullptr, nullptr);

        if (!m_window)
        {
            glfwTerminate();
            exit(-1);
        }

        glfwMakeContextCurrent(m_window);

        // VSync
        glfwSwapInterval(1);
    }

    // InitImGui
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        io.IniFilename = nullptr;

        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init();
    }

    ImGui::StyleColorsDark();
}

void ViewHandler::Update(ControlHandler& t_controlHandler, ModelHandler& t_modelHandler)
{
    // ImGui::ShowDemoWindow();
    // ImPlot::ShowDemoWindow();

    // Handle Input
    {
        if (glfwWindowShouldClose(m_window))
        {
            t_controlHandler.AddAction("ShutdownAction");
            return;
        }

        glfwPollEvents();
    }

    // Preparing Rendering
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    // Render View
    {
        Render(t_controlHandler, t_modelHandler);
    }

    // ImGui Render
    {
        ImGui::Render();
    }

    // Clear
    {
        int32_t displayWidth, displayHeight;
        glfwGetFramebufferSize(m_window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Render Draw Data
    {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // Update Viewports
    {
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* const backupCurrentContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backupCurrentContext);
        }
    }

    // Swap Buffers
    {
        glfwSwapBuffers(m_window);
    }
}

void ViewHandler::Shutdown()
{
    // ShutdownImGui
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    // shutodwnGLFW
    {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
}

void ViewHandler::GLFWErrorCallback(const int32_t t_error, const char* const t_description)
{
    GMDG_ASSERT(t_description != nullptr);

    std::println("GLFW Error {0}: {1}", t_error, t_description);
}

void ViewHandler::Render(ControlHandler& t_controlHandler, ModelHandler& t_modelHandler)
{
    const ImGuiViewport& viewport = *ImGui::GetMainViewport();

    ImGuiWindowFlags windowFlags = 0;
                     windowFlags |= ImGuiWindowFlags_NoDocking;
                     windowFlags |= ImGuiWindowFlags_NoTitleBar;
                     windowFlags |= ImGuiWindowFlags_NoResize;
                     windowFlags |= ImGuiWindowFlags_NoCollapse;
                     windowFlags |= ImGuiWindowFlags_NoSavedSettings;
                     windowFlags |= ImGuiWindowFlags_NoScrollbar;
                     windowFlags |= ImGuiWindowFlags_NoBackground;


    ImGui::SetNextWindowPos(viewport.WorkPos);
    ImGui::SetNextWindowSize(viewport.WorkSize);
    ImGui::SetNextWindowViewport(viewport.ID);

    ImGui::Begin("Logs", nullptr, windowFlags);

    RenderOpenFileButton(t_controlHandler);
    ImGui::SameLine();
    RenderClearLoadedFileButton(t_controlHandler);
    ImGui::SameLine();
    RenderAboutButton();

    ImGui::Separator();

    // Actions elements
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(20.0f, 8.0f));

        RenderSeverityFilter();
        ImGui::SameLine();
        RenderThreadFilter();
        ImGui::SameLine();
        RenderCategoryFilter();
        ImGui::SameLine();
        RenderSearchBar();
        if (t_modelHandler.HasAttemptedLoad() && t_modelHandler.GetFileValidationError() != GMDG_SUCCESS)
        {
            ImGui::SameLine();
            RenderFileHeaderValidationError();
        }

        ImGui::PopStyleVar();
    }

    ImGui::Separator();

    RenderTable();

    ImGui::End();
}

bool ViewHandler::OpenFileDialog(GLFWwindow* const t_window, std::string& t_outPath)
{
    GMDG_ASSERT(t_window != nullptr);

    char fileBuffer[MAX_PATH] = {};

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = glfwGetWin32Window(t_window);
    ofn.lpstrFilter = "gmdg log Files (*.log)\0*.log\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameA(&ofn))
    {
        return false;
    }

    t_outPath = fileBuffer;
    return true;
}

void ViewHandler::RenderOpenFileButton(ControlHandler& t_controlHandler)
{
    if (ImGui::Button("Open##open_file_button"))
    {
        std::string path;
        if (OpenFileDialog(m_window, path))
        {
            t_controlHandler.AddAction("OpenFileAction", std::string(path));
        }
    }
}

void ViewHandler::RenderClearLoadedFileButton(ControlHandler& t_controlHandler)
{
    if (ImGui::Button("Clear##clear_loaded_file_button"))
    {
        t_controlHandler.AddAction("ClearLoadedFileAction");
    }
}

void ViewHandler::RenderSeverityFilter()
{
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::BeginCombo("Severity", m_severityMask == 0xFFFFFFFF ? "All" : "Custom", ImGuiComboFlags_HeightRegular))
    {
        if (ImGui::Selectable("All", m_severityMask == 0xFFFFFFFF))
        {
            m_severityMask = 0xFFFFFFFF;
        }

        ImGui::Separator();

        for (int32_t severity = GMDG_LOG_DEBUG; severity <= GMDG_LOG_ERROR; ++severity)
        {
            bool enabled = IsSeverityEnabled(severity);

            if (ImGui::Checkbox(GMDG_Logger_Severity_To_String(static_cast<GMDGLogSeverity>(severity)), &enabled))
            {
                m_severityMask = (m_severityMask | (1u << severity)) * enabled + (m_severityMask & ~(1u << severity)) * (1 - enabled);
            }
        }

        ImGui::EndCombo();
    }
}

void ViewHandler::RenderAboutButton()
{
    if (ImGui::Button("About##about_button"))
    {
        ImGui::OpenPopup("AboutPopup");
    }

    if (ImGui::BeginPopup("AboutPopup"))
    {
        const std::vector<LogRecord>& logs = Application::GetInstance().GetModelHandler().GetLogs();

        ImGui::Text("%s version: %s", APP_NAME, APP_VERSION_STRING);
        ImGui::Separator();
        ImGui::Text("Data load: %.2fMB", sizeof(LogRecord) * logs.size() / (1024.0 * 1024.0));

        ImGui::EndPopup();
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Show about info");
        ImGui::EndTooltip();
    }
}

void ViewHandler::RenderThreadFilter()
{
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::BeginCombo("Thread", m_disabledThreadNames.size() == 0 ? "All" : "Custom", ImGuiComboFlags_HeightRegular))
    {
        if (ImGui::Selectable("All", m_disabledThreadNames.size() == 0))
        {
            m_disabledThreadNames.clear();
        }

        ImGui::Separator();

        const std::unordered_set<std::string>& threadNames = Application::GetInstance().GetModelHandler().GetThreadNames();

        for (const auto& threadName : threadNames)
        {
            bool enabled = IsThreadEnabled(threadName);

            if (ImGui::Checkbox(threadName.c_str(), &enabled))
            {
                if (enabled)
                {
                    m_disabledThreadNames.erase(threadName);
                }
                else
                {
                    m_disabledThreadNames.emplace(threadName);
                }
            }
        }

        ImGui::EndCombo();
    }
}

void ViewHandler::RenderCategoryFilter()
{
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::BeginCombo("Category", m_disabledCategories.size() == 0 ? "All" : "Custom", ImGuiComboFlags_HeightRegular))
    {
        if (ImGui::Selectable("All", m_disabledCategories.size() == 0))
        {
            m_disabledCategories.clear();
        }

        ImGui::Separator();

        const std::unordered_set<std::string>& categories = Application::GetInstance().GetModelHandler().GetCategories();

        for (const auto& category : categories)
        {
            bool enabled = IsCategoryEnabled(category);

            if (ImGui::Checkbox(category.c_str(), &enabled))
            {
                if (enabled)
                {
                    m_disabledCategories.erase(category);
                }
                else
                {
                    m_disabledCategories.emplace(category);
                }
            }
        }

        ImGui::EndCombo();
    }
}

void ViewHandler::RenderSearchBar()
{
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("Search##message_search", m_searchBuffer, sizeof(m_searchBuffer));
}

void ViewHandler::RenderFileHeaderValidationError()
{
    const ModelHandler& modelHandler = Application::GetInstance().GetModelHandler();

    if (!modelHandler.HasAttemptedLoad()) return;

    const GMDGLogFileValidationResult result = modelHandler.GetFileValidationError();

    if (result != GMDG_SUCCESS && result >= GMDG_SUCCESS && result <= GMDG_UNKNOWN)
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, { 1.0f, 0.0f, 0.0f, 1.0f });
        switch (result)
        {
        case GMDG_INVALID_MAGIC:
        {
            ImGui::TextUnformatted("Loaded file was not a gmdg_logger file!");
        } break;
        case GMDG_UNSUPPORTED_VERSION:
        {
            ImGui::TextUnformatted("Loaded file version in not supported!");
        } break;
        case GMDG_UNSUPPORTED_FILE_HEADER:
        {
            ImGui::TextUnformatted("Loaded file is corrupted!");
        } break;
        case GMDG_UNKNOWN:
        {
            ImGui::TextUnformatted("Unknwon error while loading the file!");
        } break;
        case GMDG_SUCCESS:
        {
            // Nothing
        }   break;
        default:
        {
            GMDG_ASSERT_WITH_MESSAGE(false, "unhandled GMDGLogFileValidationResult: {}", static_cast<int>(result));
        }   break;
        }
        ImGui::PopStyleColor();
    }
}

void ViewHandler::RenderTable()
{
    if (ImGui::BeginTable("logs", 6,
        ImGuiTableFlags_RowBg    |
        ImGuiTableFlags_Borders  |
        ImGuiTableFlags_ScrollY  |
        ImGuiTableFlags_Resizable))
    {
        for (int32_t col = 0; col < 5; ++col)
        {
            ImGui::TableSetupColumn(ColumnNames[col], ImGuiTableColumnFlags_WidthFixed, ColumnMinWidths[col]);
        }
        ImGui::TableSetupColumn(ColumnNames[5], ImGuiTableColumnFlags_WidthStretch, 1.0f);

        // Real-time minimum-width enforcement for the five Fixed columns. This must run
        // here -- after BeginTable() (so this frame's pending drag delta is already
        // folded into WidthRequest by BeginTable()'s internal TableBeginApplyRequests())
        // but before the first TableNextRow()/TableSetColumnIndex() call (which locks
        // layout via TableUpdateLayout() and finalizes this frame's borders).
        //
        // This writes column.WidthRequest directly instead of calling
        // ImGui::TableSetColumnWidth(): that function's own early-return guard
        // (`if (column->WidthGiven == width || column->WidthRequest == width) return;`,
        // imgui_tables.cpp:2331) compares against WidthGiven, which at this point in the
        // frame is still *last* frame's value (this frame's TableUpdateLayout() hasn't
        // run yet). Since last frame's WidthGiven was already correctly clamped, it
        // coincidentally equals this frame's clamped target on almost every frame, so the
        // guard silently no-ops the correction -- which is exactly why the floor only
        // ever visibly stuck once the mouse was released (the one frame where a stale
        // comparator happens not to match). Writing WidthRequest directly reproduces
        // TableSetColumnWidth()'s own Fixed-column code path (imgui_tables.cpp:2371-2377)
        // without going through that stale-cache guard.
        {
            ImGuiTable* const table = ImGui::GetCurrentTable();
            for (int32_t col = 0; col < 5; ++col)
            {
                const float currentWidth = table->Columns[col].WidthRequest;
                const float clamped = GMDGLoggerGUI::TableLayout::ClampToMinWidth(currentWidth, ColumnMinWidths[col]);
                if (clamped > currentWidth + 0.5f) // epsilon avoids a snap-back feedback loop from float jitter
                {
                    table->Columns[col].WidthRequest = clamped;
                    table->IsSettingsDirty = true;
                }
            }
        }

        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        float messageColumnWidth = 0.0f;
        for (int32_t col = 0; col < 6; ++col)
        {
            ImGui::TableSetColumnIndex(col);
            ImGui::TableHeader(ColumnNames[col]);

            if (col == 5) messageColumnWidth = ImGui::GetContentRegionAvail().x;
        }

        const std::vector<LogRecord>& logs = Application::GetInstance().GetModelHandler().GetLogs();

        std::vector<uint32_t> visible;
        visible.reserve(logs.size());
        for (uint32_t i = 0; i < static_cast<uint32_t>(logs.size()); ++i)
        {
            const LogRecord& log = logs[i];
            if (!IsSeverityEnabled(log.Severity)) continue;
            if (!IsThreadEnabled(log.ThreadName)) continue;
            if (!IsCategoryEnabled(log.Category)) continue;
            if (!IsMessageMatchingSearch(log.Message)) continue;
            visible.emplace_back(i);
        }

        const bool visibleSetChanged = (visible != m_cachedVisibleIndices);
        const bool columnWidthChanged = std::fabs(messageColumnWidth - m_cachedMessageColumnWidth) > 0.5f;
        if (visibleSetChanged || columnWidthChanged)
        {
            RecomputeRowLayout(logs, visible, messageColumnWidth);
            m_cachedVisibleIndices = visible;
            m_cachedMessageColumnWidth = messageColumnWidth;
        }

        const float scrollY = ImGui::GetScrollY();
        const float windowHeight = ImGui::GetContentRegionAvail().y;
        const uint32_t rowCount = m_cachedRowTopY.empty() ? 0 : static_cast<uint32_t>(m_cachedRowTopY.size()) - 1;

        uint32_t start = 0, end = rowCount;
        if (rowCount > 0)
        {
            const auto startIt = std::upper_bound(m_cachedRowTopY.begin(), m_cachedRowTopY.end(), scrollY);
            start = (startIt == m_cachedRowTopY.begin()) ? 0 : static_cast<uint32_t>(startIt - m_cachedRowTopY.begin() - 1);
            start = (start > 0) ? start - 1 : 0; // 1-row overscan

            const auto endIt = std::upper_bound(m_cachedRowTopY.begin(), m_cachedRowTopY.end(), scrollY + windowHeight);
            end = std::min(static_cast<uint32_t>(endIt - m_cachedRowTopY.begin()) + 1, rowCount);
        }

        // Top spacer: reserves the vertical space of all skipped rows above the viewport in
        // a single row, so the table's scrollbar sizing (based on cumulative row heights)
        // stays correct without laying out every skipped row individually.
        if (start > 0)
        {
            ImGui::TableNextRow(0, m_cachedRowTopY[start]);
        }

        for (uint32_t row = start; row < end; ++row)
        {
            const LogRecord& log = logs[m_cachedVisibleIndices[row]];

            ImGui::TableNextRow(0, m_cachedRowHeights[row]);

            const std::string ts = std::format("{:%F %T}", std::chrono::floor<std::chrono::milliseconds>(std::chrono::sys_time<std::chrono::nanoseconds>{std::chrono::nanoseconds{log.Timestamp_Ns}}));

            ImGui::PushStyleColor(ImGuiCol_Text, SeverityToColor(log.Severity));

            // Columns
            {
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", log.Id);

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(ts.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(log.ThreadName.c_str());

                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(GMDG_Logger_Severity_To_String(static_cast<GMDGLogSeverity>(log.Severity)));

                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(log.Category.c_str());

                ImGui::TableSetColumnIndex(5);
                ImGui::TextWrapped("%s", log.Message.c_str());
            }

            ImGui::PopStyleColor();
        }

        // Bottom spacer: same idea, for rows below the viewport.
        if (end < rowCount)
        {
            ImGui::TableNextRow(0, m_cachedRowTopY[rowCount] - m_cachedRowTopY[end]);
        }

        ImGui::EndTable();
    }
}

void ViewHandler::RecomputeRowLayout(const std::vector<LogRecord>& t_logs, const std::vector<uint32_t>& t_visible, const float t_messageColumnWidth)
{
    GMDG_ASSERT(t_messageColumnWidth >= 0.0f);

    m_cachedRowHeights.resize(t_visible.size());
    m_cachedRowTopY.resize(t_visible.size() + 1);

    const float lineHeight = ImGui::GetTextLineHeight();
    m_cachedRowTopY[0] = 0.0f;
    for (uint32_t i = 0; i < static_cast<uint32_t>(t_visible.size()); ++i)
    {
        GMDG_ASSERT(t_visible[i] < static_cast<uint32_t>(t_logs.size()));

        const std::string& message = t_logs[t_visible[i]].Message;
        const float wrappedHeight = ImGui::CalcTextSize(message.c_str(), nullptr, false, t_messageColumnWidth).y;
        const int32_t lineCount = std::max(1, static_cast<int32_t>(std::lround(wrappedHeight / lineHeight)));

        const float rowHeight = GMDGLoggerGUI::TableLayout::ComputeRowHeight(lineCount, lineHeight, RowVerticalPadding);
        m_cachedRowHeights[i] = rowHeight;
        m_cachedRowTopY[i + 1] = m_cachedRowTopY[i] + rowHeight;
    }
}

ImVec4 ViewHandler::SeverityToColor(const uint32_t t_severity)
{
    switch (t_severity)
    {
    case GMDG_LOG_DEBUG:   return { 0.0f, 1.0f, 1.0f, 1.0f };
    case GMDG_LOG_INFO:    return { 0.0f, 1.0f, 0.0f, 1.0f };
    case GMDG_LOG_WARNING: return { 1.0f, 1.0f, 0.0f, 1.0f };
    case GMDG_LOG_ERROR:   return { 1.0f, 0.0f, 0.0f, 1.0f };
    default:
        GMDG_ASSERT_WITH_MESSAGE(false, "unknown severity {}", t_severity);
        return { 1.0f, 1.0f, 1.0f, 1.0f };
    }
}

bool ViewHandler::IsSeverityEnabled(const uint32_t t_severity)
{
    return (m_severityMask & (1u << t_severity)) != 0;
}

bool ViewHandler::IsThreadEnabled(const std::string& t_threadName)
{
    return !m_disabledThreadNames.contains(t_threadName);
}

bool ViewHandler::IsCategoryEnabled(const std::string& t_category)
{
    return !m_disabledCategories.contains(t_category);
}

bool ViewHandler::IsMessageMatchingSearch(const std::string& t_message)
{
    if (m_searchBuffer[0] == '\0') return true;

    auto toLower = [](const uint8_t t_c) { return std::tolower(t_c); };

    std::string needle = m_searchBuffer;
    std::string haystack = t_message;
    std::ranges::transform(needle, needle.begin(), toLower);
    std::ranges::transform(haystack, haystack.begin(), toLower);

    return haystack.find(needle) != std::string::npos;
}
