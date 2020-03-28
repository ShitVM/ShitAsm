#include <sam/Parser.hpp>

#include <sam/Function.hpp>
#include <sam/String.hpp>
#include <sgn/ByteFile.hpp>
#include <sgn/Structure.hpp>

#include <algorithm>
#include <cctype>
#include <vector>

namespace sam {
	Parser::Parser(Assembly& assembly, std::ostream& errorStream) noexcept
		: m_Assembly(assembly), m_ErrorStream(errorStream) {}

#define INFO m_ErrorStream << "Info: Line " << m_LineNum << ", "
#define WARNING m_ErrorStream << "Warning: Line " << m_LineNum << ", "
#define ERROR m_ErrorStream << "Error: Line " << m_LineNum << ", "

	bool Parser::FirstPass() {
		bool hasError = false;

		while (std::getline(m_ReadStream, m_Line) && ++m_LineNum) {
			if (IgnoreComment()) continue;

			if (m_Line.find(':') != std::string::npos) {
				std::string mnemonic = ReadBeforeSpace(m_Line);
				std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });

				if (mnemonic == "struct") {
					hasError = !ParseStructure();
				} else if (mnemonic == "proc" || mnemonic == "func") {
					hasError = !ParseFunction(mnemonic == "proc");
				} else {
					hasError = !ParseLabel();
				}
			}
		}

		if (!m_Assembly.HasFunction("entrypoint")) {
			m_ErrorStream << "Error: There is no 'entrypoint' procedure.\n";
			hasError = true;
		}

		return !hasError;
	}

	bool Parser::IgnoreComment() {
		if (const auto commentBegin = m_Line.find(';'); commentBegin != std::string::npos) {
			m_Line.erase(m_Line.begin() + commentBegin, m_Line.end());
		}

		Trim(m_Line);
		return m_Line.empty();
	}
	bool Parser::IsValidIdentifier(const std::string& identifier) {
		if (std::isdigit(identifier.front())) return false;

		for (const char c : identifier) {
			if (std::isspace(c)) return false;
		}

		return true;
	}

	bool Parser::ParseStructure() {
		bool hasError = false;

		std::string name = ReadBeforeSpecialChar(m_Line); Trim(name);
		m_CurrentStructure = name;
		m_CurrentFunction.clear();

		if (!IsValidIdentifier(name)) {
			ERROR << "Invalid structure name '" << name << "'.\n";
			hasError = true;
		} else if (m_Assembly.HasStructure(name)) {
			ERROR << "Duplicated structure name '" << name << "'.\n";
			hasError = true;
		}

		if (m_Line.size() == 0) {
			ERROR << "Unexcepted end-of-file after structure name.\n";
			hasError = true;
		} else if (m_Line.front() != ':') {
			ERROR << "Excepted ':' after structure name.\n";
			hasError = true;
		} else if (m_Line.size() > 1) {
			ERROR << "Unexcepted characters after ':'.\n";
			hasError = true;
		}

		const sgn::StructureIndex index = m_Assembly.ByteFile.AddStructure();
		m_Assembly.Structures.push_back(Structure{ std::move(name), index });

		return !hasError;
	}
	bool Parser::ParseFunction(bool isProcedure) {
		bool hasError = false;

		std::string name = ReadBeforeSpecialChar(m_Line); Trim(name);
		m_CurrentStructure.clear();
		m_CurrentFunction = name;

		if (!IsValidIdentifier(name)) {
			ERROR << "Invalid function or procedure name '" << name << "'.\n";
			hasError = true;
		} else if (m_Assembly.HasFunction(name)) {
			ERROR << "Duplicated function or procedure name '" << name << "'.\n";
			hasError = true;
		} else if (name == "entrypoint" && !isProcedure) {
			ERROR << "Invalid function name 'entrypoint'.\n";
			INFO << "It can be used only as procedure.\n";
			hasError = true;
		}

		std::vector<LocalVariable> params;
		std::string rawParams = ReadBeforeChar(m_Line, ':'); Trim(rawParams);
		if (!rawParams.empty()) {
			if (rawParams.front() != '(') {
				ERROR << "Excepted '(' after function or procedure name.\n";
				hasError = true;
			} else if (rawParams.back() != ')') {
				ERROR << "Excepted ')' before ':'.\n";
				hasError = true;
			} else {
				rawParams.erase(rawParams.begin());
				rawParams.erase(rawParams.end() - 1);

				std::vector<std::string> strParams = Split(rawParams, ',');
				std::vector<std::string> sortedParams = strParams; std::sort(sortedParams.begin(), sortedParams.end());
				if (const auto duplicated = std::unique(sortedParams.begin(), sortedParams.end()); duplicated != sortedParams.end()) {
					ERROR << "Duplicated parameter name '" << *duplicated << "'.\n";
					hasError = true;
				}

				for (std::string& param : strParams) {
					params.push_back(LocalVariable{ std::move(param) });
				}
			}
		}

		if (m_Line.size() == 0) {
			ERROR << "Unexcepted end-of-file after function or procedure name.\n";
			hasError = true;
		} else if (m_Line.front() != ':') {
			if (params.empty()) {
				ERROR << "Excepted ':' after function or procedure name.\n";
			} else {
				ERROR << "Excepted ':' after parameters.\n";
			}
			hasError = true;
		} else if (m_Line.size() > 1) {
			ERROR << "Unexcepted characters after ':'.\n";
			hasError = true;
		}

		const sgn::FunctionIndex index = m_Assembly.ByteFile.AddFunction(static_cast<std::uint16_t>(params.size()), !isProcedure);
		m_Assembly.Functions.push_back(Function{ nullptr, std::move(name), index, {}, std::move(params) });

		return !hasError;
	}
	bool Parser::ParseLabel() {
		if (m_CurrentFunction.empty()) {
			ERROR << "Not belonged label.\n";
			return false;
		}

		Function& currentFunction = m_Assembly.GetFunction(m_CurrentFunction);
		bool hasError = false;

		std::string name = ReadBeforeSpecialChar(m_Line); Trim(name);
		if (!IsValidIdentifier(name)) {
			ERROR << "Invalid label name '" << name << "'.\n";
			hasError = true;
		} else if (currentFunction.HasLabel(name)) {
			ERROR << "Duplicated label name '" << name << "'.\n";
			hasError = true;
		}

		currentFunction.Labels.push_back(Label{ std::move(name) });

		return !hasError;
	}
}