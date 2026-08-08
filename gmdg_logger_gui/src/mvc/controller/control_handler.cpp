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
    for (auto& action : m_actionQueue)
    {
        const bool result = action->Execute(&t_modelHandler);
        if (!result)
        {
            std::println("{0} action failed!", action->GetName());
        }
    }

    m_actionQueue.clear();
}