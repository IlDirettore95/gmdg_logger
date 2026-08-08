#include "pch.h"

#include "shutdown_action.hpp"

#include "mvc/model/model_handler.hpp"

using namespace GMDGLoggerGUI;

ActionSubscriber<ShutdownAction> ShutdownAction::s_actionRegistration("ShutdownAction");

bool ShutdownAction::Execute(void* const t_context)
{
    GMDG_ASSERT(t_context != nullptr);

    ModelHandler& modelHandler = *reinterpret_cast<ModelHandler*>(t_context);

    modelHandler.ShutdownApplication();

    return true;
}