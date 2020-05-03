#include <sam/Parser.hpp>

#include <sam/String.hpp>
#include <sgn/ByteFile.hpp>
#include <svm/Type.hpp>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>



#include <sam/ExternModule.hpp>

#include <sgn/Generator.hpp>
#include <svm/detail/FileSystem.hpp>

namespace sam {
	Parser::Parser(std::string path, std::vector<Token> tokens) noexcept
		: m_Path(std::move(path)), m_Tokens(std::move(tokens)) {}

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

		if (!ThirdPass()) return;
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

		if (hasUnexceptedTokens && !hasError) {
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

		if (isFirst) {
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
		if (Accept(token, TokenType::StructKeyword)) return ParseStructure();
		else if (AcceptOr(token, TokenType::FuncKeyword, TokenType::ProcKeyword)) return ParseFunction(token->Type == TokenType::FuncKeyword);
		else if (GetToken(m_Token + 1).Type == TokenType::Colon) return ParseLabel();
		else return 2;
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

		const sgn::StructureIndex index = m_Result.ByteFile.AddStructure();
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
			std::transform(params.begin(), params.end(), std::back_inserter(params), [](auto& name) -> LocalVariable {
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
			index = m_Result.ByteFile.AddFunction(static_cast<std::uint16_t>(params.size()), hasResult);
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
		if (Accept(token, TokenType::StructKeyword)) return IgnoreStructure();
		else if (AcceptOr(token, TokenType::FuncKeyword, TokenType::ProcKeyword)) return IgnoreFunction();
		else if (GetToken(m_Token + 1).Type == TokenType::Colon) return IgnoreLabel();
		else if (m_CurrentStructure) return ParseField();
		else return 2;
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
			if ((length = std::get<std::uint64_t>(GetToken(m_Token++).Data)) == 0) {
				ERROR << "Array's length cannot be zero.\n";
				hasError = true;
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
		else if (type->ElementCount && *type->ElementCount == 0) {
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
		if (Accept(token, TokenType::StructKeyword)) return IgnoreStructure();
		else if (AcceptOr(token, TokenType::FuncKeyword, TokenType::ProcKeyword)) return IgnoreFunction();
		else if (GetToken(m_Token + 1).Type == TokenType::Colon) return IgnoreLabel();
		else if (m_CurrentStructure) return NextLine(2);
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
		const Token* valueToken = nullptr;
		if (IsInteger(GetToken(m_Token).Type)) {
			valueToken = &GetToken(m_Token++);
			const std::uint64_t value = std::get<std::uint64_t>(valueToken->Data);
			if (valueToken->Suffix == "i") {
				if (value > std::numeric_limits<std::uint32_t>::max()) {
					WARNING << "Overflowed integer literal.\n";
				}
				const auto inx = m_Result.ByteFile.AddIntConstant(static_cast<std::uint32_t>(value));
				m_CurrentFunction->Builder->Push(inx);
			} else if (valueToken->Suffix == "l") {
				const auto inx = m_Result.ByteFile.AddLongConstant(value);
				m_CurrentFunction->Builder->Push(inx);
			} else {
				if (value > std::numeric_limits<std::uint32_t>::max()) {
					const auto inx = m_Result.ByteFile.AddIntConstant(static_cast<std::uint32_t>(value));
					m_CurrentFunction->Builder->Push(inx);
				} else {
					const auto inx = m_Result.ByteFile.AddLongConstant(value);
					m_CurrentFunction->Builder->Push(inx);
				}
			}
		} else if (Accept(valueToken, TokenType::Decimal)) {
			const double value = std::get<double>(valueToken->Data);
			const auto inx = m_Result.ByteFile.AddDoubleConstant(value);
			m_CurrentFunction->Builder->Push(inx);
		} else if (Accept(valueToken, TokenType::Identifier)) {
			const auto structure = m_Result.FindStructure(valueToken->Word);
			if (structure == m_Result.Structures.end()) {
				ERROR << "Nonexistent structure name '" << valueToken->Word << "'.\n";
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
		return false;
	}
	bool Parser::ParseJeInstruction() {
		return false;
	}
	bool Parser::ParseJneInstruction() {
		return false;
	}
	bool Parser::ParseJaInstruction() {
		return false;
	}
	bool Parser::ParseJaeInstruction() {
		return false;
	}
	bool Parser::ParseJbInstruction() {
		return false;
	}
	bool Parser::ParseJbeInstruction() {
		return false;
	}
	bool Parser::ParseCallInstruction() {
		return false;
	}
	bool Parser::ParseNewInstruction() {
		return false;
	}
	bool Parser::ParseGCNewInstruction() {
		return false;
	}
	bool Parser::ParseAPushInstruction() {
		return false;
	}
	bool Parser::ParseANewInstruction() {
		return false;
	}
	bool Parser::ParseAGCNewInstruction() {
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
	std::optional<sgn::LocalVariableIndex> Parser::GetLocalVaraible(const std::string& name) {
		const auto iter = m_CurrentFunction->FindLocalVariable(name);
		if (iter == m_CurrentFunction->LocalVariables.end()) return std::nullopt;
		else return iter->Index;
	}
}

namespace sam {
	OldParser::OldParser(Assembly& assembly, std::ostream& errorStream) noexcept
		: m_Assembly(assembly), m_ErrorStream(errorStream) {}

	bool OldParser::Parse(const std::string& path) {
		m_ReadPath = svm::detail::fs::absolute(path).string();

		m_ReadStream.open(m_ReadPath);
		if (!m_ReadStream) {
			m_ErrorStream << "Error: Failed to open '" << m_ReadPath << "' file.\n";
			return false;
		}

		if (!SecondPass()) return false;

		if (!ThirdPass()) return false;

		return true;
	}
	void OldParser::Generate(const std::string& path) {
		sgn::Generator gen(m_Assembly.ByteFile);
		gen.Generate(path);
	}

#define MESSAGEBASE m_ErrorStream << "In file '" << m_ReadPath << "':\n    "
#define INFO MESSAGEBASE << "Info: Line " << m_LineNum << ", "
#define WARNING MESSAGEBASE << "Warning: Line " << m_LineNum << ", "
#define ERROR MESSAGEBASE << "Error: Line " << m_LineNum << ", "

	bool OldParser::SecondPass() {
		return true;

		/*std::size_t structInx = 0, fieldInx = 0;

		Structure* currentStructure = nullptr;

		bool hasError = false;

		while (std::getline(m_ReadStream, m_Line) && ++m_LineNum) {
			if (IgnoreComment()) continue;

			if (m_Line.find(':') != std::string::npos) {
				const std::string mnemonic = ReadMnemonic();
				if (mnemonic == "proc" || mnemonic == "func") {
					m_CurrentStructure.clear();
				} else if (mnemonic == "struct") {
					Structure& structure = m_Assembly.Structures[structInx++];
					m_CurrentStructure = structure.Name;
					fieldInx = 0;
					currentStructure = &structure;
				}
				continue;
			} else if (m_CurrentStructure.empty()) continue;

			hasError |= !ParseField(currentStructure);
		}

		return !hasError;*/
	}
	bool OldParser::ThirdPass() {
		return true;

		/*std::size_t structInx = 0, fieldInx = 0;
		std::size_t funcInx = 0, labelInx = 0;

		Function* currentFunction = nullptr;

		bool hasError = false;

		while (std::getline(m_ReadStream, m_Line) && ++m_LineNum) {
			if (IgnoreComment()) continue;

			if (m_Line.find(':') != std::string::npos) {
				const std::string mnemonic = ReadMnemonic();
				if (mnemonic == "proc" || mnemonic == "func") {
					Function& func = m_Assembly.Functions[funcInx++];
					m_CurrentStructure.clear();
					m_CurrentFunction = func.Name;
					labelInx = 0;
					currentFunction = &func;
				} else if (mnemonic == "struct") {
					m_CurrentStructure = m_Assembly.Structures[structInx++].Name;
					m_CurrentFunction.clear();
					fieldInx = 0;
				} else {
					Function& func = m_Assembly.GetFunction(m_CurrentFunction);
					Label& label = func.Labels[labelInx++];
					label.Index = func.Builder->AddLabel(label.Name);
				}

				continue;
			} else if (!m_CurrentStructure.empty()) continue;

			hasError |= !ParseInstruction(currentFunction);
		}

		return !hasError;*/
	}

	bool OldParser::IsValidIdentifier(const std::string& identifier) {
		if (std::isdigit(identifier.front())) return false;

		for (const char c : identifier) {
			if (std::isspace(c)) return false;
		}

		return true;
	}
	sgn::Type OldParser::GetType(const std::string& name) {
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
	bool OldParser::IsValidIntegerLiteral(const std::string& literal, std::string& literalMut, F&& function) {
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
	std::string OldParser::ReadOperand() {
		std::string result = ReadBeforeSpace(m_Line); Trim(result);
		if (!m_Line.empty()) {
			ERROR << "Unexcepted characters after operand.\n";
			return "";
		} else if (result.empty()) {
			ERROR << "Excepted operand after mnemonic.\n";
			return "";
		} else return result;
	}

	bool OldParser::ParseField(Structure* structure) {
		bool hasError = false;

		const auto type = ParseType(m_Line);
		if (!type) return false;
		else if (type->ElementCount && *type->ElementCount == 0) {
			ERROR << "Array in structure must have length.\n";
			hasError = true;
		}

		std::string name = ReadBeforeSpecialChar(m_Line); Trim(name);
		if (name.empty()) {
			ERROR << "Required field name.\n";
			return false;
		} else if (!IsValidIdentifier(name)) {
			ERROR << "Invalid field name '" << name << "'.\n";
			hasError = true;
		} else if (type->ElementType == nullptr) {
			ERROR << "Nonexistent type name '" << type->ElementTypeName << "'.\n";
			return false;
		}

		const sgn::StructureIndex structIndex = m_Assembly.GetStructure(m_CurrentStructure).Index;
		sgn::StructureInfo* const structureInfo = m_Assembly.ByteFile.GetStructureInfo(structIndex);
		const sgn::FieldIndex index = structureInfo->AddField(type->ElementType, type->ElementCount.value_or(0));
		structure->Fields.push_back(Field{ std::move(name), index });

		return !hasError;
	}
	bool OldParser::ParseFunction(bool isProcedure) {
		bool hasError = false;

		std::string name = ReadBeforeSpecialChar(m_Line); Trim(name);
		m_CurrentStructure.clear();
		m_CurrentFunction = name;

		if (name.empty()) {
			ERROR << "Required function or procedure name.\n";
			return false;
		} else if (!IsValidIdentifier(name)) {
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

		sgn::FunctionIndex index = sgn::FunctionIndex::OperandIndex/*Dummy*/;
		if (name != "entrypoint") {
			index = m_Assembly.ByteFile.AddFunction(static_cast<std::uint16_t>(params.size()), !isProcedure);
		}
		m_Assembly.Functions.push_back(Function{ nullptr, std::move(name), index, {}, std::move(params) });

		return !hasError;
	}
	bool OldParser::ParseLabel() {
		if (m_CurrentFunction.empty()) {
			ERROR << "Not belonged label.\n";
			return false;
		}

		Function& currentFunction = m_Assembly.GetFunction(m_CurrentFunction);
		bool hasError = false;

		std::string name = ReadBeforeSpecialChar(m_Line); Trim(name);
		if (name.empty()) {
			ERROR << "Required label name.\n";
			return false;
		} else if (!IsValidIdentifier(name)) {
			ERROR << "Invalid label name '" << name << "'.\n";
			hasError = true;
		} else if (currentFunction.HasLabel(name)) {
			ERROR << "Duplicated label name '" << name << "'.\n";
			hasError = true;
		}

		currentFunction.Labels.push_back(Label{ std::move(name) });

		return !hasError;
	}
	bool OldParser::ParseInstruction(Function* function) {
		const std::string mnemonic;// = ReadMnemonic();
		switch (CRC32(mnemonic)) {
		case "nop"_h: function->Builder->Nop(); break;

		case "push"_h: {
			std::string op = ReadOperand();
			if (op.empty()) return false;

			if (op.front() == '+' || op.front() == '-' || std::isdigit(op.front())) {
				const auto literal = ParseNumber(op);
				if (std::holds_alternative<bool>(literal)) return false;
				else if (std::holds_alternative<std::uint32_t>(literal)) {
					const auto inx = m_Assembly.ByteFile.AddIntConstant(std::get<std::uint32_t>(literal));
					function->Builder->Push(inx);
				} else if (std::holds_alternative<std::uint64_t>(literal)) {
					const auto inx = m_Assembly.ByteFile.AddLongConstant(std::get<std::uint64_t>(literal));
					function->Builder->Push(inx);
				} else if (std::holds_alternative<double>(literal)) {
					const auto inx = m_Assembly.ByteFile.AddDoubleConstant(std::get<double>(literal));
					function->Builder->Push(inx);
				}
			} else {
				const auto structure = m_Assembly.FindStructure(op);
				if (structure == m_Assembly.Structures.end()) {
					ERROR << "Nonexistent structure '" << op << "'.\n";
					return false;
				}
				function->Builder->Push(structure->Index);
			}

			break;
		}
		case "pop"_h: function->Builder->Pop(); break;
		case "load"_h: {
			const std::string op = ReadOperand();
			if (op.empty()) return false;

			const auto var = GetLocalVariable(function, op);
			if (!var.has_value()) {
				ERROR << "Nonexistent local variable '" << op << "'.\n";
				return false;
			}

			function->Builder->Load(*var);
			break;
		}
		case "store"_h: {
			const std::string op = ReadOperand();
			if (op.empty()) return false;

			const auto var = GetLocalVariable(function, op);
			if (var.has_value()) {
				function->Builder->Store(*var);
			} else {
				LocalVariable newVar;
				newVar.Name = op;
				newVar.Index = function->Builder->AddLocalVariable();
				function->LocalVariables.push_back(newVar);

				function->Builder->Store(newVar.Index);
			}

			break;
		}
		case "lea"_h: {
			const std::string op = ReadOperand();
			if (op.empty()) return false;

			const auto var = GetLocalVariable(function, op);
			if (!var.has_value()) {
				ERROR << "Nonexistent local variable '" << op << "'.\n";
				return false;
			}

			function->Builder->Lea(*var);
			break;
		}
		case "flea"_h: {
			const std::string op = ReadOperand();
			if (op.empty()) return false;

			const auto field = GetField(op);
			if (!field.has_value()) return false;

			function->Builder->FLea(*field);
			break;
		}
		case "tload"_h: function->Builder->TLoad(); break;
		case "tstore"_h: function->Builder->TStore(); break;
		case "copy"_h: function->Builder->Copy(); break;
		case "swap"_h: function->Builder->Swap(); break;

		case "add"_h: function->Builder->Add(); break;
		case "sub"_h: function->Builder->Sub(); break;
		case "mul"_h: function->Builder->Mul(); break;
		case "imul"_h: function->Builder->IMul(); break;
		case "div"_h: function->Builder->Div(); break;
		case "idiv"_h: function->Builder->IDiv(); break;
		case "mod"_h: function->Builder->Mod(); break;
		case "imod"_h: function->Builder->IMod(); break;
		case "neg"_h: function->Builder->Neg(); break;
		case "inc"_h: function->Builder->Inc(); break;
		case "dec"_h: function->Builder->Dec(); break;

		case "and"_h: function->Builder->And(); break;
		case "or"_h: function->Builder->Or(); break;
		case "xor"_h: function->Builder->Xor(); break;
		case "not"_h: function->Builder->Not(); break;
		case "shl"_h: function->Builder->Shl(); break;
		case "sal"_h: function->Builder->Sal(); break;
		case "shr"_h: function->Builder->Shr(); break;
		case "sar"_h: function->Builder->Sar(); break;

		case "cmp"_h: function->Builder->Cmp(); break;
		case "icmp"_h: function->Builder->ICmp(); break;
		case "jmp"_h: {
			const std::string op = ReadOperand();
			if (op.empty()) return false;

			const auto label = GetLabel(function, op);
			if (!label.has_value()) return false;

			function->Builder->Jmp(*label);
			break;
		}
		case "je"_h: {
			const std::string op = ReadOperand();
			if (op.empty()) return false;

			const auto label = GetLabel(function, op);
			if (!label.has_value()) return false;

			function->Builder->Je(*label);
			break;
		}
		case "jne"_h: {
			const std::string op = ReadOperand();
			if (op.empty()) return false;

			const auto label = GetLabel(function, op);
			if (!label.has_value()) return false;

			function->Builder->Jne(*label);
			break;
		}
		case "ja"_h: {
			const std::string op = ReadOperand();
			if (op.empty()) return false;

			const auto label = GetLabel(function, op);
			if (!label.has_value()) return false;

			function->Builder->Ja(*label);
			break;
		}
		case "jae"_h: {
			const std::string op = ReadOperand();
			if (op.empty()) return false;

			const auto label = GetLabel(function, op);
			if (!label.has_value()) return false;

			function->Builder->Jae(*label);
			break;
		}
		case "jb"_h: {
			const std::string op = ReadOperand();
			if (op.empty()) return false;

			const auto label = GetLabel(function, op);
			if (!label.has_value()) return false;

			function->Builder->Jb(*label);
			break;
		}
		case "jbe"_h: {
			const std::string op = ReadOperand();
			if (op.empty()) return false;

			const auto label = GetLabel(function, op);
			if (!label.has_value()) return false;

			function->Builder->Jbe(*label);
			break;
		}
		case "call"_h: {
			const std::string op = ReadOperand();
			if (op.empty()) return false;

			const auto func = GetFunction(op);
			if (!func.has_value()) return false;

			function->Builder->Call(*func);
			break;
		}
		case "ret"_h: function->Builder->Ret(); break;

		case "toi"_h: function->Builder->ToI(); break;
		case "tol"_h: function->Builder->ToL(); break;
		case "tod"_h: function->Builder->ToD(); break;
		case "top"_h: function->Builder->ToP(); break;

		case "null"_h: function->Builder->Null(); break;
		case "new"_h: {
			const auto type = ParseType(m_Line);
			if (!type) return false;
			else if (type->ElementType == nullptr) {
				ERROR << "Nonexistent type name '" << type->ElementTypeName << "'.\n";
				return false;
			} else if (type->ElementCount) {
				ERROR << "Array cannot be used here.\n";
				INFO << "Use 'anew' mnemonic instead.\n";
				return false;
			}

			function->Builder->New(m_Assembly.ByteFile.GetTypeIndex(type->ElementType));
			break;
		}
		case "delete"_h: function->Builder->Delete(); break;
		case "gcnull"_h: function->Builder->GCNull(); break;
		case "gcnew"_h: {
			const auto type = ParseType(m_Line);
			if (!type) return false;
			else if (type->ElementType == nullptr) {
				ERROR << "Nonexistent type name '" << type->ElementTypeName << "'.\n";
				return false;
			} else if (type->ElementCount) {
				ERROR << "Array cannot be used here.\n";
				INFO << "Use 'agcnew' mnemonic instead.\n";
				return false;
			}

			function->Builder->GCNew(m_Assembly.ByteFile.GetTypeIndex(type->ElementType));
			break;
		}

		case "apush"_h: {
			const auto type = ParseType(m_Line);
			if (!type) return false;
			else if (type->ElementType == nullptr) {
				ERROR << "Nonexistent type name '" << type->ElementTypeName << "'.\n";
				return false;
			} else if (!type->ElementCount) {
				ERROR << "Only array can be used here.\n";
				INFO << "Use 'push' mnemonic instead.\n";
				return false;
			} else if (*type->ElementCount != 0) {
				ERROR << "Array's length cannot be used here.\n";
				return false;
			}

			function->Builder->APush(m_Assembly.ByteFile.MakeArray(m_Assembly.ByteFile.GetTypeIndex(type->ElementType)));
			break;
		}
		case "anew"_h: {
			const auto type = ParseType(m_Line);
			if (!type) return false;
			else if (type->ElementType == nullptr) {
				ERROR << "Nonexistent type name '" << type->ElementTypeName << "'.\n";
				return false;
			} else if (!type->ElementCount) {
				ERROR << "Only array can be used here.\n";
				INFO << "Use 'new' mnemonic instead.\n";
				return false;
			} else if (*type->ElementCount != 0) {
				ERROR << "Array's length cannot be used here.\n";
				return false;
			}

			function->Builder->ANew(m_Assembly.ByteFile.MakeArray(m_Assembly.ByteFile.GetTypeIndex(type->ElementType)));
			break;
		}
		case "agcnew"_h: {
			const auto type = ParseType(m_Line);
			if (!type) return false;
			else if (type->ElementType == nullptr) {
				ERROR << "Nonexistent type name '" << type->ElementTypeName << "'.\n";
				return false;
			} else if (!type->ElementCount) {
				ERROR << "Only array can be used here.\n";
				INFO << "Use 'gcnew' mnemonic instead.\n";
				return false;
			} else if (*type->ElementCount != 0) {
				ERROR << "Array's length cannot be used here.\n";
				return false;
			}

			function->Builder->AGCNew(m_Assembly.ByteFile.MakeArray(m_Assembly.ByteFile.GetTypeIndex(type->ElementType)));
			break;
		}
		case "alea"_h: function->Builder->ALea(); break;
		case "count"_h: function->Builder->Count(); break;

		default: {
			ERROR << "Unrecognized mnemonic '" << mnemonic << "'.\n";
			return false;
		}
		}

		return true;
	}

	std::optional<Type> OldParser::ParseType(std::string& line) {
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
		if (std::holds_alternative<bool>(countVar)) {
			count = 0;
			hasError = std::get<bool>(countVar);
		} else if (std::holds_alternative<double>(countVar)) {
			ERROR << "Array's length must be integer.\n";
			count = static_cast<std::uint64_t>(std::get<double>(countVar));
			hasError = true;
		} else {
			std::visit([&count](auto v) mutable {
				count = static_cast<std::uint64_t>(v);
				}, countVar);
			if (count == 0) {
				ERROR << "Array's length cannot be zero.\n";
				hasError = true;
			}
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

		line.erase(line.begin());
		Trim(line);

		if (hasError) return std::nullopt;
		else return Type{ type, std::move(typeName), count };
	}
	OldParser::Number OldParser::ParseNumber(std::string& line) {
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
	OldParser::Number OldParser::ParseInteger(const std::string& literal, std::string& literalMut, bool isNegative, int base, F&& function) {
		if (literalMut.find('.') != std::string::npos) {
			ERROR << "Invalid integer literal '" << literal << "'.\n";
			return false;
		} else if (!IsValidIntegerLiteral(literal, literalMut, std::forward<F>(function))) return false;

		if (isNegative) {
			const long long abs = std::stoll(literalMut, nullptr, base);
			if (literalMut.back() == 'i') {
				if (abs > -static_cast<long long>(std::numeric_limits<std::int32_t>::min())) {
					WARNING << "Overflowed integer literal '" << literalMut << "'.\n";
				}
				return static_cast<std::uint32_t>(static_cast<std::int32_t>(-abs));
			} else {
				if (abs <= -static_cast<long long>(std::numeric_limits<std::int32_t>::min()) &&
					literalMut.back() != 'l') return static_cast<std::uint32_t>(static_cast<std::int32_t>(-abs));
				else return static_cast<std::uint64_t>(-abs);
			}
		} else {
			const unsigned long long abs = std::stoull(literalMut, nullptr, base);
			if (literalMut.back() == 'i') {
				if (abs > static_cast<unsigned long long>(std::numeric_limits<std::uint32_t>::max())) {
					WARNING << "Overflowed integer literal '" << literalMut << "'.\n";
				}
				return static_cast<std::uint32_t>(abs);
			} else {
				if (abs <= static_cast<unsigned long long>(std::numeric_limits<std::uint32_t>::max()) &&
					literalMut.back() != 'l') return static_cast<std::uint32_t>(abs);
				else return static_cast<std::uint64_t>(abs);
			}
		}
	}
	OldParser::Number OldParser::ParseBinInteger(const std::string& literal, std::string& literalMut, bool isNegative) {
		return ParseInteger(literal, literalMut, isNegative, 2, [](char c) {
			return !(c == '0' || c == '1');
			});
	}
	OldParser::Number OldParser::ParseOctInteger(const std::string& literal, std::string& literalMut, bool isNegative) {
		if (literalMut.find('.') != std::string::npos) return ParseDecimal(literal, literalMut, isNegative);
		else return ParseInteger(literal, literalMut, isNegative, 8, [](char c) {
			return !(c <= '0' && c <= '7');
			});
	}
	OldParser::Number OldParser::ParseDecInteger(const std::string& literal, std::string& literalMut, bool isNegative) {
		if (literalMut.find('.') != std::string::npos) return ParseDecimal(literal, literalMut, isNegative);
		else return ParseInteger(literal, literalMut, isNegative, 10, [](char c) {
			return !(std::isdigit(c));
			});
	}
	OldParser::Number OldParser::ParseHexInteger(const std::string& literal, std::string& literalMut, bool isNegative) {
		return ParseInteger(literal, literalMut, isNegative, 16, [](char c) {
			return !(std::isdigit(c) ||
				'a' <= c && c <= 'f' ||
				'A' <= c && c <= 'F');
			});
	}
	OldParser::Number OldParser::ParseDecimal(const std::string& literal, std::string& literalMut, bool isNegative) {
		const std::size_t dot = literalMut.find('.');
		if (literalMut.find('.', dot + 1) != std::string::npos) {
			ERROR << "Invalid decimal literal '" << literal << "'.\n";
			return true;
		}

		const double abs = std::stod(literalMut);
		if (isNegative) return -abs;
		else return abs;
	}

	std::optional<sgn::FieldIndex> OldParser::GetField(const std::string& name) {
		const std::size_t dot = name.find('.');
		if (dot == std::string::npos || name.find('.', dot + 1) != std::string::npos) {
			ERROR << "Invalid field name '" << name << "'.\n";
			return std::nullopt;
		}

		const std::string structName = name.substr(0, dot);
		const std::string fieldName = name.substr(dot + 1);

		const auto structIter = m_Assembly.FindStructure(structName);
		if (structIter == m_Assembly.Structures.end()) {
			ERROR << "Nonexistent structure name '" << structName << "'.\n";
			return std::nullopt;
		}

		const auto fieldIter = structIter->FindField(fieldName);
		if (fieldIter == structIter->Fields.end()) {
			ERROR << "Nonexistent field name '" << fieldName << "'.\n";
			return std::nullopt;
		}

		return fieldIter->Index;
	}
	std::optional<sgn::FunctionIndex> OldParser::GetFunction(const std::string& name) {
		const auto iter = m_Assembly.FindFunction(name);
		if (iter == m_Assembly.Functions.end()) {
			ERROR << "Nonexistent function or procedure name '" << name << "'.\n";
			return std::nullopt;
		} else if (name == "entrypoint") {
			ERROR << "Noncallable function or procedure 'entrypoint'.\n";
			return std::nullopt;
		} else return iter->Index;
	}
	std::optional<sgn::LabelIndex> OldParser::GetLabel(Function* function, const std::string& name) {
		const auto iter = function->FindLabel(name);
		if (iter == function->Labels.end()) {
			ERROR << "Nonexistent label name '" << name << "'.\n";
			return std::nullopt;
		} else return iter->Index;
	}
	std::optional<sgn::LocalVariableIndex> OldParser::GetLocalVariable(Function* function, const std::string& name) {
		const auto iter = function->FindLocalVariable(name);
		if (iter == function->LocalVariables.end()) return std::nullopt;
		else return iter->Index;
	}
}