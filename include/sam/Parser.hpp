#pragma once

#include <sam/Assembly.hpp>

#include <cstddef>
#include <fstream>
#include <ostream>
#include <string>

namespace sam {
	class Parser final {
	private:
		Assembly& m_Assembly;
		std::ifstream m_ReadStream;
		std::ostream& m_ErrorStream;

		std::string m_CurrentStructure;
		std::string m_CurrentFunction;

		std::string m_Line;
		std::size_t m_LineNum = 0;

	public:
		Parser(Assembly& assembly, std::ostream& errorStream) noexcept;
		Parser(const Assembly&) = delete;
		~Parser() = default;

	public:
		Parser& operator=(const Assembly&) = delete;
		bool operator==(const Parser&) = delete;
		bool operator!=(const Parser&) = delete;

	private:
		bool FirstPass();

		bool IgnoreComment();
		bool IsValidIdentifier(const std::string& identifier);

		bool ParseStructure();
		bool ParseFunction(bool isProcedure);
		bool ParseLabel();
	};
}