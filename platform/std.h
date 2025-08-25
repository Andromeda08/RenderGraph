#pragma once

/* Missing Standard Library functions for Apple platforms */
#ifdef __APPLE__
#include <ranges>

using std::ranges::viewable_range;
using std::ranges::zip_view;
using std::ranges::iota_view;

namespace std::ranges::views
{
    inline constexpr auto enumerate = [](viewable_range auto&& r){
        return zip_view(iota_view(0), r);
    };
}
#endif
