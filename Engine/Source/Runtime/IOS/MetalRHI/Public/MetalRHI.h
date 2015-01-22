// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRHI.h: Public Metal RHI definitions.
=============================================================================*/

#pragma once 

DECLARE_LOG_CATEGORY_EXTERN(LogMetal, Display, All);

/** This is a macro that casts a dynamically bound RHI reference to the appropriate type. */
#define DYNAMIC_CAST_METALRESOURCE(Type,Name) \
	FMetal##Type* Name = (FMetal##Type*)Name##RHI;

// Metal RHI public headers.
#include <Metal/Metal.h>
#include "MetalState.h"
#include "MetalResources.h"
#include "MetalViewport.h"

class FMetalManager;

/** The interface which is implemented by the dynamically bound RHI. */
class FMetalDynamicRHI : public FDynamicRHI
{
public:

	/** Initialization constructor. */
	FMetalDynamicRHI();

	/** Destructor */
	~FMetalDynamicRHI();


	// FDynamicRHI interface.
	virtual void Init();
	virtual void Shutdown() {}

	// The RHI methods are defined as virtual functions in URenderHardwareInterface.
	#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) virtual Type RHI##Name ParameterTypesAndNames
	#include "RHIMethods.h"
	#undef DEFINE_RHIMETHOD

protected:
    TGlobalResource<TBoundShaderStateHistory<10000>> BoundShaderStateHistory;
};

/** Implements the Metal module as a dynamic RHI providing module. */
class FMetalDynamicRHIModule : public IDynamicRHIModule
{
public:

	// IModuleInterface
	virtual bool SupportsShutdown() { return false; }

	// IDynamicRHIModule
	virtual bool IsSupported();

	virtual FDynamicRHI* CreateRHI();
};
