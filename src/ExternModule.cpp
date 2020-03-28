#include <sam/ExternModule.hpp>

#include <algorithm>

namespace sam {
	std::vector<Structure>::iterator ExternModule::FindStructure(const std::string& name) {
		return std::find_if(Structures.begin(), Structures.end(), [name](const Structure& structure) {
			return structure.Name == name;
		});
	}
	Structure& ExternModule::GetStructure(const std::string& name) {
		return *FindStructure(name);
	}
	bool ExternModule::HasStructure(const std::string& name) {
		return FindStructure(name) != Structures.end();
	}
	std::vector<Function>::iterator ExternModule::FindFunction(const std::string& name) {
		return std::find_if(Functions.begin(), Functions.end(), [name](const Function& function) {
			return function.Name == name;
		});
	}
	Function& ExternModule::GetFunction(const std::string& name) {
		return *FindFunction(name);
	}
	bool ExternModule::HasFunction(const std::string& name) {
		return FindFunction(name) != Functions.end();
	}
}