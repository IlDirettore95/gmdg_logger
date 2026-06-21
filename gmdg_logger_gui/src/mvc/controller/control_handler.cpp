#include "pch.h"

#include "control_handler.hpp"

#include "mvc/controller/actions/action.hpp"

using namespace GMDGLoggerGUI;

void ControlHandler::Initialize() {}

void ControlHandler::Update(ModelHandler& t_modelHandler)
{
    ExecuteActions(t_modelHandler);
}

void ControlHandler::ExecuteActions(ModelHandler& t_modelHandler)
{
    for (auto& action : mActionQueue)
    {
        bool result = action->Execute(&t_modelHandler);
    }

    mActionQueue.clear();
}