#include "pch.h"

#include "application.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include "version.hpp"

using namespace GMDGLoggerGUI;

void Application::Initialize()
{
    m_modelHandler.Initialize();
    m_viewHandler.Initialize();
}

void Application::Run()
{
    // Intialize
    {
        m_modelHandler.Initialize();
        m_controlHandler.Initialize();
        m_viewHandler.Initialize();
    }

    while(m_modelHandler.IsApplicationRunning())
    {
        m_modelHandler.Update();
        m_viewHandler.Update(m_controlHandler, m_modelHandler);
        m_controlHandler.Update(m_modelHandler);
    }

    // Shutdown
    {
        m_viewHandler.Shutdown();
        m_modelHandler.Shutdown();
    }

    m_modelHandler.Shutdown();
}

void Application::Shutdown()
{
    // no cleanup should be made, since the OS already clean the application memory
    m_isRunning = false;
}
