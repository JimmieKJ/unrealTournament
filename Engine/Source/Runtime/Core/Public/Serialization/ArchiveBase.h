// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/EnumAsByte.h"
#include "HAL/PlatformProperties.h"
#include "Misc/Compression.h"
#include "Misc/EngineVersionBase.h"
#include "TextNamespaceFwd.h"

class FAssetPtr;
class FCustomVersionContainer;
class FLazyObjectPtr;
struct FStringAssetReference;
struct FUntypedBulkData;


/**
 * Base class for archives that can be used for loading, saving, and garbage
 * collecting in a byte order neutral way.
 */
class CORE_API FArchive
{
public:

	/** Default constructor. */
	FArchive();

	/** Copy constructor. */
	FArchive(const FArchive&);

	/**
	 * Copy assignment operator.
	 *
	 * @param ArchiveToCopy The archive to copy from.
	 */
	FArchive& operator=(const FArchive& ArchiveToCopy);

	/** Destructor. */
	virtual ~FArchive();

public:

	/**
	 * Serializes an FName value from or into this archive.
	 *
	 * This operator can be implemented by sub-classes that wish to serialize FName instances.
	 *
	 * @param Value The value to serialize.
	 * @return This instance.
	 */
	virtual FArchive& operator<<(class FName& Value)
	{
		return *this;
	}

	/**
	 * Serializes an FText value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	virtual FArchive& operator<<(class FText& Value);

	/**
	 * Serializes an UObject value from or into this archive.
	 *
	 * This operator can be implemented by sub-classes that wish to serialize UObject instances.
	 *
	 * @param Value The value to serialize.
	 * @return This instance.
	 */
	virtual FArchive& operator<<(class UObject*& Value)
	{
		return *this;
	}

	/**
	 * Serializes a lazy object pointer value from or into this archive.
	 *
	 * Most of the time, FLazyObjectPtrs are serialized as UObject*, but some archives need to override this.
	 *
	 * @param Value The value to serialize.
	 * @return This instance.
	 */
	virtual FArchive& operator<<(class FLazyObjectPtr& Value);
	
	/**
	 * Serializes asset pointer from or into this archive.
	 *
	 * Most of the time, FAssetPtrs are serialized as UObject *, but some archives need to override this.
	 *
	 * @param Value The asset pointer to serialize.
	 * @return This instance.
	 */
	virtual FArchive& operator<<(class FAssetPtr& Value);

	/**
	 * Serializes string asset reference from or into this archive.
	 *
	 * @param Value String asset reference to serialize.
	 * @return This instance.
	 */
	virtual FArchive& operator<<(struct FStringAssetReference& Value);

	/** 
	 * Inform the archive that a blueprint would like to force finalization, normally
	 * this is triggered by CDO load, but if there's no CDO we force finalization.
	 */
	virtual void ForceBlueprintFinalization() {}
public:

	/**
	 * Serializes an ANSICHAR value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, ANSICHAR& Value)
	{
		Ar.Serialize(&Value, 1);

		return Ar;
	}

	/**
	 * Serializes a WIDECHAR value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, WIDECHAR& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));

		return Ar;
	}

	/**
	 * Serializes an unsigned 8-bit integer value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, uint8& Value)
	{
		Ar.Serialize(&Value, 1);

		return Ar;
	}

	/**
	 * Serializes an enumeration value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	template<class TEnum>
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, TEnumAsByte<TEnum>& Value)
	{
		Ar.Serialize(&Value, 1);

		return Ar;
	}

	/**
	 * Serializes a signed 8-bit integer value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, int8& Value)
	{
		Ar.Serialize(&Value, 1);

		return Ar;
	}

	/**
	 * Serializes an unsigned 16-bit integer value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, uint16& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));

		return Ar;
	}

	/**
	 * Serializes a signed 16-bit integer value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, int16& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));

		return Ar;
	}

	/**
	 * Serializes an unsigned 32-bit integer value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, uint32& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));

		return Ar;
	}

	/**
	 * Serializes a Boolean value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, bool& D )
	{
		// Serialize bool as if it were UBOOL (legacy, 32 bit int).
		uint32 OldUBoolValue = D ? 1 : 0;
		Ar.ByteOrderSerialize( &OldUBoolValue, sizeof(OldUBoolValue) );
		D = !!OldUBoolValue;
		return Ar;
	}

	/**
	 * Serializes a signed 32-bit integer value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, int32& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));

		return Ar;
	}

#if PLATFORM_COMPILER_DISTINGUISHES_INT_AND_LONG
	/**
	 * Serializes a long integer value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, long& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));

		return Ar;
	}	
#endif

	/**
	 * Serializes a single precision floating point value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, float& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));

		return Ar;
	}

	/**
	 * Serializes a double precision floating point value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, double& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));

		return Ar;
	}

	/**
	 * Serializes a unsigned 64-bit integer value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	FORCEINLINE friend FArchive& operator<<(FArchive &Ar, uint64& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));

		return Ar;
	}

	/**
	 * Serializes a signed 64-bit integer value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	/*FORCEINLINE*/friend FArchive& operator<<(FArchive& Ar, int64& Value)
	{
		Ar.ByteOrderSerialize(&Value, sizeof(Value));

		return Ar;
	}

