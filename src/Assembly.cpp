#include <sam/Assembly.hpp>

#include <algorithm>

namespace sam {
	std::vector<Structure>::iterator Assembly::FindStructure(const std::string& name) {
		return std::find_if(Structures.begin(), Structures.end(), [name](const Structure& structure) {
			return structure.Name == name;
		});
	}
	Structure& Assembly::GetStructure(const std::string& name) {
		return *FindStructure(name);
	}
	bool Assembly::HasStructure(const std::string& name) {
		return FindStructure(name) != Structures.end();
	}
	std::vector<Function>::iterator Assembly::FindFunction(const std::string& name) {
		return std::find_if(Functions.begin(), Functions.end(), [name](const Function& function) {
			return function.Name == name;
		});
	}
	Function& Assembly::GetFunction(const std::string& name) {
		return *FindFunction(name);
	}
	bool Assembly::HasFunction(const std::string& name) {
		return FindFunction(name) != Functions.end();
	}
}