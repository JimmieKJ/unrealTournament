// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectBaseUtility.h: Unreal UObject functions that only depend on UObjectBase
=============================================================================*/

#pragma once

#if _MSC_VER == 1900
	#ifdef PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
		PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
	#endif
#endif

#include "Templates/PointerIsConvertibleFromTo.h"

class COREUOBJECT_API UObjectBaseUtility : public UObjectBase
{
public:
	// Constructors.
	UObjectBaseUtility() {}
	UObjectBaseUtility( EObjectFlags InFlags )
		: UObjectBase(InFlags)
	{
	}

	/***********************/
	/******** Flags ********/
	/***********************/

	FORCEINLINE void SetFlags( EObjectFlags NewFlags )
	{
		SetFlagsTo(GetFlags() | NewFlags);
	}
	FORCEINLINE void ClearFlags( EObjectFlags NewFlags )
	{
		SetFlagsTo(GetFlags() & ~NewFlags);
	}
	/**
	 * Used to safely check whether any of the passed in flags are set. 
	 *
	 * @param FlagsToCheck	Object flags to check for.
	 * @return				true if any of the passed in flags are set, false otherwise  (including no flags passed in).
	 */
	FORCEINLINE bool HasAnyFlags( EObjectFlags FlagsToCheck ) const
	{
		return (GetFlags() & FlagsToCheck) != 0;
	}
	/**
	 * Used to safely check whether all of the passed in flags are set. 
	 *
	 * @param FlagsToCheck	Object flags to check for
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	FORCEINLINE bool HasAllFlags( EObjectFlags FlagsToCheck ) const
	{
		return ((GetFlags() & FlagsToCheck) == FlagsToCheck);
	}
	/**
	 * Returns object flags that are both in the mask and set on the object.
	 *
	 * @param Mask	Mask to mask object flags with
	 * @param Objects flags that are set in both the object and the mask
	 */
	FORCEINLINE EObjectFlags GetMaskedFlags( EObjectFlags Mask = RF_AllFlags ) const
	{
		return EObjectFlags(GetFlags() & Mask);
	}

	/**
	 * Checks the RF_PendingKill flag to see if it is dead but memory still valid
	 */
	FORCEINLINE bool IsPendingKill() const
	{
		return HasAnyFlags(RF_PendingKill);
	}

	/**
	 * Marks this object as RF_PendingKill.
	 */
	FORCEINLINE void MarkPendingKill()
	{
		check(!IsRooted());
		SetFlags( RF_PendingKill );
	}

	//
	// Add an object to the root set. This prevents the object and all
	// its descendants from being deleted during garbage collection.
	//
	FORCEINLINE void AddToRoot()
	{
		SetFlags( RF_RootSet );
	}

	//
	// Remove an object from the root set.
	//
	FORCEINLINE void RemoveFromRoot()
	{
		ClearFlags( RF_RootSet );
	}

	/**
	 * Returns true if this object is explicitly rooted
	 *
	 * @return true if the object was explicitly added as part of the root set.
	 */
	FORCEINLINE bool IsRooted()
	{
		return HasAnyFlags( RF_RootSet );
	}

	/***********************/
	/******** Marks *******  UObjectMarks.cpp */
	/***********************/

	/**
	 * Adds marks to an object
	 *
	 * @param	Marks	Logical OR of OBJECTMARK_'s to apply 
	 */
	FORCEINLINE void Mark(EObjectMark Marks) const
	{
		MarkObject(this,Marks);
	}

	/**
	 * Removes marks from and object
	 *
	 * @param	Marks	Logical OR of OBJECTMARK_'s to remove 
	 */
	FORCEINLINE void UnMark(EObjectMark Marks) const
	{
		UnMarkObject(this,Marks);
	}

	/**
	 * Tests an object for having ANY of a set of marks
	 *
	 * @param	Marks	Logical OR of OBJECTMARK_'s to test
	 * @return	true if the object has any of the given marks.
	 */
	FORCEINLINE bool HasAnyMarks(EObjectMark Marks) const
	{
		return ObjectHasAnyMarks(this,Marks);
	}