	/**
	 * Serializes an FIntRect value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	friend FArchive& operator<<(FArchive& Ar, struct FIntRect& Value);

	/**
	 * Serializes an FString value from or into an archive.
	 *
	 * @param Ar The archive to serialize from or to.
	 * @param Value The value to serialize.
	 */
	friend CORE_API FArchive& operator<<(FArchive& Ar, FString& Value);

public:

	virtual void Serialize(void* V, int64 Length) { }

	virtual void SerializeBits(void* V, int64 LengthBits)
	{
		Serialize(V, (LengthBits + 7) / 8);

		if (IsLoading() && (LengthBits % 8) != 0)
		{
			((uint8*)V)[LengthBits / 8] &= ((1 << (LengthBits & 7)) - 1);
		}
	}

	virtual void SerializeInt(uint32& Value, uint32 Max)
	{
		ByteOrderSerialize(&Value, sizeof(Value));
	}

	/** Packs int value into bytes of 7 bits with 8th bit for 'more' */
	virtual void SerializeIntPacked(uint32& Value);

	virtual void Preload(UObject* Object) { }

	virtual void CountBytes(SIZE_T InNum, SIZE_T InMax) { }

	/**
  	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 */
	virtual FString GetArchiveName() const;

	/**
	 * If this archive is a FLinkerLoad or FLinkerSave, returns a pointer to the ULinker portion.
	 *
	 * @return The linker, or nullptr if the archive is not a linker.
	 */
	virtual class FLinker* GetLinker()
	{
		return nullptr;
	}

	virtual int64 Tell()
	{
		return INDEX_NONE;
	}

	virtual int64 TotalSize()
	{
		return INDEX_NONE;
	}

	virtual bool AtEnd()
	{
		int64 Pos = Tell();

		return ((Pos != INDEX_NONE) && (Pos >= TotalSize()));
	}

	virtual void Seek(int64 InPos) { }

	/**
	 * Attaches/ associates the passed in bulk data object with the linker.
	 *
	 * @param	Owner		UObject owning the bulk data
	 * @param	BulkData	Bulk data object to associate
	 */
	virtual void AttachBulkData(UObject* Owner, FUntypedBulkData* BulkData) { }

	/**
	 * Detaches the passed in bulk data object from the linker.
	 *
	 * @param	BulkData	Bulk data object to detach
	 * @param	bEnsureBulkDataIsLoaded	Whether to ensure that the bulk data is loaded before detaching
	 */
	virtual void DetachBulkData(FUntypedBulkData* BulkData, bool bEnsureBulkDataIsLoaded) { }

	/**
	 * Hint the archive that the region starting at passed in offset and spanning the passed in size
	 * is going to be read soon and should be precached.
	 *
	 * The function returns whether the precache operation has completed or not which is an important
	 * hint for code knowing that it deals with potential async I/O. The archive is free to either not 
	 * implement this function or only partially precache so it is required that given sufficient time
	 * the function will return true. Archives not based on async I/O should always return true.
	 *
	 * This function will not change the current archive position.
	 *
	 * @param	PrecacheOffset	Offset at which to begin precaching.
	 * @param	PrecacheSize	Number of bytes to precache
	 * @return	false if precache operation is still pending, true otherwise
	 */
	virtual bool Precache(int64 PrecacheOffset, int64 PrecacheSize)
	{
		return true;
	}

	/**
	 * Flushes cache and frees internal data.
	 */

	virtual void FlushCache() { }

	/**
	 * Sets mapping from offsets/ sizes that are going to be used for seeking and serialization to what
	 * is actually stored on disk. If the archive supports dealing with compression in this way it is 
	 * going to return true.
	 *
	 * @param	CompressedChunks	Pointer to array containing information about [un]compressed chunks
	 * @param	CompressionFlags	Flags determining compression format associated with mapping
	 *
	 * @return true if archive supports translating offsets & uncompressing on read, false otherwise
	 */
	virtual bool SetCompressionMap(TArray<struct FCompressedChunk>* CompressedChunks, ECompressionFlags CompressionFlags)
	{
		return false;
	}

	virtual void Flush() { }

