// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Class.h: UClass definition.
=============================================================================*/

#pragma once

#include "ObjectBase.h"
#include "Object.h"
#include "GarbageCollection.h"

/*-----------------------------------------------------------------------------
	Mirrors of mirror structures in Object.h. These are used by generated code 
	to facilitate correct offsets and alignments for structures containing these '
	odd types.
-----------------------------------------------------------------------------*/

COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogClass, Log, All);
COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogScriptSerialization, Log, All);

struct FPropertyTag;
struct FNetDeltaSerializeInfo;

/*-----------------------------------------------------------------------------
	FRepRecord.
-----------------------------------------------------------------------------*/

//
// Information about a property to replicate.
//
struct FRepRecord
{
	UProperty* Property;
	int32 Index;
	FRepRecord(UProperty* InProperty,int32 InIndex)
	: Property(InProperty), Index(InIndex)
	{}
};

/*-----------------------------------------------------------------------------
	UField.
-----------------------------------------------------------------------------*/

//
// Base class of reflection data objects.
//
class COREUOBJECT_API UField : public UObject
{
	DECLARE_CASTED_CLASS_INTRINSIC(UField, UObject, CLASS_Abstract, TEXT("/Script/CoreUObject"), CASTCLASS_UField)

	// Variables.
	UField*			Next;

	// Constructors.
	UField(EStaticConstructor, EObjectFlags InFlags);

	// UObject interface.
	virtual void Serialize( FArchive& Ar ) override;
	virtual void PostLoad() override;

	// UField interface.
	virtual void AddCppProperty( UProperty* Property );
	virtual void Bind();
	UClass* GetOwnerClass() const;
	UStruct* GetOwnerStruct() const;

#if WITH_EDITOR || HACK_HEADER_GENERATOR
	/**
	 * Finds the localized display name or native display name as a fallback.
	 *
	 * @return The display name for this object.
	 */
	FText GetDisplayNameText() const;

	/**
	 * Finds the localized tooltip or native tooltip as a fallback.
	 *
	 * @param bShortTooltip Look for a shorter version of the tooltip (falls back to the long tooltip if none was specified)
	 *
	 * @return The tooltip for this object.
	 */
	FText GetToolTipText(bool bShortTooltip = false) const;

	/**
	 * Determines if the property has any metadata associated with the key
	 * 
	 * @param Key The key to lookup in the metadata
	 * @return true if there is a (possibly blank) value associated with this key
	 */
	bool HasMetaData(const TCHAR* Key) const;

	/**
	 * Determines if the property has any metadata associated with the key
	 * 
	 * @param Key The key to lookup in the metadata
	 * @return true if there is a (possibly blank) value associated with this key
	 */
	bool HasMetaData(const FName& Key) const;

	/**
	 * Find the metadata value associated with the key
	 * 
	 * @param Key The key to lookup in the metadata
	 * @return The value associated with the key
	 */
	const FString& GetMetaData(const TCHAR* Key) const;

	/** Find the metadata value associated with the key */
	const FString& GetMetaData(const FName& Key) const;

	/**
	 * Find the metadata value associated with the key and localization namespace and key
	 *
	 * @param Key						The key to lookup in the metadata
	 * @param LocalizationNamespace		Namespace to lookup in the localization manager
	 * @param LocalizationKey			Key to lookup in the localization manager
	 * @return							Localized metadata if available, defaults to whatever is provided via GetMetaData
	 */
	const FText GetMetaDataText(const TCHAR* MetaDataKey, const FString LocalizationNamespace = FString(), const FString LocalizationKey = FString()) const;

	/**
	 * Find the metadata value associated with the key and localization namespace and key
	 *
	 * @param Key						The key to lookup in the metadata
	 * @param LocalizationNamespace		Namespace to lookup in the localization manager
	 * @param LocalizationKey			Key to lookup in the localization manager
	 * @return							Localized metadata if available, defaults to whatever is provided via GetMetaData
	 */
	const FText GetMetaDataText(const FName& MetaDataKey, const FString LocalizationNamespace = FString(), const FString LocalizationKey = FString()) const;

	/**
	 * Sets the metadata value associated with the key
	 * 
	 * @param Key The key to lookup in the metadata
	 * @return The value associated with the key
	 */
	void SetMetaData(const TCHAR* Key, const TCHAR* InValue);

	/** Sets the metadata value associated with the key */
	void SetMetaData(const FName& Key, const TCHAR* InValue);

	/**
	* Find the metadata value associated with the key
	* and return bool 
	* @param Key The key to lookup in the metadata
	* @return return true if the value was true (case insensitive)
	*/
	bool GetBoolMetaData(const TCHAR* Key) const
	{		
		const FString& BoolString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		return (BoolString == "true");
	}
	
	/**
	 * Find the metadata value associated with the key
	 * and return bool  
	 * @param Key The key to lookup in the metadata
	 * @return return true if the value was true (case insensitive)
	 */
	bool GetBoolMetaData(const FName& Key) const
	{		
		const FString& BoolString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		return (BoolString == "true");
	}

	/**
	* Find the metadata value associated with the key
	* and return int32 
	* @param Key The key to lookup in the metadata
	* @return the int value stored in the metadata.
	*/
	int32 GetINTMetaData(const TCHAR* Key) const
	{
		const FString& INTString = GetMetaData(Key);
		int32 Value = FCString::Atoi(*INTString);
		return Value;
	}

	/**
	* Find the metadata value associated with the key
	* and return int32 
	* @param Key The key to lookup in the metadata
	* @return the int value stored in the metadata.
	*/
	int32 GetINTMetaData(const FName& Key) const
	{
		const FString& INTString = GetMetaData(Key);
		int32 Value = FCString::Atoi(*INTString);
		return Value;
	}

	/**
	* Find the metadata value associated with the key
	* and return float
	* @param Key The key to lookup in the metadata
	* @return the float value stored in the metadata.
	*/
	float GetFLOATMetaData(const TCHAR* Key) const
	{
		const FString& FLOATString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		float Value = FCString::Atof(*FLOATString);
		return Value;
	}

	/**
	* Find the metadata value associated with the key
	* and return float
	* @param Key The key to lookup in the metadata
	* @return the float value stored in the metadata.
	*/
	float GetFLOATMetaData(const FName& Key) const
	{
		const FString& FLOATString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		float Value = FCString::Atof(*FLOATString);
		return Value;
	}

	UClass* GetClassMetaData(const TCHAR* Key) const;
	UClass* GetClassMetaData(const FName& Key) const;

	/** Clear any metadata associated with the key */
	void RemoveMetaData(const TCHAR* Key);

	/** Clear any metadata associated with the key */
	void RemoveMetaData(const FName& Key);
#endif
};

/*-----------------------------------------------------------------------------
	UStruct.
-----------------------------------------------------------------------------*/

/**
 * Base class for all UObject types that contain fields.
 */
class COREUOBJECT_API UStruct : public UField
{
	DECLARE_CASTED_CLASS_INTRINSIC(UStruct, UField, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UStruct)

	// Variables.
protected:
	friend COREUOBJECT_API UClass* Z_Construct_UClass_UStruct();
private:
	UStruct* SuperStruct;
public:
	UField* Children;
	int32 PropertiesSize;

	int32 MinAlignment;
	
	TArray<uint8> Script;

	/** In memory only: Linked list of properties from most-derived to base **/
	UProperty* PropertyLink;
	/** In memory only: Linked list of object reference properties from most-derived to base **/
	UProperty* RefLink;
	/** In memory only: Linked list of properties requiring destruction. Note this does not include things that will be destroyed byt he native destructor **/
	UProperty* DestructorLink;
	/** In memory only: Linked list of properties requiring post constructor initialization.**/
	UProperty* PostConstructLink;

	/** Array of object references embedded in script code. Mirrored for easy access by realtime garbage collection code */
	TArray<UObject*> ScriptObjectReferences;

	/** Map of Class Name to Map of Old Property Name to New Property Name */
	static TMap<FName,TMap<FName,FName> > TaggedPropertyRedirects;
	static void InitTaggedPropertyRedirectsMap();

public:
	// Constructors.
	UStruct( EStaticConstructor, int32 InSize, EObjectFlags InFlags );
	explicit UStruct(UStruct* InSuperStruct, SIZE_T ParamsSize = 0, SIZE_T Alignment = 0);
	explicit UStruct(const FObjectInitializer& ObjectInitializer, UStruct* InSuperStruct, SIZE_T ParamsSize = 0, SIZE_T Alignment = 0 );

	// UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void FinishDestroy() override;
	virtual void RegisterDependencies() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	// UField interface.
	virtual void AddCppProperty(UProperty* Property) override;

	UProperty* FindPropertyByName(FName InName) const;

	/**
	 * Creates new copies of components
	 * 
	 * @param	Data						pointer to the address of the subobject referenced by this UProperty
	 * @param	DefaultData					pointer to the address of the default value of the subbject referenced by this UProperty
	 * @param	DefaultStruct				the struct corresponding to the buffer pointed to by DefaultData
	 * @param	Owner						the object that contains the component currently located at Data
	 * @param	InstanceGraph				contains the mappings of instanced objects and components to their templates
	 */
	void InstanceSubobjectTemplates( void* Data, void const* DefaultData, UStruct* DefaultStruct, UObject* Owner, FObjectInstancingGraph* InstanceGraph );

	//
	virtual UStruct* GetInheritanceSuper() const {return GetSuperStruct();}

	//
	void StaticLink(bool bRelinkExistingProperties = false);

	//
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties);

	virtual void SerializeBin( FArchive& Ar, void* Data ) const;

	/**
	 * Serializes the class properties that reside in Data if they differ from the corresponding values in DefaultData
	 *
	 * @param	Ar				the archive to use for serialization
	 * @param	Data			pointer to the location of the beginning of the property data
	 * @param	DefaultData		pointer to the location of the beginning of the data that should be compared against
	 * @param	DefaultStruct	the struct corresponding to the block of memory located at DefaultData 
	 */
	void SerializeBinEx( FArchive& Ar, void* Data, void const* DefaultData, UStruct* DefaultStruct ) const;

	virtual void SerializeTaggedProperties( FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, uint8* Defaults, const UObject* BreakRecursionIfFullyLoad=NULL) const;

	/**
	 * Initialize a struct over uninitialized memory. This may be done by calling the native constructor or individually initializing properties
	 *
	 * @param	Dest		Pointer to memory to initialize
	 * @param	ArrayDim	Number of elements in the array
	 * @param	Stride		Stride of the array, If this default (0), then we will pull the size from the struct
	 */
	virtual void InitializeStruct(void* Dest, int32 ArrayDim = 1) const;
	/**
	 * Destroy a struct in memory. This may be done by calling the native destructor and then the constructor or individually reinitializing properties
	 *
	 * @param	Dest		Pointer to memory to destory
	 * @param	ArrayDim	Number of elements in the array
	 * @param	Stride		Stride of the array. If this default (0), then we will pull the size from the struct
	 */
	virtual void DestroyStruct(void* Dest, int32 ArrayDim = 1) const;

#if WITH_EDITOR
private:
	virtual UProperty* CustomFindProperty(const FName InName) const { return NULL; };
#endif // WITH_EDITOR
public:
	virtual EExprToken SerializeExpr(int32& iCode, FArchive& Ar);
	virtual void TagSubobjects(EObjectFlags NewFlags) override;

	/**
	 * Returns the struct/ class prefix used for the C++ declaration of this struct/ class.
	 *
	 * @return Prefix character used for C++ declaration of this struct/ class.
	 */
	virtual const TCHAR* GetPrefixCPP() const { return TEXT("F"); }

	FORCEINLINE int32 GetPropertiesSize() const
	{
		return PropertiesSize;
	}

	FORCEINLINE int32 GetMinAlignment() const
	{
		return MinAlignment;
	}

	FORCEINLINE int32 GetStructureSize() const
	{
		return Align(PropertiesSize,MinAlignment);
	}

	void SetPropertiesSize( int32 NewSize )
	{
		PropertiesSize = NewSize;
	}

	template<class T>
	bool IsChildOf() const
	{
		return IsChildOf(T::StaticClass());
	}

	bool IsChildOf( const UStruct* SomeBase ) const
	{
		for (const UStruct* Struct = this; Struct; Struct = Struct->GetSuperStruct())
		{
			if (Struct == SomeBase)
				return true;
		}

		return false;
	}

	UStruct* GetSuperStruct() const
	{
		return SuperStruct;
	}

	/**
	 * Sets the super struct pointer and updates hash information as necessary.
	 * Note that this is not sufficient to actually reparent a struct, it simply sets a pointer.
	 */
	virtual void SetSuperStruct(UStruct* NewSuperStruct);

	/**
	 * Serializes the SuperStruct pointer.
	 */
	virtual void SerializeSuperStruct(FArchive& Ar);

	void LinkChild(UField* Child)
	{
		Child->Next = Children;
		Children = Child;
	}

	virtual FString PropertyNameToDisplayName(FName InName) const 
	{ 
		return InName.ToString();
	}

