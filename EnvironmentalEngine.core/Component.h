#pragma once
#include <string>

namespace EnvironmentalEngine {
	class Component {
	public:
		virtual const char* TypeName() const = 0;
	};
}