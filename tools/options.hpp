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


#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


const size_t OPTION_PRINT_LEN = 75;


template <typename U> U ToNumber(const std::string& str);
template <> inline int ToNumber<int>(const std::string& str)
{
	return std::stoi(str);
}
template <> inline float ToNumber<float>(const std::string& str)
{
	return std::stof(str);
}
template <> inline unsigned long ToNumber<unsigned long>(const std::string& str)
{
	return std::stoul(str);
}


template <typename T> class Option
{
  private:
	bool user_set = false;
	size_t order;
	size_t category;
	std::string description;
	T default_value;
	T value;
	T min_value;
	T max_value;

  public:
	Option(size_t order, const std::string& description, T default_value, T min_value, T max_value, size_t category = 0)
	{
		this->order = order;
		this->category = category;
		this->description = description;
		this->default_value = default_value;
		this->value = default_value;
		this->min_value = min_value;
		this->max_value = max_value;
	}

	// clang-format off
	bool        get_user_set() const      { return user_set; }
	size_t      get_order() const         { return order; }
	size_t      get_category() const      { return category; }
	T           get_value() const         { return value; }
	std::string get_description() const   { return description; }
	std::string get_default_value() const { return std::to_string(default_value); }
	// clang-format on

	void parse(const std::string& str)
	{
		user_set = true;
		T n = ToNumber<T>(str);

		if (n < min_value)
			value = min_value;
		else if (n > max_value)
			value = max_value;
		else
			value = n;
	}
};


template <> class Option<std::string>
{
  private:
	bool user_set = false;
	size_t order;
	size_t category;
	std::string description;
	std::string default_value;
	std::string value;
	std::string constrains;
	int value_index;

	int find_in_constrains(const std::string& str, const std::string& constrains)
	{
		size_t token_start = 0;
		size_t token_end = 0;
		int index = 0;

		while (token_end != std::string::npos)
		{
			// Tokenize constrains
			token_end = constrains.find(' ', token_end + 1);
			const std::string token = constrains.substr(token_start, token_end - token_start);
			token_start = token_end + 1;

			// Compare
			bool equals = (str.length() == token.length()) ? true : false;
			if (equals == true)
			{
				for (size_t i = 0; i < token.length(); i++)
				{
					if (std::toupper(str[i]) != std::toupper(token[i]))
					{
						equals = false;
						break;
					}
				}
			}

			// Is there?
			if (equals == true)
				return index;

			index++; // Nop, next step
		}

		return -1;
	}

  public:
	Option(size_t order, const std::string& description, const std::string& default_value,
	       const std::string& constrains, size_t category = 0)
	{
		this->order = order;
		this->category = category;
		this->description = description;
		this->default_value = default_value;
		this->constrains = constrains;
		this->value = default_value;
		this->value_index = find_in_constrains(default_value, constrains);
	}

	// clang-format off
	bool        get_user_set() const      { return user_set; }
	size_t      get_order() const         { return order; }
	size_t      get_category() const      { return category; }
	std::string get_value() const         { return value; }
	int         get_value_index() const   { return value_index; }
	std::string get_description() const   { return description; }
	std::string get_default_value() const { return default_value; }
	// clang-format on

	void parse(const std::string& str)
	{
		user_set = true;

		// Just set
		if (constrains == "")
		{
			value = str;
			return;
		}

		// Constrained options need to match
		const auto index = find_in_constrains(str, constrains);
		if (index != -1)
		{
			value = str;
			value_index = index;

			for (auto& c : value)
				c = std::toupper(c);
		}
		else
			throw "Invalid Value";
	}
};


template <> class Option<bool>
{
  private:
	bool user_set = false;
	size_t order;
	size_t category;
	std::string description;
	bool default_value;
	bool value;

  public:
	Option(size_t order, const std::string& description, size_t category = 0)
	{
		this->order = order;
		this->category = category;
		this->description = description;
		this->default_value = false;
		this->value = false;
	}

	// clang-format off
	bool        get_user_set() const      { return user_set; }
	size_t      get_order() const         { return order; }
	size_t      get_category() const      { return category; }
	bool        get_value() const         { return user_set; }
	std::string get_description() const   { return description; }
	std::string get_default_value() const { return ""; }
	// clang-format on

	void parse(const std::string& str)
	{
		user_set = true;
		std::string uppercase_str;

		size_t i = 0;
		for (auto& c : str)
			uppercase_str[i++] = std::toupper(c);

		value = (uppercase_str == "FALSE" || uppercase_str == "NO") ? false : true;
	}
};


