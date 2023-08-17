#pragma once

#include <sam/Assembly.hpp>
#include <sgn/Operand.hpp>

#include <string>

namespace sam {
	struct ExternModule final {
		std::string Path;
		sam::Assembly Assembly;
		sgn::ExternModuleIndex Index;
		std::string NameSpace;
	};
}