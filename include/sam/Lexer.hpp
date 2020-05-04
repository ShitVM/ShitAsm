#pragma once

#include <cstddef>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sam {
	enum class TokenType {
		None,
		NewLine,

		Identifier,

		ImportKeyword,
		AsKeyword,
		StructKeyword,
		FuncKeyword,
		ProcKeyword,

		IntKeyword,
		LongKeyword,
		DoubleKeyword,
		PointerKeyword,
		GCPointerKeyword,

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

	constexpr bool IsKeyword(TokenType type) noexcept;
	constexpr bool IsTypeKeyword(TokenType type) noexcept;
	constexpr bool IsInteger(TokenType type) noexcept;

	using TokenData = std::variant<std::monostate, std::uint64_t, double, std::string>;

	struct Token final {
		std::string Word;
		TokenData Data;
		TokenType Type = TokenType::None;
		std::size_t Line = 0;

		std::string Suffix;

		Token() noexcept = default;
		Token(std::string word, TokenType type, std::size_t line) noexcept;
		Token(std::string word, TokenData data, TokenType type, std::size_t line) noexcept;
		Token(std::string word, std::string suffix, TokenData data, TokenType type, std::size_t line) noexcept;
	};

	std::ostream& operator<<(std::ostream& stream, const Token& token);
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
		bool m_HasWarning = false;
		bool m_HasInfo = false;

	public:
		Lexer(std::string path, std::istream& inputStream) noexcept;
		Lexer(const Lexer&) = delete;
		~Lexer() = default;

	public:
		Lexer& operator=(const Lexer&) = delete;
		bool operator==(const Lexer&) = delete;
		bool operator!=(const Lexer&) = delete;

	public:
		void Lex();
		std::vector<Token> GetTokens() noexcept;

		bool HasError() const noexcept;
		bool HasMessage() const noexcept;
		std::string GetMessages() const;

	private:
		bool IgnoreComment() noexcept;
		char GetByte(std::size_t i) const noexcept;

		void LexSpecial();

		void LexNumber();
		std::string_view ReadNumber();
		void LexInteger(std::string_view& literal, std::size_t digitBegin, TokenType type, int base, bool(*isValidDigit)(char) noexcept);
		void LexBinInteger(std::string_view& literal);
		void LexOctInteger(std::string_view& literal);
		void LexDecInteger(std::string_view& literal);
		void LexHexInteger(std::string_view& literal);
		void LexDecimal(std::string_view& literal, std::size_t dot);

		void LexText(char firstByte);

		void LexIdentifier();
	};
}

#include "detail/impl/Lexer.hpp"