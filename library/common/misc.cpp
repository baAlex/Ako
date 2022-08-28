/*

MIT License

Copyright (c) 2021-2022 Alexander Brandt

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

#include "ako.hpp"


ako::Settings ako::DefaultSettings()
{
	Settings settings = {};
	return settings;
}


#ifndef AKO_FREESTANDING
#include <stdlib.h>
ako::Callbacks ako::DefaultCallbacks()
{
	Callbacks callbacks = {};
	callbacks.malloc = malloc;
	callbacks.realloc = realloc;
	callbacks.free = free;

	return callbacks;
}
#endif


const char* ako::StatusString(ako::Status s)
{
	switch (s)
	{
	case ako::Status::Error: return "Non specified error";
	case ako::Status::NotImplemented: return "Not implemented";
	default: return "Ok";
	}
}


int ako::VersionMajor()
{
	return ako::VERSION_MAJOR;
}

int ako::VersionMinor()
{
	return ako::VERSION_MINOR;
}

int ako::VersionPatch()
{
	return ako::VERSION_PATCH;
}

int ako::FormatVersion()
{
	return ako::FORMAT_VERSION;
}
