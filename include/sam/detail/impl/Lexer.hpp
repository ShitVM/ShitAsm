#pragma once
#include <sam/Lexer.hpp>

namespace sam {
	constexpr bool IsKeyword(TokenType type) noexcept {
		switch (type) {
		case TokenType::StructKeyword:
		case TokenType::FuncKeyword:
		case TokenType::ProcKeyword:
		case TokenType::IntKeyword:
		case TokenType::LongKeyword:
		case TokenType::DoubleKeyword:
		case TokenType::PointerKeyword:
		case TokenType::GCPointerKeyword:
			return true;

		default:
			return false;
		}
	}
}