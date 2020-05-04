#pragma once
#include <sam/Lexer.hpp>

namespace sam {
	constexpr bool IsKeyword(TokenType type) noexcept {
		switch (type) {
		case TokenType::ImportKeyword:
		case TokenType::AsKeyword:
		case TokenType::StructKeyword:
		case TokenType::FuncKeyword:
		case TokenType::ProcKeyword:
			return true;

		default:
			return IsTypeKeyword(type);
		}
	}
	constexpr bool IsTypeKeyword(TokenType type) noexcept {
		switch (type) {
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
	constexpr bool IsInteger(TokenType type) noexcept {
		switch (type) {
		case TokenType::BinInteger:
		case TokenType::OctInteger:
		case TokenType::DecInteger:
		case TokenType::HexInteger:
			return true;

		default:
			return false;
		}
	}
}