	/**
	 * Tests an object for having ALL of a set of marks
	 *
	 * @param	Marks	Logical OR of OBJECTMARK_'s to test
	 * @return	true if the object has any of the given marks.
	 */
	FORCEINLINE bool HasAllMarks(EObjectMark Marks) const
	{
		return ObjectHasAllMarks(this,Marks);
	}

	/***********************/
	/******** Names ********/
	/***********************/

	/**
	 * Returns the fully qualified pathname for this object as well as the name of the class, in the format:
	 * 'ClassName Outermost[.Outer].Name'.
	 *
	 * @param	StopOuter	if specified, indicates that the output string should be relative to this object.  if StopOuter
	 *						does not exist in this object's Outer chain, the result would be the same as passing NULL.
	 *
	 * @note	safe to call on NULL object pointers!
	 */
	FString GetFullName( const UObject* StopOuter=NULL ) const;

	/**
	 * Returns the fully qualified pathname for this object, in the format:
	 * 'Outermost[.Outer].Name'
	 *
	 * @param	StopOuter	if specified, indicates that the output string should be relative to this object.  if StopOuter
	 *						does not exist in this object's Outer chain, the result would be the same as passing NULL.
	 *
	 * @note	safe to call on NULL object pointers!
	 */
	FString GetPathName( const UObject* StopOuter=NULL ) const;

protected:
	/**
	 * Internal version of GetPathName() that eliminates lots of copies.
	 */
	void GetPathName( const UObject* StopOuter, FString& ResultString ) const;
public:
	/**
	 * Walks up the chain of packages until it reaches the top level, which it ignores.
	 *
	 * @param	bStartWithOuter		whether to include this object's name in the returned string
	 * @return	string containing the path name for this object, minus the outermost-package's name
	 */
	FString GetFullGroupName( bool bStartWithOuter ) const;

	/**
	 * Returns the name of this object (with no path information)
	 * 
	 * @return Name of the object.
	 */
	FORCEINLINE FString GetName() const
	{
		return GetFName().ToString();
	}
	// GetFullName optimization
	FORCEINLINE void GetName(FString &ResultString) const
	{
		GetFName().ToString(ResultString);
	}
	FORCEINLINE void AppendName(FString& ResultString) const
	{
		GetFName().AppendString(ResultString);
	}

	/***********************/
	/******** Outer ********/
	/***********************/

	/** 
	 * Walks up the list of outers until it finds the highest one.
	 *
	 * @return outermost non NULL Outer.
	 */
	UPackage* GetOutermost() const;

	/** 
	 * Finds the outermost package and marks it dirty. 
	 * The editor suppresses this behavior during load as it is against policy to dirty packages simply by loading them.
	 *
	 * @return false if the request to mark the package dirty was suppressed by the editor and true otherwise.
	 */
	bool MarkPackageDirty() const;

	/**
	* Determines whether this object is a template object
	*
	* @return	true if this object is a template object (owned by a UClass)
	*/
	bool IsTemplate( EObjectFlags TemplateTypes = RF_ArchetypeObject|RF_ClassDefaultObject ) const;

	/**
	 * Traverses the outer chain searching for the next object of a certain type.  (T must be derived from UObject)
	 *
	 * @param	Target class to search for
	 * @return	a pointer to the first object in this object's Outer chain which is of the correct type.
	 */
	UObject* GetTypedOuter(UClass* Target) const;

	/**
	 * Traverses the outer chain searching for the next object of a certain type.  (T must be derived from UObject)
	 *
	 * @return	a pointer to the first object in this object's Outer chain which is of the correct type.
	 */
	template<typename T>
	T* GetTypedOuter() const
	{
		return (T *)GetTypedOuter(T::StaticClass());
	}

	/**
	 * @return	true if the specified object appears somewhere in this object's outer chain.
	 */
	bool IsIn( const UObject* SomeOuter ) const;

	/**
	 * Find out if this object is inside (has an outer) that is of the specified class
	 * @param SomeBaseClass	The base class to compare against
	 * @return True if this object is in an object of the given type.
	 */
	bool IsInA( const UClass* SomeBaseClass ) const;

