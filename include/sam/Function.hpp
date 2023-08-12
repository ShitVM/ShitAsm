#pragma once

#include <sgn/Builder.hpp>
#include <sgn/Operand.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sam {
	struct Label final {
		std::string Name;
		sgn::LabelIndex Index;
	};
}

namespace sam {
	struct LocalVariable final {
		std::string Name;
		sgn::LocalVariableIndex Index;
	};
}

namespace sam {
	struct Function final {
		std::unique_ptr<sgn::Builder> Builder;

		std::string Name;
		sgn::FunctionIndex Index;
		std::vector<Label> Labels;
		std::vector<LocalVariable> LocalVariables;

		std::optional<sgn::ExternFunctionIndex> ExternIndex;
		std::optional<sgn::MappedFunctionIndex> MappedIndex;

		std::vector<Label>::iterator FindLabel(const std::string& name);
		Label& GetLabel(const std::string& name);
		bool HasLabel(const std::string& name);
		std::vector<LocalVariable>::iterator FindLocalVariable(const std::string& name);
		LocalVariable& GetLocalVariable(const std::string& name);
		bool HasLocalVariable(const std::string& name);
	};
}