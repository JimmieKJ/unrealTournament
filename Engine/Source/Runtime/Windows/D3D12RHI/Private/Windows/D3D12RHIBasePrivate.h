// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12BaseRHIPrivate.h: Private D3D RHI definitions for Windows.
=============================================================================*/

#pragma once

// Assume D3DX is available
#ifndef WITH_D3DX_LIBS
#define WITH_D3DX_LIBS	1
#endif

// Disable macro redefinition warning for compatibility with Windows SDK 8+
#pragma warning(push)
#if _MSC_VER >= 1700
#pragma warning(disable : 4005)	// macro redefinition
#endif

// D3D headers.
#if PLATFORM_64BITS
#pragma pack(push,16)
#else
#pragma pack(push,8)
#endif
#define D3D_OVERLOADS 1
#include "AllowWindowsPlatformTypes.h"
#include <d3d12.h>
#include "d3dx12.h"
#include <d3d12sdklayers.h>
#include "HideWindowsPlatformTypes.h"

#undef DrawText

#pragma pack(pop)
#pragma warning(pop)


class FD3D12Device;

class FD3D12DeviceChild
{
protected:
    FD3D12Device* Parent;

public:
    FD3D12DeviceChild(FD3D12Device* InParent = nullptr) : Parent(InParent) {}

    inline FD3D12Device* GetParentDevice() const
	{ 
        // If this fires an object was likely created with a default constructor i.e in an STL container
        // and is therefore an orphan
        check(Parent != nullptr);
        return Parent;
	}

	// To be used with delayed setup
    inline void SetParentDevice(FD3D12Device* InParent)
	{
		check(Parent == nullptr);
		Parent = InParent;
	}
};
