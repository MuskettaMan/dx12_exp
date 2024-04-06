#pragma once
#include <directxmath.h>
#include <memory>
#include <entt/entity/registry.hpp>

#include "math_helper.hpp"

class Device;

class Engine
{
public:
	Engine(std::shared_ptr<Device> device);

	void Tick();
	virtual void OnMouseMove(WPARAM buttonState, int x, int y);

private:
	std::shared_ptr<Device> _device;
	entt::registry _registry;

	XMFLOAT4X4 _world{ MathHelper::Identity4x4() };
	XMFLOAT4X4 _projection{ MathHelper::Identity4x4() };
	XMFLOAT4X4 _view{ MathHelper::Identity4x4() };

	XMFLOAT2 _lastMousePosition{ 0.0f, 0.0f };
	float _theta{ 0.0f };
	float _phi{ 0.0f };
	float _radius{ 10.0f };
};