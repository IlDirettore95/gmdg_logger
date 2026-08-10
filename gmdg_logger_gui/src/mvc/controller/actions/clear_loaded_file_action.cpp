#include "pch.h"

#include "clear_loaded_file_action.hpp"

#include "mvc/model/model_handler.hpp"

using namespace GMDGLoggerGUI;

ActionSubscriber<ClearLoadedFileAction> ClearLoadedFileAction::s_actionRegistration("ClearLoadedFileAction");

bool ClearLoadedFileAction::Execute(void* const t_context)
{
    GMDG_ASSERT(t_context != nullptr);

    ModelHandler& modelHandler = *reinterpret_cast<ModelHandler*>(t_context);

    modelHandler.ClearLoadedFile();

    return true;
}
