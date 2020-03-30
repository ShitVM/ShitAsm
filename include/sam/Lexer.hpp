#pragma once

#include <cstddef>
#include <string>

namespace sam {
	enum class TokenType {
		None,
		WhiteSpace,
		NewLine,

		Identifier,

		BinInteger,
		OctInteger,
		DecInteger,
		HexInteger,
		Decimal,

		Character,
		String,

		Colon,					// :
		Dot,					// .
		Comma,					// ,
		LeftBracket,			// [
		RightBracket,			// ]
		LeftParenthesis,		// (
		RightParenthesis,		// )
	};

	struct Token final {
		std::string Word;
		TokenType Type = TokenType::None;
		std::size_t Line = 0;

		Token(std::string word, TokenType type, std::size_t line) noexcept;
	};
}