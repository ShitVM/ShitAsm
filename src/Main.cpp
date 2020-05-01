#include <sam/Lexer.hpp>
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

	const auto result = lexer.GetTokens();
	for (const sam::Token& t : result) {
		std::cout << t << '\n';
	}

	return EXIT_SUCCESS;
}