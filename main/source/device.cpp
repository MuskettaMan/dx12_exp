#include "device.hpp"
#include "util.hpp"
#include <cassert>
#include "d3dx12.h"
#include "directxmath.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"

Device::Device(HWND hWnd, uint32_t clientWidth, uint32_t clientHeight) :
    _hWnd(hWnd),
    _clientWidth(clientWidth),
    _clientHeight(clientHeight),
    _backBufferFormat(DXGI_FORMAT_R8G8B8A8_UNORM),
    _depthStencilFormat(DXGI_FORMAT_D24_UNORM_S8_UINT),
    _screenViewport(),
    _scissorRect({ 0l, 0l, static_cast<long>(clientWidth / 2), static_cast<long>(clientHeight / 2) })
{
    _screenViewport.TopLeftX = 0.0f;
    _screenViewport.TopLeftY = 0.0f;
    _screenViewport.Width = static_cast<float>(_clientWidth);
    _screenViewport.Height = static_cast<float>(_clientHeight);
    _screenViewport.MinDepth = 0.0f;
    _screenViewport.MaxDepth = 0.0f;

#if defined(_DEBUG)
    EnableDebugLayer();
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io{ ImGui::GetIO() };
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    CreateDevice();
    CreateFence();
    QueryMSAA();
    QueryDescriptorSizes();
    CreateCommandObjects();
    CreateSwapChain();
    CreateDescriptorHeaps();
    CreateRenderTargetViews();

    ImGui_ImplWin32_Init(_hWnd);
    ImGui_ImplDX12_Init(_device.Get(), SWAP_CHAIN_BUFFER_COUNT, _backBufferFormat, _srvHeap.Get(), _srvHeap->GetCPUDescriptorHandleForHeapStart(), _srvHeap->GetGPUDescriptorHandleForHeapStart());

    OnResize();
}

Device::~Device()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
 
    FlushCommandQueue();
}

void Device::Draw()
{
    ThrowIfFailed(_commandAllocator->Reset());
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    using namespace DirectX;

    static XMFLOAT4 c;
    c.w = 1;
    ImGui::Begin("Test");
    ImGui::ColorPicker3("Clear color", &c.x);
    ImGui::End();

    ImGui::Render();
    
    auto barrierToRenderTarget{ CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET) };
    _commandList->ResourceBarrier(1, &barrierToRenderTarget);

    _commandList->RSSetViewports(1, &_screenViewport);
    _commandList->RSSetScissorRects(1, &_scissorRect);

    _commandList->ClearRenderTargetView(CurrentBackBufferView(), &c.x, 0, nullptr);
    _commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
  
    auto currentBackBufferView = CurrentBackBufferView();
    auto depthStencilView = DepthStencilView();
    _commandList->OMSetRenderTargets(1, &currentBackBufferView, true, &depthStencilView);

    _commandList->SetDescriptorHeaps(1, _srvHeap.GetAddressOf());
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), _commandList.Get());

    auto barrierToPresent{ CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT)
    };
    _commandList->ResourceBarrier(1, &barrierToPresent);

    ThrowIfFailed(_commandList->Close());

    ID3D12CommandList* cmdsList[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(static_cast<uint32_t>(std::size(cmdsList)), cmdsList);

    ThrowIfFailed(_swapChain->Present(0, 0));
    _currentBackBuffer = (_currentBackBuffer + 1) % SWAP_CHAIN_BUFFER_COUNT;

    FlushCommandQueue();
}

void Device::EnableDebugLayer()
{
    ComPtr<ID3D12Debug> debugController;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())));
    debugController->EnableDebugLayer();
}

void Device::CreateDevice()
{
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(_dxgiFactory.GetAddressOf())));

    HRESULT hardwareResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(_device.GetAddressOf()));

    if(FAILED(hardwareResult))
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.GetAddressOf())));

        ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(_device.GetAddressOf())));
    }
}

void Device::CreateFence()
{
    ThrowIfFailed(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.GetAddressOf())));
}

void Device::QueryMSAA()
{
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
    msQualityLevels.Format = _backBufferFormat;
    msQualityLevels.SampleCount = 4;
    msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msQualityLevels.NumQualityLevels = 0;
    ThrowIfFailed(_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels)));

    _4xMsaaQuality = msQualityLevels.NumQualityLevels;
    assert(_4xMsaaQuality > 0 && "Unexpected MSAA quality level!");
}

