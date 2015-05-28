// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalViewport.h: Metal viewport RHI definitions.
=============================================================================*/

#pragma once

class FMetalViewport : public FRHIViewport
{
public:

	FMetalViewport(void* WindowHandle, uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen);
	~FMetalViewport();

	void Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen);
};

template<>
struct TMetalResourceTraits<FRHIViewport>
{
	typedef FMetalViewport TConcreteType;
};

