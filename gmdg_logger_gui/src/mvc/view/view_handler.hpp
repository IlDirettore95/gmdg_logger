#pragma once

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
        void Render();
        void RenderSeverityFilter();
        void RenderThreadIDFilter();
        void RenderAboutButton();
        void RenderFileHeaderValidationError();
        void RenderTable();
        ImVec4 SeverityToColor(uint32_t t_severity);
        bool IsSeverityEnabled(uint32_t t_severity);
        bool IsThreadIDEnabled(uint32_t t_threadID);

        GLFWwindow* mWindow;

        uint32_t mSeverityMask = 0xFFFFFFFF;
        std::unordered_set<uint32_t> mDisabledThreadIDs; 
    };
}