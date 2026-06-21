#include "pch.h"

#include "application.hpp"

int32_t main()
{
    using namespace GMDGLoggerGUI;

    Application& application = Application::GetInstance();
    application.Run();

    return 0;
}