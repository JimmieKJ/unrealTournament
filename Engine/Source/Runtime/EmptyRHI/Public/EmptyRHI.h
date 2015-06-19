// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyRHI.h: Public Empty RHI definitions.
=============================================================================*/

#pragma once 

DECLARE_LOG_CATEGORY_EXTERN(LogEmpty, Display, All)

// Empty RHI public headers.
#include "EmptyState.h"
#include "EmptyResources.h"
#include "EmptyViewport.h"

/** The interface which is implemented by the dynamically bound RHI. */
class FEmptyDynamicRHI : public FDynamicRHI, public IRHICommandContext
{
public:

	/** Initialization constructor. */
	FEmptyDynamicRHI();

	/** Destructor */
	~FEmptyDynamicRHI() {}


	// FDynamicRHI interface.
	virtual void Init();
	virtual void Shutdown();

	template<typename TRHIType>
	static FORCEINLINE typename TEmptyResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
	{
		return static_cast<TEmptyResourceTraits<TRHIType>::TConcreteType*>(Resource);
	}

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
