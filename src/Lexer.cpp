#include <sam/Lexer.hpp>

#include <sam/Encoding.hpp>
#include <sam/String.hpp>

#include <cctype>
#include <utility>

namespace sam {
	Token::Token(TokenType type, std::size_t line) noexcept
		: Type(type), Line(line) {}
	Token::Token(std::string word, TokenData data, TokenType type, std::size_t line) noexcept
		: Word(std::move(word)), Data(data), Type(type), Line(line) {}
}

namespace sam {
	Lexer::Lexer(const std::string& path, std::istream& inputStream) noexcept
		: m_Path(path), m_InputStream(inputStream) {}

#define MESSAGEBASE m_ErrorStream << "In file '" << m_Path << "':\n    "
#define INFO MESSAGEBASE << "Info: Line " << m_LineNum << ", "
#define WARNING MESSAGEBASE << "Warning: Line " << m_LineNum << ", "
#define ERROR (m_HasError = true, MESSAGEBASE) << "Error: Line " << m_LineNum << ", "

	void Lexer::Lex() {
		while (std::getline(m_InputStream, m_Line) && ++m_LineNum) {
			if (IgnoreComment()) continue;
			for (m_Column = 0; m_Column < m_Line.size();) {
				const char firstByte = m_Line[m_Column];
				if (IsSpecial(firstByte)) {
					LexSpecial(firstByte);
				}
			}
		}
	}
	bool Lexer::HasError() const noexcept {
		return m_HasError;
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
	char Lexer::GetNextByte(std::size_t i) const noexcept {
		if (i >= m_Line.size()) return 0;
		else return m_Line[i];
	}

	void Lexer::LexSpecial(char firstByte) {
#define CASE(e, c) case c: m_Result.emplace_back(TokenType:: e, m_LineNum); break
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
			ERROR << "Unusable character '" << firstByte << "'.\n";
			break;
		}
#undef CASE

		++m_Column;
	}
}