// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DynamicRHI.h: Dynamically bound Render Hardware Interface definitions.
=============================================================================*/

#ifndef __DYNAMICRHI_H__
#define __DYNAMICRHI_H__

#include "ModuleInterface.h"

#if USE_DYNAMIC_RHI

//
// Statically bound RHI resource reference type definitions for the dynamically bound RHI.
//

#undef DEFINE_DYNAMICRHI_REFERENCE_TYPE

/** The interface which is implemented by the dynamically bound RHI. */
class FDynamicRHI
{
public:

	/** Declare a virtual destructor, so the dynamic RHI can be deleted without knowing its type. */
	virtual ~FDynamicRHI() {}

	/** Initializes the RHI; separate from IDynamicRHIModule::CreateRHI so that GDynamicRHI is set when it is called. */
	virtual void Init() = 0;
	/** Shutdown the RHI; handle shutdown and resource destruction before the RHI's actual destructor is called (so that all resources of the RHI are still available for shutdown). */
	virtual void Shutdown() = 0;

	#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) virtual Type RHI##Name ParameterTypesAndNames = 0
	#define DEFINE_RHIMETHOD_CMDLIST(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBAL(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) virtual Type RHI##Name ParameterTypesAndNames = 0
	#define DEFINE_RHIMETHOD_GLOBALFLUSH(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) virtual Type RHI##Name ParameterTypesAndNames = 0
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) virtual Type RHI##Name ParameterTypesAndNames = 0
	#include "RHIMethods.h"
	#undef DEFINE_RHIMETHOD
	#undef DEFINE_RHIMETHOD_CMDLIST
	#undef DEFINE_RHIMETHOD_GLOBAL
	#undef DEFINE_RHIMETHOD_GLOBALFLUSH
	#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE
};

/** A global pointer to the dynamically bound RHI implementation. */
extern RHI_API FDynamicRHI* GDynamicRHI;

#define DEFINE_RHIMETHOD_CMDLIST(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation)
#define DEFINE_RHIMETHOD_GLOBAL(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	extern RHI_API Type Name##_Internal ParameterTypesAndNames;
#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	extern RHI_API Type RHI##Name ParameterTypesAndNames;
#define DEFINE_RHIMETHOD_GLOBALFLUSH(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	extern RHI_API Type Name##_Internal ParameterTypesAndNames;
#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	extern RHI_API Type Name##_Internal ParameterTypesAndNames;
#include "RHIMethods.h"
#undef DEFINE_RHIMETHOD
#undef DEFINE_RHIMETHOD_CMDLIST
#undef DEFINE_RHIMETHOD_GLOBAL
#undef DEFINE_RHIMETHOD_GLOBALFLUSH
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE


#endif	//#if USE_DYNAMIC_RHI


#if PLATFORM_USES_DYNAMIC_RHI

/**
 * Defragment the texture pool.
 */
inline void appDefragmentTexturePool() {}

/**
 * Checks if the texture data is allocated within the texture pool or not.
 */
inline bool appIsPoolTexture( FTextureRHIParamRef TextureRHI ) { return false; }

/**
 * Log the current texture memory stats.
 *
 * @param Message	This text will be included in the log
 */
inline void appDumpTextureMemoryStats(const TCHAR* /*Message*/) {}

#endif // PLATFORM_USES_DYNAMIC_RHI

/** Defines the interface of a module implementing a dynamic RHI. */
class IDynamicRHIModule : public IModuleInterface
{
public:

	/** Checks whether the RHI is supported by the current system. */
	virtual bool IsSupported() = 0;

	/** Creates a new instance of the dynamic RHI implemented by the module. */
	virtual FDynamicRHI* CreateRHI() = 0;
};

#if USE_DYNAMIC_RHI
/**
 *	Each platform that utilizes dynamic RHIs should implement this function
 *	Called to create the instance of the dynamic RHI.
 */
FDynamicRHI* PlatformCreateDynamicRHI();

#endif	//USE_DYNAMIC_RHI

#endif
