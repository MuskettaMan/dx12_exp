#include "engine.hpp"

#include <numbers>

#include "device.hpp"

Engine::Engine(std::shared_ptr<Device> device) : _device(device)
{
    DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(0.25f * std::numbers::pi_v<float>, 1920.0f / 1080.0f, 1.0f, 100.0f);
    DirectX::XMStoreFloat4x4(&_projection, projection);
}

void Engine::Tick()
{
    float x{ _radius * sinf(_phi) * cosf(_theta) };
    float y{ _radius * sinf(_phi) * sinf(_theta) };
    float z{ _radius * cosf(_phi) };

    DirectX::XMVECTOR pos{ DirectX::XMVectorSet(x, y, z, 1.0f) };
    DirectX::XMVECTOR target{ DirectX::XMVectorZero() };
    DirectX::XMVECTOR up{ DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) };

    DirectX::XMMATRIX view{ DirectX::XMMatrixLookAtLH(pos, target, up) };
    DirectX::XMStoreFloat4x4(&_view, view);

    DirectX::XMMATRIX world{ DirectX::XMLoadFloat4x4(&_world) };
    DirectX::XMMATRIX projection{ DirectX::XMLoadFloat4x4(&_projection) };

    DirectX::XMMATRIX mvp{ world * view * projection };
    _device->SetMVP(mvp);

    _device->Draw();
}

void Engine::OnMouseMove(WPARAM buttonState, int x, int y)
{
    if ((buttonState & MK_LBUTTON) != 0)
    {
        float dx{ DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - _lastMousePosition.y)) };
        float dy{ DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - _lastMousePosition.x)) };

        _theta += dx;
        _phi += dy;

        _phi = std::clamp(_phi, 0.1f, std::numbers::pi_v<float> - 0.1f);
    }
    else if ((buttonState & MK_RBUTTON) != 0)
    {
        float dx{ 0.005f * static_cast<float>(x - _lastMousePosition.x) };
        float dy{ 0.005f * static_cast<float>(y - _lastMousePosition.y) };

        _radius += dx - dy;
        _radius = std::clamp(_radius, 3.0f, 15.0f);
    }

    _lastMousePosition.x = x;
    _lastMousePosition.y = y;
}