	virtual bool Close()
	{
		return !ArIsError;
	}

	virtual bool GetError()
	{
		return ArIsError;
	}

	void SetError() 
	{ 
		ArIsError = true; 
	}

	/**
	 * Serializes and compresses/ uncompresses data. This is a shared helper function for compression
	 * support. The data is saved in a way compatible with FIOSystem::LoadCompressedData.
	 *
	 * @param	V		Data pointer to serialize data from/ to
	 * @param	Length	Length of source data if we're saving, unused otherwise
	 * @param	Flags	Flags to control what method to use for [de]compression and optionally control memory vs speed when compressing
	 * @param	bTreatBufferAsFileReader true if V is actually an FArchive, which is used when saving to read data - helps to avoid single huge allocations of source data
	 * @param	bUsePlatformBitWindow use a platform specific bitwindow setting
	 */
	void SerializeCompressed(void* V, int64 Length, ECompressionFlags Flags, bool bTreatBufferAsFileReader = false, bool bUsePlatformBitWindow = false);


	FORCEINLINE bool IsByteSwapping()
	{
#if PLATFORM_LITTLE_ENDIAN
		bool SwapBytes = ArForceByteSwapping;
#else
		bool SwapBytes = ArIsPersistent;
#endif
		return SwapBytes;
	}

	// Used to do byte swapping on small items. This does not happen usually, so we don't want it inline
	void ByteSwap(void* V, int32 Length);

	FORCEINLINE FArchive& ByteOrderSerialize(void* V, int32 Length)
	{
		Serialize(V, Length);
		if (IsByteSwapping())
		{
			// Transferring between memory and file, so flip the byte order.
			ByteSwap(V, Length);
		}
		return *this;
	}

	/** Sets a flag indicating that this archive contains code. */
	void ThisContainsCode()
	{
		ArContainsCode = true;
	}

	/** Sets a flag indicating that this archive contains a ULevel or UWorld object. */
	void ThisContainsMap() 
	{
		ArContainsMap = true;
	}

	/** Sets a flag indicating that this archive contains data required to be gathered for localization. */
	void ThisRequiresLocalizationGather()
	{
		ArRequiresLocalizationGather = true;
	}

	/** Sets a flag indicating that this archive is currently serializing class/struct defaults. */
	void StartSerializingDefaults() 
	{
		ArSerializingDefaults++;
	}

	/** Indicate that this archive is no longer serializing class/struct defaults. */
	void StopSerializingDefaults() 
	{
		ArSerializingDefaults--;
	}

	/**
	 * Called when an object begins serializing property data using script serialization.
	 */
	virtual void MarkScriptSerializationStart(const UObject* Obj) { }

	/**
	 * Called when an object stops serializing property data using script serialization.
	 */
	virtual void MarkScriptSerializationEnd(const UObject* Obj) { }

	virtual void IndicateSerializationMismatch() { }

	// Logf implementation for convenience.
	VARARG_DECL(void, void, {}, Logf, VARARG_NONE, const TCHAR*, VARARG_NONE, VARARG_NONE);

	FORCEINLINE int32 UE4Ver() const
	{
		return ArUE4Ver;
	}
	
	FORCEINLINE int32 LicenseeUE4Ver() const
	{
		return ArLicenseeUE4Ver;
	}

	FORCEINLINE FEngineVersionBase EngineVer() const
	{
		return ArEngineVer;
	}

	FORCEINLINE uint32 EngineNetVer() const
	{
		return ArEngineNetVer;
	}

	FORCEINLINE uint32 GameNetVer() const
	{
		return ArGameNetVer;
	}

	/**
	 * Registers the custom version to the archive.  This is used to inform the archive that custom version information is about to be stored.
	 * There is no effect when the archive is being loaded from.
	 *
	 * @param Guid The guid of the custom version.  This must have previously been registered with FCustomVersionRegistration.
	 */
	void UsingCustomVersion(const struct FGuid& Guid);

	/**
	 * Queries a custom version from the archive.  If the archive is being used to write, the custom version must have already been registered.
	 *
	 * @param Key The guid of the custom version to query.
	 * @return The version number, or 0 if the custom tag isn't stored in the archive.
	 */
	int32 CustomVer(const struct FGuid& Key) const;

	FORCEINLINE bool IsLoading() const
	{
		return ArIsLoading;
	}

	FORCEINLINE bool IsSaving() const
	{
		return ArIsSaving;
	}

	FORCEINLINE bool IsTransacting() const
	{
		if (FPlatformProperties::HasEditorOnlyData())
		{
			return ArIsTransacting;
		}
		else
		{
			return false;
		}
	}

