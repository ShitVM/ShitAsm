#include <sam/Lexer.hpp>

#include <sam/Encoding.hpp>
#include <sam/String.hpp>

#include <cctype>
#include <unordered_map>
#include <utility>

namespace sam {
	Token::Token(std::string word, TokenType type, std::size_t line) noexcept
		: Word(std::move(word)), Type(type), Line(line) {}
	Token::Token(std::string word, TokenData data, TokenType type, std::size_t line) noexcept
		: Word(std::move(word)), Type(type), Data(std::move(data)), Line(line) {}
	Token::Token(std::string word, std::string suffix, TokenData data, TokenType type, std::size_t line) noexcept
		: Word(std::move(word)), Suffix(std::move(suffix)), Data(std::move(data)), Type(type), Line(line) {}

	std::ostream& operator<<(std::ostream& stream, const Token& token) {
		static constexpr std::string_view tokenTypes[] = {
			"None", "NewLine",
			"Identifier",
			"ImportKeyword", "AsKeyword", "StructKeyword", "FuncKeyword", "ProcKeyword",
			"IntKeyword", "LongKeyword", "DoubleKeyword", "PointerKeyword", "GCPointerKeyword",
			"BinInteger", "OctInteger", "DecInteger", "HexInteger", "Decimal",
			"Plus", "Minus",
			"Character", "String",
			"Colon", "Dot", "Comma", "LeftBracket", "RightBracket", "LeftParenthesis", "RightParenthesis",
		};

		stream << "Line " << token.Line << ": " << tokenTypes[static_cast<int>(token.Type)];
		if (token.Type != TokenType::NewLine) {
			stream << "\n\tWord: \"" << token.Word << '"';
		}
		if (!std::holds_alternative<std::monostate>(token.Data)) {
			stream << "\n\tData: ";
			if (std::holds_alternative<std::uint64_t>(token.Data)) {
				stream << std::get<std::uint64_t>(token.Data);
			} else if (std::holds_alternative<double>(token.Data)) {
				stream << std::get<double>(token.Data);
			} else if (std::holds_alternative<std::string>(token.Data)) {
				stream << '"' << std::get<std::string>(token.Data) << '"';
			}
		}
		if (!token.Suffix.empty()) {
			stream << "\n\tSuffix: \"" << token.Suffix << '"';
		}

		return stream;
	}
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

			for (m_Column = 0; m_Column < m_Line.size();) {
				const char firstByte = m_Line[m_Column];
				if (std::isdigit(firstByte)) {
					LexNumber();
				} else if (firstByte == '\'' || firstByte == '"') {
					LexText(firstByte);
				} else if (IsSpecial(firstByte)) {
					LexSpecial();
				} else {
					LexIdentifier();
				}
			}

