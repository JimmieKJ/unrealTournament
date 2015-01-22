// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticRHI.h: Statically bound Render Hardware Interface definitions 
	(this will support an RHI that is usable as a dynamic RHI without virtual functions)
=============================================================================*/

#pragma once

#include "RHI.h"

#if USE_STATIC_RHI

#if !defined( STATIC_RHI_NAME )
#pragma error "You must first define STATIC_RHI_NAME to the name of the static RHI before including StaticRHI.h"
#endif

#define RHI_MERGE_THREE( A, B, C ) A##B##C
#define RHI_EXPAND_AND_MERGE_THREE( A, B, C ) RHI_MERGE_THREE( A, B, C )
#define RHI_MERGE_FOUR( A, B, C, D ) A##B##C##D
#define RHI_EXPAND_AND_MERGE_FOUR( A, B, C, D ) RHI_MERGE_FOUR( A, B, C, D )

#define STATIC_RHI_CLASS_NAME RHI_EXPAND_AND_MERGE_THREE( F, STATIC_RHI_NAME, RHI )
#define MAKE_STATIC_RHI_FTYPE( InType ) RHI_EXPAND_AND_MERGE_FOUR( F, STATIC_RHI_NAME, RHI, InType )
#define MAKE_STATIC_RHI_TTYPE( InType ) RHI_EXPAND_AND_MERGE_FOUR( T, STATIC_RHI_NAME, RHI, InType )

// Forward declarations.
template<ERHIResourceType ResourceType>
class MAKE_STATIC_RHI_TTYPE( ResourceReference );

/** This type is just used to give a resource a distinct type when it's passed around outside of this reference counted wrapper. */
template<ERHIResourceType ResourceType>
class MAKE_STATIC_RHI_TTYPE( Resource )
{
};

//
// Statically bound RHI resource reference type definitions.
//
#define DEFINE_STATICRHI_REFERENCE_TYPE(Type,ParentType) \
	template<> class MAKE_STATIC_RHI_TTYPE( Resource )<RRT_##Type> : public MAKE_STATIC_RHI_TTYPE( Resource )<RRT_##ParentType> {}; \
	typedef MAKE_STATIC_RHI_TTYPE( Resource )<RRT_##Type>*			F##Type##RHIParamRef; \
	typedef MAKE_STATIC_RHI_TTYPE( ResourceReference )<RRT_##Type>	F##Type##RHIRef;

ENUM_RHI_RESOURCE_TYPES(DEFINE_STATICRHI_REFERENCE_TYPE);

#undef DEFINE_STATICRHI_REFERENCE_TYPE

/** The interface which is implemented by the statically bound RHI. */
class STATIC_RHI_CLASS_NAME
{
public:

	// For Static RHI, we declare the various RHI functions as static methods
	#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) static Type RHI##Name ParameterTypesAndNames
	#include "RHIMethods.h"
	#undef DEFINE_RHIMETHOD