#if WITH_EDITOR
	/** Try and find boolean metadata with the given key. If not found on this class, work up hierarchy looking for it. */
	bool GetBoolMetaDataHierarchical(const FName& Key) const;

	/** Try and find string metadata with the given key. If not found on this class, work up hierarchy looking for it. */
	bool GetStringMetaDataHierarchical(const FName& Key, FString* OutValue = nullptr) const;
#endif

#if HACK_HEADER_GENERATOR
	// Required by UHT makefiles for internal data serialization.
	friend struct FStructArchiveProxy;
#endif // HACK_HEADER_GENERATOR

protected:

	/** Returns the property name from the guid */
	virtual FName FindPropertyNameFromGuid(const FGuid& PropertyGuid) const { return NAME_None; }

	/** Find property guid */
	virtual FGuid FindPropertyGuidFromName(const FName InName) const { return FGuid(); }

	/** Returns if we have access to property guids */
	virtual bool ArePropertyGuidsAvailable() const { return false; }

};

enum EStructFlags
{
	// State flags.
	STRUCT_NoFlags				= 0x00000000,	
	STRUCT_Native				= 0x00000001,

	/** If set, this struct will be compared using native code */
	STRUCT_IdenticalNative		= 0x00000002,
	
	STRUCT_HasInstancedReference= 0x00000004,

	STRUCT_NoExport				= 0x00000008,

	/** Indicates that this struct should always be serialized as a single unit */
	STRUCT_Atomic				= 0x00000010,

	/** Indicates that this struct uses binary serialization; it is unsafe to add/remove members from this struct without incrementing the package version */
	STRUCT_Immutable			= 0x00000020,

	/** If set, native code needs to be run to find referenced objects */
	STRUCT_AddStructReferencedObjects = 0x00000040,

	/** Indicates that this struct should be exportable/importable at the DLL layer.  Base structs must also be exportable for this to work. */
	STRUCT_RequiredAPI			= 0x00000200,	

	/** If set, this struct will be serialized using the CPP net serializer */
	STRUCT_NetSerializeNative	= 0x00000400,	

	/** If set, this struct will be serialized using the CPP serializer */
	STRUCT_SerializeNative		= 0x00000800,	

	/** If set, this struct will be copied using the CPP operator= */
	STRUCT_CopyNative			= 0x00001000,	

	/** If set, this struct will be copied using memcpy */
	STRUCT_IsPlainOldData		= 0x00002000,	

	/** If set, this struct has no destructor and non will be called. STRUCT_IsPlainOldData implies STRUCT_NoDestructor */
	STRUCT_NoDestructor			= 0x00004000,	

	/** If set, this struct will not be constructed because it is assumed that memory is zero before construction. */
	STRUCT_ZeroConstructor		= 0x00008000,	

	/** If set, native code will be used to export text */
	STRUCT_ExportTextItemNative	= 0x00010000,	

	/** If set, native code will be used to export text */
	STRUCT_ImportTextItemNative	= 0x00020000,	

	/** If set, this struct will have PostSerialize called on it after CPP serializer or tagged property serialization is complete */
	STRUCT_PostSerializeNative  = 0x00040000,

	/** If set, this struct will have SerializeFromMismatchedTag called on it if a mismatched tag is encountered. */
	STRUCT_SerializeFromMismatchedTag = 0x00080000,

	/** If set, this struct will be serialized using the CPP net delta serializer */
	STRUCT_NetDeltaSerializeNative = 0x00100000,

	/** Struct flags that are automatically inherited */
	STRUCT_Inherit				= STRUCT_HasInstancedReference|STRUCT_Atomic,

	/** Flags that are always computed, never loaded or done with code generation */
	STRUCT_ComputedFlags		= STRUCT_NetDeltaSerializeNative | STRUCT_NetSerializeNative | STRUCT_SerializeNative | STRUCT_PostSerializeNative | STRUCT_CopyNative | STRUCT_IsPlainOldData | STRUCT_NoDestructor | STRUCT_ZeroConstructor | STRUCT_IdenticalNative | STRUCT_AddStructReferencedObjects | STRUCT_ExportTextItemNative | STRUCT_ImportTextItemNative | STRUCT_SerializeFromMismatchedTag
};


/** type traits to cover the custom aspects of a script struct **/
struct TStructOpsTypeTraitsBase
{
	enum
	{
		WithZeroConstructor            = false, // struct can be constructed as a valid object by filling its memory footprint with zeroes.
		WithNoInitConstructor          = false, // struct has a constructor which takes an EForceInit parameter which will force the constructor to perform initialization, where the default constructor performs 'uninitialization'.
		WithNoDestructor               = false, // struct will not have its destructor called when it is destroyed.
		WithCopy                       = false, // struct can be copied via its copy assignment operator.
		WithIdenticalViaEquality       = false, // struct can be compared via its operator==.  This should be mutually exclusive with WithIdentical.
		WithIdentical                  = false, // struct can be compared via an Identical(const T* Other, uint32 PortFlags) function.  This should be mutually exclusive with WithIdenticalViaEquality.
		WithExportTextItem             = false, // struct has an ExportTextItem function used to serialize its state into a string.
		WithImportTextItem             = false, // struct has an ImportTextItem function used to deserialize a string into an object of that class.
		WithAddStructReferencedObjects = false, // struct has an AddStructReferencedObjects function which allows it to add references to the garbage collector.
		WithSerializer                 = false, // struct has a Serialize function for serializing its state to an FArchive.
		WithPostSerialize              = false, // struct has a PostSerialize function which is called after it is serialized
		WithNetSerializer              = false, // struct has a NetSerialize function for serializing its state to an FArchive used for network replication.
		WithNetDeltaSerializer         = false, // struct has a NetDeltaSerialize function for serializing differences in state from a previous NetSerialize operation.
		WithSerializeFromMismatchedTag = false, // struct has a SerializeFromMismatchedTag function for converting from other property tags.
	};
};

template<class CPPSTRUCT>
struct TStructOpsTypeTraits : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = !TIsPODType<CPPSTRUCT>::Value
	};
};


/**
 * Selection of constructor behavior.
 */
template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<!TStructOpsTypeTraits<CPPSTRUCT>::WithNoInitConstructor>::Type ConstructWithNoInitOrNot(void *Data)
{
	new (Data) CPPSTRUCT();
}

template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<TStructOpsTypeTraits<CPPSTRUCT>::WithNoInitConstructor>::Type ConstructWithNoInitOrNot(void *Data)
{
	new (Data) CPPSTRUCT(ForceInit);
}


/**
 * Selection of Serialize call.
 */
template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<!TStructOpsTypeTraits<CPPSTRUCT>::WithSerializer, bool>::Type SerializeOrNot(FArchive& Ar, CPPSTRUCT *Data)
{
	return false;
}

template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<TStructOpsTypeTraits<CPPSTRUCT>::WithSerializer, bool>::Type SerializeOrNot(FArchive& Ar, CPPSTRUCT *Data)
{
	return Data->Serialize(Ar);
}


/**
 * Selection of PostSerialize call.
 */
template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<!TStructOpsTypeTraits<CPPSTRUCT>::WithPostSerialize>::Type PostSerializeOrNot(const FArchive& Ar, CPPSTRUCT *Data)
{
}

template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<TStructOpsTypeTraits<CPPSTRUCT>::WithPostSerialize>::Type PostSerializeOrNot(const FArchive& Ar, CPPSTRUCT *Data)
{
	Data->PostSerialize(Ar);
}


/**
 * Selection of NetSerialize call.
 */
template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<!TStructOpsTypeTraits<CPPSTRUCT>::WithNetSerializer, bool>::Type NetSerializeOrNot(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess, CPPSTRUCT *Data)
{
	return false;
}

template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<TStructOpsTypeTraits<CPPSTRUCT>::WithNetSerializer, bool>::Type NetSerializeOrNot(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess, CPPSTRUCT *Data)
{
	return Data->NetSerialize(Ar, Map, bOutSuccess);
}


/**
 * Selection of NetDeltaSerialize call.
 */
template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<!TStructOpsTypeTraits<CPPSTRUCT>::WithNetDeltaSerializer, bool>::Type NetDeltaSerializeOrNot(FNetDeltaSerializeInfo & DeltaParms, CPPSTRUCT *Data)
{
	return false;
}

template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<TStructOpsTypeTraits<CPPSTRUCT>::WithNetDeltaSerializer, bool>::Type NetDeltaSerializeOrNot(FNetDeltaSerializeInfo & DeltaParms, CPPSTRUCT *Data)
{
	return Data->NetDeltaSerialize(DeltaParms);
}


/**
 * Selection of Copy behavior.
 */
template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<!TStructOpsTypeTraits<CPPSTRUCT>::WithCopy, bool>::Type CopyOrNot(CPPSTRUCT* Dest, CPPSTRUCT const* Src, int32 ArrayDim)
{
	return false;
}

template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<TStructOpsTypeTraits<CPPSTRUCT>::WithCopy, bool>::Type CopyOrNot(CPPSTRUCT* Dest, CPPSTRUCT const* Src, int32 ArrayDim)
{
	static_assert((!TIsPODType<CPPSTRUCT>::Value), "You probably don't want custom copy for a POD type.");
	for (;ArrayDim;--ArrayDim)
	{
		*Dest++ = *Src++;
	}
	return true;
}


/**
 * Selection of AddStructReferencedObjects check.
 */
template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<!TStructOpsTypeTraits<CPPSTRUCT>::WithAddStructReferencedObjects>::Type AddStructReferencedObjectsOrNot(const void* A, FReferenceCollector& Collector)
{
}

template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<TStructOpsTypeTraits<CPPSTRUCT>::WithAddStructReferencedObjects>::Type AddStructReferencedObjectsOrNot(const void* A, FReferenceCollector& Collector)
{
	((CPPSTRUCT const*)A)->AddStructReferencedObjects(Collector);
}


/**
 * Selection of Identical check.
 */
template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<TStructOpsTypeTraits<CPPSTRUCT>::WithIdentical && TStructOpsTypeTraits<CPPSTRUCT>::WithIdenticalViaEquality, bool>::Type IdenticalOrNot(const CPPSTRUCT* A, const CPPSTRUCT* B, uint32 PortFlags, bool& bOutResult)
{
	static_assert(sizeof(CPPSTRUCT) == 0, "Should not have both WithIdenticalViaEquality and WithIdentical.");
}

template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<!TStructOpsTypeTraits<CPPSTRUCT>::WithIdentical && !TStructOpsTypeTraits<CPPSTRUCT>::WithIdenticalViaEquality, bool>::Type IdenticalOrNot(const CPPSTRUCT* A, const CPPSTRUCT* B, uint32 PortFlags, bool& bOutResult)
{
	bOutResult = false;
	return false;
}

template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<TStructOpsTypeTraits<CPPSTRUCT>::WithIdentical && !TStructOpsTypeTraits<CPPSTRUCT>::WithIdenticalViaEquality, bool>::Type IdenticalOrNot(const CPPSTRUCT* A, const CPPSTRUCT* B, uint32 PortFlags, bool& bOutResult)
{
	bOutResult = A->Identical(B, PortFlags);
	return true;
}

template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<!TStructOpsTypeTraits<CPPSTRUCT>::WithIdentical && TStructOpsTypeTraits<CPPSTRUCT>::WithIdenticalViaEquality, bool>::Type IdenticalOrNot(const CPPSTRUCT* A, const CPPSTRUCT* B, uint32 PortFlags, bool& bOutResult)
{
	bOutResult = (*A == *B);
	return true;
}


/**
 * Selection of ExportTextItem call.
 */
template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<!TStructOpsTypeTraits<CPPSTRUCT>::WithExportTextItem, bool>::Type ExportTextItemOrNot(FString& ValueStr, const CPPSTRUCT* PropertyValue, const CPPSTRUCT* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope)
{
	return false;
}

template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<TStructOpsTypeTraits<CPPSTRUCT>::WithExportTextItem, bool>::Type ExportTextItemOrNot(FString& ValueStr, const CPPSTRUCT* PropertyValue, const CPPSTRUCT* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope)
{
	return PropertyValue->ExportTextItem(ValueStr, *DefaultValue, Parent, PortFlags, ExportRootScope);
}


/**
 * Selection of ImportTextItem call.
 */
template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<!TStructOpsTypeTraits<CPPSTRUCT>::WithImportTextItem, bool>::Type ImportTextItemOrNot(const TCHAR*& Buffer, CPPSTRUCT* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText)
{
	return false;
}

template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<TStructOpsTypeTraits<CPPSTRUCT>::WithImportTextItem, bool>::Type ImportTextItemOrNot(const TCHAR*& Buffer, CPPSTRUCT* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText)
{
	return Data->ImportTextItem(Buffer, PortFlags, OwnerObject, ErrorText);
}


