#include <sgn/Builder.hpp>
#include <sgn/ByteFile.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using namespace sgn;

struct Identifiers {
	std::vector<std::string> Functions;
	std::unordered_map<std::string, std::vector<std::string>> Labels;
	std::unordered_map<std::string, std::vector<std::string>> LocalVariables;

	std::vector<std::string> Structures;
	std::unordered_map<std::string, std::vector<std::string>> Fields;

	std::string CurrentStructure;
	std::string CurrentFunction;
};

struct Objects {
	::ByteFile ByteFile;

	std::unordered_map<std::string, FunctionIndex> Functions;
	std::unordered_map<std::string, std::unordered_map<std::string, LabelIndex>> Labels;
	std::unordered_map<std::string, std::unordered_map<std::string, LocalVariableIndex>> LocalVariables;

	std::unordered_map<std::string, StructureIndex> Structures;
	std::unordered_map<std::string, std::unordered_map<std::string, FieldIndex>> Fields;

	std::unordered_map<std::string, Builder*> Builders;
};

bool IsSpecial(char c) noexcept;

void Trim(std::string& string);
template<typename F>
std::string ReadBefore(std::string& string, F function);
std::string ReadBeforeSpace(std::string& string);
std::string ReadBeforeChar(std::string& string, char c);
std::string ReadBeforeSpecialChar(std::string& string);
std::vector<std::string> Split(const std::string& string, char c);

bool FirstPass(std::ifstream& stream, Identifiers& identifiers, Objects& objects);
bool SecondPass(std::ifstream& stream, Identifiers& identifiers, Objects& objects);
bool ThirdPass(std::ifstream& stream, Identifiers& identifiers, Objects& objects);

bool ParseProcOrFunc(std::string& line, bool isProc, std::size_t lineNum, Identifiers& identifiers, Objects& objects);
bool ParseLabel(std::string& line, std::size_t lineNum, Identifiers& identifiers, Objects& objects);
bool ParseStruct(std::string& line, std::size_t lineNum, Identifiers& identifers, Objects& objects);

std::string ReadOperand(std::string& line, std::size_t lineNum);
std::variant<std::monostate, std::uint32_t, std::uint64_t, double> ParseNumber(std::size_t lineNum, const std::string& op);
template<typename F>
std::variant<std::monostate, std::uint32_t, std::uint64_t, double> ParseWithoutDec(std::size_t lineNum, const std::string& op, std::string& opMut, F function, int base);
std::variant<std::monostate, std::uint32_t, std::uint64_t, double> ParseBin(std::size_t lineNum, const std::string& op, std::string& opMut);
std::variant<std::monostate, std::uint32_t, std::uint64_t, double> ParseOct(std::size_t lineNum, const std::string& op, std::string& opMut);
std::variant<std::monostate, std::uint32_t, std::uint64_t, double> ParseHex(std::size_t lineNum, const std::string& op, std::string& opMut);

std::optional<FieldIndex> GetField(std::size_t lineNum, Identifiers& identifiers, Objects& objects, const std::string& operand);
std::optional<FunctionIndex> GetFunction(std::size_t lineNum, Identifiers& identifiers, Objects& objects, const std::string& operand);
std::optional<LabelIndex> GetLabel(std::size_t lineNum, Identifiers& identifiers, Objects& objects, const std::string& operand);
std::optional<LocalVariableIndex> GetLocalVariable(Identifiers& identifiers, Objects& objects, const std::string& operand);

constexpr std::uint32_t CRC32Table[] = {
	0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
	0xE963A535, 0x9E6495A3,	0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
	0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
	0xF3B97148, 0x84BE41DE,	0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
	0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,	0x14015C4F, 0x63066CD9,
	0xFA0F3D63, 0x8D080DF5,	0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
	0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,	0x35B5A8FA, 0x42B2986C,
	0xDBBBC9D6, 0xACBCF940,	0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
	0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
	0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
	0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,	0x76DC4190, 0x01DB7106,
	0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
	0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
	0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
	0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
	0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
	0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
	0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
	0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
	0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
	0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
	0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
	0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
	0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
	0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
	0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
	0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
	0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
	0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
	0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
	0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
	0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
	0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
	0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
	0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
	0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
	0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
	0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
	0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
	0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
	0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
	0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
	0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};
constexpr std::uint32_t CRC32Internal(const char* string, std::size_t index) noexcept {
	return index == static_cast<std::size_t>(-1) ? 0xFFFFFFFF :
		((CRC32Internal(string, index - 1) >> 8) ^ CRC32Table[(CRC32Internal(string, index - 1) ^ string[index]) & 0xFF]);
}
constexpr std::uint32_t CRC32(const char* string, std::size_t length) noexcept {
	return CRC32Internal(string, length) ^ 0xFFFFFFFF;
}
constexpr std::uint32_t operator""_h(const char* string, std::size_t length) noexcept {
	return CRC32(string, length);
}

