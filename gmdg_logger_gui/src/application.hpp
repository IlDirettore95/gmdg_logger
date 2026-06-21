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
            static Application application;
            return application;
        }
    
    public:
        Application();
        ~Application() = default;
        Application(const Application& t_application) = delete;
        Application& operator = (const Application& t_application) = delete; 
    
    public:
        void Initialize();
        void Run();
        void Shutdown();

        inline ModelHandler& GetModelHandler() { return mModelHandler; } 
        inline const ModelHandler& GetModelHandler() const { return mModelHandler; } 
        inline ViewHandler& GetViewHandler() { return mViewHandler; } 
        inline const ViewHandler& GetViewHandler() const { return mViewHandler; } 
        inline ControlHandler& GetControlHandler() { return mControlHandler; } 
        inline const ControlHandler& GetControlHandler() const { return mControlHandler; } 

    private:
        bool mIsRunning;

        ModelHandler mModelHandler;
        ViewHandler mViewHandler;
        ControlHandler mControlHandler;
    };
}
