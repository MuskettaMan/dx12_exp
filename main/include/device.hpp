#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <cstdint>
#include <directxmath.h>
#include <memory>
#include <vector>

#include "math_helper.hpp"
#include "upload_buffer.hpp"
#include "fwd.hpp"

constexpr uint32_t SWAP_CHAIN_BUFFER_COUNT = 2;

class App;

struct ObjectConstants
{
    XMFLOAT4X4 worldViewProj = MathHelper::Identity4x4();
};

class Device
{
public:
    Device(HWND hWnd, uint32_t clientWidth, uint32_t clientHeight);
    ~Device();

    void Draw();
    void SetMVP(XMMATRIX mvp)
    {
        _mvp = mvp;
        ObjectConstants constants;
        XMStoreFloat4x4(&constants.worldViewProj, XMMatrixTranspose(mvp));
        _uploadBuffer->CopyData(0, constants);
    }

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

    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry();
    void BuildPSO();

    void FlushCommandQueue();
    void OnResize();

    ID3D12Resource* CurrentBackBuffer() const;

    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentMsaaBackBufferView() const;
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
    ComPtr<ID3D12Resource> _offscreenRenderTargets[SWAP_CHAIN_BUFFER_COUNT];

    ComPtr<ID3D12Resource> _vertexBuffer;
    ComPtr<ID3D12Resource> _indexBuffer;

    ComPtr<ID3D12DescriptorHeap> _rtvHeap;
    ComPtr<ID3D12DescriptorHeap> _rtvMsaaHeap;
    ComPtr<ID3D12DescriptorHeap> _dsvHeap;
    ComPtr<ID3D12DescriptorHeap> _srvHeap;
    ComPtr<ID3D12DescriptorHeap> _cbvHeap;

    const uint32_t _numElements{1};
    std::unique_ptr<UploadBuffer<ObjectConstants>> _uploadBuffer;
    ComPtr<ID3D12RootSignature> _rootSignature;
    std::unique_ptr<MeshGeometry> _boxGeo;

    std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;
    ComPtr<ID3DBlob> _vsByte;
    ComPtr<ID3DBlob> _psByte;

    ComPtr<ID3D12PipelineState> _pso;

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

    XMMATRIX _mvp;
    const XMFLOAT4 backgroundColor{ 0.2f, 0.2f, 0.2f, 0.2f };
};