int main(int argc, char* argv[]) {
	if (argc == 1) {
		std::cout << "Usage: ./ShitAsm <File> [Output]\n";
		return EXIT_FAILURE;
	}

	std::ifstream inputFile(argv[1]);
	if (!inputFile) {
		std::cout << "Error: Failed to open the file.\n";
		return EXIT_FAILURE;
	}

	Identifiers identifiers;
	Objects objects;

	if (!FirstPass(inputFile, identifiers, objects)) return EXIT_FAILURE;

	inputFile.clear();
	inputFile.seekg(0, std::ifstream::beg);

	if (!SecondPass(inputFile, identifiers, objects)) return EXIT_FAILURE;

	inputFile.clear();
	inputFile.seekg(0, std::ifstream::beg);

	if (!ThirdPass(inputFile, identifiers, objects)) return EXIT_FAILURE;

	if (argc == 2) {
		objects.ByteFile.Save("./ShitAsmOutput.sbf");
	} else {
		objects.ByteFile.Save(argv[2]);
	}

	return EXIT_SUCCESS;
}

bool IsSpecial(char c) noexcept {
	switch (c) {
	case '~':
	case '`':
	case '!':
	case '@':
	case '#':
	case '$':
	case '%':
	case '^':
	case '&':
	case '*':
	case '(':
	case ')':
	case '-':
	case '+':
	case '=':
	case '|':
	case '\\':
	case '{':
	case '[':
	case '}':
	case ']':
	case ':':
	case ';':
	case '"':
	case '\'':
	case '<':
	case ',':
	case '>':
	case '.':
	case '?':
	case '/':
		return true;

	default:
		return std::isspace(c);
	}
}

void Trim(std::string& string) {
	std::size_t beginOffset = 0;
	std::size_t rbeginOffset = 0;

	while (string.size() > rbeginOffset&& std::isspace(string[string.size() - rbeginOffset - 1])) ++rbeginOffset;
	while (string.size() > beginOffset + rbeginOffset && std::isspace(string[beginOffset])) ++beginOffset;

	if (beginOffset) {
		string.erase(string.begin(), string.begin() + beginOffset);
	}
	if (rbeginOffset) {
		string.erase(string.end() - rbeginOffset, string.end());
	}
}
template<typename F>
std::string ReadBefore(std::string& string, F function) {
	std::size_t length = 0;
	while (string.size() > length && !function(string[length])) ++length;

	const std::string result = string.substr(0, length);
	string.erase(string.begin(), string.begin() + length);
	Trim(string);

	return result;
}
std::string ReadBeforeSpace(std::string& string) {
	return ReadBefore(string, [](char c) -> bool { return std::isspace(c); });
}
std::string ReadBeforeChar(std::string& string, char c) {
	return ReadBefore(string, [c](char ch) { return ch == c; });
}
std::string ReadBeforeSpecialChar(std::string& string) {
	return ReadBefore(string, IsSpecial);
}
std::vector<std::string> Split(const std::string& string, char c) {
	std::vector<std::string> result;
	std::istringstream iss(string);

	std::string value;
	while (std::getline(iss, value, c)) {
		Trim(value);
		result.push_back(std::move(value));
	}

	return result;
}

