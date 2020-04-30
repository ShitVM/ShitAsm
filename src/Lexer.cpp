#include <sam/Lexer.hpp>

#include <sam/Encoding.hpp>
#include <sam/String.hpp>

#include <cctype>
#include <utility>

namespace sam {
	Token::Token(std::string word, TokenType type, std::size_t line) noexcept
		: Word(std::move(word)), Type(type), Line(line) {}
	Token::Token(std::string word, std::string suffix, TokenData data, TokenType type, std::size_t line) noexcept
		: Word(std::move(word)), Suffix(std::move(suffix)), Data(data), Type(type), Line(line) {}
}

namespace sam {
	Lexer::Lexer(std::string path, std::istream& inputStream) noexcept
		: m_Path(std::move(path)), m_InputStream(inputStream) {}

#define MESSAGEBASE m_ErrorStream << "In file '" << m_Path << "':\n    "
#define INFO (m_HasInfo = true, MESSAGEBASE) << "Info: Line " << m_LineNum << ", "
#define WARNING (m_HasWarning = true, MESSAGEBASE) << "Warning: Line " << m_LineNum << ", "
#define ERROR (m_HasError = true, MESSAGEBASE) << "Error: Line " << m_LineNum << ", "

	void Lexer::Lex() {
		while (std::getline(m_InputStream, m_Line) && ++m_LineNum) {
			if (IgnoreComment()) continue;
			else if (m_LineNum > 1) {
				m_Result.emplace_back("\n", TokenType::NewLine, m_LineNum);
			}

			for (m_Column = 0; m_Column < m_Line.size();) {
				const char firstByte = m_Line[m_Column];
				if (IsSpecial(firstByte)) {
					LexSpecial();
				} else if (std::isdigit(firstByte)) {
					LexNumber();
				} else {
					m_Column += GetByteCount(firstByte);
				}
			}
		}
	}
	bool Lexer::HasError() const noexcept {
		return m_HasError;
	}
	bool Lexer::HasMessage() const noexcept {
		return m_HasError || m_HasWarning || m_HasInfo;
	}
	std::string Lexer::GetMessages() const {
		return m_ErrorStream.str();
	}

	bool Lexer::IgnoreComment() noexcept {
		if (const auto commentBegin = m_Line.find(';'); commentBegin != std::string::npos) {
			m_Line.erase(m_Line.begin() + commentBegin, m_Line.end());
		}

		Trim(m_Line);
		return m_Line.empty();
	}
	char Lexer::GetByte(std::size_t i) const noexcept {
		if (i >= m_Line.size()) return 0;
		else return m_Line[i];
	}

	void Lexer::LexSpecial() {
		const char firstByte = GetByte(m_Column++);
#define CASE(e, c) case c: m_Result.emplace_back(std::string(1, c), TokenType:: e, m_LineNum); break
		switch (firstByte) {
		CASE(Plus, '+');
		CASE(Minus, '-');

		CASE(Colon, ':');
		CASE(Dot, '.');
		CASE(LeftBracket, '[');
		CASE(RightBracket, ']');
		CASE(LeftParenthesis, '(');
		CASE(RightParenthesis, ')');

		default:
			if (!std::isspace(firstByte)) {
				ERROR << "Unusable character '" << firstByte << "'.\n";
			}
			break;
		}
#undef CASE
	}

	void Lexer::LexNumber() {
		std::string_view literal = ReadNumber();
		const char firstByte = literal[0];

		if (firstByte == '0' && literal.size() > 1) {
			const char secondByte = literal[1];
			if (secondByte == 'B' || secondByte == 'b') {
				LexBinInteger(literal);
			} else if (std::isdigit(secondByte)) {
				LexOctInteger(literal);
			} else if (secondByte == 'X' || secondByte == 'x') {
				LexHexInteger(literal);
			} else {
				LexDecInteger(literal);
			}
		} else {
			LexDecInteger(literal);
		}

		m_Column += literal.size();
	}
	std::string_view Lexer::ReadNumber() {
		std::size_t end = m_Column;
		char byte;
		while ((byte = GetByte(end)) && !std::isspace(byte)) {
			if (IsSpecial(byte) && byte != '.' && byte != ',') break;
			++end;
		}
		return { m_Line.data() + m_Column, end - m_Column };
	}
	void Lexer::LexInteger(std::string_view& literal, std::size_t digitBegin, TokenType type, int base, bool(*isValidDigit)(char) noexcept) {
		std::size_t digitEnd = digitBegin;
		bool prevComma = 0;
		while (digitEnd < literal.size()) {
			const char byte = literal[digitEnd];
			if (byte == ',') {
				if (prevComma) {
					ERROR << "Invalid integer literal '" << literal << "'.\n";
					return;
				}
				prevComma = true;
				goto increase;
			} else if (byte == '.') {
				if (base == 8 || base == 10) {
					LexDecimal(literal);
				} else {
					ERROR << "Invalid integer literal '" << literal << "'.\n";
				}
				return;
			} else if (!isValidDigit(byte)) {
				if (std::isdigit(byte)) {
					ERROR << "Invalid integer literal '" << literal << "'.\n";
					return;
				}
				break;
			}

			prevComma = false;
		increase:
			++digitEnd;
		}

		std::string digits;
		for (std::size_t i = digitBegin; i < digitEnd; ++i) {
			if (literal[i] == ',') continue;
			digits.push_back(literal[i]);
		}

		const std::uint64_t value = std::stoull(digits, nullptr, base);
		const std::string_view suffix = literal.substr(digitEnd);
		m_Result.emplace_back(std::string(literal), std::string(suffix), value, type, m_LineNum);
	}
	void Lexer::LexBinInteger(std::string_view& literal) {
		LexInteger(literal, 2, TokenType::BinInteger, 2, [](char c) noexcept {
			return c == '0' || c == '1';
		});
	}
	void Lexer::LexOctInteger(std::string_view& literal) {
		LexInteger(literal, 1, TokenType::OctInteger, 8, [](char c) noexcept {
			return '0' <= c && c <= '7';
		});
	}
	void Lexer::LexDecInteger(std::string_view& literal) {
		LexInteger(literal, 0, TokenType::DecInteger, 10, [](char c) noexcept {
			return !!std::isdigit(c);
		});
	}
	void Lexer::LexHexInteger(std::string_view& literal) {
		LexInteger(literal, 2, TokenType::HexInteger, 2, [](char c) noexcept {
			return std::isdigit(c) ||
				'A' <= c && c <= 'F' ||
				'a' <= c && c <= 'f';
		});
	}
	void Lexer::LexDecimal(std::string_view& literal) {
		// TODO
	}
}