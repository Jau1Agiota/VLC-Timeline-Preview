/* GPL-2.0-or-later; see COPYING. Tests the production mapping header. */
#include "modules/gui/qt/util/seekhover.hpp"
#ifdef NDEBUG
# undef NDEBUG
#endif
#include <cassert>
#include <climits>
#include <cstdio>
#include <initializer_list>
#include <limits>

int main()
{
    using SeekHover::map;
    // Example style reports handle centers at 9 and 311, travel 302.
    assert(map(9, 0, 320, 18, 9, 311, 30021, true).timestampMs == 0);
    assert(map(160, 9, 320, 18, 9, 311, 30021, true).timestampMs == 15010);
    assert(map(311, 17, 320, 18, 9, 311, 30021, true).timestampMs == 30021);
    assert(map(9, 0, 320, 18, 9, 311, 30021, true).inside);
    assert(map(311, 17, 320, 18, 9, 311, 30021, true).inside);
    for(int x : {INT_MIN, 0, 8}) {
        auto p = map(x, 9, 320, 18, 9, 311, 30021, true);
        assert(!p.inside && p.timestampMs == 0);
    }
    for(int x : {312, 319, INT_MAX}) {
        auto p = map(x, 9, 320, 18, 9, 311, 30021, true);
        assert(!p.inside && p.timestampMs == 30021);
    }
    assert(!map(160, -1, 320, 18, 9, 311, 30021, true).inside);
    assert(!map(160, 18, 320, 18, 9, 311, 30021, true).inside);
    // Different styles, odd handle widths and resized widgets.
    assert(map(7, 5, 101, 20, 7, 93, 1000, true).timestampMs == 0);
    assert(map(93, 5, 101, 20, 7, 93, 1000, true).timestampMs == 1000);
    assert(map(50, 5, 101, 20, 7, 93, 1000, true).timestampMs == 500);
    for(int duration : {-1, 0})
        assert(map(50, 5, 101, 20, 7, 93, duration, true).timestampMs == -1);
    for(int width : {-1, 0, 17, 18}) {
        auto p = map(0, 0, width, 18, 9, 311, 30021, true);
        assert(!p.inside && p.timestampMs == -1);
    }
    assert(!map(0, 0, 320, 0, 9, 311, 30021, true).inside);
    assert(!map(0, 0, 320, 18, 9, 9, 30021, true).inside);
    assert(!map(160, 9, 320, 18, 9, 311, 30021, false).inside);
    // Native styles may reverse their endpoints (RTL/inverted appearance).
    assert(map(311, 9, 320, 18, 311, 9, 30021, true).timestampMs == 0);
    assert(map(9, 9, 320, 18, 311, 9, 30021, true).timestampMs == 30021);
    assert(map(INT_MIN, 9, 320, 18, 311, 9, 30021, true).timestampMs == 30021);
    assert(map(INT_MAX, 9, 320, 18, 311, 9, 30021, true).timestampMs == 0);
    assert(!map(10, 9, 320, 18, -1, 311, 30021, true).inside);
    assert(!map(10, 9, 320, 18, 9, 320, 30021, true).inside);
    // Explicit style offsets, not an assumed full-width groove.
    assert(map(42, 9, 320, 18, 42, 280, 1000, true).timestampMs == 0);
    assert(map(161, 9, 320, 18, 42, 280, 1000, true).timestampMs == 500);
    assert(map(280, 9, 320, 18, 42, 280, 1000, true).timestampMs == 1000);
    const int64_t longest = std::numeric_limits<int64_t>::max();
    assert(map(INT_MAX - 1, 0, INT_MAX, 1, 0, INT_MAX - 1, longest, true).timestampMs == longest);
    assert(map(INT_MIN, 0, INT_MAX, 1, INT_MAX - 1, 0, longest, true).timestampMs == longest);
    // Monotonic, bounded mapping even for durations that overflow naive products.
    for(int64_t duration : {INT64_C(1), INT64_C(30021), INT64_C(86400000),
                            std::numeric_limits<int64_t>::max()}) {
        int64_t previous = -1;
        for(int x = 0; x < 1001; ++x) {
            auto p = map(x, 10, 1001, 20, 9, 991, duration, true);
            assert(p.timestampMs >= previous && p.timestampMs <= duration);
            previous = p.timestampMs;
        }
    }
    std::puts("seekhover mapping tests passed");
}
