#include "stb_image.h"
#include "stb_image_dds.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

static std::vector<unsigned char> dds(uint32_t width, uint32_t height, uint32_t fourcc = 0, uint32_t mipmaps = 0, bool cube = false)
{
    std::array<uint32_t, 32> header{};
    header[0]  = 0x20534444;
    header[1]  = 124;
    header[2]  = 0x1007;
    header[3]  = height;
    header[4]  = width;
    header[7]  = mipmaps;
    header[19] = 32;
    header[20] = fourcc ? 4 : 0x40;
    header[21] = fourcc;
    header[22] = 32;
    header[27] = 0x1000 | (mipmaps ? 0x400000 : 0);
    header[28] = cube ? 0xfe00 : 0;
    std::vector<unsigned char> result(sizeof(header));
    std::memcpy(result.data(), header.data(), sizeof(header));
    return result;
}
static bool accepted(const std::vector<unsigned char>& data)
{
    int        x = 0, y = 0, c = 0;
    auto*      pixels = stbi_dds_load_from_memory(data.data(), data.size(), &x, &y, &c, 4);
    const bool result = pixels != nullptr;
    stbi_image_free(pixels);
    return result;
}
int main()
{
    assert(!accepted(dds(1, 1)));                     // truncated raw texture
    assert(!accepted(dds(65536, 16384, 0x31545844))); // former 32-bit overflow
    assert(!accepted(dds(0, 1)));
    assert(!accepted(dds(4, 4, 0x30315844)));     // unsupported DX10 extension
    assert(!accepted(dds(4, 4, 0x31545844, 32))); // invalid shift/mip count
    auto raw = dds(1, 1);
    raw.resize(raw.size() + 4, 255);
    assert(accepted(raw));
    for (uint32_t fourcc : {0x31545844u, 0x32545844u, 0x33545844u, 0x34545844u, 0x35545844u})
    {
        const auto blockSize = fourcc == 0x31545844u ? 8 : 16;
        auto       data      = dds(5, 3, fourcc);
        data.resize(128 + 2 * blockSize, 0);
        assert(accepted(data));
        data.pop_back();
        assert(!accepted(data));
    }
    auto mip = dds(8, 8, 0x31545844, 4);
    mip.resize(128 + 32 + 8 + 8 + 8, 0);
    assert(accepted(mip));
    mip.pop_back();
    assert(!accepted(mip));
    auto cube = dds(4, 4, 0x31545844, 0, true);
    cube.resize(128 + 6 * 8, 0);
    assert(accepted(cube));
    cube.pop_back();
    assert(!accepted(cube));
    // Header truncation and unsupported raw channel counts fail closed.
    for (size_t size = 0; size < 128; ++size)
    {
        auto bad = raw;
        bad.resize(size);
        assert(!accepted(bad));
    }
    auto     channels = raw;
    uint32_t bits     = 128;
    std::memcpy(channels.data() + 22 * 4, &bits, 4);
    assert(!accepted(channels));
}