	FORCEINLINE bool WantBinaryPropertySerialization() const
	{
		return ArWantBinaryPropertySerialization;
	}

	FORCEINLINE bool IsForcingUnicode() const
	{
		return ArForceUnicode;
	}

	FORCEINLINE bool IsPersistent() const
	{
		return ArIsPersistent;
	}

	FORCEINLINE bool IsError() const
	{
		return ArIsError;
	}

	FORCEINLINE bool IsCriticalError() const
	{
		return ArIsCriticalError;
	}

	FORCEINLINE bool ContainsCode() const
	{
		return ArContainsCode;
	}

	FORCEINLINE bool ContainsMap() const
	{
		return ArContainsMap;
	}

	FORCEINLINE bool RequiresLocalizationGather() const
	{
		return ArRequiresLocalizationGather;
	}

	FORCEINLINE bool ForceByteSwapping() const
	{
		return ArForceByteSwapping;
	}

	FORCEINLINE bool IsSerializingDefaults() const
	{
		return (ArSerializingDefaults > 0) ? true : false;
	}

	FORCEINLINE bool IsIgnoringArchetypeRef() const
	{
		return ArIgnoreArchetypeRef;
	}

	FORCEINLINE bool DoDelta() const
	{
		return !ArNoDelta;
	}

	FORCEINLINE bool IsIgnoringOuterRef() const
	{
		return ArIgnoreOuterRef;
	}

	FORCEINLINE bool IsIgnoringClassGeneratedByRef() const
	{
		return ArIgnoreClassGeneratedByRef;
	}

	FORCEINLINE bool IsIgnoringClassRef() const
	{
		return ArIgnoreClassRef;
	}

	FORCEINLINE bool IsAllowingLazyLoading() const
	{
		return ArAllowLazyLoading;
	}

	FORCEINLINE bool IsObjectReferenceCollector() const
	{
		return ArIsObjectReferenceCollector;
	}

	FORCEINLINE bool IsModifyingWeakAndStrongReferences() const
	{
		return ArIsModifyingWeakAndStrongReferences;
	}

	FORCEINLINE bool IsCountingMemory() const
	{
		return ArIsCountingMemory;
	}

	FORCEINLINE uint32 GetPortFlags() const
	{
		return ArPortFlags;
	}

	FORCEINLINE bool HasAnyPortFlags(uint32 Flags) const
	{
		return ((ArPortFlags & Flags) != 0);
	}

	FORCEINLINE bool HasAllPortFlags(uint32 Flags) const
	{
		return ((ArPortFlags & Flags) == Flags);
	}

	FORCEINLINE uint32 GetDebugSerializationFlags() const
	{
#if WITH_EDITOR
		return ArDebugSerializationFlags;
#else
		return 0;
#endif
	}

	FORCEINLINE bool ShouldSkipBulkData() const
	{
		return ArShouldSkipBulkData;
	}

	FORCEINLINE int64 GetMaxSerializeSize() const
	{
		return ArMaxSerializeSize;
	}

	/**
	 * Sets the archive version number. Used by the code that makes sure that FLinkerLoad's 
	 * internal archive versions match the file reader it creates.
	 *
	 * @param UE4Ver	new version number
	 */
	void SetUE4Ver(int32 InVer)
	{
		ArUE4Ver = InVer;
	}

	/**
	 * Sets the archive licensee version number. Used by the code that makes sure that FLinkerLoad's 
	 * internal archive versions match the file reader it creates.
	 *
	 * @param Ver	new version number
	 */
	void SetLicenseeUE4Ver(int32 InVer)
	{
		ArLicenseeUE4Ver = InVer;
	}

	/**
	* Sets the archive engine version. Used by the code that makes sure that FLinkerLoad's
	* internal archive versions match the file reader it creates.
	*
	* @param InVer	new version number
	*/
	void SetEngineVer(const FEngineVersionBase& InVer)
	{
		ArEngineVer = InVer;
	}

	/**
	* Sets the archive engine network version.
	*/
	void SetEngineNetVer(const uint32 InEngineNetVer)
	{
		ArEngineNetVer = InEngineNetVer;
	}

	/**
	* Sets the archive game network version.
	*/
	void SetGameNetVer(const uint32 InGameNetVer)
	{
		ArGameNetVer = InGameNetVer;
	}

	/**
	 * Gets the custom version numbers for this archive.
	 *
	 * @return The container of custom versions in the archive.
	 */
	const FCustomVersionContainer& GetCustomVersions() const;

	/**
	 * Sets the custom version numbers for this archive.
	 *
	 * @param CustomVersionContainer - The container of custom versions to copy into the archive.
	 */
	void SetCustomVersions(const FCustomVersionContainer& CustomVersionContainer);

