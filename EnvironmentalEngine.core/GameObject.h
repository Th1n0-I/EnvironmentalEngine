#pragma once
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include "DirectXMath.h"
#include "Component.h"

namespace EnvironmentalEngine {
	class Transform {
	public:
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 rotation;
		DirectX::XMFLOAT3 scale;
	};

	class GameObject {
	public:
		std::string name{"GameObject"};
		Transform transform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f }, {1.0f, 1.0f, 1.0f} };

		template<class T> T* AddComponent() { 
			auto component = std::make_unique<T>();
			T* ptr = component.get();
			m_components.push_back(std::move(component));
			return ptr;
		};

		template<class T>
		T* GetComponent() {
			for (auto& c : m_components) {
				if (T* found = dynamic_cast<T*>(c.get()))
					return found;
			}
			return nullptr;
		}
		
	private:
		std::vector<std::unique_ptr<Component>> m_components;
	};
}