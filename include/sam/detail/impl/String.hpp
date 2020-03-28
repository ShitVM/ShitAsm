#pragma once
#include <sam/String.hpp>

namespace sam {
	template<typename F>
	std::string ReadBefore(std::string& string, F&& function) {
		std::size_t length = 0;
		while (string.size() > length && !function(string[length])) ++length;

		const std::string result = string.substr(0, length);
		string.erase(string.begin(), string.begin() + length);
		Trim(string);

		return result;
	}
}