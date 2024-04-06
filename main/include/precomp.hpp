#pragma once

#include <windows.h>
#include <WindowsX.h>
#include <wrl.h>
#include <comdef.h>
#undef max

#include <d3d12.h>
#include "d3dx12.h"
#include "directxmath.h"
#include <d3dcompiler.h>
#include <DirectXCollision.h>

#include <numbers>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <array>
#include <sstream>
#include <fstream>
#include <string>
#include <unordered_map>


#include "entt/entity/registry.hpp"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"

#include "fwd.hpp"
