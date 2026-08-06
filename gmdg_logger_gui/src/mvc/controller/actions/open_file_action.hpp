#pragma once

#include "action.hpp"

#include <string>

namespace GMDGLoggerGUI
{
    class OpenFileAction : public Action
    {
    private:
        static ActionSubscriber<OpenFileAction, std::string> sActionRegistration; // runs subscription for this action

        std::string mPath;

    public:
        explicit OpenFileAction(std::string t_path);

        inline const char* GetName() const override { return "OpenFileAction"; }

        bool Execute(void* t_context) override;
    };
}
