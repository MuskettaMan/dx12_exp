#pragma once
#include <memory>
#include "entt/entity/registry.hpp"

class Device;

class Engine
{
public:
	Engine(std::shared_ptr<Device> device);

	void Tick();

private:
	std::shared_ptr<Device> _device;
	entt::registry _registry;
};