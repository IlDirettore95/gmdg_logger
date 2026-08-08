#pragma once

#include <cstdint>

namespace GMDGLoggerGUI::TableLayout
{
    // Clamps a candidate column width up to t_minWidth. ImGui tables have no native
    // per-column minimum-width floor beyond a few px, so this is used both to seed a
    // column's initial width and, every frame, to snap a user-dragged column back up.
    [[nodiscard]] float ClampToMinWidth(const float t_width, const float t_minWidth);

    // Converts a wrapped-text line count into a final table row height. t_lineCount is
    // always treated as >= 1 (even an empty message occupies one line).
    [[nodiscard]] float ComputeRowHeight(const int32_t t_lineCount, const float t_lineHeight, const float t_verticalPadding);
}