	/** Resets the custom version numbers for this archive. */
	void ResetCustomVersions();

	/**
	 * Sets a specific custom version
	 *
	 * @param Key - The guid of the custom version to query.
	 * @param Version - The version number to set key to
	 * @param FriendlyName - Friendly name corresponding to the key
	 */
	void SetCustomVersion(const struct FGuid& Key, int32 Version, FName FriendlyName);

	/**
	 * Toggle saving as Unicode. This is needed when we need to make sure ANSI strings are saved as Unicode
	 *
	 * @param Enabled	set to true to force saving as Unicode
	 */
	void SetForceUnicode(bool Enabled)
	{
		ArForceUnicode = Enabled;
	}

	/**
	 * Toggle byte order swapping. This is needed in rare cases when we already know that the data
	 * swapping has already occurred or if we know that it will be handled later.
	 *
	 * @param Enabled	set to true to enable byte order swapping
	 */
	void SetByteSwapping(bool Enabled)
	{
		ArForceByteSwapping = Enabled;
	}

	/**
	 * Sets the archive's property serialization modifier flags
	 *
	 * @param	InPortFlags		the new flags to use for property serialization
	 */
	void SetPortFlags(uint32 InPortFlags)
	{
		ArPortFlags = InPortFlags;
	}

	/**
	 * Sets the archives custom serialization modifier flags (nothing to do with PortFlags or Custom versions)
	 * 
	 * @param InCustomFlags the new flags to use for custom serialization
	 */
	void SetDebugSerializationFlags(uint32 InCustomFlags)
	{
#if WITH_EDITOR
		ArDebugSerializationFlags = InCustomFlags;
#endif
	}

	/**
	 * Returns if an async close operation has finished or not, as well as if there was an error.
	 *
	 * @param bHasError true if there was an error.
	 * @return true if the close operation is complete (or if Close is not an async operation).
	 */
	virtual bool IsCloseComplete(bool& bHasError)
	{
		bHasError = false;
		
		return true;
	}

	/**
	 * Indicates whether this archive is filtering editor-only on save or contains data that had editor-only content stripped.
	 *
	 * @return true if the archive filters editor-only content, false otherwise.
	 */
	virtual bool IsFilterEditorOnly()
	{
		return ArIsFilterEditorOnly;
	}

	/**
	 * Sets a flag indicating that this archive needs to filter editor-only content.
	 *
	 * @param InFilterEditorOnly Whether to filter editor-only content.
	 */
	virtual void SetFilterEditorOnly(bool InFilterEditorOnly)
	{
		ArIsFilterEditorOnly = InFilterEditorOnly;
	}

	/**
	 * Indicates whether this archive is saving or loading game state
	 *
	 * @return true if the archive is dealing with save games, false otherwise.
	 */
	virtual bool IsSaveGame()
	{
		return ArIsSaveGame;
	}

	/**
	 * Checks whether the archive is used for cooking.
	 *
	 * @return true if the archive is used for cooking, false otherwise.
	 */
	FORCEINLINE bool IsCooking() const	
	{
		check(!CookingTargetPlatform || (!IsLoading() && !IsTransacting() && IsSaving()));

		return !!CookingTargetPlatform;
	}

	/**
	 * Returns the cooking target platform.
	 *
	 * @return Target platform.
	 */
	FORCEINLINE const class ITargetPlatform* CookingTarget() const 
	{
		return CookingTargetPlatform;
	}

	/**
	 * Sets the cooking target platform.
	 *
	 * @param InCookingTarget The target platform to set.
	 */
	FORCEINLINE void SetCookingTarget(const class ITargetPlatform* InCookingTarget) 
	{
		CookingTargetPlatform = InCookingTarget;
	}

	/**
	 * Checks whether the archive is used to resolve out-of-date enum indexes
	 * If function returns true, the archive should be called only for objects containing user defined enum
	 *
	 * @return true if the archive is used to resolve out-of-date enum indexes
	 */
	virtual bool UseToResolveEnumerators() const
	{
		return false;
	}

	/**
	 * Checks whether the archive wants to skip the property independent of the other flags
	 */
	virtual bool ShouldSkipProperty(const class UProperty* InProperty) const
	{
		return false;
	}

	/**
	 * Sets the property that is currently being serialized
	 * 
	 * @param InProperty Pointer to the property that is currently being serialized
	 */
	FORCEINLINE void SetSerializedProperty(class UProperty* InProperty)
	{
		SerializedProperty = InProperty;
	}

#if WITH_EDITORONLY_DATA
	/** Pushes editor-only marker to the stack of currently serialized properties */
	FORCEINLINE void PushEditorOnlyProperty()
	{
		EditorOnlyPropertyStack++;
	}
	/** Pops editor-only marker from the stack of currently serialized properties */
	FORCEINLINE void PopEditorOnlyProperty()
	{
		EditorOnlyPropertyStack--;
		check(EditorOnlyPropertyStack >= 0);
	}
	/** Returns true if the stack of currently serialized properties contains an editor-only property */
	FORCEINLINE bool IsEditorOnlyPropertyOnTheStack() const
	{
		return EditorOnlyPropertyStack > 0;
	}
#endif

