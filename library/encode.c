/*

MIT License

Copyright (c) 2021 Alexander Brandt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#include "ako-private.h"


size_t akoEncodeExt(const struct akoCallbacks* c, const struct akoSettings* s, size_t channels, size_t image_w,
                    size_t image_h, const void* in, void** out, enum akoStatus* out_status)
{
	enum akoStatus status;
	struct akoHead* h;

	size_t blob_size = 0;
	void* blob = NULL;

	// Check callbacks and settings
	const struct akoCallbacks checked_c = (c != NULL) ? *c : akoDefaultCallbacks();
	const struct akoSettings checked_s = (s != NULL) ? *s : akoDefaultSettings();

	if (checked_c.malloc == NULL || checked_c.realloc == NULL || checked_c.free == NULL)
	{
		status = AKO_INVALID_CALLBACKS;
		goto return_failure;
	}

	// Allocate blob
	blob_size = sizeof(struct akoHead);
	blob = checked_c.malloc(blob_size);

	if (blob == NULL)
	{
		status = AKO_NO_ENOUGH_MEMORY;
		goto return_failure;
	}

	// Create head
	h = blob;
	if ((status = akoHeadWrite(channels, image_w, image_h, &checked_s, h)) != AKO_OK)
		goto return_failure;

	// Bye!
	if (out != NULL)
		*out = blob;
	else
		checked_c.free(blob); // Discard encoded data

	if (out_status != NULL)
		*out_status = AKO_OK;

	return blob_size;

return_failure:
	if (blob != NULL)
		checked_c.free(blob);

	if (out_status != NULL)
		*out_status = status;

	return 0;
}
