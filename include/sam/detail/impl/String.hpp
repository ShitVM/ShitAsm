#pragma once
#include <sam/String.hpp>

#include <cstddef>

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

namespace sam {
	constexpr std::uint32_t CRC32Internal(const char* string, std::size_t index) noexcept {
		return index == static_cast<std::size_t>(-1) ? 0xFFFFFFFF :
			((CRC32Internal(string, index - 1) >> 8) ^ CRC32Table[(CRC32Internal(string, index - 1) ^ string[index]) & 0xFF]);
	}
	constexpr std::uint32_t CRC32(const char* string, std::size_t length) noexcept {
		return CRC32Internal(string, length) ^ 0xFFFFFFFF;
	}
	constexpr std::uint32_t CRC32(const std::string_view& string) {
		return CRC32(string.data(), string.size());
	}
	constexpr std::uint32_t operator""_h(const char* string, std::size_t length) noexcept {
		return CRC32(string, length);
	}
}