/**
 * Selection of SerializeFromMismatchedTag call.
 */
template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<!TStructOpsTypeTraits<CPPSTRUCT>::WithSerializeFromMismatchedTag, bool>::Type SerializeFromMismatchedTagOrNot(FPropertyTag const& Tag, FArchive& Ar, CPPSTRUCT *Data)
{
	return false;
}

template<class CPPSTRUCT>
FORCEINLINE typename TEnableIf<TStructOpsTypeTraits<CPPSTRUCT>::WithSerializeFromMismatchedTag, bool>::Type SerializeFromMismatchedTagOrNot(FPropertyTag const& Tag, FArchive& Ar, CPPSTRUCT *Data)
{
	return Data->SerializeFromMismatchedTag(Tag, Ar);
}


/**
 * Reflection data for a structure.
 */
class UScriptStruct : public UStruct
{
public:
	/** Interface to template to manage dynamic access to C++ struct construction and destruction **/
	struct COREUOBJECT_API ICppStructOps
	{
		/**
		 * Constructor
		 * @param InSize: sizeof() of the structure
		**/
		ICppStructOps(int32 InSize, int32 InAlignment)
			: Size(InSize)
			, Alignment(InAlignment)
		{
		}
		virtual ~ICppStructOps() {}
		/** return true if this class has a no-op constructor and takes EForceInit to init **/
		virtual bool HasNoopConstructor() = 0;
		/** return true if memset can be used instead of the constructor **/
		virtual bool HasZeroConstructor() = 0;
		/** Call the C++ constructor **/
		virtual void Construct(void *Dest)=0;
		/** return false if this destructor can be skipped **/
		virtual bool HasDestructor() = 0;
		/** Call the C++ destructor **/
		virtual void Destruct(void *Dest) = 0;
		/** return the sizeof() of this structure **/
		FORCEINLINE int32 GetSize()
		{
			return Size;
		}
		/** return the ALIGNOF() of this structure **/
		FORCEINLINE int32 GetAlignment()
		{
			return Alignment;
		}

		/** return true if this class can serialize **/
		virtual bool HasSerializer() = 0;
		/** 
		 * Serialize this structure 
		 * @return true if the package is new enough to support this, if false, it will fall back to ordinary script struct serialization
		**/
		virtual bool Serialize(FArchive& Ar, void *Data) = 0;

		/** return true if this class implements a post serialize call **/
		virtual bool HasPostSerialize() = 0;
		/** 
		 * Call PostLoad on this structure
		**/
		virtual void PostSerialize(const FArchive& Ar, void *Data) = 0;

		/** return true if this struct can net serialize **/
		virtual bool HasNetSerializer() = 0;
		/** 
		 * Net serialize this structure 
		 * @return true if the struct was serialized, otherwise it will fall back to ordinary script struct net serialization
		**/
		virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess, void *Data) = 0;

