#include "pch.h"

#include "open_file_action.hpp"

#include "mvc/model/model_handler.hpp"

using namespace GMDGLoggerGUI;

ActionSubscriber<OpenFileAction, std::string> OpenFileAction::s_actionRegistration("OpenFileAction");

OpenFileAction::OpenFileAction(std::string t_path)
{
    GMDG_ASSERT(!t_path.empty());

    m_path = std::move(t_path);
}

bool OpenFileAction::Execute(void* const t_context)
{
    GMDG_ASSERT(t_context != nullptr);

    ModelHandler& modelHandler = *reinterpret_cast<ModelHandler*>(t_context);

    modelHandler.LoadFile(m_path);

    return true;
}
