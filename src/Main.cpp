#include <sam/Assembly.hpp>
#include <sam/ExternModule.hpp>
#include <sam/Lexer.hpp>
#include <sam/Parser.hpp>
#include <sgn/Generator.hpp>
#include <svm/detail/FileSystem.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
	if (argc == 1) {
		std::cout << "Usage: ./ShitAsm <Input> [Output]\n";
		return EXIT_FAILURE;
	}

	const svm::detail::fs::path input(argv[1]);
	const std::string output = argc >= 3 ? argv[2] : input.stem().string() + ".sbf";
	std::ifstream inputStream(input);
	if (!inputStream) {
		std::cout << "Error: Failed to open '" << input << "'.\n";
		return EXIT_FAILURE;
	}

	sam::Lexer lexer(input.string(), inputStream);
	lexer.Lex();
	if (lexer.HasMessage()) {
		std::cout << lexer.GetMessages();
		if (lexer.HasError()) return EXIT_FAILURE;
	}

	sam::Parser parser(input.string(), lexer.GetTokens());
	parser.Parse();
	if (parser.HasMessage()) {
		std::cout << parser.GetMessages();
		if (parser.HasError()) return EXIT_FAILURE;
	}

	const sam::Assembly assembly = parser.GetAssembly();
	sgn::Generator generator(assembly.ByteFile);
	generator.Generate(output);

	return EXIT_SUCCESS;
}