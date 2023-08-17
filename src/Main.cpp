#include <sam/Assembly.hpp>
#include <sam/ExternModule.hpp>
#include <sam/Lexer.hpp>
#include <sam/Parser.hpp>
#include <sgn/Generator.hpp>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

struct ProgramOption {
	const char* Input = nullptr;
	const char* Output = nullptr;
	std::vector<const char*> ImportDirectories;
};

void PrintUsage();
bool ParseProgramOption(int argc, char* argv[], ProgramOption& programOption);

int main(int argc, char* argv[]) {
	ProgramOption programOption;
	if (!ParseProgramOption(argc, argv, programOption)) return EXIT_FAILURE;

	const std::string input = programOption.Input;
	std::ifstream inputStream(input);
	if (!inputStream) {
		std::cout << "Error: Failed to open '" << input << "'.\n";
		return EXIT_FAILURE;
	}

	sam::Lexer lexer(input, inputStream);
	lexer.Lex();
	if (lexer.HasMessage()) {
		std::cout << lexer.GetMessages();
		if (lexer.HasError()) return EXIT_FAILURE;
	}

	sam::Parser parser(programOption.ImportDirectories, input, lexer.GetTokens(), false);
	parser.Parse();
	if (parser.HasMessage()) {
		std::cout << parser.GetMessages();
		if (parser.HasError()) return EXIT_FAILURE;
	}

	std::string output;
	if (programOption.Output) {
		output = programOption.Output;
	} else {
		output = std::filesystem::path(input).replace_extension(".sbf").string();
	}

	const sam::Assembly assembly = parser.GetAssembly();
	sgn::Generator generator(assembly.ByteFile);
	generator.Generate(output);

	return EXIT_SUCCESS;
}

void PrintUsage() {
	std::cout << "Usage: ./ShitAsm <Input> [-o Output] [-I Import Directory]...\n";
}
bool ParseProgramOption(int argc, char* argv[], ProgramOption& programOption) {
	if (argc == 1) return PrintUsage(), false;

	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "-o") == 0) {
			if (i == argc || programOption.Output) return PrintUsage(), false;
			programOption.Output = argv[++i];
		} else if (std::strcmp(argv[i], "-I") == 0) {
			if (i == argc) return PrintUsage(), false;
			programOption.ImportDirectories.push_back(argv[++i]);
		} else {
			if (programOption.Input) return PrintUsage(), false;
			programOption.Input = argv[i];
		}
	}

	return programOption.Input != nullptr;
}