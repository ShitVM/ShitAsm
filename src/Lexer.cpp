#include <sam/Lexer.hpp>

#include <utility>

namespace sam {
	Token::Token(std::string word, TokenType type, std::size_t line) noexcept
		: Word(std::move(word)), Type(type), Line(line) {}
}