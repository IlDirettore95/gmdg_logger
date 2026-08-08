#pragma once

#include <cstring>
#include <functional>
#include <unordered_map>
#include <memory>
#include <thread>

namespace GMDGLoggerGUI
{
    class ModelHandler;

    class Action
    {
    public:
        virtual ~Action() = default;

        virtual const char* GetName() const = 0;
        virtual bool Execute(void* const t_context) = 0;
    };


    // Action Registry
    using ActionKey = std::string;

    template <typename... Args>
    using ActionFactory = std::function<std::unique_ptr<Action>(Args...)>;
    template <typename... Args>
    using ActionRegistry = std::unordered_map<ActionKey, ActionFactory<Args...>>;
    template <typename... Args>
    ActionRegistry<Args...>& Registry()
    {
        static ActionRegistry<Args...> s_registry;
        return s_registry;
    }

    template <typename T, typename... Args>
    struct ActionSubscriber
    {
        ActionSubscriber(const ActionKey& t_key)
        {
            GMDG_ASSERT(!t_key.empty());

            Registry<Args...>()[t_key] = [](Args... t_args)
            {
                return std::make_unique<T>(std::forward<Args>(t_args)...);
            };
        }
    };
}