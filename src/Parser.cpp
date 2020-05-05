#include <sam/Parser.hpp>

#include <sam/ExternModule.hpp>
#include <sam/String.hpp>
#include <sgn/ByteFile.hpp>
#include <svm/Type.hpp>
#include <svm/detail/FileSystem.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>

namespace sam {
	Parser::Parser(std::string path, std::vector<Token> tokens, bool isExternModule) noexcept
		: m_Path(std::move(path)), m_Tokens(std::move(tokens)), m_IsExternModule(isExternModule) {}

#define CURRENT_TOKEN (&GetToken(m_Token))

#define MESSAGEBASE m_ErrorStream << "In file '" << m_Path << "':\n    "
#define INFO (m_HasInfo = true, MESSAGEBASE) << "Info: Line " << CURRENT_TOKEN->Line << ", "
#define WARNING (m_HasWarning = true, MESSAGEBASE) << "Warning: Line " << CURRENT_TOKEN->Line << ", "
#define ERROR (m_HasError = true, MESSAGEBASE) << "Error: Line " << CURRENT_TOKEN->Line << ", "

	void Parser::Parse() {
		if (!FirstPass()) return;
		ResetState();

		if (!SecondPass()) return;
		ResetState();

		if (!m_IsExternModule) {
			ThirdPass();
		}
	}
	Assembly Parser::GetAssembly() noexcept {
		return std::move(m_Result);
	}

	bool Parser::HasError() const noexcept {
		return m_HasError;
	}
	bool Parser::HasMessage() const noexcept {
		return m_HasError || m_HasWarning || m_HasInfo;
	}
	std::string Parser::GetMessages() const {
		return m_ErrorStream.str();
	}

	void Parser::ResetState() noexcept {
		m_Token = 0;
		m_CurrentStructure = nullptr;
		m_CurrentFunction = nullptr;
	}
	const Token& Parser::GetToken(std::size_t i) const noexcept {
		if (i >= m_Tokens.size()) return m_EmptyToken;
		else return m_Tokens[i];
	}
	bool Parser::Accept(const Token*& token, TokenType type) noexcept {
		const Token& currentToken = GetToken(m_Token);
		if (currentToken.Type == type) {
			token = &currentToken;
			++m_Token;
			return true;
		} else return false;
	}
	bool Parser::AcceptOr(const Token*& token, TokenType typeA, TokenType typeB) noexcept {
		const Token& currentToken = GetToken(m_Token);
		if (currentToken.Type == typeA || currentToken.Type == typeB) {
			token = &currentToken;
			++m_Token;
			return true;
		} else return false;
	}
	bool Parser::NextLine(int hasError) {
		const Token* newLineToken = nullptr;
		bool hasUnexceptedTokens = false;
		while (!AcceptOr(newLineToken, TokenType::None, TokenType::NewLine)) {
			++m_Token;
			hasUnexceptedTokens = true;
		}

		if (hasUnexceptedTokens && hasError == 0) {
			ERROR << "Unexcepted tokens before end-of-line.\n";
			hasError = true;
		}
		return hasError == 1;
	}

	bool Parser::Pass(int(Parser::*function)(), bool isFirst) {
		bool hasError = false;
		for (m_Token = 0; m_Token < m_Tokens.size();) {
			m_EmptyToken.Line = m_Tokens[m_Token].Line;
			hasError |= NextLine((this->*function)());
		}

		if (isFirst && !m_IsExternModule) {
			if (!m_Result.HasFunction("entrypoint")) {
				MESSAGEBASE << "Error: There is no 'entrypoint' procedure.\n";
				hasError = true;
			}
			GenerateBuilders();
		}

		return !hasError;
	}
	bool Parser::FirstPass() {
		return Pass(&Parser::ParsePrototypes, true);
	}
	bool Parser::SecondPass() {
		return Pass(&Parser::ParseFields, false);
	}
	bool Parser::ThirdPass() {
		return Pass(&Parser::ParseInstructions, false);
	}
	void Parser::GenerateBuilders() {
		for (auto& func : m_Result.Functions) {
			if (func.Name == "entrypoint") {
				func.Builder = std::make_unique<sgn::Builder>(m_Result.ByteFile, m_Result.ByteFile.GetEntrypoint());
			} else {
				func.Builder = std::make_unique<sgn::Builder>(m_Result.ByteFile, func.Index);
			}

			for (auto& label : func.Labels) {
				label.Index = func.Builder->ReserveLabel(label.Name);
			}

			std::uint32_t i = 0;
			for (auto& param : func.LocalVariables) {
				param.Index = func.Builder->GetArgument(i++);
			}
		}
	}

