#pragma once

#include "action.hpp"

#include <string>

namespace GMDGLoggerGUI
{
    class ClearLoadedFileAction : public Action
    {
    private:
        static ActionSubscriber<ClearLoadedFileAction> s_actionRegistration; // runs subscription for this action

    public:
        inline const char* GetName() const override { return "ClearLoadedFileAction"; }

        bool Execute(void* const t_context) override;
    };
}