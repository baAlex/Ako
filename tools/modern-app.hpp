

#ifndef MODERN_APP_CPP
#define MODERN_APP_CPP

#include <fstream>
#include <iostream>
#include <memory>
#include <vector>


namespace Modern // Cpp newbie here! (a sarcastic one)
{
struct Error
{
	std::string info;

	Error(const std::string info = "Error with no information provided")
	{
		this->info = info;
	}
};

inline int CheckArgumentSingle(const std::string arg1, const std::string arg2,
                               const std::vector<std::string>::const_iterator& it)
{
	if (*it == arg1 || *it == arg2)
		return 0;

	return 1;
}

inline int CheckArgumentPair(const std::string arg1, const std::string arg2,
                             std::vector<std::string>::const_iterator& it,
                             const std::vector<std::string>::const_iterator& it_end)
{
	if (*it == arg1 || *it == arg2)
	{
		if ((++it) != it_end)
			return 0;
		else
			throw Modern::Error("No value provided for '" + arg1 + "/" + arg2 + "'");
	}

	return 1;
}
} // namespace Modern

#endif
