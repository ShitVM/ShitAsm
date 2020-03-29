#include <sam/Parser.hpp>

#include <sam/Function.hpp>
#include <sam/String.hpp>
#include <sgn/ByteFile.hpp>
#include <sgn/Structure.hpp>
#include <svm/Type.hpp>

#include <algorithm>
#include <cctype>
#include <limits>
#include <unordered_map>
#include <utility>
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
					hasError |= !ParseStructure();
				} else if (mnemonic == "proc" || mnemonic == "func") {
					hasError |= !ParseFunction(mnemonic == "proc");
				} else {
					hasError |= !ParseLabel();
				}
			}
		}

		if (!m_Assembly.HasFunction("entrypoint")) {
			m_ErrorStream << "Error: There is no 'entrypoint' procedure.\n";
			hasError = true;
		}

		return !hasError;
	}
	bool Parser::SecondPass() {
		std::size_t structInx = 0;
		std::size_t fieldInx = 0;

		bool hasError = false;

		while (std::getline(m_ReadStream, m_Line) && ++m_LineNum) {
			if (IgnoreComment()) continue;

			if (m_Line.find(':') != std::string::npos) {
				std::string mnemonic = ReadBeforeSpace(m_Line);
				std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });

				if (mnemonic == "proc" || mnemonic == "func") {
					m_CurrentStructure.clear();
				} else if (mnemonic == "struct") {
					m_CurrentStructure = m_Assembly.Structures[structInx++].Name;
					fieldInx = 0;
				}
				continue;
			} else if (m_CurrentStructure.empty()) continue;

			hasError |= !ParseField();
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
	sgn::Type Parser::GetType(const std::string& name) {
		static const std::unordered_map<std::string, sgn::Type> fundamental = {
			{ "int", sgn::IntType },
			{ "long", sgn::LongType },
			{ "double", sgn::DoubleType },
			{ "pointer", sgn::PointerType },
			{ "gcpointer", sgn::GCPointerType },
		};

		const auto iter = fundamental.find(name);
		if (iter != fundamental.end()) return svm::GetType(sgn::Structures(), iter->second->Code);

		const auto structIter = m_Assembly.FindStructure(name);
		if (structIter != m_Assembly.Structures.end()) return m_Assembly.ByteFile.GetStructureInfo(structIter->Index)->Type;
		else return nullptr;
	}
	template<typename F>
	bool Parser::IsValidIntegerLiteral(const std::string& literal, std::string& literalMut, F&& function) {
		for (std::size_t i = 0; i < literalMut.size(); ++i) {
			if (i != 0 && i != literalMut.size() - 1 && literalMut[i] == ',') {
				literalMut.erase(literalMut.begin() + i);
				--i;
			} else if (i == literalMut.size() - 1 && (literalMut[i] == 'i' || literalMut[i] == 'l')) continue;
			else if (function(literalMut[i])) {
				ERROR << "Invalid integer literal '" << literal << "'.\n";
				return false;
			}
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
	bool Parser::ParseField() {
		bool hasError = false;

		const auto type = ParseType(m_Line);
		if (!type) return false;
		else if (!type->ElementCount || *type->ElementCount == 0) {
			ERROR << "Array in structure must have length.\n";
			hasError = true;
		}

		std::string name = ReadBeforeSpecialChar(m_Line); Trim(name);
		if (!IsValidIdentifier(name)) {
			ERROR << "Invalid field name '" << name << "'.\n";
			hasError = true;
		} else if (type->ElementType == nullptr) {
			ERROR << "Nonexistent type name '" << type->ElementTypeName << "'.\n";
			return false;
		}

		sgn::StructureIndex structIndex = m_Assembly.GetStructure(m_CurrentStructure).Index;
		sgn::StructureInfo* const structure = m_Assembly.ByteFile.GetStructureInfo(structIndex);
		structure->AddField(type->ElementType, type->ElementCount.value_or(0));

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

	std::optional<Type> Parser::ParseType(std::string& line) {
		bool hasError = false;

		std::string typeName;
		if (line.find('[') != std::string::npos) {
			std::string temp = ReadBeforeChar(line, '['); Trim(typeName);
			typeName = ReadBeforeSpace(temp); Trim(typeName);
			if (!temp.empty()) {
				ERROR << "Unexcepted characters after type name '" << typeName << "'.\n";
				hasError = true;
			}
		} else {
			typeName = ReadBeforeSpace(line); Trim(typeName);
		}

		const sgn::Type type = GetType(typeName);
		if (line.empty() || line.front() != '[') return Type{ type, std::move(typeName) };

		std::optional<std::uint64_t> count;
		std::string countStr = ReadBeforeChar(line, ']'); countStr.erase(countStr.begin()); Trim(countStr);

		const auto countVar = ParseNumber(countStr);
		if (std::holds_alternative<bool>(countVar) && std::get<bool>(countVar)) {
			hasError = true;
		} else if (std::holds_alternative<double>(countVar)) {
			ERROR << "Array's length must be integer.\n";
			count = static_cast<std::uint64_t>(std::get<double>(countVar));
			hasError = true;
		} else {
			std::visit([&count](auto v) mutable {
				count = static_cast<std::uint64_t>(v);
			}, countVar);
		}

		if (line.empty()) {
			if (count) {
				ERROR << "Unexcepted end-of-file after array's length.\n";
			} else {
				ERROR << "Unexcpeted end-of-file after '['.\n";
			}
			hasError = true;
		} else if (line.front() != ']') {
			if (count) {
				ERROR << "Excepted ']' after array's length.\n";
			} else {
				ERROR << "Excpeted ']' after '['.\n";
			}
			hasError = true;
		}

		if (hasError) return std::nullopt;
		else return Type{ type, std::move(typeName), count };
	}
	Parser::Number Parser::ParseNumber(std::string& line) {
		std::string literal = ReadBeforeSpace(line); Trim(literal);
		if (literal.empty()) return false;

		std::string literalMut = literal;
		const bool isNegative = literal.front() == '-';
		if (isNegative || literal.front() == '+') {
			if (literal.size() == 1) {
				literal = ReadBeforeSpace(line); Trim(literal);
				if (literal.empty()) {
					ERROR << "Excepted number literal after '" << literalMut.front() << "'.\n";
					return true;
				}
				literalMut = literal;
			}
		}

		if (literalMut.front() == '0') {
			if (literalMut.size() == 1) return 0u;
			switch (literalMut[1]) {
			case 'x':
			case 'X':
				literalMut.erase(literalMut.begin(), literalMut.begin() + 2);
				return ParseHexInteger(literal, literalMut, isNegative);

			case 'b':
			case 'B':
				literalMut.erase(literalMut.begin(), literalMut.begin() + 2);
				return ParseBinInteger(literal, literalMut, isNegative);

			default:
				return ParseOctInteger(literal, literalMut, isNegative);
			}
		} else return ParseDecInteger(literal, literalMut, isNegative);
	}
	template<typename F>
	Parser::Number Parser::ParseInteger(const std::string& literal, std::string& literalMut, bool isNegative, int base, F&& function) {
		if (literalMut.find('.') != std::string::npos) {
			ERROR << "Invalid integer literal '" << literal << "'.\n";
			return false;
		} else if (!IsValidIntegerLiteral(literal, literalMut, std::forward<F>(function))) return false;

		const unsigned long long abs = std::stoull(literalMut, nullptr, base);
		if (isNegative) {
			if (literalMut.back() == 'i') {
				if (abs > -std::numeric_limits<std::int32_t>::min()) {
					WARNING << "Overflowed integer literal '" << literalMut << "'.\n";
				}
				return static_cast<std::uint32_t>(-abs);
			} else {
				if (abs > -std::numeric_limits<std::int64_t>::min()) {
					WARNING << "Overflowed integer literal '" << literalMut << "'.\n";
				}
				return static_cast<std::uint64_t>(-abs);
			}
		} else {
			if (literalMut.back() == 'i') {
				if (abs > std::numeric_limits<std::int32_t>::max()) {
					WARNING << "Overflowed integer literal '" << literalMut << "'.\n";
				}
				return static_cast<std::uint32_t>(abs);
			} else return static_cast<std::uint64_t>(abs);
		}
	}
	Parser::Number Parser::ParseBinInteger(const std::string& literal, std::string& literalMut, bool isNegative) {
		return ParseInteger(literal, literalMut, isNegative, 2, [](char c) {
			return !(c == '0' || c == '1');
		});
	}
	Parser::Number Parser::ParseOctInteger(const std::string& literal, std::string& literalMut, bool isNegative) {
		if (literalMut.find('.') != std::string::npos) return ParseDecimal(literal, literalMut, isNegative);
		else return ParseInteger(literal, literalMut, isNegative, 8, [](char c) {
			return !(c <= '0' && c <= '7');
		});
	}
	Parser::Number Parser::ParseDecInteger(const std::string& literal, std::string& literalMut, bool isNegative) {
		if (literalMut.find('.') != std::string::npos) return ParseDecimal(literal, literalMut, isNegative);
		else return ParseInteger(literal, literalMut, isNegative, 10, [](char c) {
			return !(std::isdigit(c));
		});
	}
	Parser::Number Parser::ParseHexInteger(const std::string& literal, std::string& literalMut, bool isNegative) {
		return ParseInteger(literal, literalMut, isNegative, 16, [](char c) {
			return !(std::isdigit(c) ||
					 'a' <= c && c <= 'f' ||
					 'A' <= c && c <= 'F');
		});
	}
	Parser::Number Parser::ParseDecimal(const std::string& literal, std::string& literalMut, bool isNegative) {
		const std::size_t dot = literalMut.find('.');
		if (literalMut.find('.', dot + 1) != std::string::npos) {
			ERROR << "Invalid decimal literal '" << literal << "'.\n";
			return true;
		}

		const double abs = std::stod(literalMut);
		if (isNegative) return -abs;
		else return abs;
	}
}