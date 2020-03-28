#include <sam/Function.hpp>

#include <algorithm>

namespace sam {
	std::vector<Label>::iterator Function::FindLabel(const std::string& name) {
		return std::find(Labels.begin(), Labels.end(), [name](const Label& label) {
			return label.Name == name;
		});
	}
	Label& Function::GetLabel(const std::string& name) {
		return *FindLabel(name);
	}
	bool Function::HasLabel(const std::string& name) {
		return FindLabel(name) != Labels.end();
	}
	std::vector<LocalVariable>::iterator Function::FindLocalVariable(const std::string& name) {
		return std::find(LocalVariables.begin(), LocalVariables.end(), [name](const LocalVariable& var) {
			return var.Name == name;
		});
	}
	LocalVariable& Function::GetLocalVariable(const std::string& name) {
		return *FindLocalVariable(name);
	}
	bool Function::HasLocalVariable(const std::string& name) {
		return FindLocalVariable(name) != LocalVariables.end();
	}
}