bool FirstPass(std::ifstream& stream, Identifiers& identifiers, Objects& objects) {
	std::string line;
	std::size_t lineNum = 0;
	bool hasError = false;

	while (std::getline(stream, line) && ++lineNum) {
		if (const auto commentBegin = line.find(';'); commentBegin != std::string::npos) {
			line.erase(line.begin() + commentBegin, line.end());
		}
		Trim(line);
		if (line.empty()) continue;

		if (line.find(':') != std::string::npos) {
			std::string lineCopied = line;
			std::string mnemonic = ReadBeforeSpace(lineCopied);
			std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });

			if (mnemonic == "proc" || mnemonic == "func") {
				line = lineCopied;
				if (!ParseProcOrFunc(line, lineNum, mnemonic == "proc", identifiers, objects)) {
					hasError = true;
				}
			} else if (mnemonic == "struct") {
				line = lineCopied;
				if (!ParseStruct(line, lineNum, identifiers, objects)) {
					hasError = true;
				}
			} else {
				if (!ParseLabel(line, lineNum, identifiers, objects)) {
					hasError = true;
				}
			}
		}
	}

	return !hasError;
}
bool SecondPass(std::ifstream& stream, Identifiers& identifiers, Objects& objects) {
	std::string line;
	std::size_t lineNum = 0;
	std::size_t structInx = 0, fieldInx = 0;
	bool hasError = false;

	identifiers.CurrentStructure.clear();
	identifiers.CurrentFunction.clear();

	while (std::getline(stream, line) && ++lineNum) {
		if (const auto commentBegin = line.find(';'); commentBegin != std::string::npos) {
			line.erase(line.begin() + commentBegin, line.end());
		}
		Trim(line);
		if (line.empty()) continue;

		if (line.find(':') != std::string::npos) {
			std::string lineCopied = line;
			std::string mnemonic = ReadBeforeSpace(lineCopied);
			std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });

			if (mnemonic == "proc" || mnemonic == "func") {
				identifiers.CurrentStructure.clear();
			} else if (mnemonic == "struct") {
				identifiers.CurrentStructure = identifiers.Structures[structInx++];
				fieldInx = 0;
			}

			continue;
		} else if (identifiers.CurrentStructure.empty()) continue;

		Structure* structure = objects.ByteFile.GetStructure(objects.Structures[identifiers.CurrentStructure]);

		static const std::unordered_map<std::uint32_t, const Type*> fundamental = {
			{ "int"_h, IntType },
			{ "long"_h, LongType },
			{ "double"_h, DoubleType },
			{ "pointer"_h, PointerType },
		};

		const std::string mnemonic = ReadBeforeSpace(line);
		const std::uint32_t mnemonicHash = CRC32(mnemonic.c_str(), mnemonic.size());
		if (const auto iter = fundamental.find(mnemonicHash); iter != fundamental.end()) {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			} else if (objects.Fields.find(op) != objects.Fields.end()) {
				std::cout << "Error: Line " << lineNum << ", Duplicated field name '" << op << "'.\n";
				hasError = true;
				break;
			} else if (std::isdigit(op.front())) {
				std::cout << "Error: Line " << lineNum << ", Invalid field name '" << op << "'.\n";
				hasError = true;
				break;
			}
			for (const char c : op) {
				if (IsSpecial(c)) {
					std::cout << "Error: Line " << lineNum << ", Invalid field name '" << op << "'.\n";
					hasError = true;
					goto out1;
				}
			}

			identifiers.Fields[identifiers.CurrentStructure].push_back(op);
			objects.Fields[identifiers.CurrentStructure][op] = structure->AddField(iter->second);

		out1:
			continue;
		}

		if (const auto iter = std::find(identifiers.Structures.begin(), identifiers.Structures.end(), mnemonic);
			iter != identifiers.Structures.end()) {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			} else if (objects.Fields.find(op) != objects.Fields.end()) {
				std::cout << "Error: Line " << lineNum << ", Duplicated field name '" << op << "'.\n";
				hasError = true;
				break;
			} else if (std::isdigit(op.front())) {
				std::cout << "Error: Line " << lineNum << ", Invalid field name '" << op << "'.\n";
				hasError = true;
				break;
			}
			for (const char c : op) {
				if (IsSpecial(c)) {
					std::cout << "Error: Line " << lineNum << ", Invalid field name '" << op << "'.\n";
					hasError = true;
					goto out2;
				}
			}

			identifiers.Fields[identifiers.CurrentStructure].push_back(op);
			objects.Fields[identifiers.CurrentStructure][op] = structure->AddField(objects.ByteFile.GetStructure(objects.Structures[*iter])->GetType());

		out2:
			continue;
		} else {
			std::cout << "Error: Line " << lineNum << ", Nonexistent type name '" << mnemonic << "'.\n";
			hasError = true;
		}
	}

	return !hasError;
}
bool ThirdPass(std::ifstream& stream, Identifiers& identifiers, Objects& objects) {
	std::string line;
	std::size_t lineNum = 0;
	std::size_t funcInx = 0, labelInx = 0;
	std::size_t structInx = 0, fieldInx = 0;
	bool hasError = false;

	identifiers.CurrentStructure.clear();

	while (std::getline(stream, line) && ++lineNum) {
		if (const auto commentBegin = line.find(';'); commentBegin != std::string::npos) {
			line.erase(line.begin() + commentBegin, line.end());
		}
		Trim(line);
		if (line.empty()) continue;

		if (line.find(':') != std::string::npos) {
			std::string lineCopied = line;
			std::string mnemonic = ReadBeforeSpace(lineCopied);
			std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });

			if (mnemonic == "proc" || mnemonic == "func") {
				identifiers.CurrentStructure.clear();
				identifiers.CurrentFunction = identifiers.Functions[funcInx++];
				labelInx = 0;
			} else if (mnemonic == "struct") {
				identifiers.CurrentStructure = identifiers.Structures[structInx++];
				identifiers.CurrentFunction.clear();
				fieldInx = 0;
			} else {
				const std::string name = identifiers.Labels[identifiers.CurrentFunction][labelInx++];
				objects.Builders[identifiers.CurrentFunction]->AddLabel(name);
			}

			continue;
		} else if (!identifiers.CurrentStructure.empty()) continue;

		std::string mnemonic = ReadBeforeSpace(line);
		std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });

		switch (CRC32(mnemonic.c_str(), mnemonic.size())) {
		case "nop"_h: objects.Builders[identifiers.CurrentFunction]->Nop(); break;

		case "push"_h: {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			}

			if (op.front() == '+' || op.front() == '-' || std::isdigit(op.front())) {
				const auto val = ParseNumber(lineNum, op);
				if (std::holds_alternative<std::monostate>(val)) {
					hasError = true;
				} else if (std::holds_alternative<std::uint32_t>(val)) {
					const auto inx = objects.ByteFile.AddIntConstant(std::get<std::uint32_t>(val));
					objects.Builders[identifiers.CurrentFunction]->Push(inx);
				} else if (std::holds_alternative<std::uint64_t>(val)) {
					const auto inx = objects.ByteFile.AddLongConstant(std::get<std::uint64_t>(val));
					objects.Builders[identifiers.CurrentFunction]->Push(inx);
				} else if (std::holds_alternative<double>(val)) {
					const auto inx = objects.ByteFile.AddDoubleConstant(std::get<double>(val));
					objects.Builders[identifiers.CurrentFunction]->Push(inx);
				}
			} else {
				const auto structure = objects.Structures.find(op);
				if (structure == objects.Structures.end()) {
					std::cout << "Error: Line " << lineNum << ", Nonexistent structure name '" << op << "'.\n";
					hasError = true;
				} else {
					objects.Builders[identifiers.CurrentFunction]->Push(structure->second);
				}
			}

			break;
		}
		case "pop"_h: objects.Builders[identifiers.CurrentFunction]->Pop(); break;
		case "load"_h: {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			}
			const auto var = GetLocalVariable(identifiers, objects, op);
			if (!var.has_value()) {
				std::cout << "Error: Line " << lineNum << ", Nonexistent local variable '" << op << "'.\n";
				hasError = true;
				break;
			}

			objects.Builders[identifiers.CurrentFunction]->Load(var.value());
			break;
		}
		case "store"_h: {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) break;
			const auto var = GetLocalVariable(identifiers, objects, op);
			if (var.has_value()) {
				objects.Builders[identifiers.CurrentFunction]->Store(var.value());
			} else {
				const LocalVariableIndex inx = objects.Builders[identifiers.CurrentFunction]->AddLocalVariable();
				identifiers.LocalVariables[identifiers.CurrentFunction].push_back(op);
				objects.LocalVariables[identifiers.CurrentFunction][op] = inx;
				objects.Builders[identifiers.CurrentFunction]->Store(inx);
			}

			break;
		}
		case "lea"_h: {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			}
			const auto var = GetLocalVariable(identifiers, objects, op);
			if (!var.has_value()) {
				std::cout << "Error: Line " << lineNum << ", Nonexistent local variable '" << op << "'.\n";
				hasError = true;
				break;
			}

			objects.Builders[identifiers.CurrentFunction]->Lea(var.value());
			break;
		}
		case "flea"_h: {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			}
			const auto field = GetField(lineNum, identifiers, objects, op);
			if (!field.has_value()) {
				hasError = true;
				break;
			}

			objects.Builders[identifiers.CurrentFunction]->FLea(field.value());
			break;
		}
		case "tload"_h: objects.Builders[identifiers.CurrentFunction]->TLoad(); break;
		case "tstore"_h: objects.Builders[identifiers.CurrentFunction]->TStore(); break;
		case "copy"_h: objects.Builders[identifiers.CurrentFunction]->Copy(); break;
		case "swap"_h: objects.Builders[identifiers.CurrentFunction]->Swap(); break;

		case "add"_h: objects.Builders[identifiers.CurrentFunction]->Add(); break;
		case "sub"_h: objects.Builders[identifiers.CurrentFunction]->Sub(); break;
		case "mul"_h: objects.Builders[identifiers.CurrentFunction]->Mul(); break;
		case "imul"_h: objects.Builders[identifiers.CurrentFunction]->IMul(); break;
		case "div"_h: objects.Builders[identifiers.CurrentFunction]->Div(); break;
		case "idiv"_h: objects.Builders[identifiers.CurrentFunction]->IDiv(); break;
		case "mod"_h: objects.Builders[identifiers.CurrentFunction]->Mod(); break;
		case "imod"_h: objects.Builders[identifiers.CurrentFunction]->IMod(); break;
		case "neg"_h: objects.Builders[identifiers.CurrentFunction]->Neg(); break;
		case "inc"_h: objects.Builders[identifiers.CurrentFunction]->Inc(); break;
		case "dec"_h: objects.Builders[identifiers.CurrentFunction]->Dec(); break;

		case "and"_h: objects.Builders[identifiers.CurrentFunction]->And(); break;
		case "or"_h: objects.Builders[identifiers.CurrentFunction]->Or(); break;
		case "xor"_h: objects.Builders[identifiers.CurrentFunction]->Xor(); break;
		case "not"_h: objects.Builders[identifiers.CurrentFunction]->Not(); break;
		case "shl"_h: objects.Builders[identifiers.CurrentFunction]->Shl(); break;
		case "sal"_h: objects.Builders[identifiers.CurrentFunction]->Sal(); break;
		case "shr"_h: objects.Builders[identifiers.CurrentFunction]->Shr(); break;
		case "sar"_h: objects.Builders[identifiers.CurrentFunction]->Sar(); break;

		case "cmp"_h: objects.Builders[identifiers.CurrentFunction]->Cmp(); break;
		case "icmp"_h: objects.Builders[identifiers.CurrentFunction]->ICmp(); break;
		case "jmp"_h: {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			}
			const auto label = GetLabel(lineNum, identifiers, objects, op);
			if (!label.has_value()) {
				hasError = true;
				break;
			}

			objects.Builders[identifiers.CurrentFunction]->Jmp(label.value());
			break;
		}
		case "je"_h: {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			}
			const auto label = GetLabel(lineNum, identifiers, objects, op);
			if (!label.has_value()) {
				hasError = true;
				break;
			}

			objects.Builders[identifiers.CurrentFunction]->Je(label.value());
			break;
		}
		case "jne"_h: {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			}
			const auto label = GetLabel(lineNum, identifiers, objects, op);
			if (!label.has_value()) {
				hasError = true;
				break;
			}

			objects.Builders[identifiers.CurrentFunction]->Jne(label.value());
			break;
		}
		case "ja"_h: {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			}
			const auto label = GetLabel(lineNum, identifiers, objects, op);
			if (!label.has_value()) {
				hasError = true;
				break;
			}

			objects.Builders[identifiers.CurrentFunction]->Ja(label.value());
			break;
		}
		case "jae"_h: {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			}
			const auto label = GetLabel(lineNum, identifiers, objects, op);
			if (!label.has_value()) {
				hasError = true;
				break;
			}

			objects.Builders[identifiers.CurrentFunction]->Jae(label.value());
			break;
		}
		case "jb"_h: {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			}
			const auto label = GetLabel(lineNum, identifiers, objects, op);
			if (!label.has_value()) {
				hasError = true;
				break;
			}

			objects.Builders[identifiers.CurrentFunction]->Jb(label.value());
			break;
		}
		case "jbe"_h: {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			}
			const auto label = GetLabel(lineNum, identifiers, objects, op);
			if (!label.has_value()) {
				hasError = true;
				break;
			}

			objects.Builders[identifiers.CurrentFunction]->Jbe(label.value());
			break;
		}
		case "call"_h: {
			const std::string op = ReadOperand(line, lineNum);
			if (op.empty()) {
				hasError = true;
				break;
			}
			const auto func = GetFunction(lineNum, identifiers, objects, op);
			if (!func.has_value()) {
				hasError = true;
				break;
			}

			objects.Builders[identifiers.CurrentFunction]->Call(func.value());
			break;
		}
		case "ret"_h: objects.Builders[identifiers.CurrentFunction]->Ret(); break;

		case "toi"_h: objects.Builders[identifiers.CurrentFunction]->ToI(); break;
		case "tol"_h: objects.Builders[identifiers.CurrentFunction]->ToL(); break;
		case "tod"_h: objects.Builders[identifiers.CurrentFunction]->ToD(); break;
		case "top"_h: objects.Builders[identifiers.CurrentFunction]->ToP(); break;

		default: {
			std::cout << "Error: Line " << lineNum << ", Unrecognized mnemonic '" << mnemonic << "'.\n";
			hasError = true;
			break;
		}
		}
	}

	return !hasError;
}

