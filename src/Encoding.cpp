#include <sam/Encoding.hpp>

namespace sam {
	int GetByteCount(char firstByte) noexcept {
		const unsigned char uc = static_cast<unsigned char>(firstByte);
		if (uc < 0x80) return 1;
		else if ((uc & 0xF0) == 0xF0) return 4;
		else if ((uc & 0xE0) == 0xE0) return 3;
		else if ((uc & 0xC0) == 0xC0) return 2;
		else return 0;
	}
}