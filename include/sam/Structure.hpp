#pragma once

#include <sgn/Operand.hpp>

#include <optional>
#include <string>
#include <vector>

namespace sam {
	struct Field final {
		std::string Name;
		sgn::FieldIndex Index;
	};
}

namespace sam {
	struct Structure final {
		std::string Name;
		sgn::StructureIndex Index;
		std::vector<Field> Fields;

		std::optional<sgn::ExternStructureIndex> ExternIndex;
		std::optional<sgn::MappedStructureIndex> MappedIndex;

		std::vector<Field>::iterator FindField(const std::string& name);
		Field& GetField(const std::string& name);
		bool HasField(const std::string& name);
	};
}