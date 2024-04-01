#pragma once
#include <directxmath.h>

class MathHelper
{
public:
    static DirectX::XMFLOAT4X4 Identity4x4()
    {
        static DirectX::XMFLOAT4X4 i(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );

        return i;
    }
};