			m_Result.emplace_back("\n", TokenType::NewLine, m_LineNum);
		}
	}
	std::vector<Token> Lexer::GetTokens() noexcept {
		return std::move(m_Result);
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
		switch (firstByte) {
#define CASE(e, c) case c: m_Result.emplace_back(std::string(1, c), TokenType:: e, m_LineNum); break
		CASE(Plus, '+');
		CASE(Minus, '-');

		CASE(Colon, ':');
		CASE(Dot, '.');
		CASE(Comma, ',');
		CASE(LeftBracket, '[');
		CASE(RightBracket, ']');
		CASE(LeftParenthesis, '(');
		CASE(RightParenthesis, ')');
#undef CASE

		default:
			if (!std::isspace(firstByte)) {
				ERROR << "Unusable character '" << firstByte << "'.\n";
			}
			break;
		}
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
		if (const std::size_t dot = literal.find('.'); dot != std::string_view::npos) {
			if (base != 8 && base != 10) {
				ERROR << "Invalid integer literal '" << literal << "'.\n";
			} else {
				LexDecimal(literal, dot);
			}
			return;
		}

		std::size_t digitEnd = digitBegin;
		bool prevComma = true;
		while (digitEnd < literal.size()) {
			const char byte = literal[digitEnd++];
			if (byte == ',') {
				if (prevComma) {
					ERROR << "Invalid integer literal '" << literal << "'.\n";
					return;
				}
				prevComma = true;
				continue;
			} else if (!isValidDigit(byte)) {
				--digitEnd;
				break;
			}

			prevComma = false;
		}
		if (literal[digitEnd - 1] == ',') {
			ERROR << "Invalid integer literal '" << literal << "'.\n";
			return;
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
		LexInteger(literal, 2, TokenType::HexInteger, 16, [](char c) noexcept {
			return std::isdigit(c) ||
				'A' <= c && c <= 'F' ||
				'a' <= c && c <= 'f';
		});
	}
	void Lexer::LexDecimal(std::string_view& literal, std::size_t dot) {
		if (literal.find('.', dot + 1) != std::string_view::npos) {
			ERROR << "Invalid decimal literal '" << literal << "'.\n";
			return;
		}

		std::string digits;
		std::size_t digitEnd = 0;
		while (digitEnd < literal.size()) {
			const char byte = literal[digitEnd++];
			if (std::isdigit(byte)) {
				digits.push_back(byte);
			} else if (byte == ',' || byte == '.') {
				if (digits.empty() || digits.back() == ',' || digits.back() == '.') {
					ERROR << "Invalid decimal literal '" << literal << "'.\n";
					return;
				} else if (byte == '.') {
					digits.push_back(byte);
				}
			} else {
				--digitEnd;
				break;
			}
		}
		if (const char back = literal[digitEnd - 1]; back == ',' || back == '.') {
			ERROR << "Invalid decimal literal '" << literal << "'.\n";
			return;
		}

		const double value = std::stod(digits);
		const std::string_view suffix = literal.substr(digitEnd);
		m_Result.emplace_back(std::string(literal), std::string(suffix), value, TokenType::Decimal, m_LineNum);
	}

	void Lexer::LexText(char firstByte) {
		std::string text;
		std::size_t textEnd = m_Column + 1;
		while (true) {
			const char byte = GetByte(textEnd++);
			if (byte == 0) {
				ERROR << "Unterminated text literal.\n";
				m_Column = static_cast<std::size_t>(-1);
				return;
			} else if (byte == '\\') {
				const char nextByte = GetByte(textEnd++);
				if (nextByte == 0) {
					ERROR << "Unterminated text literal.\n";
					m_Column = static_cast<std::size_t>(-1);
					return;
				} else if (nextByte == firstByte) {
					text.push_back(firstByte);
				} else {
					ERROR << "Invalid escape sequence '\\" << nextByte << "'.\n";
					m_Column = static_cast<std::size_t>(-1);
					return;
				}
			} else if (byte == firstByte) break;
			else {
				text.push_back(byte);
			}
		}

		m_Result.emplace_back(m_Line.substr(m_Column, textEnd + 1), text, firstByte == '"' ? TokenType::String : TokenType::Character, m_LineNum);
		m_Column = textEnd + 1;
	}

	void Lexer::LexIdentifier() {
		std::size_t end = m_Column;
		char byte;
		while ((byte = GetByte(end)) && !std::isspace(byte)) {
			if (IsSpecial(byte)) break;
			++end;
		}

		const std::string identifier = m_Line.substr(m_Column, end - m_Column);
		m_Result.emplace_back(identifier, identifier, TokenType::Identifier, m_LineNum);
		m_Column += identifier.size();

		static const std::unordered_map<std::string, TokenType> keywords = {
			{ "import", TokenType::ImportKeyword },
			{ "as", TokenType::AsKeyword },
			{ "struct", TokenType::StructKeyword },
			{ "func", TokenType::FuncKeyword },
			{ "proc", TokenType::ProcKeyword },

			{ "int", TokenType::IntKeyword },
			{ "long", TokenType::LongKeyword },
			{ "double", TokenType::DoubleKeyword },
			{ "pointer", TokenType::PointerKeyword },
			{ "gcpointer", TokenType::GCPointerKeyword },
		};
		if (const auto keyword = keywords.find(identifier); keyword != keywords.end()) {
			m_Result.back().Type = keyword->second;
		}
	}
}