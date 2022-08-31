

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "ako.hpp"


using namespace ako;


/*static uint32_t sLfsr32(uint32_t& state)
{
    // https://en.wikipedia.org/wiki/Xorshift#Example_implementation
    uint32_t x = state;
    x ^= static_cast<uint32_t>(x << 13);
    x ^= static_cast<uint32_t>(x >> 17);
    x ^= static_cast<uint32_t>(x << 5);
    state = x;

    return x;
}*/


int main()
{
	return EXIT_SUCCESS;
}