class OptionsManager
{
  private:
	size_t counter = 0;
	std::vector<std::string> categories;

	std::unordered_map<std::string, std::shared_ptr<Option<int>>> integer_options;
	std::unordered_map<std::string, std::shared_ptr<Option<float>>> float_options;
	std::unordered_map<std::string, std::shared_ptr<Option<unsigned long>>> ulong_options;
	std::unordered_map<std::string, std::shared_ptr<Option<std::string>>> string_options;
	std::unordered_map<std::string, std::shared_ptr<Option<bool>>> bool_options;

	int parse_pair(const std::string& key, const std::string& value)
	{
		try
		{
			if (integer_options.count(key) != 0)
				integer_options.at(key)->parse(value);
			else if (float_options.count(key) != 0)
				float_options.at(key)->parse(value);
			else if (ulong_options.count(key) != 0)
				ulong_options.at(key)->parse(value);
			else if (string_options.count(key) != 0)
				string_options.at(key)->parse(value);
			else if (bool_options.count(key) != 0)
			{
				bool_options.at(key)->parse("true");
				return 0; // TODO, magic numbers
			}
			else
				return 2; // Unknown option
		}
		catch (...)
		{
			return 3; // Invalid value
		}

		return 1;
	}

	int parse_single(const std::string& key)
	{
		if (integer_options.count(key) != 0 || float_options.count(key) != 0 || ulong_options.count(key) != 0 ||
		    string_options.count(key) != 0)
			return 1; // No value specified (in an option that requires a key-value pair)
		else if (bool_options.count(key) == 0)
			return 2; // Unknown option

		bool_options.at(key)->parse("true");
		return 0;
	}

	struct GatheredInfo
	{
		size_t order;
		size_t category;
		const char* long_command;
		const char* short_command;
		std::string description;
		std::string default_value;
	};

	template <typename T>
	void gather_options_info(const std::unordered_map<std::string, T>& hashmap, std::vector<GatheredInfo>& list) const
	{
		for (const auto& item : hashmap)
		{
			int already_listed = false; // Hashmap keep options under two keys
			size_t at = 0;

			// We already visited this option?...
			for (size_t i = 0; i < list.size(); i++)
			{
				if (item.second->get_order() == list[i].order)
				{
					already_listed = true;
					at = i;
					break;
				}
			}

			// Nop, gather its information
			if (already_listed == false)
			{
				const GatheredInfo info = {
				    item.second->get_order(), item.second->get_category(),    item.first.c_str(),
				    item.first.c_str(),       item.second->get_description(), item.second->get_default_value()};
				list.emplace_back(info);
			}

			// Yes, update short/long commands
			else
			{
				list[at].short_command = item.first.c_str();

				if (std::strlen(list[at].short_command) > std::strlen(list[at].long_command))
				{
					list[at].short_command = list[at].long_command;
					list[at].long_command = item.first.c_str();
				}
			}
		}
	}

	bool is_vowel(char c) const
	{
		if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || // This feels horrible
		    c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U')
			return true;

		return false;
	}

	void print_with_wrap(const std::string& str) const
	{
		size_t column = 4;
		bool break_line = false;

		std::cout << "    ";

		for (auto& c : str)
		{
			if (((++column) % OPTION_PRINT_LEN) == 0)
				break_line = true;

			// Works in spanish ¯\_(ツ)_/¯
			if (break_line == true && (is_vowel(c) == true || c == ' ' || c == '(' || c == ')'))
			{
				break_line = false;
				std::cout << ((is_vowel(c) == true) ? "-" : "");
				std::cout << "\n    ";

				if (c == ' ')
					continue;
			}

			std::cout << c;
		}

		std::cout << "\n";
	}

  public:
	OptionsManager() {}

	// clang-format off
	int           get_integer(const std::string& command) const      { return integer_options.at(command)->get_value(); }
	float         get_float(const std::string& command) const        { return float_options.at(command)->get_value(); }
	unsigned long get_ulong(const std::string& command) const        { return ulong_options.at(command)->get_value(); }
	std::string   get_string(const std::string& command) const       { return string_options.at(command)->get_value(); }
	int           get_string_index(const std::string& command) const { return string_options.at(command)->get_value_index(); }
	bool          get_bool(const std::string& command) const         { return bool_options.at(command)->get_value(); }
	// clang-format on

