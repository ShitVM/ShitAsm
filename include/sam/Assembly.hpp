#pragma once

#include <sam/Function.hpp>
#include <sam/Structure.hpp>
#include <sgn/ByteFile.hpp>

#include <vector>

namespace sam {
	struct Assembly final {
		sgn::ByteFile ByteFile;

		std::vector<Structure> Structures;
		std::vector<Function> Functions;

		std::vector<Structure>::iterator FindStructure(const std::string& name);
		Structure& GetStructure(const std::string& name);
		bool HasStructure(const std::string& name);
		std::vector<Function>::iterator FindFunction(const std::string& name);
		Function& GetFunction(const std::string& name);
		bool HasFunction(const std::string& name);
	};
}