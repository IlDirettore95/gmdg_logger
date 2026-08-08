#include "mvc/view/table_layout.hpp"

#include "gmdg_asserting.hpp"

#include <algorithm>

namespace GMDGLoggerGUI::TableLayout
{
    float ClampToMinWidth(const float t_width, const float t_minWidth)
    {
        GMDG_ASSERT(t_minWidth >= 0.0f);

        return std::max(t_width, t_minWidth);
    }

    float ComputeRowHeight(const int32_t t_lineCount, const float t_lineHeight, const float t_verticalPadding)
    {
        GMDG_ASSERT(t_lineHeight > 0.0f);
        GMDG_ASSERT(t_verticalPadding >= 0.0f);

        const int32_t lineCount = std::max(t_lineCount, 1);
        return static_cast<float>(lineCount) * t_lineHeight + t_verticalPadding;
    }
}
