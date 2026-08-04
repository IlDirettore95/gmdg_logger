#include "pch.h"

#include "application.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include "version.hpp"

using namespace GMDGLoggerGUI;

// Mind the bools initialization is the initial state
Application::Application()
    : mIsRunning(true) {}

void Application::Initialize()
{
    mModelHandler.Initialize();
    mViewHandler.Initialize();   
}

void Application::Run()
{    
    // Intialize
    {        
        mModelHandler.Initialize();
        mControlHandler.Initialize();
        mViewHandler.Initialize();
    }

    while(mModelHandler.IsApplicationRunning())
    {
        mModelHandler.Update();
        mViewHandler.Update(mControlHandler);
        mControlHandler.Update(mModelHandler);
    }

    // Shutdown
    {
        mViewHandler.Shutdown();
        mModelHandler.Shutdown();
    }

    mModelHandler.Shutdown();
}

void Application::Shutdown()
{
    // no cleanup should be made, since the OS already clean the application memory
    mIsRunning = false;
}