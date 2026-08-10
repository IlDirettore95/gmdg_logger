#pragma once

#include "mvc/model/model_handler.hpp"
#include "mvc/view/view_handler.hpp"
#include "mvc/controller/control_handler.hpp"

namespace GMDGLoggerGUI
{
    class Application
    {
    public:
        inline static Application& GetInstance()
        {
            static Application s_application;
            return s_application;
        }

    public:
        Application() = default;
        ~Application() = default;
        Application(const Application& t_application) = delete;
        Application& operator = (const Application& t_application) = delete;

    public:
        void Run();
        void Shutdown();

        inline ModelHandler& GetModelHandler() { return m_modelHandler; }
        inline const ModelHandler& GetModelHandler() const { return m_modelHandler; }
        inline ViewHandler& GetViewHandler() { return m_viewHandler; }
        inline const ViewHandler& GetViewHandler() const { return m_viewHandler; }
        inline ControlHandler& GetControlHandler() { return m_controlHandler; }
        inline const ControlHandler& GetControlHandler() const { return m_controlHandler; }

    private:
        bool m_isRunning = true; // mind the bools initialization is the initial state

        ModelHandler m_modelHandler;
        ViewHandler m_viewHandler;
        ControlHandler m_controlHandler;
    };
}
