#pragma once

#include <string>
#include <vector>

namespace sam {
	bool IsSpecial(char c) noexcept;

	void Trim(std::string& string) noexcept;
	std::vector<std::string> Split(const std::string& string, char c);

	template<typename F>
	std::string ReadBefore(std::string& string, F&& function);
	std::string ReadBeforeSpace(std::string& string);
	std::string ReadBeforeChar(std::string& string, char c);
	std::string ReadBeforeSpecialChar(std::string& string);
}

#include "detail/impl/String.hpp"