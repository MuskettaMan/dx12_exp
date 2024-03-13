#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

constexpr uint32_t SWAP_CHAIN_BUFFER_COUNT = 2;

class App;

class Device
{
public:
    Device(HWND hWnd, uint32_t clientWidth, uint32_t clientHeight);
    ~Device();

    void Draw();

private:
    friend App;

    void EnableDebugLayer();
    void CreateDevice();
    void CreateFence();
    void QueryMSAA();
    void QueryDescriptorSizes();
    void CreateCommandObjects();
    void CreateSwapChain();
    void CreateDescriptorHeaps();
    void CreateRenderTargetViews();
    void CreateDepthStencilView();
    void SetViewport();

    void FlushCommandQueue();
    void OnResize();

    ID3D12Resource* CurrentBackBuffer() const;

    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

    ComPtr<IDXGIFactory4> _dxgiFactory;
    ComPtr<ID3D12Device> _device;

    uint64_t _currentFence;
    ComPtr<ID3D12Fence1> _fence;
    
    ComPtr<ID3D12CommandQueue> _commandQueue;
    ComPtr<ID3D12CommandAllocator> _commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> _commandList;
    
    ComPtr<IDXGISwapChain1> _swapChain;
    ComPtr<ID3D12Resource> _swapChainBuffer[SWAP_CHAIN_BUFFER_COUNT];
    ComPtr<ID3D12Resource> _depthStencilBuffer;

    ComPtr<ID3D12DescriptorHeap> _rtvHeap;
    ComPtr<ID3D12DescriptorHeap> _dsvHeap;

    HWND _hWnd;
    uint32_t _clientWidth;
    uint32_t _clientHeight;

    uint32_t _rtvDescriptorSize;
    uint32_t _dsvDescriptorSize;
    uint32_t _cbvDescriptorSize;
    DXGI_FORMAT _backBufferFormat;
    DXGI_FORMAT _depthStencilFormat;
    
    uint32_t _currentBackBuffer = 0;

    bool _4xMsaaState = true;
    uint32_t _4xMsaaQuality;

    D3D12_VIEWPORT _screenViewport;
    RECT _scissorRect;
};