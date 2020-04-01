#pragma once

#include <cstddef>
#include <istream>
#include <sstream>
#include <string>
#include <variant>
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
		LeftBracket,			// [
		RightBracket,			// ]
		LeftParenthesis,		// (
		RightParenthesis,		// )
	};

	using Char = std::variant<char, char16_t, char32_t>;
	using String = std::variant<std::string, std::u16string, std::u32string>;
	using Number = std::variant<std::uint32_t, std::uint64_t, double>;
	using TokenData = std::variant<std::monostate, Char, String, Number>;

	struct Token final {
		std::string Word;
		TokenData Data;
		TokenType Type = TokenType::None;
		std::size_t Line = 0;

		Token(TokenType type, std::size_t line) noexcept;
		Token(std::string word, TokenData data, TokenType type, std::size_t line) noexcept;
	};
}

namespace sam {
	class Lexer final {
	private:
		std::string m_Path;
		std::istream& m_InputStream;
		std::ostringstream m_ErrorStream;

		std::string m_Line;
		std::size_t m_LineNum = 0;
		std::size_t m_Column = 0;

		std::vector<Token> m_Result;
		bool m_HasError = false;

	public:
		Lexer(const std::string& path, std::istream& inputStream) noexcept;
		Lexer(const Lexer&) = delete;
		~Lexer() = default;

	public:
		Lexer& operator=(const Lexer&) = delete;
		bool operator==(const Lexer&) = delete;
		bool operator!=(const Lexer&) = delete;

	public:
		void Lex();
		bool HasError() const noexcept;
		std::string GetMessages() const;

	private:
		bool IgnoreComment() noexcept;
		char GetNextByte(std::size_t i) const noexcept;

		void LexSpecial(char firstByte);
	};
}