bool ParseProcOrFunc(std::string& line, bool isProc, std::size_t lineNum, Identifiers& identifiers, Objects& objects) {
	bool hasError = false;

	std::string name = ReadBeforeSpecialChar(line);
	Trim(name);

	if (std::isdigit(name.front())) {
		std::cout << "Error: Line " << lineNum << ", Invalid procedure or function name '" << name << "'.\n";
		hasError = true;
	}
	for (const char c : name) {
		if (std::isspace(c)) {
			std::cout << "Error: Line " << lineNum << ", Invalid procedure or function name '" << name << "'.\n";
			hasError = true;
			break;
		}
	}

	const auto funcIter = std::find(identifiers.Functions.begin(), identifiers.Functions.end(), name);
	if (funcIter != identifiers.Functions.end()) {
		std::cout << "Error: Line " << lineNum << ", Duplicated procedure or function name '" << name << "'.\n";
		hasError = true;
	}
	identifiers.CurrentStructure.clear();
	identifiers.Functions.push_back(name);
	identifiers.CurrentFunction = name;

	if (name == "entrypoint" && !isProc) {
		std::cout << "Error: Line " << lineNum << ", Invalid function name 'entrypoint'. It can be used only for procedure.";
		hasError = true;
	}

	std::string rawParams = ReadBeforeChar(line, ':');
	Trim(rawParams);
	if (!rawParams.empty()) {
		if (rawParams.front() != '(') {
			std::cout << "Error: Line " << lineNum << ", Excepted '(' before procedure or function name.\n";
			hasError = true;
		} else if (rawParams.back() != ')') {
			std::cout << "Error: Line " << lineNum << ", Excepted ')' after procedure or function name.\n";
			hasError = true;
		} else {
			rawParams.erase(rawParams.begin());
			rawParams.erase(rawParams.end() - 1);

			std::vector<std::string> params = Split(rawParams, ',');
			std::vector<std::string> paramsSorted = params;
			std::sort(paramsSorted.begin(), paramsSorted.end());
			if (const auto dp = std::unique(paramsSorted.begin(), paramsSorted.end()); dp != paramsSorted.end()) {
				std::cout << "Error: Line " << lineNum << ", Duplicated parameter name '" << *dp << "'.\n";
				hasError = true;
			}

			identifiers.LocalVariables[name] = std::move(params);
		}
	}

	if (line.front() != ':') {
		std::cout << "Error: Line " << lineNum << ", Excepted ':' after parameters.\n";
		hasError = true;
	} else if (line.size() != 1) {
		std::cout << "Error: Line " << lineNum << ", Unexcepted characters after ':'.\n";
		hasError = true;
	}

	if (name == "entrypoint") {
		objects.Builders[name] = new Builder(objects.ByteFile, objects.ByteFile.GetEntryPoint());
	} else {
		const auto func = objects.ByteFile.AddFunction(static_cast<std::uint16_t>(identifiers.LocalVariables[name].size()), !isProc);
		objects.Functions[name] = func;
		objects.Builders[name] = new Builder(objects.ByteFile, func);
	}

	return !hasError;
}
bool ParseLabel(std::string& line, std::size_t lineNum, Identifiers& identifiers, Objects& objects) {
	if (identifiers.CurrentFunction.empty()) {
		std::cout << "Error: Line " << lineNum << ", Not belonged label.\n";
		return false;
	}

	bool hasError = false;

	std::string name = ReadBeforeSpecialChar(line);
	Trim(name);

	if (std::isdigit(name.front())) {
		std::cout << "Error: Line " << lineNum << ", Invalid label name '" << name << "'.\n";
		hasError = true;
	}
	for (const char c : name) {
		if (std::isspace(c)) {
			std::cout << "Error: Line " << lineNum << ", Invalid procedure or function name '" << name << "'.\n";
			hasError = true;
			break;
		}
	}

	const auto labelIter = std::find(identifiers.Labels[identifiers.CurrentFunction].begin(), identifiers.Labels[identifiers.CurrentFunction].end(), name);
	if (labelIter != identifiers.Labels[identifiers.CurrentFunction].end()) {
		std::cout << "Error: Line " << lineNum << ", Duplicated label name '" << name << "'.\n";
		hasError = true;
	}

	identifiers.Labels[identifiers.CurrentFunction].push_back(name);

	const auto label = objects.Builders[identifiers.CurrentFunction]->ReserveLabel(name);
	objects.Labels[identifiers.CurrentFunction][name] = label;

	if (line.front() != ':') {
		std::cout << "Error: Line " << lineNum << ", Excepted ':' after label name.\n";
		hasError = true;
	} else if (line.size() != 1) {
		std::cout << "Error: Line " << lineNum << ", Unexcepted characters after ':'.\n";
		hasError = true;
	}

	return !hasError;
}
bool ParseStruct(std::string& line, std::size_t lineNum, Identifiers& identifiers, Objects& objects) {
	bool hasError = false;

	std::string name = ReadBeforeSpecialChar(line);
	Trim(name);

	if (std::isdigit(name.front())) {
		std::cout << "Error: Line " << lineNum << ", Invalid structure name '" << name << "'.\n";
		hasError = true;
	}
	for (const char c : name) {
		if (std::isspace(c)) {
			std::cout << "Error: Line " << lineNum << ", Invalid procedure or function name '" << name << "'.\n";
			hasError = true;
			break;
		}
	}

	const auto structIter = std::find(identifiers.Structures.begin(), identifiers.Structures.end(), name);
	if (structIter != identifiers.Structures.end()) {
		std::cout << "Error: Line " << lineNum << ", Duplicated structure name '" << name << "'.\n";
		hasError = true;
	}
	identifiers.Structures.push_back(name);
	identifiers.CurrentStructure = name;
	identifiers.CurrentFunction.clear();

	if (line.front() != ':') {
		std::cout << "Error: Line " << lineNum << ", Excepted ':' after structure name.\n";
		hasError = true;
	} else if (line.size() != 1) {
		std::cout << "Error: Line " << lineNum << ", Unexcepted characters after ':'.\n";
		hasError = true;
	}

	objects.Structures[name] = objects.ByteFile.AddStructure();
	return !hasError;
}