	/**
	 * Gets the property that is currently being serialized
	 *
	 * @return Pointer to the property that is currently being serialized
	 */
	FORCEINLINE class UProperty* GetSerializedProperty() const
	{
		return SerializedProperty;
	}

#if USE_STABLE_LOCALIZATION_KEYS
	/**
	 * Set the localization namespace that this archive should use when serializing text properties.
	 * This is typically the namespace used by the package being serialized (if serializing a package, or an object within a package).
	 */
	virtual void SetLocalizationNamespace(const FString& InLocalizationNamespace);

	/**
	 * Get the localization namespace that this archive should use when serializing text properties.
	 * This is typically the namespace used by the package being serialized (if serializing a package, or an object within a package).
	 */
	virtual FString GetLocalizationNamespace() const;
#endif // USE_STABLE_LOCALIZATION_KEYS

protected:

	/** Resets all of the base archive members. */
	void Reset();

private:

	/** Copies all of the members except CustomVersionContainer */
	void CopyTrivialFArchiveStatusMembers(const FArchive& ArchiveStatusToCopy);

protected:

	/** Holds the archive version. */
	int32 ArUE4Ver;
	
	/** Holds the archive version for licensees. */
	int32 ArLicenseeUE4Ver;

	/** Holds the engine version. */
	FEngineVersionBase ArEngineVer;

	/** Holds the engine network protocol version. */
	uint32 ArEngineNetVer;

	/** Holds the game network protocol version. */
	uint32 ArGameNetVer;

private:

	/**
	 * All the custom versions stored in the archive.
	 * Stored as a pointer to a heap-allocated object because of a 3-way dependency between TArray, FCustomVersionContainer and FArchive, which is too much work to change right now.
	 */
	FCustomVersionContainer* CustomVersionContainer;

public:

	/** Whether this archive is for loading data. */
	bool ArIsLoading;
	
	/** Whether this archive is for saving data. */
	bool ArIsSaving;
	
	/** Whether archive is transacting. */
	bool ArIsTransacting;
	
	/** Whether this archive wants properties to be serialized in binary form instead of tagged. */
	bool ArWantBinaryPropertySerialization;
	
	/** Whether this archive wants to always save strings in unicode format */
	bool ArForceUnicode;
	
	/** Whether this archive saves to persistent storage. */
	bool ArIsPersistent;
			
	/** Whether this archive contains errors. */
	bool ArIsError;

	/** Whether this archive contains critical errors. */
	bool ArIsCriticalError;
	
	/** Quickly tell if an archive contains script code. */
	bool ArContainsCode;
	
	/** Used to determine whether FArchive contains a level or world. */
	bool ArContainsMap;

	/** Used to determine whether FArchive contains data required to be gathered for localization. */
	bool ArRequiresLocalizationGather;
	
	/** Whether we should forcefully swap bytes. */
	bool ArForceByteSwapping;
	
	/** If true, we will not serialize the ObjectArchetype reference in UObject. */
	bool ArIgnoreArchetypeRef;
	
	/** If true, we will not serialize the ObjectArchetype reference in UObject. */
	bool ArNoDelta;

	/** If true, we will not serialize the Outer reference in UObject. */
	bool ArIgnoreOuterRef;

	/** If true, we will not serialize ClassGeneratedBy reference in UClass. */
	bool ArIgnoreClassGeneratedByRef;

	/** If true, UObject::Serialize will skip serialization of the Class property. */
	bool ArIgnoreClassRef;
	
	/** Whether to allow lazy loading. */
	bool ArAllowLazyLoading;
	
	/** Whether this archive only cares about serializing object references. */
	bool ArIsObjectReferenceCollector;
	
	/** Whether a reference collector is modifying the references and wants both weak and strong ones */
	bool ArIsModifyingWeakAndStrongReferences;
	
	/** Whether this archive is counting memory and therefore wants e.g. TMaps to be serialized. */
	bool ArIsCountingMemory;
	
	/** Whether bulk data serialization should be skipped or not. */
	bool ArShouldSkipBulkData;

	/** Whether editor only properties are being filtered from the archive (or has been filtered). */
	bool ArIsFilterEditorOnly;

	/** Whether this archive is saving/loading game state */
	bool ArIsSaveGame;

