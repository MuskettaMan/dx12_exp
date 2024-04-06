#pragma once
#include <cstdint>

#include "d3d12.h"
#include "d3dx12.h"
#include "util.hpp"
#include "fwd.hpp"

template <typename T>
class UploadBuffer
{
public:
    UploadBuffer(ID3D12Device* device, uint32_t elementCount, bool isConstantBuffer) : _isConstantBuffer(isConstantBuffer)
    {
        _elementByteSize = _isConstantBuffer ? D3dUtil::CalcConstantBufferByteSize(sizeof(T)) : sizeof(T);

        ThrowIfFailed(device->CreateCommittedResource(
            &keep(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)),
            D3D12_HEAP_FLAG_NONE,
            &keep(CD3DX12_RESOURCE_DESC::Buffer(_elementByteSize* elementCount)),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_uploadBuffer)));

        ThrowIfFailed(_uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&_mappedData)));
    }

    ~UploadBuffer()
    {
        if (_uploadBuffer)
            _uploadBuffer->Unmap(0, nullptr);

        _mappedData = nullptr;
    }

    NON_COPYABLE(UploadBuffer);
    NON_MOVABLE(UploadBuffer);

    ID3D12Resource* Resource() const { return _uploadBuffer.Get(); }

    void CopyData(uint32_t elementIndex, const T& data)
    {
        memcpy(&_mappedData[elementIndex * _elementByteSize], &data, sizeof(T));
    }

private:
    ComPtr<ID3D12Resource> _uploadBuffer;
    BYTE* _mappedData;

    uint32_t _elementByteSize;
    bool _isConstantBuffer;
};
