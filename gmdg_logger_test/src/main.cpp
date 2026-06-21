#define GMDG_LOGGER_ENABLED
#include "gmdg_logger.hpp"

int main()
{
    GMDG_Logger_Initialize("app.log");

    LOG_DEBUG("APPLICATION", "This is a debug");
    LOG_INFO("APPLICATION", "This is an info");
    LOG_WARNING("APPLICATION", "This is a warning");
    LOG_ERROR("APPLICATION", "This is an error");

    GMDG_Logger_Shutdown();

    return 0;
}