	/**
	 * Checks whether this object's top-most package has any of the specified flags
	 *
	 * @param	CheckFlagMask	a bitmask of EPackageFlags values to check for
	 *
	 * @return	true if the PackageFlags member of this object's top-package has any bits from the mask set.
	 */
	bool RootPackageHasAnyFlags( uint32 CheckFlagMask ) const;

	/***********************/
	/******** Class ********/
	/***********************/

	/**
	 * @return	true if this object is of the specified type.
	 */
	#if UCLASS_FAST_ISA_IMPL == 2
	private:
		template <typename ClassType>
		static FORCEINLINE bool IsAWorkaround(const ClassType* ObjClass, const ClassType* TestCls)
		{
			return ObjClass->IsAUsingFastTree(*TestCls);
		}

	public:
		template <typename OtherClassType>
		FORCEINLINE bool IsA( OtherClassType SomeBase ) const
		{
			// We have a cyclic dependency between UObjectBaseUtility and UClass,
			// so we use a template to allow inlining of something we haven't yet seen, because it delays compilation until the function is called.

			// 'static_assert' that this thing is actually a UClass pointer or convertible to it.
			const UClass* SomeBaseClass = SomeBase;
			(void)SomeBaseClass;
			checkfSlow(SomeBaseClass, TEXT("IsA(NULL) cannot yield meaningful results"));

			const UClass* ThisClass = GetClass();

			// Stop the compiler doing some unnecessary branching for nullptr checks
			ASSUME(SomeBaseClass);
			ASSUME(ThisClass);

			return IsAWorkaround(ThisClass, SomeBaseClass);
		}
	#else
		bool IsA( const UClass* SomeBase ) const;
	#endif

	/**
	 * @return	true if this object is of the template type.
	 */
	template<class T>
	bool IsA() const
	{
		return IsA(T::StaticClass());
	}

	/**
	 * Finds the most-derived class which is a parent of both TestClass and this object's class.
	 *
	 * @param	TestClass	the class to find the common base for
	 */
	const UClass* FindNearestCommonBaseClass( const UClass* TestClass ) const;

	/**
	 * Returns a pointer to this object safely converted to a pointer of the specified interface class.
	 *
	 * @param	InterfaceClass	the interface class to use for the returned type
	 *
	 * @return	a pointer that can be assigned to a variable of the interface type specified, or NULL if this object's
	 *			class doesn't implement the interface indicated.  Will be the same value as 'this' if the interface class
	 *			isn't native.
	 */
	void* GetInterfaceAddress( UClass* InterfaceClass );

	/** 
	 *	Returns a pointer to the I* native interface object that this object implements.
	 *	Returns NULL if this object does not implement InterfaceClass, or does not do so natively.
	 */
	void* GetNativeInterfaceAddress(UClass* InterfaceClass);

	/** 
	 *	Returns a pointer to the const I* native interface object that this object implements.
	 *	Returns NULL if this object does not implement InterfaceClass, or does not do so natively.
	 */
	const void* GetNativeInterfaceAddress(UClass* InterfaceClass) const
	{
		return const_cast<UObjectBaseUtility*>(this)->GetNativeInterfaceAddress(InterfaceClass);
	}

	/**
	 * Returns whether this component was instanced from a component/subobject template, or if it is a component/subobject template.
	 * This is based on a name comparison with the outer class instance lookup table
	 *
	 * @return	true if this component was instanced from a template.  false if this component was created manually at runtime.
	 */
	bool IsDefaultSubobject() const;
	

	/***********************/
	/******** Linker ****  UObjectLinker.cpp */
	/***********************/

	/**
	 * Returns the linker for this object.
	 *
	 * @return	a pointer to the linker for this object, or NULL if this object has no linker
	 */
	class FLinkerLoad* GetLinker() const;
	/**
	 * Returns this object's LinkerIndex.
	 *
	 * @return	the index into my linker's ExportMap for the FObjectExport
	 *			corresponding to this object.
	 */
	int32 GetLinkerIndex() const;

