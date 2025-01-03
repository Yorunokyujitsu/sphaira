#include "ui/scrollbar.hpp"
#include "ui/nvg_util.hpp"

namespace sphaira::ui {

ScrollBar::ScrollBar(Vec4 bounds, float entry_height, std::size_t entries)
    : m_bounds{bounds}
    , m_entries{entries}
    , m_entry_height{entry_height} {
    Setup();
}

auto ScrollBar::OnLayoutChange() -> void {

}

auto ScrollBar::Draw(NVGcontext* vg, Theme* theme) -> void {
    if (m_should_draw) {
        gfx::drawRect(vg, m_pos, theme->elements[ThemeEntryID_SCROLLBAR].colour);
    }
}

auto ScrollBar::Setup(Vec4 bounds, float entry_height, std::size_t entries) -> void {
    m_bounds = bounds;
    m_entry_height = entry_height;
    m_entries = entries;
    Setup();
}

auto ScrollBar::Setup() -> void {
    m_bounds.y += 5.f;
    m_bounds.h = std::min(m_bounds.h - 10.f, m_bounds.h * 0.94f);

    const float total_size = m_entry_height * m_entries;
    m_should_draw = (total_size > m_bounds.h);

    if (m_should_draw) {
        m_step_size = m_bounds.h / m_entries;
        m_pos = {m_bounds.x, m_bounds.y, 2.f, std::max(m_bounds.h * (m_bounds.h / total_size), 10.f)};
    }
}

auto ScrollBar::Move(Direction direction) -> void {
    switch (direction) {
        case Direction::DOWN:
            if (m_index < (m_entries - 1)) {
                m_index++;
                m_pos.y += m_step_size;
            }
            break;
        case Direction::UP:
            if (m_index != 0) {
                m_index--;
                m_pos.y -= m_step_size;
            }
            break;
    }
}

} // namespace sphaira::ui
