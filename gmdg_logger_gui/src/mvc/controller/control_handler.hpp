#pragma once

#include "actions/action.hpp"

namespace GMDGLoggerGUI
{
    class ModelHandler;

    class ControlHandler
    {
    public:
        // Action Queue
        using ActionQueue = std::vector<std::unique_ptr<Action>>;
        
        template <typename... Args>
        void AddAction(const ActionKey& t_key, Args&&... args)
        {
            GMDG_ASSERT_WITH_MESSAGE(Registry<Args...>().contains(t_key), "unregistered action key: {}", t_key);
            if (!Registry<Args...>().contains(t_key)) return;

            mActionQueue.push_back(Registry<Args...>().at(t_key)(std::forward<Args>(args)...));
        }

    private:
        ActionQueue mActionQueue;

        void ExecuteActions(ModelHandler& t_modelHandler);

    public:
        ControlHandler() = default;
        ~ControlHandler() = default;

        void Initialize();
        void Update(ModelHandler& t_modelHandler);

        inline const ActionQueue& GetActionQueue() const { return mActionQueue; }
        inline ActionQueue& GetActionQueue() { return mActionQueue; }
    };
}