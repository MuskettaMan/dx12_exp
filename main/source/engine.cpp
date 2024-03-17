#include "engine.hpp"
#include "device.hpp"

Engine::Engine(std::shared_ptr<Device> device) : _device(device)
{
}

void Engine::Tick()
{
	_device->Draw();
}