std::string ReadOperand(std::string& line, std::size_t lineNum) {
	const std::string result = ReadBeforeSpace(line);
	if (!line.empty()) {
		std::cout << "Error: Line " << lineNum << ", Unexcepted characters after operand.\n";
		return "";
	}
	return result;
}
std::variant<std::monostate, std::uint32_t, std::uint64_t, double> ParseNumber(std::size_t lineNum, const std::string& op) {
	std::string opMut = op;

	if (opMut.front() == '0') {
		if (opMut.size() > 1) {
			switch (opMut[1]) {
			case 'x':
			case 'X':
				opMut.erase(opMut.begin(), opMut.begin() + 2);
				return ParseHex(lineNum, op, opMut);

			case 'b':
			case 'B':
				opMut.erase(opMut.begin(), opMut.begin() + 2);
				return ParseBin(lineNum, op, opMut);

			default:
				opMut.erase(opMut.begin());
				return ParseOct(lineNum, op, opMut);
			}
		} else return 0u;
	}

	for (std::size_t i = 0; i < opMut.size(); ++i) {
		if (!std::isdigit(opMut[i])) {
			if (i == 0 && (opMut[i] == '+' || opMut[i] == '-')) continue;
			else if (i != 0 && i != opMut.size() - 1 && opMut[i] == ',') {
				opMut.erase(opMut.begin() + i);
				--i;
			} else if (i != 0 && opMut[i] == '.') continue;
			else if (i == opMut.size() - 1 && (opMut[i] == 'i' || opMut[i] == 'l')) continue;

			std::cout << "Error: Line " << lineNum << ", Invalid number literal '" << op << "'.\n";
			return std::monostate();
		}
	}

	if (const std::size_t dot = opMut.find('.'); dot != std::string::npos) {
		if (opMut.find('.', dot + 1) != std::string::npos) {
			std::cout << "Error: Line " << lineNum << ", Invalid number literal '" << op << "'.\n";
			return std::monostate();
		}

		const double value = std::stod(opMut);
		if (!std::isdigit(opMut.back())) {
			std::cout << "Error: Line " << lineNum << ", Invalid number literal '" << op << "'.\n";
			return std::monostate();
		}

		return value;
	}

	if (opMut.front() == '-') {
		long long value = std::stoll(opMut);
		if (opMut.back() == 'i') {
			if (value > std::numeric_limits<std::int32_t>::max() ||
				value < std::numeric_limits<std::int32_t>::min()) {
				std::cout << "Warning: Line " << lineNum << ", Overflowed int literal '" << op << "'.\n";
			}
			return static_cast<std::uint32_t>(value);
		} else {
			if (value <= std::numeric_limits<std::int32_t>::max() &&
				value >= std::numeric_limits<std::int32_t>::min()) {
				return static_cast<std::uint32_t>(value);
			} else return static_cast<std::uint64_t>(value);
		}
	} else {
		long long value = std::stoull(opMut);
		if (opMut.back() == 'i') {
			if (value > std::numeric_limits<std::uint32_t>::max()) {
				std::cout << "Warning: Line " << lineNum << ", Overflowed int literal '" << op << "'.\n";
			}
			return static_cast<std::uint32_t>(value);
		} else {
			if (value <= std::numeric_limits<std::uint32_t>::max()) {
				return static_cast<std::uint32_t>(value);
			} else return static_cast<std::uint64_t>(value);
		}
	}
}
template<typename F>
std::variant<std::monostate, std::uint32_t, std::uint64_t, double> ParseWithoutDec(std::size_t lineNum, const std::string& op, std::string& opMut, F function, int base) {
	if (opMut.find('.') != std::string::npos) {
		std::cout << "Error: Line " << lineNum << ", Invalid number literal '" << op << "'.\n";
		return std::monostate();
	}

	for (std::size_t i = 0; i < opMut.size(); ++i) {
		if (!std::isdigit(opMut[i])) {
			if (i == 0 && (opMut[i] == '+' || opMut[i] == '-')) continue;
			else if (i != 0 && i != opMut.size() - 1 && opMut[i] == ',') {
				opMut.erase(opMut.begin() + i);
				--i;
			} else if (i != 0 && opMut[i] == '.') continue;
			else if (i == opMut.size() - 1 && (opMut[i] == 'i' || opMut[i] == 'l')) continue;
			else if (function(opMut[i])) {
				std::cout << "Error: Line " << lineNum << ", Invalid number literal '" << op << "'.\n";
				return std::monostate();
			}
		}
	}

	if (opMut.front() == '-') {
		long long value = std::stoll(opMut, nullptr, base);
		if (opMut.back() == 'i') {
			if (value > std::numeric_limits<std::int32_t>::max() ||
				value < std::numeric_limits<std::int32_t>::min()) {
				std::cout << "Warning: Line " << lineNum << ", Overflowed int literal '" << op << "'.\n";
			}
			return static_cast<std::uint32_t>(value);
		} else {
			if (value <= std::numeric_limits<std::int32_t>::max() &&
				value >= std::numeric_limits<std::int32_t>::min()) {
				return static_cast<std::uint32_t>(value);
			} else return static_cast<std::uint64_t>(value);
		}
	} else {
		long long value = std::stoull(opMut, nullptr, base);
		if (opMut.back() == 'i') {
			if (value > std::numeric_limits<std::uint32_t>::max()) {
				std::cout << "Warning: Line " << lineNum << ", Overflowed int literal '" << op << "'.\n";
			}
			return static_cast<std::uint32_t>(value);
		} else {
			if (value <= std::numeric_limits<std::uint32_t>::max()) {
				return static_cast<std::uint32_t>(value);
			} else return static_cast<std::uint64_t>(value);
		}
	}
}
std::variant<std::monostate, std::uint32_t, std::uint64_t, double> ParseBin(std::size_t lineNum, const std::string& op, std::string& opMut) {
	return ParseWithoutDec(lineNum, op, opMut, [](char c) {
		return c != '0' && c != '1' && c != ',';
		}, 2);
}
std::variant<std::monostate, std::uint32_t, std::uint64_t, double> ParseOct(std::size_t lineNum, const std::string& op, std::string& opMut) {
	if (const std::size_t dot = opMut.find('.'); dot != std::string::npos) {
		if (opMut.find('.', dot + 1) != std::string::npos) {
			std::cout << "Error: Line " << lineNum << ", Invalid number literal '" << op << "'.\n";
			return std::monostate();
		}

		const double value = std::stod(opMut);
		if (!std::isdigit(opMut.back())) {
			std::cout << "Error: Line " << lineNum << ", Invalid number literal '" << op << "'.\n";
			return std::monostate();
		}

		return value;
	}

	return ParseWithoutDec(lineNum, op, opMut, [](char c) {
		return '0' > c&& c > '7' && c != ',';
		}, 8);
}
std::variant<std::monostate, std::uint32_t, std::uint64_t, double> ParseHex(std::size_t lineNum, const std::string& op, std::string& opMut) {
	return ParseWithoutDec(lineNum, op, opMut, [](char c) {
		return !std::isdigit(c) && c != ',';
		}, 16);
}

