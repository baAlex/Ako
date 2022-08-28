

#undef NDEBUG
#include <cassert>
#include <iostream>
#include <vector>

#include "ako.hpp"


using namespace ako;


static uint32_t sLfsr32(uint32_t& state)
{
	// https://en.wikipedia.org/wiki/Xorshift#Example_implementation
	uint32_t x = state;
	x ^= static_cast<uint32_t>(x << 13);
	x ^= static_cast<uint32_t>(x >> 17);
	x ^= static_cast<uint32_t>(x << 5);
	state = x;

	return x;
}


static void sTest(size_t width, size_t height, size_t channels, uint32_t seed = 1)
{
	std::cout << "Test(), " << width << "x" << height << " px, channels: " << channels << ", seed: " << seed << "\n";

	// Generate an image
	std::cout << " - Generate image\n";
	auto image = std::vector<uint8_t>();
	{
		image.resize(width * height * channels);

		for (auto& i : image)
			i = sLfsr32(seed) & UINT8_MAX;
	}

	// Encode it
	std::cout << " - Encode\n";
	void* encoded_blob = NULL;
	size_t encoded_blob_size = 0;
	{
		auto status = Status::Error;

		encoded_blob_size =
		    Encode(DefaultCallbacks(), DefaultSettings(), width, height, channels, image.data(), &encoded_blob, status);

		if (status != Status::Ok || encoded_blob == NULL)
			std::cout << StatusString(status) << "\n";

		assert(status == Status::Ok);
		assert(encoded_blob_size != 0);
		assert(encoded_blob != NULL);
	}

	// And decode!
	std::cout << " - Decode\n";
	uint8_t* decoded_image = NULL;
	{
		auto status = Status::Error;

		Settings settings = {};
		size_t width = 0;
		size_t height = 0;
		size_t channels = 0;

		decoded_image =
		    Decode(DefaultCallbacks(), encoded_blob_size, encoded_blob, settings, width, height, channels, status);

		if (status != Status::Ok || decoded_image == NULL)
			std::cout << StatusString(status) << "\n";

		assert(status == Status::Ok);
		assert(decoded_image != NULL);
	}

	// Bye!
	DefaultCallbacks().free(encoded_blob);
	DefaultCallbacks().free(decoded_image);
}


int main()
{
	sTest(300, 300, 1);
	sTest(500, 500, 4);

	sTest(640, 480, 3);
	sTest(1280, 720, 3);

	// sTest(1920, 1080, 4);
	// sTest(2048, 1024, 8);

	return 0;
}