	/** Whether we are currently serializing defaults. > 0 means yes, <= 0 means no. */
	int32 ArSerializingDefaults;

	/** Modifier flags that be used when serializing UProperties */
	uint32 ArPortFlags;
			
	/** Max size of data that this archive is allowed to serialize. */
	int64 ArMaxSerializeSize;

	/** Custom property list attribute. If the flag below is set, only these properties will be iterated during serialization. If NULL, then no properties will be iterated. */
	const struct FCustomPropertyListNode* ArCustomPropertyList;

	/** Set TRUE to use the custom property list attribute for serialization. */
	bool ArUseCustomPropertyList;

	class FScopeSetDebugSerializationFlags
	{
	private:
#if WITH_EDITOR
		uint32 PreviousFlags;
		FArchive& Ar;
#endif
	public:
		/**
		 * Initializes an object which will set flags for the scope of this code
		 * 
		 * @param NewFlags new flags to set 
		 * @param Remove should we add these flags or remove them default is to add
		 */
#if WITH_EDITOR
		FScopeSetDebugSerializationFlags(FArchive& InAr, uint32 NewFlags, bool Remove = false)
			: Ar(InAr)
		{

			PreviousFlags = Ar.GetDebugSerializationFlags();
			if (Remove)
			{
				Ar.SetDebugSerializationFlags( PreviousFlags & ~NewFlags);
			}
			else
			{
				Ar.SetDebugSerializationFlags( PreviousFlags | NewFlags);
			}

		}
		~FScopeSetDebugSerializationFlags()
		{

			Ar.SetDebugSerializationFlags( PreviousFlags);
		}
#else
		FScopeSetDebugSerializationFlags(FArchive& InAr, uint32 NewFlags, bool Remove = false)
		{}
		~FScopeSetDebugSerializationFlags()
		{}
#endif
	};

#if WITH_EDITOR
	/** Custom serialization modifier flags can be used for anything */
	uint32 ArDebugSerializationFlags;
	/** Debug stack storage if you want to add data to the archive for usage further down the serialization stack this should be used in conjunction with the FScopeAddDebugData struct */
	
	virtual void PushDebugDataString(const FName& DebugData);
	virtual void PopDebugDataString() { }

	class FScopeAddDebugData
	{
	private:
		FArchive& Ar;
	public:
		CORE_API FScopeAddDebugData(FArchive& InAr, const FName& DebugData);

		~FScopeAddDebugData()
		{
			Ar.PopDebugDataString();
		}
	};
#endif	
private:
	/** Holds the cooking target platform. */
	const ITargetPlatform* CookingTargetPlatform;

	/** Holds the pointer to the property that is currently being serialized */
	class UProperty* SerializedProperty;

#if WITH_EDITORONLY_DATA
	/** Non-zero if on the current stack of serialized properties there's an editor-only property. Needs to live in FArchive because of SerializedProperty. */
	int32 EditorOnlyPropertyStack;
#endif

#if USE_STABLE_LOCALIZATION_KEYS
	/**
	 * The localization namespace that this archive should use when serializing text properties.
	 * This is typically the namespace used by the package being serialized (if serializing a package, or an object within a package).
	 * Stored as a pointer to a heap-allocated string because of a dependency between TArray (thus FString) and FArchive; null should be treated as an empty string.
	 */
	class FString* LocalizationNamespacePtr;

	/** See SetLocalizationNamespace */
	void SetBaseLocalizationNamespace(const FString& InLocalizationNamespace);

	/** See GetLocalizationNamespace */
	FString GetBaseLocalizationNamespace() const;
#endif // USE_STABLE_LOCALIZATION_KEYS

	/**
	 * Indicates if the custom versions container is in a 'reset' state.  This will be used to defer the choice about how to
	 * populate the container until it is needed, where the read/write state will be known.
	 */
	mutable bool bCustomVersionsAreReset;
};


/**
 * Template for archive constructors.
 */
template<class T> T Arctor(FArchive& Ar)
{
	T Tmp;
	Ar << Tmp;

	return Tmp;
}


/**
 * Base class for archive proxies.
 *
 * Archive proxies are archive types that modify the behavior of another archive type.
 */
class FArchiveProxy : public FArchive
{
public:
	/**
	 * Creates and initializes a new instance of the archive proxy.
	 *
	 * @param InInnerArchive The inner archive to proxy.
	 */
	CORE_API FArchiveProxy(FArchive& InInnerArchive);

	virtual FArchive& operator<<(class FName& Value) override
	{
		InnerArchive << Value;
		return *this;
	}

	virtual FArchive& operator<<(class FText& Value) override
	{
		InnerArchive << Value;
		return *this;
	}

