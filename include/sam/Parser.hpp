#pragma once

#include <sam/Assembly.hpp>
#include <sam/Function.hpp>
#include <sam/Lexer.hpp>
#include <sam/Structure.hpp>
#include <sgn/Operand.hpp>
#include <sgn/Type.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace sam {
	struct Type final {
		sgn::Type ElementType;
		std::string ElementTypeName;
		std::optional<std::uint64_t> ElementCount;
	};
}

namespace sam {
	struct Name final {
		std::string NameSpace;
		std::string Identifier;
		std::string Full;
	};
}

namespace sam {
	class Parser final {
	private:
		const std::vector<const char*>& m_ImportDirectories;
		std::string m_Path;
		std::vector<Token> m_Tokens;
		std::ostringstream m_ErrorStream;
		int m_Depth = 0;

		std::size_t m_Token = 0;
		Token m_EmptyToken;
		Structure* m_CurrentStructure = nullptr;
		Function* m_CurrentFunction = nullptr;

		Assembly m_Result;
		bool m_HasError = false;
		bool m_HasWarning = false;
		bool m_HasInfo = false;

	public:
		Parser(const std::vector<const char*>& importDirectories,
			std::string path, std::vector<Token> tokens, int depth) noexcept;
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
		bool Accept(const Token*& token, TokenType type) noexcept;
		bool AcceptOr(const Token*& token, TokenType typeA, TokenType typeB) noexcept;
		bool NextLine(int hasError);

		bool Pass(int(Parser::*function)(), bool isFirst);
		bool FirstPass();
		bool SecondPass();
		bool ThirdPass();
		bool FourthPass();
		void GenerateBuilders();

		bool IgnoreImport();
		bool IgnoreStructure();
		bool IgnoreFunction();
		bool IgnoreLabel();

		int ParsePrototypes();
		bool ParseStructure();
		bool ParseFunction(bool hasResult);
		bool ParseLabel();

		int ParseDependencies();
		std::optional<Name> ParseName(const std::string& required, int dot, bool isType, bool isField = false);
		bool ParseExternModule(const Name& namespaceName, const std::string& path);
		bool ParseImport();

		int ParseFields();
		std::variant<std::monostate, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t, float, double> ParseNumber();
		std::variant<std::monostate, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t, float, double> MakeNegative(std::variant<std::uint32_t, std::uint64_t, float, double> literal, bool isNegative);
		bool IsNegative(std::variant<std::monostate, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t, float, double> value);
		sgn::Type GetType(const Name& name, const Structure** outStructure = nullptr);
		std::optional<Type> ParseType(bool isField = false);
		bool ParseField();

		int ParseInstructions();
		bool ParseInstruction();
		bool ParsePushInstruction();
		bool ParseLoadInstruction();
		bool ParseStoreInstruction();
		bool ParseLeaInstruction();
		bool ParseFLeaInstruction();
		bool ParseJmpInstruction();
		bool ParseJeInstruction();
		bool ParseJneInstruction();
		bool ParseJaInstruction();
		bool ParseJaeInstruction();
		bool ParseJbInstruction();
		bool ParseJbeInstruction();
		bool ParseCallInstruction();
		bool ParseNewInstruction();
		bool ParseGCNewInstruction();
		bool ParseAPushInstruction();
		bool ParseANewInstruction();
		bool ParseAGCNewInstruction();
		bool ParseString32Statement(); // ShitAsm Extension

		std::optional<sgn::FieldIndex> GetField(const Name& name);
		std::variant<std::monostate, sgn::FunctionIndex, sgn::MappedFunctionIndex> GetFunction(const Name& name);
		std::optional<sgn::LabelIndex> GetLabel(const std::string& name);
		std::optional<sgn::LocalVariableIndex> GetLocalVaraible(const std::string& name);
	};
}