#pragma once
#include <cstdint>

#include "d3d12.h"
#include "util.hpp"

struct FrameResource
{
    FrameResource(ID3D12Device* device, uint32_t passCount, uint32_t objectCount);
    ~FrameResource();

    NON_COPYABLE(FrameResource);

     
};
