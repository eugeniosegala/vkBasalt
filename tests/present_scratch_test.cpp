#include "present_scratch.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include <type_traits>

namespace { size_t allocations = 0; }
void* operator new(size_t bytes)
{
    ++allocations;
    if (auto* result = std::malloc(bytes ? bytes : 1)) return result;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, size_t) noexcept { std::free(pointer); }

template<typename T> T handle(size_t value)
{
    if constexpr (std::is_pointer_v<T>) return reinterpret_cast<T>(value);
    else return static_cast<T>(value);
}

int main()
{
    for (size_t waits : {0u, 1u, 4u, 5u, 129u})
    {
        for (size_t swapchains : {0u, 1u, 4u, 5u, 97u})
        {
            const auto before = allocations;
            vkBasalt::PresentScratch scratch(waits, swapchains);
            if (waits <= 4 && swapchains <= 4) assert(allocations == before);
            for (size_t i = 0; i < waits; ++i)
                assert(scratch.waitStages()[i] == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            for (size_t i = 0; i < swapchains; ++i)
                scratch.semaphores()[i] = handle<VkSemaphore>(i + 1);
            // All entries remain live together until the downstream present.
            for (size_t i = 0; i < swapchains; ++i)
                assert(scratch.semaphores()[i] == handle<VkSemaphore>(i + 1));
        }
    }
    std::cout << "Presentation scratch allocation and overflow checks passed\n";
}