void Device::QueryDescriptorSizes()
{
    _rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    _dsvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    _cbvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Device::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(_commandQueue.GetAddressOf())));
    _commandQueue->SetName(L"Main command queue");

    ThrowIfFailed(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_commandAllocator.GetAddressOf())));
    _commandAllocator->SetName(L"Main command allocator");

    ThrowIfFailed(_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        _commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(_commandList.GetAddressOf())));
    _commandList->SetName(L"Main command list");

    _commandList->Close();
}

void Device::CreateSwapChain()
{
    _swapChain.Reset();

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    swapChainDesc.Width = _clientWidth;
    swapChainDesc.Height = _clientHeight;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
    swapChainDesc.Format = _backBufferFormat;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainFSDesc = {};
    swapChainFSDesc.Windowed = TRUE;


    ThrowIfFailed(_dxgiFactory->CreateSwapChainForHwnd(
        _commandQueue.Get(), 
        _hWnd, 
        &swapChainDesc, 
        &swapChainFSDesc, 
        nullptr, 
        _swapChain.GetAddressOf()));
}

void Device::CreateDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SWAP_CHAIN_BUFFER_COUNT;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(_rtvHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(_dsvHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    srvDesc.NumDescriptors = 1;
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(_device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(_srvHeap.GetAddressOf())));
}

void Device::CreateRenderTargetViews()
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle{ _rtvHeap->GetCPUDescriptorHandleForHeapStart() };
    for(uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
    {
        ThrowIfFailed(_swapChain->GetBuffer(i, IID_PPV_ARGS(&_swapChainBuffer[i])));
        _device->CreateRenderTargetView(_swapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
        rtvHeapHandle.Offset(1, _rtvDescriptorSize);
    }
}

void Device::CreateDepthStencilView()
{
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = _clientWidth;
    depthStencilDesc.Height = _clientHeight;
    depthStencilDesc.DepthOrArraySize = 2;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = _depthStencilFormat;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = _depthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    auto heapProperties{ CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT } };
    ThrowIfFailed(_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(_depthStencilBuffer.GetAddressOf())));

    _device->CreateDepthStencilView(_depthStencilBuffer.Get(), nullptr, DepthStencilView());
    _depthStencilBuffer->SetName(L"Depths/stencil buffer");

    auto barrier{ CD3DX12_RESOURCE_BARRIER::Transition(
        _depthStencilBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_DEPTH_WRITE) };

    _commandList->ResourceBarrier(1, &barrier);
}

void Device::SetViewport()
{
    _commandList->RSSetViewports(1, &_screenViewport);
    _commandList->RSSetScissorRects(1, &_scissorRect);
}

void Device::FlushCommandQueue()
{
    _currentFence++;

    ThrowIfFailed(_commandQueue->Signal(_fence.Get(), _currentFence));

    if (_fence->GetCompletedValue() < _currentFence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

        ThrowIfFailed(_fence->SetEventOnCompletion(_currentFence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void Device::OnResize()
{
    assert(_device);
    assert(_swapChain);
    assert(_commandAllocator);

    FlushCommandQueue();

    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    for (size_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
        _swapChainBuffer[i].Reset();
    _depthStencilBuffer.Reset();

    ThrowIfFailed(_swapChain->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, _clientWidth, _clientHeight, _backBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    _currentBackBuffer = 0;

    CreateRenderTargetViews();
    CreateDepthStencilView();
    
    ThrowIfFailed(_commandList->Close());

    ID3D12CommandList* cmdsList[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(static_cast<uint32_t>(std::size(cmdsList)), cmdsList);

    FlushCommandQueue();

    _screenViewport.TopLeftX = 0;
    _screenViewport.TopLeftY = 0;
    _screenViewport.Width = static_cast<float>(_clientWidth);
    _screenViewport.Height = static_cast<float>(_clientHeight);
    _screenViewport.MinDepth = 0.0f;
    _screenViewport.MaxDepth = 0.0f;
    _scissorRect = { 0, 0, static_cast<long>(_clientWidth), static_cast<long>(_clientHeight) };
}

ID3D12Resource* Device::CurrentBackBuffer() const
{
    return _swapChainBuffer[_currentBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE Device::CurrentBackBufferView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        _rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        _currentBackBuffer,
        _rtvDescriptorSize
    );
}

D3D12_CPU_DESCRIPTOR_HANDLE Device::DepthStencilView() const
{
    return _dsvHeap->GetCPUDescriptorHandleForHeapStart();
}
