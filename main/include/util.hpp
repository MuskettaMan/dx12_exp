#pragma once
#include <wrl.h>
#include <string>
#include <d3d12.h>
#include <DirectXCollision.h>
#include <unordered_map>


template <class T>
constexpr auto& keep(T&& x) noexcept
{
    return x;
}

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

    std::wstring ToString() const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, uint64_t byteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

#define NON_COPYABLE(classname) \
        classname(const classname&) = delete; \
        classname& operator=(const classname&) = delete

#define NON_MOVABLE(classname) \
        classname(classname&&) = delete; \
        classname& operator=(classname&&) = delete


class D3dUtil
{
public:
    static uint32_t CalcConstantBufferByteSize(uint32_t byteSize)
    {
        // Constant buffers must be a multiple of the minimum hardware
        // allocation size (usually 256 bytes).  So round up to nearest
        // multiple of 256.  We do this by adding 255 and then masking off
        // the lower 2 bytes which store all bits < 256.
        // Example: Suppose byteSize = 300.
        // (300 + 255) & ~255
        // 555 & ~255
        // 0x022B & ~0x00ff
        // 0x022B & 0xff00
        // 0x0200
        // 512
        return (byteSize + 255) & ~255;
    }

    static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const std::wstring& fileName, const D3D_SHADER_MACRO* defines, const std::string& entryPoint, const std::string& target);
    static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& fileName);
};

struct SubmeshGeometry
{
    uint32_t indexCount = 0;
    uint32_t startIndexLocation = 0;
    int32_t baseVertexLocation = 0;
    DirectX::BoundingBox bounds;
};

struct MeshGeometry
{
    std::string name;
    Microsoft::WRL::ComPtr<ID3DBlob> vertexBufferCPU = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> indexBufferCPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferGPU = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferGPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferUploader = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferUploader = nullptr;

    uint32_t vertexByteStride = 0;
    uint32_t vertexBufferByteSize = 0;
    DXGI_FORMAT indexFormat = DXGI_FORMAT_R16_UINT;
    uint32_t indexBufferByteSize = 0;

    std::unordered_map<std::string, SubmeshGeometry> drawArgs;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = vertexBufferGPU->GetGPUVirtualAddress();
        vbv.StrideInBytes = vertexByteStride;
        vbv.SizeInBytes = vertexBufferByteSize;

        return vbv;
    }
    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
    {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = indexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = indexFormat;
        ibv.SizeInBytes = indexBufferByteSize;

        return ibv;
    }

    void DisposeUploaders()
    {
        vertexBufferUploader = nullptr;
        indexBufferUploader = nullptr;
    }
};
