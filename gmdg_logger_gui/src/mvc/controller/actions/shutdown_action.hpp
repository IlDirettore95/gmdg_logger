#pragma once

#include "action.hpp"

namespace GMDGLoggerGUI
{
    class ShutdownAction : public Action
    {
    private:
        static ActionSubscriber<ShutdownAction> s_actionRegistration; // runs subscription for this action

    public:
        ShutdownAction() = default;

        inline const char* GetName() const override { return "ShutdownAction"; }

        bool Execute(void* const t_context) override;
    };
}