	// Reference counting API for the different resource types.
	#define DEFINE_STATICRHI_REFCOUNTING_FORTYPE(Type,ParentType) \
		virtual void AddResourceRef(MAKE_STATIC_RHI_TTYPE( Resource )<RRT_##Type>* Reference) = 0; \
		virtual void RemoveResourceRef(MAKE_STATIC_RHI_TTYPE( Resource )<RRT_##Type>* Reference) = 0; \
		virtual uint32 GetRefCount(MAKE_STATIC_RHI_TTYPE( Resource )<RRT_##Type>* Reference) = 0;
	ENUM_RHI_RESOURCE_TYPES(DEFINE_STATICRHI_REFCOUNTING_FORTYPE);
	#undef DEFINE_STATICRHI_REFCOUNTING_FORTYPE
};

/** A global pointer to the statically bound RHI implementation. */
extern STATIC_RHI_CLASS_NAME* GStaticRHI;

/**
 * A reference to a statically bound RHI resource.
 * When using the statically bound RHI, the reference counting goes through a statically bound interface in GStaticRHI.
 * @param ResourceType - The type of resource the reference may point to.
 */
template<ERHIResourceType ResourceType>
class MAKE_STATIC_RHI_TTYPE( ResourceReference )
{
public:

	typedef MAKE_STATIC_RHI_TTYPE( Resource )<ResourceType>* ReferenceType;

	/** Default constructor. */
	MAKE_STATIC_RHI_TTYPE( ResourceReference )():
		Reference(NULL)
	{}

	/** Initialization constructor. */
	MAKE_STATIC_RHI_TTYPE( ResourceReference )(ReferenceType InReference)
	{
		Reference = InReference;
		if(Reference)
		{
			GStaticRHI->AddResourceRef(Reference);
		}
	}

	/** Copy constructor. */
	MAKE_STATIC_RHI_TTYPE( ResourceReference )(const MAKE_STATIC_RHI_TTYPE( ResourceReference )& Copy)
	{
		Reference = Copy.Reference;
		if(Reference)
		{
			GStaticRHI->AddResourceRef(Reference);
		}
	}

	/** Destructor. */
	~MAKE_STATIC_RHI_TTYPE( ResourceReference )()
	{
		if(Reference)
		{
			GStaticRHI->RemoveResourceRef(Reference);
		}
	}

	/** Assignment operator. */
	MAKE_STATIC_RHI_TTYPE( ResourceReference )& operator=(ReferenceType InReference)
	{
		ReferenceType OldReference = Reference;
		if(InReference)
		{
			GStaticRHI->AddResourceRef(InReference);
		}
		Reference = InReference;
		if(OldReference)
		{
			GStaticRHI->RemoveResourceRef(OldReference);
		}
		return *this;
	}

	/** Assignment operator. */
	MAKE_STATIC_RHI_TTYPE( ResourceReference )& operator=(const MAKE_STATIC_RHI_TTYPE( ResourceReference )& InPtr)
	{
		return *this = InPtr.Reference;
	}

	/** Equality operator. */
	bool operator==(const MAKE_STATIC_RHI_TTYPE( ResourceReference )& Other) const
	{
		return Reference == Other.Reference;
	}

	/** Dereference operator. */
	void* operator->() const
	{
		return Reference;
	}

	/** Dereference operator. */
	operator ReferenceType() const
	{
		return Reference;
	}

	/** Type hashing. */
	friend uint32 GetTypeHash(ReferenceType Reference)
	{
		return PointerHash(Reference);
	}

	// RHI reference interface.
	friend bool IsValidRef(const MAKE_STATIC_RHI_TTYPE( ResourceReference )& Ref)
	{
		return Ref.Reference != NULL;
	}
	void SafeRelease()
	{
		*this = NULL;
	}
	uint32 GetRefCount()
	{
		if(Reference)
		{
			return GStaticRHI->GetRefCount(Reference);
		}
		else
		{
			return 0;
		}
	}

private:
	ReferenceType Reference;
};

// Implement the statically bound RHI methods to simply call the static RHI.
#define DEFINE_RHIMETHOD_CMDLIST(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	FORCEINLINE Type Name##_Internal ParameterTypesAndNames \
	{ \
		ReturnStatement GStaticRHI->RHI##Name ParameterNames; \
	}
#define DEFINE_RHIMETHOD_GLOBAL(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	FORCEINLINE Type Name##_Internal ParameterTypesAndNames \
	{ \
		ReturnStatement GStaticRHI->RHI##Name ParameterNames; \
	}
#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	FORCEINLINE Type RHI##Name ParameterTypesAndNames \
	{ \
		ReturnStatement GStaticRHI->RHI##Name ParameterNames; \
	}
#define DEFINE_RHIMETHOD_GLOBALFLUSH(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	FORCEINLINE Type Name##_Internal ParameterTypesAndNames \
	{ \
		ReturnStatement GStaticRHI->RHI##Name ParameterNames; \
	}
#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	FORCEINLINE Type Name##_Internal ParameterTypesAndNames \
	{ \
		ReturnStatement GStaticRHI->RHI##Name ParameterNames; \
	}
#include "RHIMethods.h"
#undef DEFINE_RHIMETHOD
#undef DEFINE_RHIMETHOD_CMDLIST
#undef DEFINE_RHIMETHOD_GLOBAL
#undef DEFINE_RHIMETHOD_GLOBALFLUSH
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE

#endif	//#if USE_STATIC_RHI


#if !USE_DYNAMIC_RHI

/**
 * Defragment the texture pool.
 */
void appDefragmentTexturePool();

/**
 * Checks if the texture data is allocated within the texture pool or not.
 */
bool appIsPoolTexture( FTextureRHIParamRef TextureRHI );

/**
 * Log the current texture memory stats.
 *
 * @param Message	This text will be included in the log
 */
void appDumpTextureMemoryStats(const TCHAR* /*Message*/);

#endif
