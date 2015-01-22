// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11State.h: D3D state definitions.
=============================================================================*/

#pragma once

class FD3D11SamplerState : public FRHISamplerState
{
public:

	TRefCountPtr<ID3D11SamplerState> Resource;
};

class FD3D11RasterizerState : public FRHIRasterizerState
{
public:

	TRefCountPtr<ID3D11RasterizerState> Resource;
};

enum EDepthStencilAccessType
{
	DSAT_Writable = 0,
	DSAT_ReadOnlyDepth = 1,
	DSAT_ReadOnlyStencil = 2,
	DSAT_ReadOnlyDepthAndStencil = 3,

	DSAT_Count = 4
};

class FD3D11DepthStencilState : public FRHIDepthStencilState
{
public:
	
	TRefCountPtr<ID3D11DepthStencilState> Resource;

	/* Describes the read/write state of the separate depth and stencil components of the DSV. */
	EDepthStencilAccessType AccessType;
};

class FD3D11BlendState : public FRHIBlendState
{
public:

	TRefCountPtr<ID3D11BlendState> Resource;
};

