#include "device.hpp"

#include <array>

#include "util.hpp"
#include <cassert>
#include <d3dcompiler.h>

#include "d3dx12.h"
#include "directxmath.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include <iterator>
#include <numbers>

using namespace DirectX; 



struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT4 color;
};

struct Vertex2
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 tex0;
    XMFLOAT2 tex1;
};


D3D12_INPUT_ELEMENT_DESC desc2[]{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex2, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex2, tex1),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex2, normal),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex2, tex0),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

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

    OnResize();

    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildBoxGeometry();
    BuildPSO();

    ImGui_ImplWin32_Init(_hWnd);
    ImGui_ImplDX12_Init(_device.Get(), SWAP_CHAIN_BUFFER_COUNT, _backBufferFormat, _srvHeap.Get(), _srvHeap->GetCPUDescriptorHandleForHeapStart(), _srvHeap->GetGPUDescriptorHandleForHeapStart());


    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(std::size(cmdLists), cmdLists);
    

    FlushCommandQueue();
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
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), _pso.Get()));

    _commandList->RSSetViewports(1, &_screenViewport);
    _commandList->RSSetScissorRects(1, &_scissorRect);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    static XMFLOAT4 backgroundColor;
    backgroundColor.w = 1;
    ImGui::Begin("Background color panel");
    ImGui::ColorPicker3("Color", &backgroundColor.x);
    ImGui::End();

    ImGui::Render();
    
    _commandList->ResourceBarrier(1, &keep(CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET)));


    _commandList->ClearRenderTargetView(CurrentBackBufferView(), &backgroundColor.x, 0, nullptr);
    _commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
  
    _commandList->OMSetRenderTargets(1, 
                                     &keep(CurrentBackBufferView()), 
                                     true, 
                                     &keep(DepthStencilView()));




    _commandList->SetDescriptorHeaps(1, _cbvHeap.GetAddressOf());
    _commandList->SetGraphicsRootSignature(_rootSignature.Get());
    _commandList->IASetVertexBuffers(0, 1, &keep(_boxGeo->VertexBufferView()));
    _commandList->IASetIndexBuffer(&keep(_boxGeo->IndexBufferView()));
    _commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    _commandList->SetGraphicsRootDescriptorTable(0, _cbvHeap->GetGPUDescriptorHandleForHeapStart());

    _commandList->DrawIndexedInstanced(_boxGeo->drawArgs["box"].indexCount, 1, 0, 0, 0);


    
    _commandList->SetDescriptorHeaps(1, _srvHeap.GetAddressOf());
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), _commandList.Get());

    _commandList->ResourceBarrier(1, &keep(CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT)));

    ThrowIfFailed(_commandList->Close());

    ID3D12CommandList* cmdsList[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(std::size(cmdsList), cmdsList);

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

    D3D12_DESCRIPTOR_HEAP_DESC cbvDesc;
    cbvDesc.NumDescriptors = 1;
    cbvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvDesc.NodeMask = 0;
    ThrowIfFailed(_device->CreateDescriptorHeap(&cbvDesc, IID_PPV_ARGS(_cbvHeap.GetAddressOf())));
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

void Device::BuildConstantBuffers()
{
    _uploadBuffer = std::make_unique<UploadBuffer<ObjectConstants>>(_device.Get(), _numElements, true);

    std::uint32_t objCBByteSize = D3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = _uploadBuffer->Resource()->GetGPUVirtualAddress();

    std::int32_t boxCBufferIndex = 0;
    cbAddress += boxCBufferIndex * objCBByteSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = objCBByteSize;


    _device->CreateConstantBufferView(&cbvDesc, _cbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void Device::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{ 1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };

    ComPtr<ID3DBlob> serializedRootSig{ nullptr };
    ComPtr<ID3DBlob> errorBlob{ nullptr };

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if(errorBlob != nullptr)
    {
        OutputDebugStringA(static_cast<LPCSTR>(errorBlob->GetBufferPointer()));
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(_device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&_rootSignature)));
}

void Device::BuildShadersAndInputLayout()
{
    _vsByte = D3dUtil::CompileShader(L"assets\\shaders\\vs.hlsl", nullptr, "VS", "vs_5_0");
    _psByte = D3dUtil::CompileShader(L"assets\\shaders\\vs.hlsl", nullptr, "PS", "ps_5_0");

    _inputLayout = 
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void Device::BuildBoxGeometry()
{
    std::array vertices = {
        Vertex{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        Vertex{ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        Vertex{ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        Vertex{ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        Vertex{ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        Vertex{ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        Vertex{ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        Vertex{ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
    };
    std::array<uint16_t, 36> indices{
        0, 1, 2,
        0, 2, 3,

        4, 6, 5,
        4, 7, 6,

        4, 5, 1,
        4, 1, 0,

        3, 2, 6,
        3, 6, 7,

        1, 5, 6,
        1, 6, 2,

        4, 0, 3,
        4, 3, 7,
    };

    const uint32_t vbByteSize = vertices.size() * sizeof(Vertex);
    const uint32_t ibByteSize = indices.size() * sizeof(uint16_t);

    _boxGeo = std::make_unique<MeshGeometry>();
    _boxGeo->name = "boxGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &_boxGeo->vertexBufferCPU));
    CopyMemory(_boxGeo->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &_boxGeo->indexBufferCPU));
    CopyMemory(_boxGeo->indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    _boxGeo->vertexBufferGPU = CreateDefaultBuffer(_device.Get(), _commandList.Get(), vertices.data(), vbByteSize, _boxGeo->vertexBufferUploader);
    _boxGeo->indexBufferGPU = CreateDefaultBuffer(_device.Get(), _commandList.Get(), indices.data(), ibByteSize, _boxGeo->indexBufferUploader);

    _boxGeo->vertexByteStride = sizeof(Vertex);
    _boxGeo->vertexBufferByteSize = vbByteSize;
    _boxGeo->indexFormat = DXGI_FORMAT_R16_UINT;
    _boxGeo->indexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.indexCount = indices.size();
    submesh.startIndexLocation = 0;
    submesh.baseVertexLocation = 0;

    _boxGeo->drawArgs["box"] = submesh;
}

void Device::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(psoDesc));
    psoDesc.InputLayout = { _inputLayout.data(), static_cast<uint32_t>(_inputLayout.size()) };
    psoDesc.pRootSignature = _rootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(_vsByte->GetBufferPointer()), _vsByte->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(_psByte->GetBufferPointer()), _psByte->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC{ D3D12_DEFAULT };
    psoDesc.BlendState = CD3DX12_BLEND_DESC{ D3D12_DEFAULT };
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC{ D3D12_DEFAULT };
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = _backBufferFormat;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.DSVFormat = _depthStencilFormat;

    ThrowIfFailed(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pso)));
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
