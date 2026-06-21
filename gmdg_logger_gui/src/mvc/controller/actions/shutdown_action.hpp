#pragma once

#include "action.hpp"

namespace GMDGLoggerGUI
{
    class ShutdownAction : public Action
    {
    private:
        static ActionSubscriber<ShutdownAction> sActionRegistration; // runs subscription for this action

    public:
        ShutdownAction() = default;

        inline const char* GetName() const override { return "ShutdownAction"; }

        bool Execute(void* t_context) override;
    };
}