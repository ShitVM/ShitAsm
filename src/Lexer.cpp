#include <sam/Lexer.hpp>

#include <sam/Encoding.hpp>
#include <sam/String.hpp>

#include <cctype>
#include <utility>

namespace sam {
	Token::Token(TokenType type, std::size_t line) noexcept
		: Type(type), Line(line) {}
	Token::Token(std::string word, TokenType type, std::size_t line) noexcept
		: Word(std::move(word)), Type(type), Line(line) {}
}

namespace sam {
	Lexer::Lexer(std::istream& inputStream) noexcept
		: m_InputStream(inputStream) {}

#define ERROR (m_HasError = true, m_ErrorStream) << "    Error: Line " << m_LineNum << ", "

	void Lexer::Lex() {
		while (std::getline(m_InputStream, m_Line) && ++m_LineNum) {
			if (IgnoreComment()) continue;
			for (std::size_t i = 0; i < m_Line.size(); ++i) {
				const char firstByte = m_Line[i];
				if (IsSpecial(firstByte)) {
					LexSpecial(firstByte);
				}
			}
		}
	}

	void Lexer::LexSpecial(char firstByte) {
#define CASE(c, e) case c: m_Result.emplace_back(TokenType:: e, m_Line); return
		switch (firstByte) {
		CASE('+', Plus);
		CASE('-', Minus);

		CASE(':', Colon);
		CASE('.', Dot);
		CASE(',', Comma);
		CASE('[', LeftBracket);
		CASE(']', RightBracket);
		CASE('(', LeftParenthesis);
		CASE(')', RightParenthesis);
		default:
			ERROR << "Unusable character '" << firstByte << "'.\n";
			return;
		}
#undef CASE
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

#undef ERROR
}