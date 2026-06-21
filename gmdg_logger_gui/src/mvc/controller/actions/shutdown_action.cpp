#include "pch.h"

#include "shutdown_action.hpp"

#include "mvc/model/model_handler.hpp"

using namespace GMDGLoggerGUI;

ActionSubscriber<ShutdownAction> ShutdownAction::sActionRegistration("ShutdownAction");

bool ShutdownAction::Execute(void* t_context)
{        
    ModelHandler& modelHandler = *reinterpret_cast<ModelHandler*>(t_context);
    
    modelHandler.ShutdownApplication();

    return true;
}