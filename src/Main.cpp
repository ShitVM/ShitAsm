#include <sam/Assembly.hpp>
#include <sam/ExternModule.hpp>
#include <sam/Parser.hpp>
#include <svm/detail/FileSystem.hpp>

#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
	if (argc == 1) {
		std::cout << "Usage: ./ShitAsm <Input> [Output]\n";
		return EXIT_FAILURE;
	}

	const std::string input = argv[1];
	std::string output;
	if (argc >= 3) {
		output = argv[2];
	} else {
		output = svm::detail::fs::path(input).stem().string() + ".sbf";
	}

	sam::Assembly assembly;
	sam::Parser parser(assembly, std::cout);
	if (!parser.Parse(input)) return EXIT_FAILURE;
	else return parser.Generate(output), EXIT_SUCCESS;
}