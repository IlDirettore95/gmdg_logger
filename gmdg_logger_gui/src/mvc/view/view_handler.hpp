#pragma once

class GLFWwindow;
struct ImVec4;

namespace GMDGLoggerGUI
{
    class ModelHandler;
    class ControlHandler;
    
    class ViewHandler
    {        
    public:
        ViewHandler() = default;
        ~ViewHandler() = default;

        void Initialize();
        void Update(ControlHandler& t_controlHandler, const ModelHandler& t_modelHandler);
        void Shutdown();

    private:
        static void GLFWErrorCallback(int32_t t_error, const char* t_description);
        void Render();
        void RenderSeverityFilter();
        void RenderAboutButton();
        void RenderTable();
        ImVec4 SeverityToColor(uint32_t t_severity);
        bool IsSeverityEnabled(uint32_t t_severity);

        GLFWwindow* mWindow;

        uint32_t mSeverityMask = 0xFFFFFFFF;
    };
}