#pragma once

#include <cstddef>
#include <istream>
#include <sstream>
#include <string>
#include <vector>

namespace sam {
	enum class TokenType {
		None,
		NewLine,

		Identifier,

		BinInteger,
		OctInteger,
		DecInteger,
		HexInteger,
		Decimal,

		Plus,					// +
		Minus,					// -

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

		Token(TokenType type, std::size_t line) noexcept;
		Token(std::string word, TokenType type, std::size_t line) noexcept;
	};
}

namespace sam {
	class Lexer final {
	private:
		std::istream& m_InputStream;
		std::ostringstream m_ErrorStream;

		std::string m_Line;
		std::size_t m_LineNum = 0;

		std::vector<Token> m_Result;
		bool m_HasError = false;

	public:
		Lexer(std::istream& inputStream) noexcept;
		Lexer(const Lexer&) = delete;
		~Lexer() = default;

	public:
		Lexer& operator=(const Lexer&) = delete;
		bool operator==(const Lexer&) = delete;
		bool operator!=(const Lexer&) = delete;

	public:
		void Lex();

	private:
		void LexSpecial(char firstByte);

		bool IgnoreComment() noexcept;

		char GetNextByte(std::size_t i) const noexcept;
	};
}