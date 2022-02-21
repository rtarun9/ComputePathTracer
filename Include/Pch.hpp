#pragma once

// Dont include the mostly unused stuff from the windows header.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#ifndef UNICODE
#define UNICODE
#endif

// System includes.
#include <Windows.h>

// DirectX and DXGI includes.
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include <wrl/client.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

// Namespace aliases and global headers.
#include "Helpers.hpp"

namespace wrl = Microsoft::WRL;
namespace dx = DirectX;
