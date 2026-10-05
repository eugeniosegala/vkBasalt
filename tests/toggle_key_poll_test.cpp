#include "toggle_key_poll.hpp"

#include <cassert>
#include <chrono>

int main()
{
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    vkBasalt::ToggleKeyPoll poll;
    const auto start = Clock::time_point{} + 10s;

    // The first present always reads the key.
    assert(poll.due(start));

    // Repeated presents inside one interval do not query the display server,
    // regardless of how many outputs a frame-generation layer submits.
    for (int output = 1; output < 12; output++)
        assert(!poll.due(start + output * 4ms));

    assert(poll.due(start + vkBasalt::ToggleKeyPoll::interval));
    assert(!poll.due(start + vkBasalt::ToggleKeyPoll::interval + 1ms));

    // A long gap between presents polls once, then restarts the interval.
    const auto later = start + 5s;
    assert(poll.due(later));
    assert(!poll.due(later + 49ms));
    assert(poll.due(later + 50ms));

    return 0;
}
