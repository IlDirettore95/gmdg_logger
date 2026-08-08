#pragma once

#include "action.hpp"

#include <string>

namespace GMDGLoggerGUI
{
    class OpenFileAction : public Action
    {
    private:
        static ActionSubscriber<OpenFileAction, std::string> s_actionRegistration; // runs subscription for this action

        std::string m_path;

    public:
        explicit OpenFileAction(std::string t_path);

        inline const char* GetName() const override { return "OpenFileAction"; }

        bool Execute(void* const t_context) override;
    };
}