	virtual FArchive& operator<<(class UObject*& Value) override
	{
		InnerArchive << Value;
		return *this;
	}

	virtual FArchive& operator<<(struct FStringAssetReference& Value) override
	{
		InnerArchive << Value;
		return *this;
	}

	virtual void Serialize(void* V, int64 Length) override
	{
		InnerArchive.Serialize(V, Length);
	}

	virtual void SerializeBits(void* Bits, int64 LengthBits) override
	{
		InnerArchive.SerializeBits(Bits, LengthBits);
	}

	virtual void SerializeInt(uint32& Value, uint32 Max) override
	{
		InnerArchive.SerializeInt(Value, Max);
	}

	virtual void Preload(UObject* Object) override
	{
		InnerArchive.Preload(Object);
	}

	virtual void CountBytes(SIZE_T InNum, SIZE_T InMax) override
	{
		InnerArchive.CountBytes(InNum, InMax);
	}

	CORE_API virtual FString GetArchiveName() const override;

	virtual class FLinker* GetLinker() override
	{
		return InnerArchive.GetLinker();
	}

#if USE_STABLE_LOCALIZATION_KEYS
	CORE_API virtual void SetLocalizationNamespace(const FString& InLocalizationNamespace) override;
	CORE_API virtual FString GetLocalizationNamespace() const override;
#endif // USE_STABLE_LOCALIZATION_KEYS

	virtual int64 Tell() override
	{
		return InnerArchive.Tell();
	}

	virtual int64 TotalSize() override
	{
		return InnerArchive.TotalSize();
	}

	virtual bool AtEnd() override
	{
		return InnerArchive.AtEnd();
	}

	virtual void Seek(int64 InPos) override
	{
		InnerArchive.Seek(InPos);
	}

	virtual void AttachBulkData(UObject* Owner, FUntypedBulkData* BulkData) override
	{
		InnerArchive.AttachBulkData(Owner, BulkData);
	}

	virtual void DetachBulkData(FUntypedBulkData* BulkData, bool bEnsureBulkDataIsLoaded) override
	{
		InnerArchive.DetachBulkData(BulkData, bEnsureBulkDataIsLoaded);
	}

	virtual bool Precache(int64 PrecacheOffset, int64 PrecacheSize) override
	{
		return InnerArchive.Precache(PrecacheOffset, PrecacheSize);
	}

	virtual bool SetCompressionMap(TArray<struct FCompressedChunk>* CompressedChunks, ECompressionFlags CompressionFlags) override
	{
		return InnerArchive.SetCompressionMap(CompressedChunks, CompressionFlags);
	}

	virtual void Flush() override
	{
		InnerArchive.Flush();
	}

	virtual bool Close() override
	{
		return InnerArchive.Close();
	}

	virtual bool GetError() override
	{
		return InnerArchive.GetError();
	}

	virtual void MarkScriptSerializationStart(const UObject* Obj) override
	{
		InnerArchive.MarkScriptSerializationStart(Obj);
	}

	virtual void MarkScriptSerializationEnd(const UObject* Obj) override
	{
		InnerArchive.MarkScriptSerializationEnd(Obj);
	}

	virtual bool IsCloseComplete(bool& bHasError) override
	{
		return InnerArchive.IsCloseComplete(bHasError);
	}
#if WITH_EDITOR
	virtual void PushDebugDataString(const FName& DebugData) override
	{
		InnerArchive.PushDebugDataString(DebugData);
	}
	virtual void PopDebugDataString() override
	{
		InnerArchive.PopDebugDataString();
	}
#endif
protected:

	/** Holds the archive that this archive is a proxy to. */
	FArchive& InnerArchive;
};


/**
 * Implements a proxy archive that serializes FNames as string data.
 */
struct FNameAsStringProxyArchive : public FArchiveProxy
{
	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InInnerArchive The inner archive to proxy.
	 */
	 FNameAsStringProxyArchive(FArchive& InInnerArchive)
		 :	FArchiveProxy(InInnerArchive)
	 { }

	 CORE_API virtual FArchive& operator<<(class FName& N);
};


/**
 * Implements a helper structure for compression support
 *
 * This structure contains information on the compressed and uncompressed size of a chunk of data.
 */
struct FCompressedChunkInfo
{
	/** Holds the data's compressed size. */
	int64 CompressedSize;

	/** Holds the data's uncompresses size. */
	int64 UncompressedSize;
};


/**
 * Serializes an FCompressedChunkInfo value from or into an archive.
 *
 * @param Ar The archive to serialize from or to.
 * @param Value The value to serialize.
 */
CORE_API FArchive& operator<<(FArchive& Ar, FCompressedChunkInfo& Value);