	int Parser::ParsePrototypes() {
		const Token* token = nullptr;
		if (Accept(token, TokenType::ImportKeyword)) return ParseImport();
		else if (Accept(token, TokenType::StructKeyword)) return ParseStructure();
		else if (AcceptOr(token, TokenType::FuncKeyword, TokenType::ProcKeyword)) return ParseFunction(token->Type == TokenType::FuncKeyword);
		else if (GetToken(m_Token + 1).Type == TokenType::Colon) return ParseLabel();
		else return 2;
	}
	std::optional<Name> Parser::ParseName(const std::string& required, int dot, bool isType) {
		bool hasError = false;
		std::string name;

		const Token* token = nullptr;
		const Token* beforeToken = nullptr;
		while (true) {
			if (AcceptOr(token, TokenType::None, TokenType::NewLine)) {
				if (!beforeToken) {
					ERROR << "Required " << required << ".\n";
					return std::nullopt;
				} else break;
			} else if (Accept(token, TokenType::Identifier)) {
				if (beforeToken && beforeToken->Type == TokenType::Identifier) {
					ERROR << "Excepted '.' after namespace name.\n";
					hasError = true;
				} else if (beforeToken && isType && IsTypeKeyword(beforeToken->Type)) {
					ERROR << "Unexcepted identifier after " << required << ".\n";
					hasError = true;
				} else {
					name.append(token->Word);
				}
			} else if (Accept(token, TokenType::Dot)) {
				if (!beforeToken) {
					ERROR << "Excepted " << required << " before '.'.\n";
					hasError = true;
				} else if (isType && IsTypeKeyword(beforeToken->Type)) {
					ERROR << "Excepted namespace name after '.'.\n";
					hasError = true;
				} else if (beforeToken->Type == TokenType::Dot) {
					ERROR << "Excepted " << required << " after '.'.\n";
					hasError = true;
				} else {
					name.push_back('.');
				}
			} else {
				if (isType && IsTypeKeyword(GetToken(m_Token).Type)) {
					++m_Token;
					if (beforeToken && beforeToken->Type == TokenType::Identifier) {
						ERROR << "Excepted '.' after namespace name.\n";
						hasError = true;
					} else if (beforeToken && isType && IsTypeKeyword(beforeToken->Type)) {
						--m_Token;
						break;
					} else {
						name.append(token->Word);
					}
				} else if (!beforeToken) {
					ERROR << "Required " << required << ".\n";
					return std::nullopt;
				} else break;
			}

			beforeToken = token;
		}

		if (hasError) return std::nullopt;
		else if (beforeToken->Type == TokenType::Dot) {
			ERROR << "Excepted " << required << " after '.'.\n";
			return std::nullopt;
		}

		std::size_t lastDot = name.find_last_of('.');
		while (dot--) {
			lastDot = name.find_last_of('.', lastDot - 1);
		}
		return Name{ name.substr(0, lastDot), name.substr(lastDot + 1), std::move(name) };
	}
	bool Parser::ParseImport() {
		const Token* pathToken = nullptr;
		if (!Accept(pathToken, TokenType::String)) {
			if (AcceptOr(pathToken, TokenType::None, TokenType::NewLine)) {
				ERROR << "Unexcepted end-of-line.\n";
			} else if (Accept(pathToken, TokenType::AsKeyword)) {
				ERROR << "Required module path.\n";
			} else {
				ERROR << "Excepted module path.\n";
			}
			return true;
		}

		const Token* asToken = nullptr;
		if (!Accept(asToken, TokenType::AsKeyword)) {
			ERROR << "Excepted 'as' after module path.\n";
			return true;
		}

		bool hasError = false;
		const auto namespaceName = ParseName("namespace name", 0, false);
		if (!namespaceName) return true;
		--m_Token;

		const auto iter = std::find_if(m_Result.Dependencies.begin(), m_Result.Dependencies.end(), [&namespaceName](const auto& module) {
			return namespaceName->Full == module.NameSpace;
		});
		if (iter != m_Result.Dependencies.end()) {
			ERROR << "Duplicated namespace name '" << namespaceName->Full << "'.\n";
			return true;
		}

		const std::string path = std::get<std::string>(pathToken->Data);
		const std::string absPath = svm::detail::GetAbsolutePath(path);
		if (!m_IsExternModule) {
			if (m_Result.HasDependency(absPath)) {
				ERROR << "Already imported module '" << path << "'.\n";
				return true;
			}

			std::ifstream inputStream(path);
			if (!inputStream) {
				ERROR << "Failed to open '" << path << "'.\n";
				return true;
			}

			ExternModule& module = m_Result.Dependencies.emplace_back(ExternModule{ absPath });

			Lexer lexer(path, inputStream);
			lexer.Lex();
			if (lexer.HasMessage()) {
				m_ErrorStream << lexer.GetMessages();
				if (lexer.HasError()) {
					m_HasError = hasError = true;
					goto parsed;
				}
			}

			Parser parser(path, lexer.GetTokens(), true);
			parser.Parse();
			if (parser.HasMessage()) {
				m_ErrorStream << parser.GetMessages();
				if (parser.HasError()) {
					m_HasError = hasError = true;
					goto parsed;
				}
			}

			module.Assembly = parser.GetAssembly();
			module.Index = m_Result.ByteFile.AddExternModule(svm::detail::fs::relative(path).string());
			module.NameSpace = std::move(namespaceName->Full);
		}

	parsed:
		return hasError;
	}
	bool Parser::ParseStructure() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			if (AcceptOr(nameToken, TokenType::None, TokenType::NewLine)) {
				ERROR << "Unexcepted end-of-line.\n";
			} else if (Accept(nameToken, TokenType::Colon)) {
				ERROR << "Required structure name.\n";
			} else {
				ERROR << "Invalid structure name.\n";
			}
			return true;
		}

		const Token* colonToken = nullptr;
		if (!Accept(colonToken, TokenType::Colon)) {
			ERROR << "Excepted ':' after structure name.\n";
			return true;
		}

		bool hasError = false;

		if (m_Result.HasStructure(nameToken->Word)) {
			ERROR << "Duplicated structure name '" << nameToken->Word << "'.\n";
			hasError = true;
		}

		const sgn::StructureIndex index = m_Result.ByteFile.AddStructure(nameToken->Word);
		m_Result.Structures.push_back(Structure{ nameToken->Word, index });

		m_CurrentStructure = &m_Result.Structures.back();
		m_CurrentFunction = nullptr;
		return hasError;
	}
	bool Parser::ParseFunction(bool hasResult) {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			if (AcceptOr(nameToken, TokenType::None, TokenType::NewLine)) {
				ERROR << "Unexcepted end-of-line.\n";
			} else if (Accept(nameToken, TokenType::Colon)) {
				ERROR << "Required function or procedure name.\n";
			} else {
				ERROR << "Invalid function or procedure name.\n";
			}
			return true;
		}

		const Token* colonOrParamBeginToken = nullptr;
		if (!AcceptOr(colonOrParamBeginToken, TokenType::Colon, TokenType::LeftParenthesis)) {
			ERROR << "Excepted ':' after function or procedure name.\n";
			return true;
		}

		bool hasError = false;

		std::vector<LocalVariable> params;
		if (colonOrParamBeginToken->Type == TokenType::LeftParenthesis) {
			std::vector<std::string> strParams;

			const Token* token = nullptr;
			const Token* beforeToken = nullptr;
			while (true) {
				if (AcceptOr(token, TokenType::None, TokenType::NewLine)) {
					ERROR << "Unexcepted end-of-line.\n";
					hasError = true;
					break;
				} else if (Accept(token, TokenType::RightParenthesis)) break;
				else if (Accept(token, TokenType::Identifier)) {
					if (beforeToken && beforeToken->Type == TokenType::Identifier) {
						ERROR << "Excepted ',' after parameter name.\n";
						hasError = true;
					} else {
						strParams.push_back(token->Word);
					}
				} else if (Accept(token, TokenType::Comma)) {
					if (!beforeToken) {
						ERROR << "Excepted ')' after '('.\n";
						hasError = true;
					} else if (beforeToken->Type == TokenType::Comma) {
						ERROR << "Excepted parameter name after ','.\n";
						hasError = true;
					}
				} else {
					if (!beforeToken) {
						ERROR << "Excepted ')' after '('.\n";
					} else if (beforeToken->Type == TokenType::Identifier) {
						ERROR << "Excepted ')' after parameter name.\n";
					} else if (beforeToken->Type == TokenType::Comma) {
						ERROR << "Excepted parameter name after ','.\n";
					}
					hasError = true;
				}

				beforeToken = token;
			}

			std::vector<std::string> sortedStrParams = strParams;
			std::sort(sortedStrParams.begin(), sortedStrParams.end());
			if (const auto duplicated = std::unique(sortedStrParams.begin(), sortedStrParams.end()); duplicated != sortedStrParams.end()) {
				ERROR << "Duplicated parameter name '" << *duplicated << "'.\n";
				hasError = true;
			}
			std::transform(strParams.begin(), strParams.end(), std::back_inserter(params), [](auto& name) -> LocalVariable {
				return { std::move(name) };
			});

			if (token->Type == TokenType::RightParenthesis) {
				const Token* colonToken = nullptr;
				if (!Accept(colonToken, TokenType::Colon)) {
					ERROR << "Excepted ':' after ')'.\n";
					return true;
				}
			} else return true;
		}

		if (m_Result.HasFunction(nameToken->Word)) {
			ERROR << "Duplicated function or procedure name '" << nameToken->Word << "'.\n";
			hasError = true;
		}

		sgn::FunctionIndex index = sgn::FunctionIndex::OperandIndex/*Dummy*/;
		if (nameToken->Word != "entrypoint") {
			index = m_Result.ByteFile.AddFunction(nameToken->Word, static_cast<std::uint16_t>(params.size()), hasResult);
		} else if (hasResult) {
			ERROR << "Invalid function name 'entrypoint'.\n";
			INFO << "It can be used only for procedure.\n";
			hasError = true;
		}
		m_Result.Functions.push_back(Function{ nullptr, nameToken->Word, index, {}, std::move(params) });

		m_CurrentStructure = nullptr;
		m_CurrentFunction = &m_Result.Functions.back();
		return hasError;
	}
	bool Parser::ParseLabel() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			ERROR << "Invalid label name.\n";
			return true;
		} else if (m_CurrentFunction == nullptr) {
			ERROR << "Not belonged label.\n";
			return true;
		}

		bool hasError = false;

		if (m_CurrentFunction->HasLabel(nameToken->Word)) {
			ERROR << "Duplicated label name '" << nameToken->Word << "'.\n";
			hasError = true;
		}

		m_CurrentFunction->Labels.push_back(Label{ nameToken->Word });
		++m_Token;
		return hasError;
	}

	bool Parser::IgnoreImport() {
		const Token* token = nullptr;
		while (!AcceptOr(token, TokenType::None, TokenType::NewLine)) {
			++m_Token;
		}

		--m_Token;
		return false;
	}
	bool Parser::IgnoreStructure() {
		m_CurrentStructure = &m_Result.GetStructure(GetToken(m_Token).Word);
		m_CurrentFunction = nullptr;

		m_Token += 2;
		return false;
	}
	bool Parser::IgnoreFunction() {
		m_CurrentStructure = nullptr;
		m_CurrentFunction = &m_Result.GetFunction(GetToken(m_Token).Word);

		const Token* token = nullptr;
		while (!Accept(token, TokenType::Colon)) {
			++m_Token;
		}

		return false;
	}
	bool Parser::IgnoreLabel() {
		m_Token += 2;
		return false;
	}

	int Parser::ParseFields() {
		const Token* token = nullptr;
		if (Accept(token, TokenType::ImportKeyword)) return IgnoreImport();
		else if (Accept(token, TokenType::StructKeyword)) return IgnoreStructure();
		else if (AcceptOr(token, TokenType::FuncKeyword, TokenType::ProcKeyword)) return IgnoreFunction();
		else if (GetToken(m_Token + 1).Type == TokenType::Colon) return IgnoreLabel();
		else if (m_CurrentStructure) return ParseField();
		else return 2;
	}
	std::variant<std::monostate, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t, double> Parser::ParseNumber() {
		const Token* maybeMinusToken = nullptr;
		Accept(maybeMinusToken, TokenType::Minus);

		const Token* literalToken = nullptr;
		if (IsInteger(GetToken(m_Token).Type)) {
			literalToken = &GetToken(m_Token++);
			const std::uint64_t value = std::get<std::uint64_t>(literalToken->Data);
			if (literalToken->Suffix == "i") {
				if (value > std::numeric_limits<std::uint32_t>::max()) {
					WARNING << "Overflowed integer literal.\n";
				}
				return MakeNegative(static_cast<std::uint32_t>(value), maybeMinusToken);
			} else if (literalToken->Suffix == "l") return MakeNegative(value, maybeMinusToken);
			else if (value <= std::numeric_limits<std::uint32_t>::max()) return MakeNegative(static_cast<std::uint32_t>(value), maybeMinusToken);
			else return MakeNegative(value, maybeMinusToken);
		} else if (Accept(literalToken, TokenType::Decimal)) return MakeNegative(std::get<double>(literalToken->Data), maybeMinusToken);
		else return std::monostate();
	}
	std::variant<std::monostate, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t, double> Parser::MakeNegative(std::variant<std::uint32_t, std::uint64_t, double> literal, bool isNegative) {
		if (isNegative) {
			if (std::holds_alternative<std::uint32_t>(literal)) {
				const std::uint32_t value = std::get<std::uint32_t>(literal);
				if (value > 2147483648/*2^31*/) {
					WARNING << "Overflowed integer literal.\n";
				}
				return static_cast<std::int32_t>(value);
			} else if (std::holds_alternative<std::uint64_t>(literal)) {
				const std::uint64_t value = std::get<std::uint64_t>(literal);
				if (value > 922337236854775808/*2^63*/) {
					WARNING << "Overflowed integer literal.\n";
				}
				return static_cast<std::int64_t>(value);
			} else if (std::holds_alternative<double>(literal)) return -std::get<double>(literal);
			else return std::monostate();
		} else return std::visit([](auto value) -> std::variant<std::monostate, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t, double> {
			return value;
		}, literal);
	}
	bool Parser::IsNegative(std::variant<std::monostate, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t, double> value) {
		if (std::holds_alternative<double>(value)) return std::get<double>(value) < 0;
		else return std::holds_alternative<std::int32_t>(value) || std::holds_alternative<std::int64_t>(value);
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

		const auto structIter = m_Result.FindStructure(name);
		if (structIter != m_Result.Structures.end()) return m_Result.ByteFile.GetStructureInfo(structIter->Index)->Type;
		else return nullptr;
	}
	std::optional<Type> Parser::ParseType() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			if (AcceptOr(nameToken, TokenType::None, TokenType::NewLine)) {
				ERROR << "Unexcepted end-of-line.\n";
			} else if (Accept(nameToken, TokenType::LeftBracket)) {
				ERROR << "Required type name.\n";
			} else if (IsTypeKeyword(GetToken(m_Token).Type)) {
				nameToken = &GetToken(m_Token++);
				goto parseType;
			} else {
				ERROR << "Invalid function or procedure name.\n";
			}
			return std::nullopt;
		}

	parseType:
		const sgn::Type type = GetType(nameToken->Word);

		const Token* maybeLeftBracketToken = nullptr;
		if (!Accept(maybeLeftBracketToken, TokenType::LeftBracket)) return Type{ type, nameToken->Word };

		bool hasError = false;
		std::uint64_t length = 0;

		const Token* lengthOrRightBracketToken = nullptr;
		if (Accept(lengthOrRightBracketToken, TokenType::RightBracket)) return Type{ type, nameToken->Word, 0 };
		else if (Accept(lengthOrRightBracketToken, TokenType::Decimal)) {
			ERROR << "Array's length must be integer.\n";
			hasError = true;
			length = 1; // Dummy
		} else if (IsInteger(GetToken(m_Token).Type)) {
			const auto lengthTemp = ParseNumber();
			if (IsNegative(lengthTemp)) {
				ERROR << "Array's length cannot be negative.\n";
				hasError = true;
				length = 1; // Dummy
			} else {
				if (std::holds_alternative<std::uint32_t>(lengthTemp)) {
					length = std::get<std::uint32_t>(lengthTemp);
				} else if (std::holds_alternative<std::uint64_t>(lengthTemp)) {
					length = std::get<std::uint64_t>(lengthTemp);
				}

				if ((length = std::get<std::uint64_t>(GetToken(m_Token++).Data)) == 0) {
					ERROR << "Array's length cannot be zero.\n";
					hasError = true;
				}
			}
		} else {
			ERROR << "Excepted ']' after '['.\n";
			return std::nullopt;
		}

		const Token* rightBracketToken = nullptr;
		if (!Accept(rightBracketToken, TokenType::RightBracket)) {
			ERROR << "Excepted ']' after array's length.\n";
			return std::nullopt;
		} else return Type{ type, nameToken->Word, length };
	}
	bool Parser::ParseField() {
		bool hasError = false;

		const auto type = ParseType();
		if (!type) return true;
		else if (type->ElementType == nullptr) {
			ERROR << "Nonexistent type '" << type->ElementTypeName << "'.\n";
			return true;
		} else if (type->ElementCount && *type->ElementCount == 0) {
			ERROR << "Required array's length.\n";
			hasError = true;
		}

		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			if (AcceptOr(nameToken, TokenType::None, TokenType::NewLine)) {
				ERROR << "Unexcepted end-of-line.\n";
			} else {
				ERROR << "Invalid field name.\n";
			}
			return true;
		}

		const sgn::StructureIndex structIndex = m_CurrentStructure->Index;
		sgn::StructureInfo* const structureInfo = m_Result.ByteFile.GetStructureInfo(structIndex);

		if (m_CurrentStructure->HasField(nameToken->Word)) {
			ERROR << "Duplicated field name '" << nameToken->Word << "'.\n";
			hasError = true;
		}

		const sgn::FieldIndex index = structureInfo->AddField(type->ElementType, type->ElementCount.value_or(0));
		m_CurrentStructure->Fields.push_back(Field{ nameToken->Word, index });
		return hasError;
	}

	int Parser::ParseInstructions() {
		const Token* token = nullptr;
		if (Accept(token, TokenType::ImportKeyword)) return IgnoreImport();
		else if (Accept(token, TokenType::StructKeyword)) return IgnoreStructure();
		else if (AcceptOr(token, TokenType::FuncKeyword, TokenType::ProcKeyword)) return IgnoreFunction();
		else if (GetToken(m_Token + 1).Type == TokenType::Colon) {
			m_CurrentFunction->Builder->AddLabel(GetToken(m_Token).Word);
			return IgnoreLabel();
		} else if (m_CurrentStructure) return 2;
		else return ParseInstruction();
	}
	bool Parser::ParseInstruction() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			if (AcceptOr(nameToken, TokenType::None, TokenType::NewLine)) return false;
			else {
				ERROR << "Invalid mnemonic.\n";
				return true;
			}
		} else if (m_CurrentFunction == nullptr) {
			ERROR << "Not belonged instruction.\n";
			return true;
		}

		std::string mnemonic = nameToken->Word;
		std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), [](char c) {
			return static_cast<char>(std::tolower(c));
		});

		switch (CRC32(mnemonic)) {
		case "nop"_h: m_CurrentFunction->Builder->Nop(); break;

		case "push"_h: return ParsePushInstruction();
		case "pop"_h: m_CurrentFunction->Builder->Pop(); break;
		case "load"_h: return ParseLoadInstruction();
		case "store"_h: return ParseStoreInstruction();
		case "lea"_h: return ParseLeaInstruction();
		case "flea"_h: return ParseFLeaInstruction();
		case "tload"_h: m_CurrentFunction->Builder->TLoad(); break;
		case "tstore"_h: m_CurrentFunction->Builder->TStore(); break;
		case "copy"_h: m_CurrentFunction->Builder->Copy(); break;
		case "swap"_h: m_CurrentFunction->Builder->Swap(); break;

		case "add"_h: m_CurrentFunction->Builder->Add(); break;
		case "sub"_h: m_CurrentFunction->Builder->Sub(); break;
		case "mul"_h: m_CurrentFunction->Builder->Mul(); break;
		case "imul"_h: m_CurrentFunction->Builder->IMul(); break;
		case "div"_h: m_CurrentFunction->Builder->Div(); break;
		case "idiv"_h: m_CurrentFunction->Builder->IDiv(); break;
		case "mod"_h: m_CurrentFunction->Builder->Mod(); break;
		case "imod"_h: m_CurrentFunction->Builder->IMod(); break;
		case "neg"_h: m_CurrentFunction->Builder->Neg(); break;
		case "inc"_h: m_CurrentFunction->Builder->Inc(); break;
		case "dec"_h: m_CurrentFunction->Builder->Dec(); break;

		case "and"_h: m_CurrentFunction->Builder->And(); break;
		case "or"_h: m_CurrentFunction->Builder->Or(); break;
		case "xor"_h: m_CurrentFunction->Builder->Xor(); break;
		case "not"_h: m_CurrentFunction->Builder->Not(); break;
		case "shl"_h: m_CurrentFunction->Builder->Shl(); break;
		case "sal"_h: m_CurrentFunction->Builder->Sal(); break;
		case "shr"_h: m_CurrentFunction->Builder->Shr(); break;
		case "sar"_h: m_CurrentFunction->Builder->Sar(); break;

		case "cmp"_h: m_CurrentFunction->Builder->Cmp(); break;
		case "icmp"_h: m_CurrentFunction->Builder->ICmp(); break;
		case "jmp"_h: return ParseJmpInstruction();
		case "je"_h: return ParseJeInstruction();
		case "jne"_h: return ParseJneInstruction();
		case "ja"_h: return ParseJaInstruction();
		case "jae"_h: return ParseJaeInstruction();
		case "jb"_h: return ParseJbInstruction();
		case "jbe"_h: return ParseJbeInstruction();
		case "call"_h: return ParseCallInstruction();
		case "ret"_h: m_CurrentFunction->Builder->Ret(); break;

		case "toi"_h: m_CurrentFunction->Builder->ToI(); break;
		case "tol"_h: m_CurrentFunction->Builder->ToL(); break;
		case "tod"_h: m_CurrentFunction->Builder->ToD(); break;
		case "top"_h: m_CurrentFunction->Builder->ToP(); break;

		case "null"_h: m_CurrentFunction->Builder->Null(); break;
		case "new"_h: return ParseNewInstruction();
		case "delete"_h: m_CurrentFunction->Builder->Delete(); break;
		case "gcnull"_h: m_CurrentFunction->Builder->GCNull(); break;
		case "gcnew"_h: return ParseGCNewInstruction();
		case "apush"_h: return ParseAPushInstruction();
		case "anew"_h: return ParseANewInstruction();
		case "agcnew"_h: return ParseAGCNewInstruction();
		case "alea"_h: m_CurrentFunction->Builder->ALea(); break;
		case "count"_h: m_CurrentFunction->Builder->Count(); break;

		default:
			ERROR << "Unknown mnemonic.\n";
			return true;
		}

		return false;
	}
	bool Parser::ParsePushInstruction() {
		const auto number = ParseNumber();
		const Token* structureToken = nullptr;
		if (!std::holds_alternative<std::monostate>(number)) {
			if (std::holds_alternative<std::int32_t>(number)) {
				const auto inx = m_Result.ByteFile.AddIntConstant(static_cast<std::uint32_t>(std::get<std::int32_t>(number)));
				m_CurrentFunction->Builder->Push(inx);
			} else if (std::holds_alternative<std::uint32_t>(number)) {
				const auto inx = m_Result.ByteFile.AddIntConstant(std::get<std::uint32_t>(number));
				m_CurrentFunction->Builder->Push(inx);
			} else if (std::holds_alternative<std::int64_t>(number)) {
				const auto inx = m_Result.ByteFile.AddLongConstant(static_cast<std::uint64_t>(std::get<std::int64_t>(number)));
				m_CurrentFunction->Builder->Push(inx);
			} else if (std::holds_alternative<std::uint64_t>(number)) {
				const auto inx = m_Result.ByteFile.AddLongConstant(std::get<std::uint64_t>(number));
				m_CurrentFunction->Builder->Push(inx);
			} else {
				const auto inx = m_Result.ByteFile.AddDoubleConstant(std::get<double>(number));
				m_CurrentFunction->Builder->Push(inx);
			}
		} else if (Accept(structureToken, TokenType::Identifier)) {
			const auto structure = m_Result.FindStructure(structureToken->Word);
			if (structure == m_Result.Structures.end()) {
				ERROR << "Nonexistent structure name '" << structureToken->Word << "'.\n";
				return true;
			}
			m_CurrentFunction->Builder->Push(structure->Index);
		} else {
			ERROR << "Excepted literal or structure name.\n";
			return true;
		}
		return false;
	}
	bool Parser::ParseLoadInstruction() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			ERROR << "Excepted parameter or local variable name.\n";
			return true;
		}

		const auto var = GetLocalVaraible(nameToken->Word);
		if (!var) {
			ERROR << "Nonexistent local variable '" << nameToken->Word << "'.\n";
			return true;
		}

		m_CurrentFunction->Builder->Load(*var);
		return false;
	}
	bool Parser::ParseStoreInstruction() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			ERROR << "Excepted parameter or local variable name.\n";
			return true;
		}

		auto var = GetLocalVaraible(nameToken->Word);
		if (!var) {
			var = m_CurrentFunction->Builder->AddLocalVariable();
			m_CurrentFunction->LocalVariables.push_back(LocalVariable{ nameToken->Word, *var });
		}

		m_CurrentFunction->Builder->Store(*var);
		return false;
	}
	bool Parser::ParseLeaInstruction() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			ERROR << "Excepted parameter or local variable name.\n";
			return true;
		}

		const auto var = GetLocalVaraible(nameToken->Word);
		if (!var) {
			ERROR << "Nonexistent local variable '" << nameToken->Word << "'.\n";
			return true;
		}

		m_CurrentFunction->Builder->Lea(*var);
		return false;
	}
	bool Parser::ParseFLeaInstruction() {
		const auto field = GetField();
		if (!field) return true;

		m_CurrentFunction->Builder->FLea(*field);
		return false;
	}
	bool Parser::ParseJmpInstruction() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			ERROR << "Excepted label name.\n";
			return true;
		}

		const auto label = GetLabel(nameToken->Word);
		if (!label) return true;

		m_CurrentFunction->Builder->Jmp(*label);
		return false;
	}
	bool Parser::ParseJeInstruction() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			ERROR << "Excepted label name.\n";
			return true;
		}

		const auto label = GetLabel(nameToken->Word);
		if (!label) return true;

		m_CurrentFunction->Builder->Je(*label);
		return false;
	}
	bool Parser::ParseJneInstruction() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			ERROR << "Excepted label name.\n";
			return true;
		}

		const auto label = GetLabel(nameToken->Word);
		if (!label) return true;

		m_CurrentFunction->Builder->Jne(*label);
		return false;
	}
	bool Parser::ParseJaInstruction() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			ERROR << "Excepted label name.\n";
			return true;
		}

		const auto label = GetLabel(nameToken->Word);
		if (!label) return true;

		m_CurrentFunction->Builder->Ja(*label);
		return false;
	}
	bool Parser::ParseJaeInstruction() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			ERROR << "Excepted label name.\n";
			return true;
		}

		const auto label = GetLabel(nameToken->Word);
		if (!label) return true;

		m_CurrentFunction->Builder->Jae(*label);
		return false;
	}
	bool Parser::ParseJbInstruction() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			ERROR << "Excepted label name.\n";
			return true;
		}

		const auto label = GetLabel(nameToken->Word);
		if (!label) return true;

		m_CurrentFunction->Builder->Jb(*label);
		return false;
	}
	bool Parser::ParseJbeInstruction() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			ERROR << "Excepted label name.\n";
			return true;
		}

		const auto label = GetLabel(nameToken->Word);
		if (!label) return true;

		m_CurrentFunction->Builder->Jbe(*label);
		return false;
	}
	bool Parser::ParseCallInstruction() {
		const Token* nameToken = nullptr;
		if (!Accept(nameToken, TokenType::Identifier)) {
			ERROR << "Excepted function or procedure name.\n";
			return true;
		}

		const auto function = GetFunction(nameToken->Word);
		if (!function) return true;

		m_CurrentFunction->Builder->Call(*function);
		return false;
	}
	bool Parser::ParseNewInstruction() {
		const auto type = ParseType();
		if (!type) return true;
		else if (type->ElementType == nullptr) {
			ERROR << "Nonexistent type '" << type->ElementTypeName << "'.\n";
			return true;
		} else if (type->ElementCount) {
			ERROR << "Array cannot be used here.\n";
			INFO << "Use 'anew' mnemonic instead.\n";
			return true;
		}

		m_CurrentFunction->Builder->New(m_Result.ByteFile.GetTypeIndex(type->ElementType));
		return false;
	}
	bool Parser::ParseGCNewInstruction() {
		const auto type = ParseType();
		if (!type) return true;
		else if (type->ElementType == nullptr) {
			ERROR << "Nonexistent type '" << type->ElementTypeName << "'.\n";
			return true;
		} else if (type->ElementCount) {
			ERROR << "Array cannot be used here.\n";
			INFO << "Use 'agcnew' mnemonic instead.\n";
			return true;
		}

		m_CurrentFunction->Builder->GCNew(m_Result.ByteFile.GetTypeIndex(type->ElementType));
		return false;
	}
	bool Parser::ParseAPushInstruction() {
		const auto type = ParseType();
		if (!type) return true;
		else if (type->ElementType == nullptr) {
			ERROR << "Nonexistent type '" << type->ElementTypeName << "'.\n";
			return true;
		} else if (!type->ElementCount) {
			ERROR << "Only array can be used here.\n";
			INFO << "Use 'push' mnemonic instead.\n";
			return true;
		} else if (*type->ElementCount != 0) {
			ERROR << "Array's length cannot be used here.\n";
			return true;
		}

		m_CurrentFunction->Builder->APush(m_Result.ByteFile.MakeArray(m_Result.ByteFile.GetTypeIndex(type->ElementType)));
		return false;
	}
	bool Parser::ParseANewInstruction() {
		const auto type = ParseType();
		if (!type) return true;
		else if (type->ElementType == nullptr) {
			ERROR << "Nonexistent type '" << type->ElementTypeName << "'.\n";
			return true;
		} else if (!type->ElementCount) {
			ERROR << "Only array can be used here.\n";
			INFO << "Use 'new' mnemonic instead.\n";
			return true;
		} else if (*type->ElementCount != 0) {
			ERROR << "Array's length cannot be used here.\n";
			return true;
		}

		m_CurrentFunction->Builder->ANew(m_Result.ByteFile.MakeArray(m_Result.ByteFile.GetTypeIndex(type->ElementType)));
		return false;
	}
	bool Parser::ParseAGCNewInstruction() {
		const auto type = ParseType();
		if (!type) return true;
		else if (type->ElementType == nullptr) {
			ERROR << "Nonexistent type '" << type->ElementTypeName << "'.\n";
			return true;
		} else if (!type->ElementCount) {
			ERROR << "Only array can be used here.\n";
			INFO << "Use 'gcnew' mnemonic instead.\n";
			return true;
		} else if (*type->ElementCount != 0) {
			ERROR << "Array's length cannot be used here.\n";
			return true;
		}

		m_CurrentFunction->Builder->AGCNew(m_Result.ByteFile.MakeArray(m_Result.ByteFile.GetTypeIndex(type->ElementType)));
		return false;
	}

	std::optional<sgn::FieldIndex> Parser::GetField() {
		const Token* structureNameToken = nullptr;
		std::vector<Structure>::iterator structure;
		if (!Accept(structureNameToken, TokenType::Identifier)) {
			ERROR << "Excepted structure name.\n";
			return std::nullopt;
		} else if ((structure = m_Result.FindStructure(structureNameToken->Word)) == m_Result.Structures.end()) {
			ERROR << "Nonexistent structure '" << structureNameToken->Word << "'.\n";
			return std::nullopt;
		}

		const Token* dotToken = nullptr;
		if (!Accept(dotToken, TokenType::Dot)) {
			ERROR << "Excepted dot after structure name.\n";
			return std::nullopt;
		}

		const Token* fieldNameToken = nullptr;
		std::vector<Field>::iterator field;
		if (!Accept(fieldNameToken, TokenType::Identifier)) {
			ERROR << "Excepted field name.\n";
			return std::nullopt;
		} else if ((field = structure->FindField(fieldNameToken->Word)) == structure->Fields.end()) {
			ERROR << "Nonexistent field '" << structureNameToken->Word << '.' << fieldNameToken->Word << "'.\n";
			return std::nullopt;
		}

		return field->Index;
	}
	std::optional<sgn::FunctionIndex> Parser::GetFunction(const std::string& name) {
		const auto iter = m_Result.FindFunction(name);
		if (iter == m_Result.Functions.end()) {
			ERROR << "Nonexistent function or procedure '" << name << "'.\n";
			return std::nullopt;
		} else if (name == "entrypoint") {
			ERROR << "Noncallable procedure 'entrypoint'.\n";
			return std::nullopt;
		} else return iter->Index;
	}
	std::optional<sgn::LabelIndex> Parser::GetLabel(const std::string& name) {
		const auto iter = m_CurrentFunction->FindLabel(name);
		if (iter == m_CurrentFunction->Labels.end()) {
			ERROR << "Nonexistent label '" << name << "'.\n";
			return std::nullopt;
		} else return iter->Index;
	}
	std::optional<sgn::LocalVariableIndex> Parser::GetLocalVaraible(const std::string& name) {
		const auto iter = m_CurrentFunction->FindLocalVariable(name);
		if (iter == m_CurrentFunction->LocalVariables.end()) return std::nullopt;
		else return iter->Index;
	}
}