std::optional<FieldIndex> GetField(std::size_t lineNum, Identifiers& identifiers, Objects& objects, const std::string& operand) {
	const std::size_t dotOffset = operand.find('.');

	if (dotOffset != std::string::npos) {
		std::cout << "Error: Line " << lineNum << ", Invalid field name '" << operand << "'.\n";
		return std::nullopt;
	}

	const std::string structName = operand.substr(0, dotOffset);
	const auto structIter = std::find(identifiers.Structures.begin(), identifiers.Structures.end(), structName);
	if (structIter == identifiers.Structures.end()) {
		std::cout << "Error: Line " << lineNum << ", Nonexistent structure name '" << structName << "'.\n";
		return std::nullopt;
	}

	const std::string fieldName = operand.substr(dotOffset + 1);
	if (fieldName.empty()) {
		std::cout << "Error: Line " << lineNum << ", Invalid field name '" << operand << "'.\n";
		return std::nullopt;
	}

	const auto fieldIter = std::find(identifiers.Fields[structName].begin(), identifiers.Fields[structName].end(), fieldName);
	if (fieldIter != identifiers.Fields[structName].end()) {
		std::cout << "Error: Line " << lineNum << ", Nonexistent field name '" << operand << "'.\n";
		return std::nullopt;
	}

	return objects.Fields[structName][*fieldIter];
}
std::optional<FunctionIndex> GetFunction(std::size_t lineNum, Identifiers& identifiers, Objects& objects, const std::string& operand) {
	const auto funcIter = std::find(identifiers.Functions.begin(), identifiers.Functions.end(), operand);
	if (funcIter == identifiers.Functions.end()) {
		std::cout << "Error: Line " << lineNum << ", Nonexistent procedure or function name '" << operand << "'.\n";
		return std::nullopt;
	}

	return objects.Functions[*funcIter];
}
std::optional<LabelIndex> GetLabel(std::size_t lineNum, Identifiers& identifiers, Objects& objects, const std::string& operand) {
	const auto labelIter = std::find(identifiers.Labels[identifiers.CurrentFunction].begin(), identifiers.Labels[identifiers.CurrentFunction].end(), operand);
	if (labelIter == identifiers.Labels[identifiers.CurrentFunction].end()) {
		std::cout << "Error: Line " << lineNum << ", Nonexistent label name '" << operand << "'.\n";
		return std::nullopt;
	}

	return objects.Labels[identifiers.CurrentFunction][*labelIter];
}
std::optional<LocalVariableIndex> GetLocalVariable(Identifiers& identifiers, Objects& objects, const std::string& operand) {
	const auto varIter = std::find(identifiers.LocalVariables[identifiers.CurrentFunction].begin(), identifiers.LocalVariables[identifiers.CurrentFunction].end(), operand);
	if (varIter == identifiers.LocalVariables[identifiers.CurrentFunction].end()) {
		return std::nullopt;
	}

	return objects.LocalVariables[identifiers.CurrentFunction][*varIter];
}