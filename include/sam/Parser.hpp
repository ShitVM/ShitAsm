#pragma once

#include <sam/Assembly.hpp>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <ostream>
#include <string>
#include <variant>

namespace sam {
	struct Type final {
		sgn::Type ElementType;
		std::string ElementTypeName;
		std::optional<std::uint64_t> ElementCount;
	};
}

namespace sam {
	class Parser final {
	private:
		using Number = std::variant<bool, std::uint32_t, std::uint64_t, double>;

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

	public:
		bool Parse(const std::string& path);
		void Generate(const std::string& path);

	private:
		bool FirstPass();
		bool SecondPass();
		bool ThirdPass();
		
		void ResetState();
		void GenerateBuilders();

		bool IgnoreComment();
		std::string ReadMnemonic();
		bool IsValidIdentifier(const std::string& identifier);
		sgn::Type GetType(const std::string& name);
		template<typename F>
		bool IsValidIntegerLiteral(const std::string& literal, std::string& literalMut, F&& function);
		std::string ReadOperand();

		bool ParseStructure();
		bool ParseField(Structure* structure);
		bool ParseFunction(bool isProcedure);
		bool ParseLabel();
		bool ParseInstruction(Function* function);

		std::optional<Type> ParseType(std::string& line);
		Number ParseNumber(std::string& line);
		template<typename F>
		Number ParseInteger(const std::string& literal, std::string& literalMut, bool isNegative, int base, F&& function);
		Number ParseBinInteger(const std::string& literal, std::string& literalMut, bool isNegative);
		Number ParseOctInteger(const std::string& literal, std::string& literalMut, bool isNegative);
		Number ParseDecInteger(const std::string& literal, std::string& literalMut, bool isNegative);
		Number ParseHexInteger(const std::string& literal, std::string& literalMut, bool isNegative);
		Number ParseDecimal(const std::string& literal, std::string& literalMut, bool isNegative);

		std::optional<sgn::FieldIndex> GetField(const std::string& name);
		std::optional<sgn::FunctionIndex> GetFunction(const std::string& name);
		std::optional<sgn::LabelIndex> GetLabel(Function* function, const std::string& name);
		std::optional<sgn::LocalVariableIndex> GetLocalVariable(Function* function, const std::string& name);
	};
}