#ifndef TOGGLE_KEY_POLL_HPP_INCLUDED
#define TOGGLE_KEY_POLL_HPP_INCLUDED

#include <chrono>

namespace vkBasalt
{
    // Reading the toggle key is a synchronous display-server round trip. A
    // frame-generation layer above vkBasalt can present several times per
    // game frame, so poll on a short wall-clock interval instead of on every
    // present. Ordinary key presses last longer than the interval.
    class ToggleKeyPoll
    {
    public:
        static constexpr std::chrono::milliseconds interval{50};

        bool due(const std::chrono::steady_clock::time_point now)
        {
            if (now < nextPoll)
                return false;
            nextPoll = now + interval;
            return true;
        }

    private:
        std::chrono::steady_clock::time_point nextPoll = std::chrono::steady_clock::time_point::min();
    };
} // namespace vkBasalt

#endif // TOGGLE_KEY_POLL_HPP_INCLUDED
