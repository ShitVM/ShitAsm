#pragma once

#include <sam/Assembly.hpp>
#include <sam/Lexer.hpp>

#include <sstream>
#include <string>
#include <vector>




#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <ostream>
#include <string>
#include <variant>

namespace sam {
	class Parser final {
	private:
		std::string m_Path;
		std::vector<Token> m_Tokens;
		std::ostringstream m_ErrorStream;

		std::size_t m_Token = 0;
		Token m_EmptyToken;
		const std::string* m_CurrentStructure = nullptr;
		const std::string* m_CurrentFunction = nullptr;

		Assembly m_Result;
		bool m_HasError = false;
		bool m_HasWarning = false;
		bool m_HasInfo = false;

	public:
		Parser(std::string path, std::vector<Token> tokens) noexcept;
		Parser(const Parser&) = delete;
		~Parser() = default;

	public:
		Parser& operator=(const Parser&) = delete;

	public:
		void Parse();
		Assembly GetAssembly() noexcept;

		bool HasError() const noexcept;
		bool HasMessage() const noexcept;
		std::string GetMessages() const;

	private:
		void ResetState() noexcept;
		const Token& GetToken(std::size_t i) const noexcept;
		bool SkipOtherTokens(bool hasError);

		bool FirstPass();
		bool SecondPass();
		bool ThirdPass();

		void GenerateBuilders();

		bool ParseStructure();
		bool ParseFunction();
		bool ParseLabel();
	};
}

namespace sam {
	struct Type final {
		sgn::Type ElementType;
		std::string ElementTypeName;
		std::optional<std::uint64_t> ElementCount;
	};
}

namespace sam {
	class OldParser final {
	private:
		using Number = std::variant<bool, std::uint32_t, std::uint64_t, double>;

	private:
		Assembly& m_Assembly;
		std::ifstream m_ReadStream;
		std::string m_ReadPath;
		std::ostream& m_ErrorStream;

		std::string m_CurrentStructure;
		std::string m_CurrentFunction;

		std::string m_Line;
		std::size_t m_LineNum = 0;

	public:
		OldParser(Assembly& assembly, std::ostream& errorStream) noexcept;
		OldParser(const Assembly&) = delete;
		~OldParser() = default;

	public:
		OldParser& operator=(const Assembly&) = delete;
		bool operator==(const OldParser&) = delete;
		bool operator!=(const OldParser&) = delete;

	public:
		bool Parse(const std::string& path);
		void Generate(const std::string& path);

	private:
		bool SecondPass();
		bool ThirdPass();

		bool IsValidIdentifier(const std::string& identifier);
		sgn::Type GetType(const std::string& name);
		template<typename F>
		bool IsValidIntegerLiteral(const std::string& literal, std::string& literalMut, F&& function);
		std::string ReadOperand();

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