	/**
	 * Returns the UE4 version of the linker for this object.
	 *
	 * @return	the UE4 version of the engine's package file when this object
	 *			was last saved, or GPackageFileUE4Version (current version) if
	 *			this object does not have a linker, which indicates that
	 *			a) this object is a native only class, or
	 *			b) this object's linker has been detached, in which case it is already fully loaded
	 */
	int32 GetLinkerUE4Version() const;

	/**
	 * Returns the licensee version of the linker for this object.
	 *
	 * @return	the licensee version of the engine's package file when this object
	 *			was last saved, or GPackageFileLicenseeVersion (current version) if
	 *			this object does not have a linker, which indicates that
	 *			a) this object is a native only class, or
	 *			b) this object's linker has been detached, in which case it is already fully loaded
	 */
	int32 GetLinkerLicenseeUE4Version() const;

	/**
	 * Returns the custom version of the linker for this object corresponding to the given custom version key.
	 *
	 * @return	the custom version of the engine's package file when this object
	 *			was last saved, or the current version if
	 *			this object does not have a linker, which indicates that
	 *			a) this object is a native only class, or
	 *			b) this object's linker has been detached, in which case it is already fully loaded
	 */
	int32 GetLinkerCustomVersion(FGuid CustomVersionKey) const;

	/** 
	 * Overloaded < operator. Compares objects by name.
	 *
	 * @return true if this object's name is lexicographically smaller than the other object's name
	 */
	FORCEINLINE bool operator<( const UObjectBaseUtility& Other ) const
	{
		return GetName() < Other.GetName();
	}
};

/**
 * Returns the name of this object (with no path information)
 * @param Object object to retrieve the name for; NULL gives "None"
 * @return Name of the object.
*/
FORCEINLINE FString GetNameSafe(const UObjectBaseUtility *Object)
{
	if( Object == NULL )
	{
		return TEXT("None");
	}
	else
	{
		return Object->GetName();
	}
}

/**
 * Returns the path name of this object
 * @param Object object to retrieve the path name for; NULL gives "None"
 * @return path name of the object.
*/
FORCEINLINE FString GetPathNameSafe(const UObjectBaseUtility *Object)
{
	if( Object == NULL )
	{
		return TEXT("None");
	}
	else
	{
		return Object->GetPathName();
	}
}

/**
 * Returns the full name of this object
 * @param Object object to retrieve the full name for; NULL (or a null class!) gives "None"
 * @return full name of the object.
*/
FORCEINLINE FString GetFullNameSafe(const UObjectBaseUtility *Object)
{
	if( !Object || !Object->GetClass())
	{
		return TEXT("None");
	}
	else
	{
		return Object->GetFullName();
	}
}

#if STATS
struct FScopeCycleCounterUObject : public FCycleCounter
{
	/**
	 * Constructor, starts timing
	 */
	FORCEINLINE_STATS FScopeCycleCounterUObject(const UObjectBaseUtility *Object)
	{
		if (Object)
		{
			TStatId StatId = Object->GetStatID();
			if (FThreadStats::IsCollectingData(StatId))
			{
				Start(StatId);
			}
		}
	}
	/**
	 * Constructor, starts timing with an alternate enable stat to use high performance disable for only SOME UObject stats
	 */
	FORCEINLINE_STATS FScopeCycleCounterUObject(const UObjectBaseUtility *Object, TStatId OtherStat)
	{
		if (FThreadStats::IsCollectingData(OtherStat) && Object)
		{
			TStatId StatId = Object->GetStatID();
			if (!StatId.IsNone())
			{
				Start(StatId);
			}
		}
	}
	/**
	 * Updates the stat with the time spent
	 */
	FORCEINLINE_STATS ~FScopeCycleCounterUObject()
	{
		Stop();
	}
};
#else
struct FScopeCycleCounterUObject
{
	FORCEINLINE_STATS FScopeCycleCounterUObject(const UObjectBaseUtility *Object)
	{
	}
	FORCEINLINE_STATS FScopeCycleCounterUObject(const UObjectBaseUtility *Object, TStatId OtherStat)
	{
	}
};
#endif

#if _MSC_VER == 1900
	#ifdef PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
		PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
	#endif
#endif