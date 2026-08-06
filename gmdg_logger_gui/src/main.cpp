#include "pch.h"

#include "application.hpp"

// Hint to hybrid-GPU (NVIDIA Optimus / AMD PowerXpress) laptops to run this app on the
// discrete GPU instead of the integrated one, which drivers pick up by symbol name.
extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int32_t main()
{
    using namespace GMDGLoggerGUI;

    Application& application = Application::GetInstance();
    application.Run();

    return 0;
}