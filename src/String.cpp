#include <sam/String.hpp>

#include <cctype>

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
}