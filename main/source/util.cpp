#include "util.hpp"
#include <comdef.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <fstream>

DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
    ErrorCode(hr),
    FunctionName(functionName),
    Filename(filename),
    LineNumber(lineNumber)
{
}

std::wstring DxException::ToString() const
{
    // Get the string description of the error code.
    _com_error err(ErrorCode);
    std::wstring msg = err.ErrorMessage();

    return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, uint64_t byteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D12Resource> defaultBuffer;

    // Create the buffer on the GPU.
    CD3DX12_RESOURCE_DESC resourceDesc{ CD3DX12_RESOURCE_DESC::Buffer(byteSize) };
    ThrowIfFailed(device->CreateCommittedResource(&keep(CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT }), D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    // Create an upload buffer to read data from the CPU to the GPU.
    // NOTE: HEAP_TYPE_UPLOAD is only good for CPU-read-once and GPU-read-once.
    ThrowIfFailed(device->CreateCommittedResource(&keep(CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_UPLOAD }), D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    // Data to upload to GPU buffer.
    D3D12_SUBRESOURCE_DATA subResourceData{};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    // Transition buffer to copy state.
    cmdList->ResourceBarrier(1, &keep(CD3DX12_RESOURCE_BARRIER{ CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST) }));

    // Perform the copy.
    UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

    // Transition buffer to read state.
    cmdList->ResourceBarrier(1, &keep(CD3DX12_RESOURCE_BARRIER{ CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ) }));

    return defaultBuffer;
}

Microsoft::WRL::ComPtr<ID3DBlob> D3dUtil::CompileShader(const std::wstring& fileName, const D3D_SHADER_MACRO* defines,
    const std::string& entryPoint, const std::string& target)
{
    using Microsoft::WRL::ComPtr;

    std::uint32_t compileFlags{ 0 };
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> byteCode{ nullptr };
    ComPtr<ID3DBlob> errors{ nullptr };

    hr = D3DCompileFromFile(fileName.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

    if (errors != nullptr)
        OutputDebugStringA(static_cast<LPCSTR>(errors->GetBufferPointer()));

    ThrowIfFailed(hr);

    return byteCode;
}

Microsoft::WRL::ComPtr<ID3DBlob> D3dUtil::LoadBinary(const std::wstring& fileName)
{
    std::ifstream fin{ fileName, std::ios::binary };

    fin.seekg(0, std::ios_base::end);
    std::ifstream::pos_type size = fin.tellg();
    fin.seekg(0, std::ios_base::beg);

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    ThrowIfFailed(D3DCreateBlob(size, blob.GetAddressOf()));

    fin.read(static_cast<char*>(blob->GetBufferPointer()), size);
    fin.close();

    return blob;
}
