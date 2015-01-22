// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyRHI.h: Public Empty RHI definitions.
=============================================================================*/

#pragma once 

DECLARE_LOG_CATEGORY_EXTERN(LogEmpty, Display, All)

/** This is a macro that casts a dynamically bound RHI reference to the appropriate type. */
#define DYNAMIC_CAST_EMPTYRESOURCE(Type,Name) \
	FEmpty##Type* Name = (FEmpty##Type*)Name##RHI;

// Empty RHI public headers.
#include "EmptyState.h"
#include "EmptyResources.h"
#include "EmptyViewport.h"

/** The interface which is implemented by the dynamically bound RHI. */
class FEmptyDynamicRHI : public FDynamicRHI
{
public:

	/** Initialization constructor. */
	FEmptyDynamicRHI();

	/** Destructor */
	~FEmptyDynamicRHI() {}


	// FDynamicRHI interface.
	virtual void Init();
	virtual void Shutdown();

	// The RHI methods are defined as virtual functions in URenderHardwareInterface.
	#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) virtual Type RHI##Name ParameterTypesAndNames
	#include "RHIMethods.h"
	#undef DEFINE_RHIMETHOD

};

/** Implements the Empty module as a dynamic RHI providing module. */
class FEmptyDynamicRHIModule : public IDynamicRHIModule
{
public:

	// IModuleInterface
	virtual bool SupportsShutdown() { return false; }

	// IDynamicRHIModule
	virtual bool IsSupported();

	virtual FDynamicRHI* CreateRHI();
};