	size_t add_category(const std::string& name)
	{
		categories.emplace_back(name);
		return categories.size();
	}

	void add_integer(const std::string& short_command, const std::string& long_command, const std::string& description,
	                 int default_value, int min_value, int max_value, size_t category = 0)
	{
		const auto option =
		    std::make_shared<Option<int>>(counter++, description, default_value, min_value, max_value, category);
		integer_options[short_command] = option;
		integer_options[long_command] = option;
	}

	void add_float(const std::string& short_command, const std::string& long_command, const std::string& description,
	               float default_value, float min_value, float max_value, size_t category = 0)
	{
		const auto option =
		    std::make_shared<Option<float>>(counter++, description, default_value, min_value, max_value, category);
		float_options[short_command] = option;
		float_options[long_command] = option;
	}

	void add_ulong(const std::string& short_command, const std::string& long_command, const std::string& description,
	               unsigned long default_value, unsigned long min_value, unsigned long max_value, size_t category = 0)
	{
		const auto option = std::make_shared<Option<unsigned long>>(counter++, description, default_value, min_value,
		                                                            max_value, category);
		ulong_options[short_command] = option;
		ulong_options[long_command] = option;
	}

	void add_string(const std::string& short_command, const std::string& long_command, const std::string& description,
	                const std::string& default_value, const std::string& constrains, size_t category = 0)
	{
		const auto option =
		    std::make_shared<Option<std::string>>(counter++, description, default_value, constrains, category);
		string_options[short_command] = option;
		string_options[long_command] = option;
	}

	void add_bool(const std::string& short_command, const std::string& long_command, const std::string& description,
	              size_t category = 0)
	{
		const auto option = std::make_shared<Option<bool>>(counter++, description, category);
		bool_options[short_command] = option;
		bool_options[long_command] = option;
	}

	int parse_arguments(int argc, const char* argv[])
	{
		int i = 1;

		// Parse key-value pairs, even if a option doesn't require a value
		for (; i < (argc - 1);)
		{
			switch (parse_pair(argv[i], argv[i + 1]))
			{
			case 1: i += 2; break;
			case 0: i += 1; break;
			case 2: std::cerr << "Error, unknown option '" << argv[i] << "'.\n"; return 1;
			case 3:
				std::cerr << "Error, invalid value '" << argv[i + 1] << "' for option '" << argv[i] << "'.\n";
				return 1;
			}
		}

		// At this point we don't have a value
		if (i == argc - 1)
		{
			switch (parse_single(argv[argc - 1]))
			{
			case 2: std::cerr << "Error, unknown option '" << argv[argc - 1] << "'.\n"; return 1;
			case 1: std::cerr << "Error, no value specified for option '" << argv[argc - 1] << "'.\n"; return 1;
			default:;
			}
		}

		return 0;
	}

	void print_help() const
	{
		// Gather info
		auto info = std::vector<GatheredInfo>();

		gather_options_info<std::shared_ptr<Option<int>>>(integer_options, info);
		gather_options_info<std::shared_ptr<Option<float>>>(float_options, info);
		gather_options_info<std::shared_ptr<Option<unsigned long>>>(ulong_options, info);
		gather_options_info<std::shared_ptr<Option<std::string>>>(string_options, info);
		gather_options_info<std::shared_ptr<Option<bool>>>(bool_options, info);

		std::sort(info.begin(), info.end(),
		          [](const GatheredInfo& a, const GatheredInfo& b) { return (a.order < b.order); });

		// Print
		size_t previous_category = 0;
		bool double_line_end = false;

		for (size_t i = 0; i < info.size(); i++)
		{
			// Category
			if (info[i].category != previous_category)
			{
				previous_category = info[i].category;

				if (info[i].category != 0)
				{
					if (i != 0 && double_line_end == false)
						std::cout << "\n";

					std::cout << categories[info[i].category - 1] << ":\n";
				}
			}

			// Commands
			std::cout << info[i].short_command << ", " << info[i].long_command << "\n";
			double_line_end = false;

			// Description
			const auto description = info[i].description;
			if (description != "")
				print_with_wrap(description);

			// Default value
			const auto default_value = info[i].default_value;
			if (default_value != "" && default_value != "\"\"")
			{
				if (description != "")
					std::cout << "\n";

				std::cout << "    Default value: " << default_value << "\n";

				if (i != info.size() - 1)
				{
					std::cout << "\n";
					double_line_end = true;
				}
			}
		}
	}
};

#endif
