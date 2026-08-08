#pragma once

#include <string>
#include <unordered_set>
#include <vector>

struct GLFWwindow;
struct ImVec4;

namespace GMDGLoggerGUI
{
    class ControlHandler;
    class ModelHandler;
    struct LogRecord;

    class ViewHandler
    {
    public:
        ViewHandler() = default;
        ~ViewHandler() = default;

        void Initialize();
        void Update(ControlHandler& t_controlHandler, ModelHandler& t_modelHandler);
        void Shutdown();

    private:
        static void GLFWErrorCallback(const int32_t t_error, const char* const t_description);
        static bool OpenFileDialog(GLFWwindow* const t_window, std::string& t_outPath);
        void Render(ControlHandler& t_controlHandler, ModelHandler& t_modelHandler);
        void RenderOpenFileButton(ControlHandler& t_controlHandler);
        void RenderSeverityFilter();
        void RenderThreadFilter();
        void RenderCategoryFilter();
        void RenderSearchBar();
        void RenderAboutButton();
        void RenderFileHeaderValidationError();
        void RenderTable();
        void RecomputeRowLayout(const std::vector<LogRecord>& t_logs, const std::vector<uint32_t>& t_visible, const float t_messageColumnWidth);
        ImVec4 SeverityToColor(const uint32_t t_severity);
        bool IsSeverityEnabled(const uint32_t t_severity);
        bool IsThreadEnabled(const std::string& t_threadName);
        bool IsCategoryEnabled(const std::string& t_category);
        bool IsMessageMatchingSearch(const std::string& t_message);

        GLFWwindow* m_window = nullptr;

        uint32_t m_severityMask = 0xFFFFFFFF;
        std::unordered_set<std::string> m_disabledThreadNames;
        std::unordered_set<std::string> m_disabledCategories;
        char m_searchBuffer[256] = {};

        std::vector<uint32_t> m_cachedVisibleIndices;
        std::vector<float> m_cachedRowHeights;   // parallel to m_cachedVisibleIndices
        std::vector<float> m_cachedRowTopY;      // prefix sum, size == m_cachedRowHeights.size() + 1
        float m_cachedMessageColumnWidth = -1.0f;
    };
}
