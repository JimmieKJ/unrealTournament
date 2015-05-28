// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** A null implementation of the dynamically bound RHI. */
class FNullDynamicRHI : public FDynamicRHI , public IRHICommandContext
{
public:

	FNullDynamicRHI();

	// FDynamicRHI interface.
	virtual void Init();
	virtual void Shutdown();

	// Implement the dynamic RHI interface using the null implementations defined in RHIMethods.h
	#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	virtual Type RHI##Name ParameterTypesAndNames{ NullImplementation; }
	#include "RHIMethods.h"
	#undef DEFINE_RHIMETHOD

private:

	/** Allocates a static buffer for RHI functions to return as a write destination. */
	static void* GetStaticBuffer();
};
