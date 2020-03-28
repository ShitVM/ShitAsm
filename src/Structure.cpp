#include <sam/Structure.hpp>

#include <algorithm>

namespace sam {
	std::vector<Field>::iterator Structure::FindField(const std::string& name) {
		return std::find(Fields.begin(), Fields.end(), [name](const Field& field) {
			return field.Name == name;
		});
	}
	Field& Structure::GetField(const std::string& name) {
		return *FindField(name);
	}
	bool Structure::HasField(const std::string& name) {
		return FindField(name) != Fields.end();
	}
}