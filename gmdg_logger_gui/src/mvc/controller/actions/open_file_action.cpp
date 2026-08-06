#include "pch.h"

#include "open_file_action.hpp"

#include "mvc/model/model_handler.hpp"

using namespace GMDGLoggerGUI;

ActionSubscriber<OpenFileAction, std::string> OpenFileAction::sActionRegistration("OpenFileAction");

OpenFileAction::OpenFileAction(std::string t_path)
    : mPath(std::move(t_path)) {}

bool OpenFileAction::Execute(void* t_context)
{
    ModelHandler& modelHandler = *reinterpret_cast<ModelHandler*>(t_context);

    modelHandler.LoadFile(mPath);

    return true;
}
