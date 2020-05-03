#include <sam/Lexer.hpp>
#include <sam/Parser.hpp>
#include <svm/detail/FileSystem.hpp>

// Temp
#include <sam/ExternModule.hpp>

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

	const auto assembly = parser.GetAssembly();
	std::cout << "<Structures>\n";
	for (const auto& s : assembly.Structures) {
		std::cout << s.Name << '\n';
	}

	std::cout << "<Functions>\n";
	for (const auto& f : assembly.Functions) {
		std::cout << f.Name << '\n';
		for (const auto& l : f.Labels) {
			std::cout << '\t' << l.Name << '\n';
		}
	}

	return EXIT_SUCCESS;
}