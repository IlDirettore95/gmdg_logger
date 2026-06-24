#include "pch.h"

#include "view_handler.hpp"

#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "gmdg_logger.h"

#include "application.hpp"
#include "version.hpp"

using namespace GMDGLoggerGUI;

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

        mWindow = glfwCreateWindow(1280, 720, APP_NAME, nullptr, nullptr);

        if (!mWindow)
        {
            glfwTerminate();
            exit(-1);
        }

        glfwMakeContextCurrent(mWindow);
        
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

        ImGui_ImplGlfw_InitForOpenGL(mWindow, true);
        ImGui_ImplOpenGL3_Init();
    }

    ImGui::StyleColorsDark();
}

void ViewHandler::Update(ControlHandler& t_controlHandler, const ModelHandler& t_modelHandler)
{
    // ImGui::ShowDemoWindow();
    // ImPlot::ShowDemoWindow();

    // Handle Input
    {    
        if (glfwWindowShouldClose(mWindow))
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
        Render();
    }

    // ImGui Render
    {
        ImGui::Render();
    }

    // Clear
    {
        int32_t display_w, display_h;
        glfwGetFramebufferSize(mWindow, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
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
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }

    // Swap Buffers
    {
        glfwSwapBuffers(mWindow);
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
        glfwDestroyWindow(mWindow);
        glfwTerminate();
    }
}

void ViewHandler::GLFWErrorCallback(int32_t t_error, const char* t_description)
{
    std::println("GLFW Error {0}: {1}", t_error, t_description);
}

void ViewHandler::Render()
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

    RenderSeverityFilter();
    ImGui::SameLine();
    RenderThreadIDFilter();
    ImGui::SameLine();
    RenderAboutButton();
    RenderFileHeaderValidationError();

    ImGui::Separator();
    RenderTable();

    ImGui::End();
}

void ViewHandler::RenderSeverityFilter()
{
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::BeginCombo("Severity", mSeverityMask == 0xFFFFFFFF ? "All" : "Custom"))
    {
        if (ImGui::Selectable("All", mSeverityMask == 0xFFFFFFFF))
        {
            mSeverityMask = 0xFFFFFFFF;
        }

        ImGui::Separator();

        for (int32_t severity = GMDG_LOG_DEBUG; severity <= GMDG_LOG_ERROR; ++severity)
        {
            bool enabled = IsSeverityEnabled(severity);

            if (ImGui::Checkbox(GMDG_Logger_Severity_To_String(static_cast<GMDGLogSeverity>(severity)), &enabled))
            {
                mSeverityMask = (mSeverityMask | (1u << severity)) * enabled + (mSeverityMask & ~(1u << severity)) * (1 - enabled);
            }
        }

        ImGui::EndCombo();
    }
}

void ViewHandler::RenderAboutButton()
{
    if (ImGui::Button("About##about_button", ImVec2(60, 22)))
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

void ViewHandler::RenderThreadIDFilter()
{
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::BeginCombo("ThreadID", mDisabledThreadIDs.size() == 0 ? "All" : "Custom"))
    {
        if (ImGui::Selectable("All", mDisabledThreadIDs.size() == 0))
        {
            mDisabledThreadIDs.clear();
        }

        ImGui::Separator();

        const std::unordered_set<uint32_t>& threadIDs = Application::GetInstance().GetModelHandler().GetThreadIDs();

        for (const auto& threadID : threadIDs)
        {
            bool enabled = IsThreadIDEnabled(threadID);

            if (ImGui::Checkbox(std::format("{0}", threadID).c_str(), &enabled))
            {
                if (enabled)
                {
                    mDisabledThreadIDs.erase(threadID);
                }
                else
                {
                    mDisabledThreadIDs.emplace(threadID);
                }
            }
        }

        ImGui::EndCombo();
    }
}

void ViewHandler::RenderFileHeaderValidationError()
{
    GMDGLogFileValidationResult result = Application::GetInstance().GetModelHandler().GetFileValidationError();
    
    if (result != GMDG_SUCCESS && result >= GMDG_SUCCESS && result <= GMDG_UNUPPORTED_FILE_HEADER)
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, { 1.0f, 0.0f, 0.0f, 1.0f });
        switch (result)
        {
        case GMDG_INVALID_MAGIC:
        {
            ImGui::TextUnformatted("Loaded file was not a gmdg_logger file!");
        } break;
        case GMDG_UNUPPORTED_VERSION:
        {
            ImGui::TextUnformatted("Loaded file version in not supported!");
        } break;
        case GMDG_UNUPPORTED_FILE_HEADER:
        {
            ImGui::TextUnformatted("Loaded file is corrupted!");
        } break; 
        case GMDG_UNKNOWN:
        {
            ImGui::TextUnformatted("Unknwon error while loading the file!");
        } break;        
        }
        ImGui::PopStyleColor();
    }
}

void ViewHandler::RenderTable()
{
    if (ImGui::BeginTable("logs", 5,
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn("Time");
        ImGui::TableSetupColumn("Thread ID");
        ImGui::TableSetupColumn("Severity");
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Message");

        ImGui::TableHeadersRow();

        const std::vector<LogRecord>& logs = Application::GetInstance().GetModelHandler().GetLogs();

        for (const auto& log : logs)
        {
            if (!IsSeverityEnabled(log.severity)) continue;
            if (!IsThreadIDEnabled(log.thread_id)) continue;

            ImGui::TableNextRow();

            const std::string ts = std::format("{:%F %T}", std::chrono::floor<std::chrono::milliseconds>(std::chrono::sys_time<std::chrono::nanoseconds>{std::chrono::nanoseconds{log.timestamp_ns}}));

            ImGui::PushStyleColor(ImGuiCol_Text, SeverityToColor(log.severity));

            // Columns
            {
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(ts.c_str());
                
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", log.thread_id);
                
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(GMDG_Logger_Severity_To_String(static_cast<GMDGLogSeverity>(log.severity)));
                
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(log.category.c_str());
                
                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(log.message.c_str());
            }
         
            ImGui::PopStyleColor();
        }

        ImGui::EndTable();
    }
}

ImVec4 ViewHandler::SeverityToColor(uint32_t t_severity)
{
    switch (t_severity)
    {
    case GMDG_LOG_DEBUG:   return { 0.0f, 1.0f, 1.0f, 1.0f };
    case GMDG_LOG_INFO:    return { 0.0f, 1.0f, 0.0f, 1.0f };
    case GMDG_LOG_WARNING: return { 1.0f, 1.0f, 0.0f, 1.0f };
    case GMDG_LOG_ERROR:   return { 1.0f, 0.0f, 0.0f, 1.0f };
    default:               return { 1.0f, 1.0f, 1.0f, 1.0f };
    }
}

bool ViewHandler::IsSeverityEnabled(uint32_t t_severity)
{
    return (mSeverityMask & (1u << t_severity)) != 0;
}

bool ViewHandler::IsThreadIDEnabled(uint32_t t_threadID)
{
    return !mDisabledThreadIDs.contains(t_threadID);
}