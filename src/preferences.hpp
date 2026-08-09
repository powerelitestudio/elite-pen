#pragma once

#include "elite_pen/core.hpp"

#include <windows.h>

#include <string>

namespace elite_pen::win {

struct Preferences {
    bool confirm_clear{false};
    bool exclude_palette_from_capture{true};
    bool start_in_interact_mode{false};
    bool highlight_cursor{false};
    int fade_seconds{0};
    bool has_palette_position{false};
    int palette_x{};
    int palette_y{};
    int palette_size{1};
    float zoom_factor{2.0F};
    int zoom_view{0};
    bool zoom_invert{false};
    float thickness{4.0F};
    Color color{kBlack};
};

class PreferencesStore {
public:
    PreferencesStore();

    [[nodiscard]] Preferences load() const;
    bool save(const Preferences& preferences) const;
    [[nodiscard]] const std::wstring& path() const noexcept { return path_; }
    [[nodiscard]] bool portable() const noexcept { return portable_; }

private:
    std::wstring path_;
    bool portable_{};
};

}  // namespace elite_pen::win
