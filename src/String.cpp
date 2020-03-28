#include <sam/String.hpp>

#include <cctype>
#include <cstddef>
#include <sstream>
#include <utility>

namespace sam {
	bool IsSpecial(char c) noexcept {
		switch (c) {
		case '~':
		case '`':
		case '!':
		case '@':
		case '#':
		case '$':
		case '%':
		case '^':
		case '&':
		case '*':
		case '(':
		case ')':
		case '-':
		case '+':
		case '=':
		case '|':
		case '\\':
		case '{':
		case '[':
		case '}':
		case ']':
		case ':':
		case ';':
		case '"':
		case '\'':
		case '<':
		case ',':
		case '>':
		case '.':
		case '?':
		case '/':
			return true;

		default:
			return std::isspace(c);
		}
	}

	void Trim(std::string& string) noexcept {
		std::size_t beginOffset = 0;
		std::size_t rbeginOffset = 0;

		while (string.size() > rbeginOffset && std::isspace(string[string.size() - rbeginOffset - 1])) ++rbeginOffset;
		while (string.size() > beginOffset + rbeginOffset && std::isspace(string[beginOffset])) ++beginOffset;

		if (beginOffset) {
			string.erase(string.begin(), string.begin() + beginOffset);
		}
		if (rbeginOffset) {
			string.erase(string.end() - rbeginOffset, string.end());
		}
	}
	std::vector<std::string> Split(const std::string& string, char c) {
		std::vector<std::string> result;
		std::istringstream iss(string);

		std::string value;
		while (std::getline(iss, value, c)) {
			Trim(value);
			result.push_back(std::move(value));
		}

		return result;
	}

	std::string ReadBeforeSpace(std::string& string) {
		return ReadBefore(string, [](char c) -> bool { return std::isspace(c); });
	}
	std::string ReadBeforeChar(std::string& string, char c) {
		return ReadBefore(string, [c](char ch) { return ch == c; });
	}
	std::string ReadBeforeSpecialChar(std::string& string) {
		return ReadBefore(string, IsSpecial);
	}
}