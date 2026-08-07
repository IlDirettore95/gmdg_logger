#pragma once

#include <string>
#include <unordered_set>

struct GLFWwindow;
struct ImVec4;

namespace GMDGLoggerGUI
{
    class ControlHandler;
    
    class ViewHandler
    {        
    public:
        ViewHandler() = default;
        ~ViewHandler() = default;

        void Initialize();
        void Update(ControlHandler& t_controlHandler);
        void Shutdown();

    private:
        static void GLFWErrorCallback(int32_t t_error, const char* t_description);
        static bool OpenFileDialog(GLFWwindow* t_window, std::string& t_outPath);
        void Render(ControlHandler& t_controlHandler);
        void RenderOpenFileButton(ControlHandler& t_controlHandler);
        void RenderSeverityFilter();
        void RenderThreadIDFilter();
        void RenderCategoryFilter();
        void RenderSearchBar();
        void RenderAboutButton();
        void RenderFileHeaderValidationError();
        void RenderTable();
        ImVec4 SeverityToColor(uint32_t t_severity);
        bool IsSeverityEnabled(uint32_t t_severity);
        bool IsThreadIDEnabled(uint32_t t_threadID);
        bool IsCategoryEnabled(const std::string& t_category);
        bool IsMessageMatchingSearch(const std::string& t_message);

        GLFWwindow* mWindow;

        uint32_t mSeverityMask = 0xFFFFFFFF;
        std::unordered_set<uint32_t> mDisabledThreadIDs;
        std::unordered_set<std::string> mDisabledCategories;
        char mSearchBuffer[256] = {};
    };
}