// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyViewport.h: Empty viewport RHI definitions.
=============================================================================*/

#pragma once

class FEmptyViewport : public FRHIViewport
{
public:

	FEmptyViewport(void* WindowHandle, uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen);
	~FEmptyViewport();

};