		/** return true if this struct can net delta serialize delta (serialize a network delta from a base state) **/
		virtual bool HasNetDeltaSerializer() = 0;
		/** 
		 * Net serialize delta this structure. Serialize a network delta from a base state
		 * @return true if the struct was serialized, otherwise it will fall back to ordinary script struct net delta serialization
		**/
		virtual bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms, void *Data) = 0;

		/** return true if this struct should be memcopied **/
		virtual bool IsPlainOldData() = 0;

		/** return true if this struct can copy **/
		virtual bool HasCopy() = 0;
		/** 
		 * Copy this structure 
		 * @return true if the copy was handled, otherwise it will fall back to CopySingleValue
		**/
		virtual bool Copy(void* Dest, void const* Src, int32 ArrayDim) = 0;

		/** return true if this struct can compare **/
		virtual bool HasIdentical() = 0;
		/** 
		 * Compare this structure 
		 * @return true if the copy was handled, otherwise it will fall back to UStructProperty::Identical
		**/
		virtual bool Identical(const void* A, const void* B, uint32 PortFlags, bool& bOutResult) = 0;

		/** return true if this struct can export **/
		virtual bool HasExportTextItem() = 0;
		/** 
		 * export this structure 
		 * @return true if the copy was exported, otherwise it will fall back to UStructProperty::ExportTextItem
		**/
		virtual bool ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, class UObject* Parent, int32 PortFlags, class UObject* ExportRootScope) = 0;

		/** return true if this struct can import **/
		virtual bool HasImportTextItem() = 0;
		/** 
		 * import this structure 
		 * @return true if the copy was imported, otherwise it will fall back to UStructProperty::ImportText
		**/
		virtual bool ImportTextItem(const TCHAR*& Buffer, void* Data, int32 PortFlags, class UObject* OwnerObject, FOutputDevice* ErrorText) = 0;

		/** return true if this struct has custom GC code **/
		virtual bool HasAddStructReferencedObjects() = 0;
		/** 
		 * return a pointer to a function that can add referenced objects
		 * @return true if the copy was imported, otherwise it will fall back to UStructProperty::ImportText
		**/
		typedef void (*TPointerToAddStructReferencedObjects)(const void* A, class FReferenceCollector& Collector);
		virtual TPointerToAddStructReferencedObjects AddStructReferencedObjects() = 0;

		/** return true if this class wants to serialize from some other tag (usually for conversion purposes) **/
		virtual bool HasSerializeFromMismatchedTag() = 0;
		/** 
		 * Serialize this structure, from some other tag
		 * @return true if this succeeded, false will trigger a warning and not serialize at all
		**/
		virtual bool SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar, void *Data) = 0;

	private:
		/** sizeof() of the structure **/
		const int32 Size;
		/** ALIGNOF() of the structure **/
		const int32 Alignment;
	};



	/** Template to manage dynamic access to C++ struct construction and destruction **/
	template<class CPPSTRUCT>
	struct TCppStructOps : public ICppStructOps
	{
		typedef TStructOpsTypeTraits<CPPSTRUCT> TTraits;
		TCppStructOps()
			: ICppStructOps(sizeof(CPPSTRUCT), ALIGNOF(CPPSTRUCT))
		{
		}
		virtual bool HasNoopConstructor() override
		{
			return TTraits::WithNoInitConstructor;
		}		
		virtual bool HasZeroConstructor() override
		{
			return TTraits::WithZeroConstructor;
		}
		virtual void Construct(void *Dest) override
		{
			check(!TTraits::WithZeroConstructor); // don't call this if we have indicated it is not necessary
			// that could have been an if statement, but we might as well force optimization above the virtual call
			// could also not attempt to call the constructor for types where this is not possible, but I didn't do that here
			ConstructWithNoInitOrNot<CPPSTRUCT>(Dest);
		}
		virtual bool HasDestructor() override
		{
			return !(TTraits::WithNoDestructor || TIsPODType<CPPSTRUCT>::Value);
		}
		virtual void Destruct(void *Dest) override
		{
			check(!(TTraits::WithNoDestructor || TIsPODType<CPPSTRUCT>::Value)); // don't call this if we have indicated it is not necessary
			// that could have been an if statement, but we might as well force optimization above the virtual call
			// could also not attempt to call the destructor for types where this is not possible, but I didn't do that here
			((CPPSTRUCT*)Dest)->~CPPSTRUCT();
		}
		virtual bool HasSerializer() override
		{
			return TTraits::WithSerializer;
		}
		virtual bool Serialize(FArchive& Ar, void *Data) override
		{
			check(TTraits::WithSerializer); // don't call this if we have indicated it is not necessary
			return SerializeOrNot(Ar, (CPPSTRUCT*)Data);
		}
		virtual bool HasPostSerialize() override
		{
			return TTraits::WithPostSerialize;
		}
		virtual void PostSerialize(const FArchive& Ar, void *Data) override
		{
			check(TTraits::WithPostSerialize); // don't call this if we have indicated it is not necessary
			PostSerializeOrNot(Ar, (CPPSTRUCT*)Data);
		}
		virtual bool HasNetSerializer() override
		{
			return TTraits::WithNetSerializer;
		}
		virtual bool HasNetDeltaSerializer() override
		{
			return TTraits::WithNetDeltaSerializer;
		}
		virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess, void *Data) override
		{
			return NetSerializeOrNot(Ar, Map, bOutSuccess, (CPPSTRUCT*)Data);
		}
		virtual bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms, void *Data) override
		{
			return NetDeltaSerializeOrNot(DeltaParms, (CPPSTRUCT*)Data);
		}
		virtual bool IsPlainOldData() override
		{
			return TIsPODType<CPPSTRUCT>::Value;
		}
		virtual bool HasCopy() override
		{
			return TTraits::WithCopy;
		}
		virtual bool Copy(void* Dest, void const* Src, int32 ArrayDim) override
		{
			return CopyOrNot((CPPSTRUCT*)Dest, (CPPSTRUCT const*)Src, ArrayDim);
		}
		virtual bool HasIdentical() override
		{
			return TTraits::WithIdentical || TTraits::WithIdenticalViaEquality;
		}
		virtual bool Identical(const void* A, const void* B, uint32 PortFlags, bool& bOutResult) override
		{
			check((TTraits::WithIdentical || TTraits::WithIdenticalViaEquality)); // don't call this if we have indicated it is not necessary
			return IdenticalOrNot((const CPPSTRUCT*)A, (const CPPSTRUCT*)B, PortFlags, bOutResult);
		}
		virtual bool HasExportTextItem() override
		{
			return TTraits::WithExportTextItem;
		}
		virtual bool ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, class UObject* Parent, int32 PortFlags, class UObject* ExportRootScope) override
		{
			check(TTraits::WithExportTextItem); // don't call this if we have indicated it is not necessary
			return ExportTextItemOrNot(ValueStr, (const CPPSTRUCT*)PropertyValue, (const CPPSTRUCT*)DefaultValue, Parent, PortFlags, ExportRootScope);
		}
		virtual bool HasImportTextItem() override
		{
			return TTraits::WithImportTextItem;
		}
		virtual bool ImportTextItem(const TCHAR*& Buffer, void* Data, int32 PortFlags, class UObject* OwnerObject, FOutputDevice* ErrorText) override
		{
			check(TTraits::WithImportTextItem); // don't call this if we have indicated it is not necessary
			return ImportTextItemOrNot(Buffer, (CPPSTRUCT*)Data, PortFlags, OwnerObject, ErrorText);
		}
		virtual bool HasAddStructReferencedObjects() override
		{
			return TTraits::WithAddStructReferencedObjects;
		}
		virtual TPointerToAddStructReferencedObjects AddStructReferencedObjects() override
		{
			check(TTraits::WithAddStructReferencedObjects); // don't call this if we have indicated it is not necessary
			return &AddStructReferencedObjectsOrNot<CPPSTRUCT>;
		}
		virtual bool HasSerializeFromMismatchedTag() override
		{
			return TTraits::WithSerializeFromMismatchedTag;
		}
		virtual bool SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar, void *Data) override
		{
			check(TTraits::WithSerializeFromMismatchedTag); // don't call this if we have indicated it is not allowed
			return SerializeFromMismatchedTagOrNot(Tag, Ar, (CPPSTRUCT*)Data);
		}
	};

	/** Template for noexport classes to autoregister before main starts **/
	template<class CPPSTRUCT>
	struct TAutoCppStructOps
	{
		TAutoCppStructOps(FName InName)
		{
			DeferCppStructOps(InName,new TCppStructOps<CPPSTRUCT>);
		}
	};
	#define IMPLEMENT_STRUCT(BaseName) \
		static UScriptStruct::TAutoCppStructOps<F##BaseName> BaseName##_Ops(TEXT(#BaseName)); 

	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(UScriptStruct, UStruct, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UScriptStruct, COREUOBJECT_API)

	COREUOBJECT_API UScriptStruct( EStaticConstructor, int32 InSize, EObjectFlags InFlags );
	COREUOBJECT_API explicit UScriptStruct(const FObjectInitializer& ObjectInitializer, UScriptStruct* InSuperStruct, ICppStructOps* InCppStructOps = NULL, EStructFlags InStructFlags = STRUCT_NoFlags, SIZE_T ExplicitSize = 0, SIZE_T ExplicitAlignment = 0);
	COREUOBJECT_API explicit UScriptStruct(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	EStructFlags StructFlags;


#if HACK_HEADER_GENERATOR
	int32 StructMacroDeclaredLineNumber;

	// Required by UHT makefiles for internal data serialization.
	friend struct FScriptStructArchiveProxy;
#endif

private:
	/** true if these cpp ops are not for me, but rather this is an incomplete cpp ops from my base class. **/
	bool bCppStructOpsFromBaseClass;
	/** true if we have performed PrepareCppStructOps **/
	bool bPrepareCppStructOpsCompleted;
	/** Holds the Cpp ctors and dtors, sizeof, etc. Is not owned by this and is not released. **/
	ICppStructOps* CppStructOps;
public:

	// UObject Interface
	virtual COREUOBJECT_API void Serialize( FArchive& Ar ) override;
	virtual COREUOBJECT_API void PostLoad() override;

	// UStruct interface.
	virtual COREUOBJECT_API void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	virtual COREUOBJECT_API void InitializeStruct(void* Dest, int32 ArrayDim = 1) const override;
	virtual COREUOBJECT_API void DestroyStruct(void* Dest, int32 ArrayDim = 1) const override;
	// End of UStruct interface.

	/** Stash a CppStructOps for future use 
	 * @param Target Name of the struct 
	 * @param InCppStructOps Cpp ops for this struct
	**/
	static COREUOBJECT_API void DeferCppStructOps(FName Target, ICppStructOps* InCppStructOps);

	/** Look for the CppStructOps and hook it up **/
	COREUOBJECT_API void PrepareCppStructOps();

	FORCEINLINE ICppStructOps* GetCppStructOps() const
	{
		check(bPrepareCppStructOpsCompleted);
		return CppStructOps;
	}

	/** return true if these cpp ops are not for me, but rather this is an incomplete cpp ops from my base class **/
	FORCEINLINE bool InheritedCppStructOps() const
	{
		check(bPrepareCppStructOpsCompleted);
		return bCppStructOpsFromBaseClass;
	}

	void ClearCppStructOps()
	{
		StructFlags = EStructFlags(StructFlags & ~STRUCT_ComputedFlags);
		bPrepareCppStructOpsCompleted = false;
		bCppStructOpsFromBaseClass = false;
		CppStructOps = NULL;
	}
	/** 
	 * If it is native, it is assumed to have defaults because it has a constructor
	 * @return true if this struct has defaults
	**/
	FORCEINLINE bool HasDefaults() const
	{
		return !!GetCppStructOps();
	}

	/**
	 * Returns whether this struct should be serialized atomically.
	 *
	 * @param	Ar	Archive the struct is going to be serialized with later on
	 */
	bool ShouldSerializeAtomically(FArchive& Ar) const
	{
		if( (StructFlags&STRUCT_Atomic) != 0)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	/** Returns true if this struct has a native serialize function */
	bool UseNativeSerialization() const
	{
		if ((StructFlags&STRUCT_SerializeNative) != 0)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	/** Returns true if this struct should be binary serialized for the given archive */
	COREUOBJECT_API bool UseBinarySerialization(const FArchive& Ar) const;

	/** 
	 * Serializes a specific instance of a struct 
	 *
	 * @param	Ar			Archive we are serializing to/from
	 * @param	Value		Pointer to memory of struct
	 * @param	Defaults	Default value for this struct, pass nullptr to not use defaults 
	 */
	COREUOBJECT_API void SerializeItem(FArchive& Ar, void* Value, void const* Defaults);

	/**
	 * Compare two script structs
	 *
	 * @param	Dest		Pointer to memory to a struct
	 * @param	Src			Pointer to memory to the other struct
	 * @param	PortFlags	Comparison flags
	 * @return true if the structs are identical
	 */
	COREUOBJECT_API bool CompareScriptStruct(const void* A, const void* B, uint32 PortFlags) const;

	/**
	 * Copy a struct over an existing struct
	 *
	 * @param	Dest		Pointer to memory to initialize
	 * @param	Src			Pointer to memory to copy from
	 * @param	ArrayDim	Number of elements in the array
	 * @param	Stride		Stride of the array, If this default (0), then we will pull the size from the struct
	 */
	COREUOBJECT_API void CopyScriptStruct(void* Dest, void const* Src, int32 ArrayDim = 1) const;
	/**
	 * Reinitialize a struct in memory. This may be done by calling the native destructor and then the constructor or individually reinitializing properties
	 *
	 * @param	Dest		Pointer to memory to reinitialize
	 * @param	ArrayDim	Number of elements in the array
	 * @param	Stride		Stride of the array, only relevant if there more than one element. If this default (0), then we will pull the size from the struct
	 */
	void ClearScriptStruct(void* Dest, int32 ArrayDim = 1) const;

	virtual COREUOBJECT_API void RecursivelyPreload();

	virtual COREUOBJECT_API FGuid GetCustomGuid() const;

#if WITH_EDITOR
	/**
	* Initializes this structure to its default values
	* @param InStructData		The memory location to initialize
	*/
	virtual COREUOBJECT_API void InitializeDefaultValue(uint8* InStructData) const;
#endif // WITH_EDITOR
};

/*-----------------------------------------------------------------------------
	UFunction.
-----------------------------------------------------------------------------*/

//
// Reflection data for a replicated or Kismet callable function.
//
class COREUOBJECT_API UFunction : public UStruct
{
	DECLARE_CASTED_CLASS_INTRINSIC(UFunction, UStruct, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UFunction)
	DECLARE_WITHIN(UClass)
public:
	// Persistent variables.
	uint32 FunctionFlags;
	uint16 RepOffset;

	// Variables in memory only.
	uint8 NumParms;
	uint16 ParmsSize;
	uint16 ReturnValueOffset;
	/** Id of this RPC function call (must be FUNC_Net & (FUNC_NetService|FUNC_NetResponse)) */
	uint16 RPCId;
	/** Id of the corresponding response call (must be FUNC_Net & FUNC_NetService) */
	uint16 RPCResponseId;

	/** pointer to first local struct property in this UFunction that contains defaults */
	UProperty* FirstPropertyToInit;

#if UE_BLUEPRINT_EVENTGRAPH_FASTCALLS
	// The event graph this function calls in to (persistent)
	UFunction* EventGraphFunction;

	// The state offset inside of the event graph (persistent)
	int32 EventGraphCallOffset;
#endif

private:
	Native Func;

public:
	/**
	 * Returns the native func pointer.
	 *
	 * @return The native function pointer.
	 */
	FORCEINLINE Native GetNativeFunc() const
	{
		return Func;
	}

	/**
	 * Sets the native func pointer.
	 *
	 * @param InFunc - The new function pointer.
	 */
	FORCEINLINE void SetNativeFunc(Native InFunc)
	{
		Func = InFunc;
	}

	/**
	 * Invokes the UFunction on a UObject.
	 *
	 * @param Obj    - The object to invoke the function on.
	 * @param Stack  - The parameter stack for the function call.
	 * @param Result - The result of the function.
	 */
	void Invoke(UObject* Obj, FFrame& Stack, RESULT_DECL);

	// Constructors.
	explicit UFunction(const FObjectInitializer& ObjectInitializer, UFunction* InSuperFunction, uint32 InFunctionFlags = 0, uint16 InRepOffset = 0, SIZE_T ParamsSize = 0 );
	explicit UFunction(UFunction* InSuperFunction, uint32 InFunctionFlags = 0, uint16 InRepOffset = 0, SIZE_T ParamsSize = 0);

	void InitializeDerivedMembers();

	// UObject interface.
	virtual void Serialize( FArchive& Ar ) override;

	// UField interface.
	virtual void Bind() override;

	// UStruct interface.
	virtual UStruct* GetInheritanceSuper() const override { return NULL;}
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;

	// UFunction interface.
	UFunction* GetSuperFunction() const;

	UProperty* GetReturnProperty() const;

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagToCheck		Class flag to check for
	 *
	 * @return	true if the passed in flag is set, false otherwise
	 *			(including no flag passed in, unless the FlagsToCheck is CLASS_AllFlags)
	 */
	FORCEINLINE bool HasAnyFunctionFlags( uint32 FlagsToCheck ) const
	{
		return (FunctionFlags&FlagsToCheck) != 0 || FlagsToCheck == FUNC_AllFlags;
	}

	/**
	 * Used to safely check whether all of the passed in flags are set.
	 *
	 * @param FlagsToCheck	Function flags to check for
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	FORCEINLINE bool HasAllFunctionFlags( uint32 FlagsToCheck ) const
	{
		return ((FunctionFlags & FlagsToCheck) == FlagsToCheck);
	}

	/**
	 * Returns the flags that are ignored by default when comparing function signatures.
	 */
	FORCEINLINE static uint64 GetDefaultIgnoredSignatureCompatibilityFlags()
	{
		//@TODO: UCREMOVAL: CPF_ConstParm added as a hack to get blueprints compiling with a const DamageType parameter.
		const uint64 IgnoreFlags = CPF_PersistentInstance | CPF_ExportObject | CPF_InstancedReference 
			| CPF_ContainsInstancedReference | CPF_ComputedFlags | CPF_ConstParm | CPF_HasGetValueTypeHash | CPF_UObjectWrapper
			| CPF_NativeAccessSpecifiers | CPF_AdvancedDisplay;
		return IgnoreFlags;
	}

	/**
	 * Determines if two functions have an identical signature (note: currently doesn't allow
	 * matches with class parameters that differ only in how derived they are; there is no
	 * directionality to the call)
	 *
	 * @param	OtherFunction	Function to compare this function against.
	 *
	 * @return	true if function signatures are compatible.
	 */
	bool IsSignatureCompatibleWith(const UFunction* OtherFunction) const;

	/**
	 * Determines if two functions have an identical signature (note: currently doesn't allow
	 * matches with class parameters that differ only in how derived they are; there is no
	 * directionality to the call)
	 *
	 * @param	OtherFunction	Function to compare this function against.
	 * @param   IgnoreFlags     Custom flags to ignore when comparing parameters between the functions.
	 *
	 * @return	true if function signatures are compatible.
	 */
	bool IsSignatureCompatibleWith(const UFunction* OtherFunction, uint64 IgnoreFlags) const;
};

class COREUOBJECT_API UDelegateFunction : public UFunction
{
	DECLARE_CASTED_CLASS_INTRINSIC(UDelegateFunction, UFunction, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UDelegateFunction)
	DECLARE_WITHIN(UObject)
public:
	explicit UDelegateFunction(const FObjectInitializer& ObjectInitializer, UFunction* InSuperFunction, uint32 InFunctionFlags = 0, uint16 InRepOffset = 0, SIZE_T ParamsSize = 0);
	explicit UDelegateFunction(UFunction* InSuperFunction, uint32 InFunctionFlags = 0, uint16 InRepOffset = 0, SIZE_T ParamsSize = 0);
};

/*-----------------------------------------------------------------------------
	UEnum.
-----------------------------------------------------------------------------*/

//
// Reflection data for an enumeration.
//
class COREUOBJECT_API UEnum : public UField
{
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(UEnum, UField, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UEnum, NO_API)

public:
	enum class ECppForm
	{
		Regular,
		Namespaced,
		EnumClass
	};

	// This will be the true type of the enum as a string, e.g. "ENamespacedEnum::InnerType" or "ERegularEnum" or "EEnumClass"
	FString CppType;

	/** Gets enum name by index in Names array. Returns NAME_None if Index is not valid. */
	FName GetNameByIndex(uint8 Index) const;
		
	/** Gets enum value by index in Names array. */
	uint8 GetValueByIndex(uint8 Index) const;

	/** Gets enum name by value. Returns NAME_None if value is not found. */
	FName GetNameByValue(uint8 InValue) const;
		
	/** Gets enum value by name. Returns INDEX_NONE when name is not found. */
	int32 GetValueByName(FName InName) const;

	/** Gets index of name in enum. Returns INDEX_NONE when name is not found. */
	int32 GetIndexByName(FName InName) const;

	/** Gets max value of Enum. Defaults to zero if there are no entries. */
	uint8 GetMaxEnumValue() const;

	/** Checks if enum has entry with given value. Includes autogenerated _MAX entry. */
	bool IsValidEnumValue(uint8 InValue) const;

	/** Checks if enum has entry with given name. Includes autogenerated _MAX entry. */
	bool IsValidEnumName(FName InName) const;

protected:
	// Variables.
	/** List of pairs of all enum names and values. */
	TArray<TPair<FName, uint8>> Names;

	/** How the enum was originally defined. */
	ECppForm CppForm;

	/** global list of all value names used by all enums in memory, used for property text import */
	static TMap<FName, UEnum*> AllEnumNames;

protected: 
	
	/** adds the Names in this enum to the master AllEnumNames list */
	void AddNamesToMasterList();

public:

	/** removes the Names in this enum from the master AllEnumNames list */
	void RemoveNamesFromMasterList();

	// UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface.

	~UEnum();

	/*
	 *	Try to update an out-of-date enum index after an enum's change
	 */
	virtual int32 ResolveEnumerator(FArchive& Ar, int32 EnumeratorIndex) const;

	/**
	 * Returns the type of enum: whether it's a regular enum, namespaced enum or C++11 enum class.
	 *
	 * @return The enum type.
	 */
	ECppForm GetCppForm() const
	{
		return CppForm;
	}

	/**
	 * Checks if a enum name is fully qualified name.
	 *
	 * @param InEnumName Name to check.
	 * @return true if the specified name is full enum name, false otherwise.
	 */
	static bool IsFullEnumName(const TCHAR* InEnumName)
	{
		return !!FCString::Strstr(InEnumName, TEXT("::"));
	}

	/**
	 * Generates full enum name give the enum type name and enum name.
	 *
	 * @param InEnum	 Enum Object
	 * @param InEnumName Enum name.
	 * @return Full enum name.
	 */
	static FString GenerateFullEnumName(const UEnum* InEnum, const TCHAR* InEnumName);

	/**
	 * Generates full enum name give enum name.
	 *
	 * @param InEnumName Enum name.
	 * @return Full enum name.
	 */
	virtual FString GenerateFullEnumName(const TCHAR* InEnumName) const;

	/** searches the list of all enum value names for the specified name
	 * @return the value the specified name represents if found, otherwise INDEX_NONE
	 */
	static int32 LookupEnumName(FName TestName, UEnum** FoundEnum = nullptr)
	{
		UEnum* TheEnum = AllEnumNames.FindRef(TestName);
		if (FoundEnum != nullptr)
		{
			*FoundEnum = TheEnum;
		}
		return (TheEnum != nullptr) ? TheEnum->GetValueByName(TestName) : INDEX_NONE;
	}

	/** searches the list of all enum value names for the specified name
	 * @return the value the specified name represents if found, otherwise INDEX_NONE
	 */
	static int32 LookupEnumNameSlow(const TCHAR* InTestShortName, UEnum** FoundEnum = NULL)
	{
		int32 EnumIndex = LookupEnumName(InTestShortName, FoundEnum);
		if (EnumIndex == INDEX_NONE)
		{
			FString TestShortName = FString(TEXT("::")) + InTestShortName;
			UEnum* TheEnum = NULL;
			for (TMap<FName, UEnum*>::TIterator It(AllEnumNames); It; ++It)
			{
				if (It.Key().ToString().Contains(TestShortName) )
				{
					TheEnum = It.Value();
				}
			}
			if (FoundEnum != NULL)
			{
				*FoundEnum = TheEnum;
			}
			EnumIndex = (TheEnum != NULL) ? TheEnum->GetValueByName(InTestShortName) : INDEX_NONE;
		}
		return EnumIndex;
	}

	/** parses the passed in string for a name, then searches for that name in any Enum (in any package)
	 * @param Str	pointer to string to parse; if we successfully find an enum, this pointer is advanced past the name found
	 * @return index of the value the parsed enum name matches, or INDEX_NONE if no matches
	 */
	static int32 ParseEnum(const TCHAR*& Str);

	/**
	 * Sets the array of enums.
	 *
	 * @param InNames List of enum names.
	 * @param InCppForm The form of enum.
	 * @param bAddMaxKeyIfMissing Should a default Max item be added.
	 * @return	true unless the MAX enum already exists and isn't the last enum.
	 */
	virtual bool SetEnums(TArray<TPair<FName, uint8>>& InNames, ECppForm InCppForm, bool bAddMaxKeyIfMissing = true);

	/**
	 * @return	The enum name at the specified Index.
	 */
	FName GetEnum(int32 InIndex) const
	{
		if (Names.IsValidIndex(InIndex))
		{
			return Names[InIndex].Key;
		}
		return NAME_None;
	}

	int32 GetIndexByValue(int32 Value) const
	{
		for (int32 i = 0; i < Names.Num(); ++i)
		{
			if (Names[i].Value == Value)
			{
				return i;
			}
		}

		return INDEX_NONE;
	}
	FString GetEnumNameStringByValue(int32 Value) const
	{
		int32 Index = GetIndexByValue(Value);
		return GetEnumName(Index);
	}

	/**
	 * @return	The short enum name at the specified Index.
	 */
	FString GetEnumName(int32 InIndex) const;

	FText GetEnumTextByValue(int32 Value)
	{
		int32 Index = GetIndexByValue(Value);
		return GetEnumText(Index);
	}

	/**
	 * @return	The enum string at the specified index.
	 */
	virtual FText GetEnumText(int32 InIndex) const;

	/**
	 * @return	The index of the specified name, if it exists in the enum names list.
	 */
	int32 FindEnumIndex(FName InName) const;

	/**
	 * @return	 The number of enum names.
	 */
	int32 NumEnums() const
	{
		return Names.Num();
	}

	/**
	 * Find the longest common prefix of all items in the enumeration.
	 * 
	 * @return	the longest common prefix between all items in the enum.  If a common prefix
	 *			cannot be found, returns the full name of the enum.
	 */
	FString GenerateEnumPrefix() const;

#if WITH_EDITOR
	/**
	* Finds the localized display name or native display name as a fallback.
	 *
	 * @param	NameIndex	if specified, will search for metadata linked to a specified value in this enum; otherwise, searches for metadata for the enum itself
	 *
	 * @return The display name for this object.
	 */
	FText GetDisplayNameText(int32 NameIndex=INDEX_NONE) const;
	FText GetDisplayNameTextByValue(int32 Value = INDEX_NONE) const;

	/**
	 * Finds the localized tooltip or native tooltip as a fallback.
	 *
	 * @param	NameIndex	if specified, will search for metadata linked to a specified value in this enum; otherwise, searches for metadata for the enum itself
	 *
	 * @return The tooltip for this object.
	 */
	FText GetToolTipText(int32 NameIndex=INDEX_NONE) const;
#endif

#if WITH_EDITOR || HACK_HEADER_GENERATOR
	/**
	 * Wrapper method for easily determining whether this enum has metadata associated with it.
	 * 
	 * @param	Key			the metadata tag to check for
	 * @param	NameIndex	if specified, will search for metadata linked to a specified value in this enum; otherwise, searches for metadata for the enum itself
	 *
	 * @return true if the specified key exists in the list of metadata for this enum, even if the value of that key is empty
	 */
	bool HasMetaData( const TCHAR* Key, int32 NameIndex=INDEX_NONE ) const;

	/**
	 * Return the metadata value associated with the specified key.
	 * 
	 * @param	Key			the metadata tag to find the value for
	 * @param	NameIndex	if specified, will search the metadata linked for that enum value; otherwise, searches the metadata for the enum itself
	 *
	 * @return	the value for the key specified, or an empty string if the key wasn't found or had no value.
	 */
	const FString& GetMetaData( const TCHAR* Key, int32 NameIndex=INDEX_NONE ) const;

	/**
	 * Set the metadata value associated with the specified key.
	 * 
	 * @param	Key			the metadata tag to find the value for
	 * @param	NameIndex	if specified, will search the metadata linked for that enum value; otherwise, searches the metadata for the enum itself
	 * @param	InValue		Value of the metadata for the key
	 *
	 */
	void SetMetaData( const TCHAR* Key, const TCHAR* InValue, int32 NameIndex=INDEX_NONE) const;
	
	/**
	 * Remove given key meta data
	 * 
	 * @param	Key			the metadata tag to find the value for
	 * @param	NameIndex	if specified, will search the metadata linked for that enum value; otherwise, searches the metadata for the enum itself
	 *
	 */
	void RemoveMetaData( const TCHAR* Key, int32 NameIndex=INDEX_NONE ) const;
#endif
	/**
	 * Find the enum and entry value from EnumRedirects
	 * 
	 * @param	Enum			Enum Object Ptr
	 * @param	EnumEntryName	Name of the entry of the enum
	 *
	 */
	static int32 FindEnumRedirects(const UEnum* Enum, FName EnumEntryName);


	/**
	 * @param EnumPath	- Full enum path
	 * @param EnumValue - Enum value
	 *
	 * @return the string associated with the specified enum value for the enum specified by a path
	 */
	template <typename T>
	FORCEINLINE static FString GetValueAsString( const TCHAR* EnumPath, const T EnumValue )
	{
		// For the C++ enum.
		static_assert(TIsEnum<T>::Value, "Should only call this with enum types");
		return GetValueAsString_Internal(EnumPath, (int32)EnumValue);
	}

	template <typename T>
	FORCEINLINE static FString GetValueAsString( const TCHAR* EnumPath, const TEnumAsByte<T> EnumValue )
	{
		return GetValueAsString_Internal(EnumPath, (int32)EnumValue.GetValue());
	}

	template< class T >
	FORCEINLINE static void GetValueAsString( const TCHAR* EnumPath, const T EnumValue, FString& out_StringValue )
	{
		out_StringValue = GetValueAsString( EnumPath, EnumValue );
	}

	/**
	 * @param EnumPath	- Full enum path
	 * @param EnumValue - Enum value
	 *
	 * @return the localized display string associated with the specified enum value for the enum specified by a path
	 */
	template <typename T>
	FORCEINLINE static FText GetDisplayValueAsText( const TCHAR* EnumPath, const T EnumValue )
	{
		// For the C++ enum.
		static_assert(TIsEnum<T>::Value, "Should only call this with enum types");
		return GetDisplayValueAsText_Internal(EnumPath, (int32)EnumValue);
	}

	template <typename T>
	FORCEINLINE static FText GetDisplayValueAsText( const TCHAR* EnumPath, const TEnumAsByte<T> EnumValue )
	{
		return GetDisplayValueAsText_Internal(EnumPath, (int32)EnumValue.GetValue());
	}

	template< class T >
	FORCEINLINE static void GetDisplayValueAsText( const TCHAR* EnumPath, const T EnumValue, FText& out_TextValue )
	{
		out_TextValue = GetDisplayValueAsText( EnumPath, EnumValue );
	}

private:
	/** Map of Enum Name to Map of Old Enum entry to New Enum entry */
	static TMap<FName,TMap<FName,FName> > EnumRedirects;
	/** Map of Enum Name to Map of Old Enum substring to New Enum substring, to handle many renames at once */
	static TMap<FName,TMap<FString,FString> > EnumSubstringRedirects;
	static void InitEnumRedirectsMap();

	FORCEINLINE static FString GetValueAsString_Internal( const TCHAR* EnumPath, const int32 Value )
	{
		UEnum* EnumClass = FindObject<UEnum>( nullptr, EnumPath );
		UE_CLOG( !EnumClass, LogClass, Fatal, TEXT("Couldn't find enum '%s'"), EnumPath );
		return EnumClass->GetEnumName(Value);
	}

	FORCEINLINE static FText GetDisplayValueAsText_Internal( const TCHAR* EnumPath, const int32 Value )
	{
		UEnum* EnumClass = FindObject<UEnum>(nullptr, EnumPath);
		UE_CLOG(!EnumClass, LogClass, Fatal, TEXT("Couldn't find enum '%s'"), EnumPath);
		return EnumClass->GetEnumText(Value);
	}

	/**
	 *
	 * Renames enum values to use duplicated enum name instead of base one, e.g.:
	 * 
	 * MyEnum::MyVal
	 * MyEnum::MyEnum_MAX
	 * 
	 * becomes
	 * 
	 * MyDuplicatedEnum::MyVal
	 * MyDuplicatedEnum::MyDuplicatedEnum_MAX
	 * 
	 **/
	void RenameNamesAfterDuplication();

	/** Gets name of enum "this" is duplicate of. If we're not duplicating, just returns "this" name. */
	FString GetBaseEnumNameOnDuplication() const;
};

/*-----------------------------------------------------------------------------
	UClass.
-----------------------------------------------------------------------------*/

/** information about an interface a class implements */
struct COREUOBJECT_API FImplementedInterface
{
	/** the interface class */
	UClass* Class;
	/** the pointer offset of the interface's vtable */
	int32 PointerOffset;
	/** whether or not this interface has been implemented via K2 */
	bool bImplementedByK2;

	FImplementedInterface()
		: Class(NULL)
		, PointerOffset(0)
		, bImplementedByK2(false)
	{}
	FImplementedInterface(UClass* InClass, int32 InOffset, bool InImplementedByK2)
		: Class(InClass)
		, PointerOffset(InOffset)
		, bImplementedByK2(InImplementedByK2)
	{}

	friend COREUOBJECT_API FArchive& operator<<(FArchive& Ar, FImplementedInterface& A);
};

#include "CoreNative.h"

/** A struct that maps a string name to a native function */
struct FNativeFunctionLookup
{
	FName Name;
	Native Pointer;

	FNativeFunctionLookup(FName InName,Native InPointer)
		:	Name(InName)
		,	Pointer(InPointer)
	{}
};


namespace EIncludeSuperFlag
{
	enum Type
	{
		ExcludeSuper,
		IncludeSuper
	};
}


#if UCLASS_FAST_ISA_IMPL & 2

	class FFastIndexingClassTreeRegistrar
	{
	public:
		COREUOBJECT_API FFastIndexingClassTreeRegistrar();
		COREUOBJECT_API FFastIndexingClassTreeRegistrar(const FFastIndexingClassTreeRegistrar&);
		COREUOBJECT_API ~FFastIndexingClassTreeRegistrar();

	private:
		friend class UClass;
		friend class UObjectBaseUtility;
		friend class FFastIndexingClassTree;

		FORCEINLINE bool IsAUsingFastTree(const FFastIndexingClassTreeRegistrar& Parent) const
		{
			return ClassTreeIndex - Parent.ClassTreeIndex <= Parent.ClassTreeNumChildren;
		}

		FFastIndexingClassTreeRegistrar& operator=(const FFastIndexingClassTreeRegistrar&) = delete;

		uint32 ClassTreeIndex;
		uint32 ClassTreeNumChildren;
	};

#endif


/**
 * An object class.
 */
class COREUOBJECT_API UClass : public UStruct
#if UCLASS_FAST_ISA_IMPL & 2
	, private FFastIndexingClassTreeRegistrar
#endif
{
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(UClass, UStruct, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UClass, NO_API)
	DECLARE_WITHIN(UPackage)

public:
#if UCLASS_FAST_ISA_IMPL & 2
	friend class FFastIndexingClassTree;
#endif
	friend class FRestoreClassInfo;

	typedef void		(*ClassConstructorType)				(const FObjectInitializer&);
#if WITH_HOT_RELOAD_CTORS
	typedef UObject*	(*ClassVTableHelperCtorCallerType)	(FVTableHelper& Helper);
#endif // WITH_HOT_RELOAD_CTORS
	typedef void		(*ClassAddReferencedObjectsType)	(UObject*, class FReferenceCollector&);
	typedef UClass* (*StaticClassFunctionType)();

	ClassConstructorType ClassConstructor;
#if WITH_HOT_RELOAD_CTORS
	ClassVTableHelperCtorCallerType ClassVTableHelperCtorCaller;
#endif // WITH_HOT_RELOAD_CTORS
	/** Pointer to a static AddReferencedObjects method. */
	ClassAddReferencedObjectsType ClassAddReferencedObjects;

	// Class pseudo-unique counter; used to accelerate unique instance name generation
	int32 ClassUnique;

	// Class flags; See EClassFlags for more information
	uint32 ClassFlags;

	// Cast flags used to accelerate dynamic_cast<T*> on objects of this type for common T
	EClassCastFlags ClassCastFlags;

	// The required type for the outer of instances of this class.
	UClass* ClassWithin;

	// This is the blueprint that caused the generation of this class, or NULL if it is a native compiled-in class
	UObject* ClassGeneratedBy;

#if WITH_EDITOR
	/**
	 * Conditionally recompiles the class after loading, in case any dependencies were also newly loaded
	 * @param ObjLoaded	If set this is the list of objects that are currently loading, usualy GObjLoaded
	 */
	virtual void ConditionalRecompileClass(TArray<UObject*>* ObjLoaded) {};
#endif //WITH_EDITOR

	//
	FName ClassConfigName;

	// Used to check if the class was cooked or not.
	bool bCooked;

	// List of replication records
	TArray<FRepRecord> ClassReps;

	// List of network relevant fields (properties and functions)
	TArray<UField*> NetFields;

	virtual bool IsNameStableForNetworking() const override { return true; }		// For now, assume all classes have stable net names

#if WITH_EDITOR || HACK_HEADER_GENERATOR 
	// Editor only properties
	void GetHideFunctions(TArray<FString>& OutHideFunctions) const;
	bool IsFunctionHidden(const TCHAR* InFunction) const;
	void GetAutoExpandCategories(TArray<FString>& OutAutoExpandCategories) const;
	bool IsAutoExpandCategory(const TCHAR* InCategory) const;
	void GetAutoCollapseCategories(TArray<FString>& OutAutoCollapseCategories) const;
	bool IsAutoCollapseCategory(const TCHAR* InCategory) const;
	void GetClassGroupNames(TArray<FString>& OutClassGroupNames) const;
	bool IsClassGroupName(const TCHAR* InGroupName) const;
#endif
	/**
	 * Calls AddReferencedObjects static method on the specified object.
	 *
	 * @param This Object to call ARO on.
	 * @param Collector Reference collector.
	 */
	FORCEINLINE void CallAddReferencedObjects(UObject* This, FReferenceCollector& Collector) const
	{
		// The object must of this class type.
		check(This->IsA(this)); 
		// This is should always be set to something, at the very least to UObject::ARO
		check(ClassAddReferencedObjects != NULL); 
		ClassAddReferencedObjects(This, Collector);
	}

	// The class default object; used for delta serialization and object initialization
	UObject* ClassDefaultObject;

	/**
	* Assemble reference token streams for all classes if they haven't had it assembled already
	*/
	static void AssembleReferenceTokenStreams();

private:
	/** Map of all functions by name contained in this class */
	TMap<FName, UFunction*> FuncMap;

	/** A cache of all functions by name that exist in a parent context */
	mutable TMap<FName, UFunction*> ParentFuncMap;

	/** A cache of all functions by name that exist in an interface context */
	mutable TMap<FName, UFunction*> InterfaceFuncMap;

public:
	/**
	 * The list of interfaces which this class implements, along with the pointer property that is located at the offset of the interface's vtable.
	 * If the interface class isn't native, the property will be NULL.
	 **/
	TArray<FImplementedInterface> Interfaces;

	/**
	 * Prepends reference token stream with super class's stream.
	 *
	 * @param SuperClass Super class to prepend stream with.
	 */
	void PrependStreamWithSuperClass(UClass& SuperClass);

	/** Reference token stream used by realtime garbage collector, finalized in AssembleReferenceTokenStream */
	FGCReferenceTokenStream ReferenceTokenStream;

#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
	/* TokenIndex map to look-up token stream index origin. */
	FGCDebugReferenceTokenMap DebugTokenMap;
#endif

	/** This class's native functions. */
	TArray<FNativeFunctionLookup> NativeFunctionLookupTable;

public:
	// Constructors
	UClass(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	explicit UClass(const FObjectInitializer& ObjectInitializer, UClass* InSuperClass);
	UClass( EStaticConstructor, FName InName, uint32 InSize, uint32 InClassFlags, EClassCastFlags InClassCastFlags,
		const TCHAR* InClassConfigName, EObjectFlags InFlags, ClassConstructorType InClassConstructor,
#if WITH_HOT_RELOAD_CTORS
		ClassVTableHelperCtorCallerType InClassVTableHelperCtorCaller,
#endif // WITH_HOT_RELOAD_CTORS
		ClassAddReferencedObjectsType InClassAddReferencedObjects);

#if WITH_HOT_RELOAD
	/**
	 * Called when a class is reloading from a DLL...updates various information in-place.
	 * @param	InSize							sizeof the class
	 * @param	InClassFlags					Class flags for the class
	 * @param	InClassCastFlags				Cast Flags for the class
	 * @param	InConfigName					Config Name
	 * @param	InClassConstructor				Pointer to InternalConstructor<TClass>
	 * @param	TClass_Super_StaticClass		Static class of the super class
	 * @param	TClass_WithinClass_StaticClass	Static class of the WithinClass
	 **/
	bool HotReloadPrivateStaticClass(
		uint32			InSize,
		uint32			InClassFlags,
		EClassCastFlags	InClassCastFlags,
		const TCHAR*    InConfigName,
		ClassConstructorType InClassConstructor,
#if WITH_HOT_RELOAD_CTORS
		ClassVTableHelperCtorCallerType InClassVTableHelperCtorCaller,
#endif // WITH_HOT_RELOAD_CTORS
		ClassAddReferencedObjectsType InClassAddReferencedObjects,
		class UClass* TClass_Super_StaticClass,
		class UClass* TClass_WithinClass_StaticClass
		);


	/**
	* Replace a native function in the  internal native function table
	* @param	InName							name of the function
	* @param	InPointer						pointer to the function
	* @param	bAddToFunctionRemapTable		For C++ hot-reloading, UFunctions are patched in a deferred manner and this should be true
	*											For script hot-reloading, script integrations may have a many to 1 mapping of UFunction to native pointer
	*											because dispatch is shared, so the C++ remap table does not work in this case, and this should be false
	* @return	true if the function was found and replaced, false if it was not
	*/
	bool ReplaceNativeFunction(FName InName, Native InPointer, bool bAddToFunctionRemapTable);
#endif

	/**
	 * If there are potentially multiple versions of this class (e.g. blueprint generated classes), this function will return the authoritative version, which should be used for references
	 *
	 * @return The version of this class that references should be stored to
	 */
	virtual UClass* GetAuthoritativeClass()
	{
		return this;
	}

	/**
	 * Add a native function to the internal native function table
	 * @param	InName							name of the function
	 * @param	InPointer						pointer to the function
	 **/
	void AddNativeFunction(const ANSICHAR* InName, Native InPointer);

	/**
	 * Add a native function to the internal native function table, but with a unicode name. Used when generating code from blueprints, 
	 * which can have unicode identifiers for functions and properties.
	 * @param	InName							name of the function
	 * @param	InPointer						pointer to the function
	 **/
	void AddNativeFunction(const WIDECHAR* InName, Native InPointer);

	// Add a function to the function map
	void AddFunctionToFunctionMap(UFunction* NewFunction)
	{
		FuncMap.Add(NewFunction->GetFName(), NewFunction);
	}

	// This is used by the code generator, which instantiates UFunctions with a name that is later overridden. Overridden names
	// are needed to support generated versions of blueprint classes, properties of which do not have the same naming 
	// restrictions as native C++ properties.
	void AddFunctionToFunctionMapWithOverriddenName(UFunction* NewFunction, FName OverriddenName)
	{
		FuncMap.Add(OverriddenName, NewFunction);
	}

	// Remove a function from the function map
	void RemoveFunctionFromFunctionMap(UFunction* Function)
	{
		FuncMap.Remove(Function->GetFName());
	}

	void ClearFunctionMapsCaches()
	{
		ParentFuncMap.Empty();
		InterfaceFuncMap.Empty();
	}

	UFunction* FindFunctionByName(FName InName, EIncludeSuperFlag::Type IncludeSuper = EIncludeSuperFlag::IncludeSuper) const;

	// UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void FinishDestroy() override;
	virtual void DeferredRegister(UClass *UClassStaticClass,const TCHAR* PackageName,const TCHAR* InName) override;
	virtual bool Rename(const TCHAR* NewName = NULL, UObject* NewOuter = NULL, ERenameFlags Flags = REN_None) override;
	virtual void TagSubobjects(EObjectFlags NewFlags) override;
	virtual void PostInitProperties() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual FRestoreForUObjectOverwrite* GetRestoreForUObjectOverwrite() override;
	virtual FString GetDesc() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual bool IsAsset() const override { return false; }
	// End of UObject interface.

	// UField interface.
	virtual void Bind() override;
	virtual const TCHAR* GetPrefixCPP() const override;
	// End of UField interface.

	// UStruct interface.
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	virtual void SetSuperStruct(UStruct* NewSuperStruct) override;
	virtual void SerializeSuperStruct(FArchive& Ar) override;
	// End of UStruct interface.
	
	/**
	 * Translates the hardcoded script config names (engine, editor, input and 
	 * game) to their global pendants and otherwise uses config(myini) name to
	 * look for a game specific implementation and creates one based on the
	 * default if it doesn't exist yet.
	 *
	 * @return	name of the class specific ini file
	 */
	const FString GetConfigName() const;

	UClass* GetSuperClass() const
	{
		return (UClass*)GetSuperStruct();
	}

	/** Feedback context for default property import **/
	static class FFeedbackContext& GetDefaultPropertiesFeedbackContext();

	int32 GetDefaultsCount()
	{
		return ClassDefaultObject != NULL ? GetPropertiesSize() : 0;
	}

	/**
	  * Get the default object from the class
	  * @param	bCreateIfNeeded if true (default) then the CDO is created if it is NULL.
	  * @return		the CDO for this class
	**/
	UObject* GetDefaultObject(bool bCreateIfNeeded = true)
	{
		if (ClassDefaultObject == NULL && bCreateIfNeeded)
		{
			CreateDefaultObject();
		}

		return ClassDefaultObject;
	}

	/**
	 * Override to return a linked list of properties with default values that differ from the parent default object. If non-NULL, only these properties will 
	 * be copied post-construction. Otherwise, all properties will be copied to the new instance, even if the default value matches the inherited default value.
	 */
	virtual const FCustomPropertyListNode* GetCustomPropertyListForPostConstruction() const { return nullptr; }

	/**
	* Get the name of the CDO for the this class
	* @return The name of the CDO
	*/
	FName GetDefaultObjectName();

	virtual uint8* GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const
	{
		return NULL;
	}

	virtual void CreatePersistentUberGraphFrame(UObject* Obj, bool bCreateOnlyIfEmpty = false, bool bSkipSuperClass = false, UClass* OldClass = nullptr) const
	{
	}

	virtual void DestroyPersistentUberGraphFrame(UObject* Obj, bool bSkipSuperClass = false) const
	{
	}

	/**
	  * Get the default object from the class and cast to a particular type
	  * @return		the CDO for this class
	**/
	template<class T>
	T* GetDefaultObject()
	{
		UObject *Ret = GetDefaultObject();
		checkSlow(Ret->IsA(T::StaticClass()));
		return (T*)Ret;
	}

	/** Searches for the default instanced object (often a component) by name **/
	UObject* GetDefaultSubobjectByName(FName ToFind);

	/** Adds a new default instance map item **/
	void AddDefaultSubobject(UObject* NewSubobject, UClass* BaseClass)
	{
		// this compoonent must be a derived class of the base class
		check(NewSubobject->IsA(BaseClass));
		// the outer of the component must be of my class or some superclass of me
		check(IsChildOf(NewSubobject->GetOuter()->GetClass()));
	}

	/**
	 * Gets all default instanced objects (often components).
	 *
	 * @param OutDefaultSubobjects An array to be filled with default subobjects.
	 */
	void GetDefaultObjectSubobjects(TArray<UObject*>& OutDefaultSubobjects);

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagToCheck		Class flag to check for
	 *
	 * @return	true if the passed in flag is set, false otherwise
	 *			(including no flag passed in, unless the FlagsToCheck is CLASS_AllFlags)
	 */
	FORCEINLINE bool HasAnyClassFlags( uint32 FlagsToCheck ) const
	{
		return (ClassFlags & FlagsToCheck) != 0;
	}

	/**
	 * Used to safely check whether all of the passed in flags are set.
	 *
	 * @param FlagsToCheck	Class flags to check for
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	FORCEINLINE bool HasAllClassFlags( uint32 FlagsToCheck ) const
	{
		return ((ClassFlags & FlagsToCheck) == FlagsToCheck);
	}

	/**
	 * Gets the class flags.
	 *
	 * @return	The class flags.
	 */
	FORCEINLINE uint32 GetClassFlags() const
	{
		return ClassFlags;
	}

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagToCheck		the cast flag to check for (value should be one of the EClassCastFlags enums)
	 *
	 * @return	true if the passed in flag is set, false otherwise
	 *			(including no flag passed in)
	 */
	FORCEINLINE bool HasAnyCastFlag(EClassCastFlags FlagToCheck) const
	{
		return (ClassCastFlags&FlagToCheck) != 0;
	}
	FORCEINLINE bool HasAllCastFlags(EClassCastFlags FlagsToCheck) const
	{
		return (ClassCastFlags&FlagsToCheck) == FlagsToCheck;
	}

	FString GetDescription() const;

	/**
	 * Realtime garbage collection helper function used to emit token containing information about a 
	 * direct UObject reference at the passed in offset.
	 *
	 * @param Offset	Offset into object at which object reference is stored.
	 * @param DebugName	DebugName for this objects token. Only used in non-shipping builds.
	 * @param Kind		Optional parameter the describe the type of the reference.
	 */
	void EmitObjectReference(int32 Offset, const FName& DebugName, EGCReferenceType Kind = GCRT_Object);

	/**
	 * Realtime garbage collection helper function used to emit token containing information about a 
	 * an array of UObject references at the passed in offset. Handles both TArray and TTransArray.
	 *
	 * @param Offset	Offset into object at which array of objects is stored.
	 * @param DebugName	DebugName for this objects token. Only used in non-shipping builds.
	 */
	void EmitObjectArrayReference(int32 Offset, const FName& DebugName);

	/**
	 * Realtime garbage collection helper function used to indicate an array of structs at the passed in 
	 * offset.
	 *
	 * @param Offset	Offset into object at which array of structs is stored
	 * @param DebugName	DebugName for this objects token. Only used in non-shipping builds.
	 * @param Stride	Size/stride of struct
	 * @return	Index into token stream at which later on index to next token after the array is stored
	 *			which is used to skip over empty dynamic arrays
	 */
	uint32 EmitStructArrayBegin(int32 Offset, const FName& DebugName, int32 Stride);

	/**
	 * Realtime garbage collection helper function used to indicate the end of an array of structs. The
	 * index following the current one will be written to the passed in SkipIndexIndex in order to be
	 * able to skip tokens for empty dynamic arrays.
	 *
	 * @param SkipIndexIndex
	 */
	void EmitStructArrayEnd(uint32 SkipIndexIndex);

	/**
	 * Realtime garbage collection helper function used to indicate the beginning of a fixed array.
	 * All tokens issues between Begin and End will be replayed Count times.
	 *
	 * @param Offset	Offset at which fixed array starts.
	 * @param DebugName	DebugName for this objects token. Only used in non-shipping builds.
	 * @param Stride	Stride of array element, e.g. sizeof(struct) or sizeof(UObject*).
	 * @param Count		Fixed array count.
	 */
	void EmitFixedArrayBegin(int32 Offset, const FName& DebugName, int32 Stride, int32 Count);
	
	/**
	 * Realtime garbage collection helper function used to indicated the end of a fixed array.
	 */
	void EmitFixedArrayEnd();

	/**
	 * Assembles the token stream for realtime garbage collection by combining the per class only
	 * token stream for each class in the class hierarchy. This is only done once and duplicate
	 * work is avoided by using an object flag.
	 * @param bForce Assemble the stream even if it has been already assembled (deletes the old one)
	 */
	void AssembleReferenceTokenStream(bool bForce = false);

	/** 
	 * This will return whether or not this class implements the passed in class / interface 
	 *
	 * @param SomeClass - the interface to check and see if this class implements it
	 **/
	bool ImplementsInterface(const class UClass* SomeInterface) const;

	/** serializes the passed in object as this class's default object using the given archive
	 * @param Object the object to serialize as default
	 * @param Ar the archive to serialize from
	 */
	virtual void SerializeDefaultObject(UObject* Object, FArchive& Ar);

	/** Wraps the PostLoad() call for the class default object.
	 * @param Object the default object to call PostLoad() on
	 */
	virtual void PostLoadDefaultObject(UObject* Object) { Object->PostLoad(); }

	/** 
	 * Purges out the properties of this class in preparation for it to be regenerated
	 * @param bRecompilingOnLoad - true if we are recompiling on load
	 */
	virtual void PurgeClass(bool bRecompilingOnLoad);

	/**
	 * Finds the common base class that parents the two classes passed in.
	 *
	 * @param InClassA		the first class to find the common base for
	 * @param InClassB		the second class to find the common base for
	 * @return				the common base class or NULL
	 */
	static UClass* FindCommonBase(UClass* InClassA, UClass* InClassB);

	/**
	 * Finds the common base class that parents the array of classes passed in.
	 *
	 * @param InClasses		the array of classes to find the common base for
	 * @return				the common base class or NULL
	 */
	static UClass* FindCommonBase(const TArray<UClass*>& InClasses);

	/**
	 * Determines if the specified function has been implemented in a Blueprint
	 *
	 * @param InFunctionName	The name of the function to test
	 * @return					True if the specified function exists and is implemented in a blueprint generated class
	 */
	virtual bool IsFunctionImplementedInBlueprint(FName InFunctionName) const;

	/**
	 * Checks if the property exists on this class or a parent class.
	 * @param InProperty	The property to check if it is contained in this or a parent class.
	 * @return				True if the property exists on this or a parent class.
	 */
	virtual bool HasProperty(UProperty* InProperty) const;

	virtual UObject* FindArchetype(UClass* ArchetypeClass, const FName ArchetypeName) const { return nullptr; }

	/**
	 * On save, we order a package's exports in class dependency order (so that
	 * on load, we create the class dependencies before we create the class).  
	 * More often than not, the class doesn't require any non-struct objects 
	 * before it is created/serialized (only super-classes, and its UField 
	 * members, see FExportReferenceSorter::operator<<() for reference). 
	 * However, in some special occasions, there might be an export that we 
	 * would like force loaded prior to the class's serialization (like  
	 * component templates for blueprint classes). This function returns a list
	 * of those non-struct dependencies, so that FExportReferenceSorter knows to 
	 * prioritize them earlier in the ExportMap.
	 * 
	 * @param  DependenciesOut	Will be filled with a list of dependencies that need to be created before this class is recreated (on load).
	 */
	virtual void GetRequiredPreloadDependencies(TArray<UObject*>& DependenciesOut) {}

	virtual UObject* GetArchetypeForCDO() const;

	/** Returns true if this class implements script instrumentation. */
	virtual bool HasInstrumentation() const { return false; }

private:
	#if UCLASS_FAST_ISA_IMPL & 2
		// For UObjectBaseUtility
		friend class UObjectBaseUtility;
		using FFastIndexingClassTreeRegistrar::IsAUsingFastTree;
	#endif

	// This signature intentionally hides the method declared in UObjectBaseUtility to make it private.
	// Call IsChildOf instead; Hidden because calling IsA on a class almost always indicates an error where the caller should use IsChildOf.
	bool IsA(const UClass* Parent) const
	{
		return UObject::IsA(Parent);
	}

	// This signature intentionally hides the method declared in UObject to make it private.
	// Call FindFunctionByName instead; This method will search for a function declared in UClass instead of the class it was called on
	UFunction* FindFunction(FName InName) const
	{
		return UObject::FindFunction(InName);
	}

	// This signature intentionally hides the method declared in UObject to make it private.
	// Call FindFunctionByName instead; This method will search for a function declared in UClass instead of the class it was called on
	UFunction* FindFunctionChecked(FName InName) const
	{
		return UObject::FindFunctionChecked(InName);
	}

protected:
	/**
	 * Get the default object from the class, creating it if missing, if requested or under a few other circumstances
	 * @return		the CDO for this class
	 **/
	virtual UObject* CreateDefaultObject();

#if HACK_HEADER_GENERATOR
	// Required by UHT makefiles for internal data serialization.
	friend struct FClassArchiveProxy;
#endif // HACK_HEADER_GENERATOR
};

/**
* Dynamic class (can be constructed after initial startup)
*/
class COREUOBJECT_API UDynamicClass : public UClass
{
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(UDynamicClass, UClass, 0, TEXT("/Script/CoreUObject"), CASTCLASS_None, NO_API)
	DECLARE_WITHIN(UPackage)

public:

	UDynamicClass(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	explicit UDynamicClass(const FObjectInitializer& ObjectInitializer, UClass* InSuperClass);
	UDynamicClass(EStaticConstructor, FName InName, uint32 InSize, uint32 InClassFlags, EClassCastFlags InClassCastFlags,
		const TCHAR* InClassConfigName, EObjectFlags InFlags, ClassConstructorType InClassConstructor,
#if WITH_HOT_RELOAD_CTORS
		ClassVTableHelperCtorCallerType InClassVTableHelperCtorCaller,
#endif // WITH_HOT_RELOAD_CTORS
		ClassAddReferencedObjectsType InClassAddReferencedObjects);

	// UObject interface.
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	// UClass interface
	virtual void PurgeClass(bool bRecompilingOnLoad) override;
	virtual UObject* FindArchetype(UClass* ArchetypeClass, const FName ArchetypeName) const override;

	UStructProperty* FindStructPropertyChecked(const TCHAR* PropertyName) const;

	/** Misc objects owned by the class. */
	TArray<UObject*> MiscConvertedSubobjects;

	/** Additional converted fields, that are used by the class. */
	TArray<UField*> ReferencedConvertedFields;

	/** Outer assets used by the class */
	TArray<UObject*> UsedAssets;

	// Specialized sub-object containers
	TArray<UObject*> DynamicBindingObjects;
	TArray<UObject*> ComponentTemplates;
	TArray<UObject*> Timelines;

	// IAnimClassInterface (UAnimClassData) or null
	UObject* AnimClassImplementation;
};

/**
 * Helper template to call the default constructor for a class
 */
template<class T>
void InternalConstructor( const FObjectInitializer& X )
{ 
	T::__DefaultConstructor(X);
}


/**
 * Helper template to call the vtable ctor caller for a class
 */
template<class T>
UObject* InternalVTableHelperCtorCaller(FVTableHelper& Helper)
{
#if WITH_HOT_RELOAD_CTORS
	return T::__VTableCtorCaller(Helper);
#else
	return nullptr;
#endif // WITH_HOT_RELOAD_CTORS
}

COREUOBJECT_API void InitializePrivateStaticClass(
	class UClass* TClass_Super_StaticClass,
	class UClass* TClass_PrivateStaticClass,
	class UClass* TClass_WithinClass_StaticClass,
	const TCHAR* PackageName,
	const TCHAR* Name
	);

/**
 * Helper template allocate and construct a UClass
 *
 * @param PackageName name of the package this class will be inside
 * @param Name of the class
 * @param ReturnClass reference to pointer to result. This must be PrivateStaticClass.
 * @param RegisterNativeFunc Native function registration function pointer.
 * @param InSize Size of the class
 * @param InClassFlags Class flags
 * @param InClassCastFlags Class cast flags
 * @param InConfigName Class config name
 * @param InClassConstructor Class constructor function pointer
 * @param InClassVTableHelperCtorCaller Class constructor function for vtable pointer
 * @param InClassAddReferencedObjects Class AddReferencedObjects function pointer
 * @param InSuperClassFn Super class function pointer
 * @param WithinClass Within class
 * @param bIsDynamic true if the class can be constructed dynamically at runtime
 */
COREUOBJECT_API void GetPrivateStaticClassBody(
	const TCHAR* PackageName,
	const TCHAR* Name,
	UClass*& ReturnClass,
	void(*RegisterNativeFunc)(),
	uint32 InSize,
	uint32 InClassFlags,
	EClassCastFlags InClassCastFlags,
	const TCHAR* InConfigName,
	UClass::ClassConstructorType InClassConstructor,
	UClass::ClassVTableHelperCtorCallerType InClassVTableHelperCtorCaller,
	UClass::ClassAddReferencedObjectsType InClassAddReferencedObjects,
	UClass::StaticClassFunctionType InSuperClassFn,
	UClass::StaticClassFunctionType InWithinClassFn,
	bool bIsDynamic = false);

/*-----------------------------------------------------------------------------
	FObjectInstancingGraph.
-----------------------------------------------------------------------------*/

struct COREUOBJECT_API FObjectInstancingGraph
{
public:

	/** 
	 * Default Constructor 
	 * @param bDisableInstancing - if true, start with component instancing disabled
	**/
	FObjectInstancingGraph(bool bDisableInstancing = false);

	/**
	 * Standard constructor
	 *
	 * @param	DestinationSubobjectRoot	the top-level object that is being created
	 */
	FObjectInstancingGraph( class UObject* DestinationSubobjectRoot );

	/**
	 * Sets the DestinationRoot for this instancing graph.
	 *
	 * @param	DestinationSubobjectRoot	the top-level object that is being created
	 * @param	InSourceRoot	Archetype of DestinationSubobjectRoot
	 */
	void SetDestinationRoot( class UObject* DestinationSubobjectRoot, class UObject* InSourceRoot = nullptr );

	/**
	 * Finds the destination object instance corresponding to the specified source object.
	 *
	 * @param	SourceObject			the object to find the corresponding instance for
	 */
	class UObject* GetDestinationObject(class UObject* SourceObject);

	/**
	 * Returns the component that has SourceComponent as its archetype, instancing the component as necessary.
	 *
	 * @param	SourceComponent		the component to find the corresponding component instance for
	 * @param	CurrentValue		the component currently assigned as the value for the component property
	 *								being instanced.  Used when updating archetypes to ensure that the new instanced component
	 *								replaces the existing component instance in memory.
	 * @param	CurrentObject		the object that owns the component property currently being instanced;  this is NOT necessarily the object
	 *								that should be the Outer for the new component.
	 * @param	bIsTransient		is this for a transient property?
	 * @param	bCausesInstancing	if true, then this property causes an instance to be created...if false, this is just a pointer to a uobject that should be remapped if the object is instanced for some other property
	 * @param	bAllowSelfReference If true, instance the reference to the subobjectroot, so far only delegates remap a self reference
	 *
	 * @return	As with GetInstancedSubobject, above, but also deals with archetype creation and a few other special cases
	 */
	class UObject* InstancePropertyValue( class UObject* SourceComponent, class UObject* CurrentValue, class UObject* CurrentObject, bool bIsTransient, bool bCausesInstancing = false, bool bAllowSelfReference = false );

	/**
	 * Adds a partially built object instance to the map(s) of source objects to their instances.
	 * @param	ObjectInstance  Object that was just allocated, but has not been constructed yet
	 * @param	InArchetype     Archetype of ObjectInstance
	 */
	void AddNewObject(class UObject* ObjectInstance, class UObject* InArchetype = nullptr);

	/**
	 * Adds an object instance to the map of source objects to their instances.  If there is already a mapping for this object, it will be replaced
	 * and the value corresponding to ObjectInstance's archetype will now point to ObjectInstance.
	 *
	 * @param	ObjectInstance  the object that should be added as the corresopnding instance for ObjectSource
	 * @param	InArchetype     Archetype of ObjectInstance
	 */
	void AddNewInstance(class UObject* ObjectInstance, class UObject* InArchetype = nullptr);

	/**
	 * Retrieves a list of objects that have the specified Outer
	 *
	 * @param	SearchOuter		the object to retrieve object instances for
	 * @param	out_Components	receives the list of objects contained by SearchOuter
	 */
	void RetrieveObjectInstances( class UObject* SearchOuter, TArray<class UObject*>& out_Objects );

	/**
	 * Enables / disables component instancing.
	 */
	void EnableSubobjectInstancing( bool bEnabled )
	{
		bEnableSubobjectInstancing = bEnabled;
	}

	/**
	 * Returns whether component instancing is enabled
	 */
	bool IsSubobjectInstancingEnabled() const
	{
		return bEnableSubobjectInstancing;
	}

	/**
	 * Sets whether DestinationRoot is currently being loaded from disk.
	 */
	void SetLoadingObject( bool bIsLoading )
	{
		bLoadingObject = bIsLoading;
	}

private:
	/**
	 * Returns whether this instancing graph has a valid destination root.
	 */
	bool HasDestinationRoot() const
	{
		return DestinationRoot != NULL;
	}

	/**
	 * Returns whether DestinationRoot corresponds to an archetype object.
	 *
	 * @param	bUserGeneratedOnly	true indicates that we only care about cases where the user selected "Create [or Update] Archetype" in the editor
	 *								false causes this function to return true even if we are just loading an archetype from disk
	 */
	bool IsCreatingArchetype( bool bUserGeneratedOnly=true ) const
	{
		// if we only want cases where we are creating an archetype in response to user input, return false if we are in fact just loading the object from disk
		return bCreatingArchetype && (!bUserGeneratedOnly || !bLoadingObject);
	}

	/**
	 * Returns whether DestinationRoot is currently being loaded from disk.
	 */
	bool IsLoadingObject() const
	{
		return bLoadingObject;
	}

	/**
	 * Returns the component that has SourceComponent as its archetype, instancing the component as necessary.
	 *
	 * @param	SourceComponent		the component to find the corresponding component instance for
	 * @param	CurrentValue		the component currently assigned as the value for the component property
	 *								being instanced.  Used when updating archetypes to ensure that the new instanced component
	 *								replaces the existing component instance in memory.
	 * @param	CurrentObject		the object that owns the component property currently being instanced;  this is NOT necessarily the object
	 *								that should be the Outer for the new component.
	 * @param	bDoNotCreateNewInstance If true, then we do not create a new instance, but we will reassign one if there is already a mapping in the table
	 * @param	bAllowSelfReference If true, instance the reference to the subobjectroot, so far only delegates remap a self reference
	 *
	 * @return	if SourceComponent is contained within SourceRoot, returns a pointer to a unique component instance corresponding to
	 *			SourceComponent if SourceComponent is allowed to be instanced in this context, or NULL if the component isn't allowed to be
	 *			instanced at this time (such as when we're a client and the component isn't loaded on clients)
	 *			if SourceComponent is not contained by SourceRoot, return INVALID_OBJECT, indicating that the that has SourceComponent as its ObjectArchetype, or NULL if SourceComponent is not contained within
	 *			SourceRoot.
	 */
	class UObject* GetInstancedSubobject( class UObject* SourceSubobject, class UObject* CurrentValue, class UObject* CurrentObject, bool bDoNotCreateNewInstance, bool bAllowSelfReference );

	/**
	 * The root of the object tree that is the source used for instancing components;
	 * - when placing an instance of an actor class, this would be the actor class default object
	 * - when placing an instance of an archetype, this would be the archetype
	 * - when creating an archetype, this would be the actor instance
	 * - when duplicating an object, this would be the duplication source
	 */
	class		UObject*						SourceRoot;

	/**
	 * The root of the object tree that is the destination used for instancing components
	 * - when placing an instance of an actor class, this would be the placed actor
	 * - when placing an instance of an archetype, this would be the placed actor
	 * - when creating an archetype, this would be the actor archetype
	 * - when updating an archetype, this would be the source archetype
	 * - when duplicating an object, this would be the copied object (destination)
	 */
	class		UObject*						DestinationRoot;

	/**
	 * Indicates whether we are currently instancing components for an archetype.  true if we are creating or updating an archetype.
	 */
	bool										bCreatingArchetype;

	/**
	 * If false, components will not be instanced.
	 */
	bool										bEnableSubobjectInstancing;

	/**
	 * true when loading object data from disk.
	 */
	bool										bLoadingObject;

	/**
	 * Maps the source (think archetype) to the destination (think instance)
	 */
	TMap<class UObject*,class UObject*>			SourceToDestinationMap;
};

// UFunction interface.

inline UFunction* UFunction::GetSuperFunction() const
{
	UStruct* Result = GetSuperStruct();
	checkSlow(!Result || Result->IsA<UFunction>());
	return (UFunction*)Result;
}


// UObject.h

/**
 * Returns true if this object implements the interface T, false otherwise.
 */
template<class T>
FORCEINLINE bool UObject::Implements() const
{
	UClass const* const MyClass = GetClass();
	return MyClass && MyClass->ImplementsInterface(T::StaticClass());
}

// UObjectGlobals.h

/**
 * Construct an object of a particular class.
 * 
 * @param	Class		the class of object to construct
 * @param	Outer		the outer for the new object.  If not specified, object will be created in the transient package.
 * @param	Name		the name for the new object.  If not specified, the object will be given a transient name via
 *						MakeUniqueObjectName
 * @param	SetFlags	the object flags to apply to the new object
 * @param	Template	the object to use for initializing the new object.  If not specified, the class's default object will
 *						be used
 * @param	bInCopyTransientsFromClassDefaults - if true, copy transient from the class defaults instead of the pass in archetype ptr (often these are the same)
 * @param	InstanceGraph
 *						contains the mappings of instanced objects and components to their templates
 *
 * @return	a pointer of type T to a new object of the specified class
 */
template< class T >
DEPRECATED(4.8, "ConstructObject is deprecated. Use NewObject instead")
T* ConstructObject(UClass* Class, UObject* Outer, FName Name, EObjectFlags SetFlags, UObject* Template, bool bCopyTransientsFromClassDefaults, struct FObjectInstancingGraph* InstanceGraph )
{
	checkf(Class, TEXT("ConstructObject called with a NULL class object"));
	checkSlow(Class->IsChildOf(T::StaticClass()));
	return (T*)StaticConstructObject_Internal(Class, Outer, Name, SetFlags, EInternalObjectFlags::None, Template, bCopyTransientsFromClassDefaults, InstanceGraph);
}

/**
 * Gets the default object of a class.
 *
 * In most cases, class default objects should not be modified. This method therefore returns
 * an immutable pointer. If you need to modify the default object, use GetMutableDefault instead.
 *
 * @param Class - The class to get the CDO for.
 *
 * @return Class default object (CDO).
 *
 * @see GetMutableDefault
 */
template< class T > 
inline const T* GetDefault(UClass *Class)
{
	check(Class->GetDefaultObject()->IsA(T::StaticClass()));
	return (const T*)Class->GetDefaultObject();
}

/**
 * Gets the mutable default object of a class.
 *
 * @param Class - The class to get the CDO for.
 *
 * @return Class default object (CDO).
 *
 * @see GetDefault
 */
template< class T > 
inline T* GetMutableDefault(UClass *Class)
{
	check(Class->GetDefaultObject()->IsA(T::StaticClass()));
	return (T*)Class->GetDefaultObject();
}

struct FStructUtils
{
	static bool ArePropertiesTheSame(const UProperty* A, const UProperty* B, bool bCheckPropertiesNames);

	// does structures have exactly the same memory layout
	COREUOBJECT_API static bool TheSameLayout(const UStruct* StructA, const UStruct* StructB, bool bCheckPropertiesNames = false);
};

template< class T > struct TBaseStructure
{
};

template<> struct TBaseStructure<FRotator>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct TBaseStructure<FTransform>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct TBaseStructure<FLinearColor>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct TBaseStructure<FColor>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct  TBaseStructure<FVector>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct TBaseStructure<FVector2D>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct TBaseStructure<FRandomStream>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct TBaseStructure<FGuid>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct TBaseStructure<FBox2D>
{
	COREUOBJECT_API static UScriptStruct* Get();
};	

struct FFallbackStruct;
template<> struct TBaseStructure<FFallbackStruct>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct TBaseStructure<FFloatRangeBound>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct TBaseStructure<FFloatRange>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct TBaseStructure<FInt32RangeBound>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct TBaseStructure<FInt32Range>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct TBaseStructure<FFloatInterval>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

template<> struct TBaseStructure<FInt32Interval>
{
	COREUOBJECT_API static UScriptStruct* Get();
};

