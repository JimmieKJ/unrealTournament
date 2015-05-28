// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "UObject/UTextProperty.h"
#include "Interface.h"
#include "TargetPlatform.h"
#include "UObject/UObjectThreadContext.h"


DEFINE_LOG_CATEGORY_STATIC(LogSavePackage, Log, All);

static const int32 MAX_MERGED_COMPRESSION_CHUNKSIZE = 1024 * 1024;
static const FName WorldClassName = FName("World");


static bool HasDeprecatedOrPendingKillOuter(UObject* InObj, UPackage* InSavingPackage)
{
	UObject* Obj = InObj;
	while (Obj)
	{
		if (Obj->GetClass()->HasAnyClassFlags(CLASS_Deprecated) && !Obj->HasAnyFlags(RF_ClassDefaultObject))
		{
			if (!InObj->IsPendingKill() && InObj->GetOutermost() == InSavingPackage)
			{
				UE_LOG(LogSavePackage, Warning, TEXT("%s has a deprecated outer %s, so it will not be saved"), *InObj->GetFullName(), *Obj->GetFullName());
			}
			return true; 
		}

		if(Obj->IsPendingKill())
		{
			return true;
		}

		Obj = Obj->GetOuter();
	}
	return false;
}

static void CheckObjectPriorToSave(FArchiveUObject& Ar, UObject* InObj, UPackage* InSavingPackage)
{
	if (!InObj)
	{
		return;
	}
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	if (!InObj->IsValidLowLevelFast() || !InObj->IsValidLowLevel())
	{
		UE_LOG(LogLinker, Fatal, TEXT("Attempt to save bogus object %p ThreadContext.SerializedObject=%s  SerializedProperty=%s"), (void*)InObj, *GetFullNameSafe(ThreadContext.SerializedObject), *GetFullNameSafe(Ar.GetSerializedProperty()));
		return;
	}
	// if the object class is abstract or has been marked as deprecated, mark this
	// object as transient so that it isn't serialized
	if ( InObj->GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) )
	{
		if ( !InObj->HasAnyFlags(RF_ClassDefaultObject) || InObj->GetClass()->HasAnyClassFlags(CLASS_Deprecated) )
		{
			InObj->SetFlags(RF_Transient);
		}
		if ( !InObj->HasAnyFlags(RF_ClassDefaultObject) && InObj->GetClass()->HasAnyClassFlags(CLASS_HasInstancedReference) )
		{
			TArray<UObject*> ComponentReferences;
			FReferenceFinder ComponentCollector(ComponentReferences, InObj, false, true, true);
			ComponentCollector.FindReferences(InObj, ThreadContext.SerializedObject, Ar.GetSerializedProperty());

			for ( int32 Index = 0; Index < ComponentReferences.Num(); Index++ )
			{
				ComponentReferences[Index]->SetFlags(RF_Transient);
			}
		}
	}
	else if ( HasDeprecatedOrPendingKillOuter(InObj, InSavingPackage) )
	{
		InObj->SetFlags(RF_Transient);
	}

	if ( InObj->HasAnyFlags(RF_ClassDefaultObject) )
	{
		// if this is the class default object, make sure it's not
		// marked transient for any reason, as we need it to be saved
		// to disk
		InObj->ClearFlags(RF_Transient);
	}

	//@todo instance, we may not need this for very long. It verifies that the fixup on component names will not be needed when we load this
	{
		UObject *Object = InObj;
		UObject *Template = Object->GetArchetype();
		UObject *ThisParent = Object->GetOuter();

		if (
			!Object->IsTemplate(RF_ClassDefaultObject) && 
			Template && 
			ThisParent &&
			!ThisParent->GetClass()->GetDefaultSubobjectByName(Object->GetFName()) &&
			ThisParent->GetClass()->GetDefaultSubobjectByName(Template->GetFName()) &&
			!ThisParent->IsTemplate(RF_ClassDefaultObject) &&
			ThisParent->GetArchetype() == Template->GetOuter()
			)
		{
			ensureMsgf(0, TEXT("Component has wrong name on save: %s archetype: %s"), *Object->GetFullName(), *Template->GetFullName());
		}
	}
}

static bool EndSavingIfCancelled( FLinkerSave* Linker, const FString& TempFilename ) 
{
	if ( GWarn->ReceivedUserCancel() )
	{
		// free the file handle and delete the temporary file
		Linker->Detach();
		IFileManager::Get().Delete( *TempFilename );
		return true;
	}
	return false;
}

static FThreadSafeCounter OutstandingAsyncWrites;

void UPackage::WaitForAsyncFileWrites()
{
	while (OutstandingAsyncWrites.GetValue())
	{
		FPlatformProcess::Sleep(0.0f);
	}
}

void AsyncWriteFile(const TArray<uint8>& Data, const TCHAR* Filename, const FDateTime& TimeStamp)
{
	class FAsyncWriteWorker : public FNonAbandonableTask
	{
	public:
		/** Filename To write to**/
		FString Filename;
		/** Data for the file **/
		TArray<uint8> Data;
		/** Timestamp to give the file. MinValue if shouldn't be modified */
		FDateTime FinalTimeStamp;

		/** Constructor
		*/
		FAsyncWriteWorker(const TCHAR* InFilename, const TArray<uint8>* InData, const FDateTime& InTimeStamp)
			: Filename(InFilename)
			, Data(*InData)
			, FinalTimeStamp(InTimeStamp)
		{
		}
		
		/** Write the file  */
		void DoWork()
		{
			check(Data.Num());
			FString TempFilename; 
			TempFilename = FPaths::GetBaseFilename(Filename, false);
			TempFilename += TEXT(".t");
			if (FFileHelper::SaveArrayToFile(Data,*TempFilename))
			{
				// Clean-up the memory as soon as we save the file to reduce the memory footprint.
				const int64 DataSize = Data.Num(); 
				Data.Empty();
				if (IFileManager::Get().FileSize(*TempFilename) == DataSize)
				{
					if (!IFileManager::Get().Move(*Filename, *TempFilename, true, true, false, false))
					{
						UE_LOG(LogSavePackage, Fatal, TEXT("Could not move to %s."),*Filename);
					}
					else
					{
						if (FinalTimeStamp != FDateTime::MinValue())
						{
							IFileManager::Get().SetTimeStamp(*Filename, FinalTimeStamp);
						}
					}
				}
				else
				{
					UE_LOG(LogSavePackage, Fatal, TEXT("Could not save to %s!"),*TempFilename);
				}
			}
			else
			{
				UE_LOG(LogSavePackage, Fatal, TEXT("Could not write to %s!"),*TempFilename);
			}
			// if everything worked, this is not necessary, but we will make every effort to avoid leaving junk in the cache
			if (FPaths::FileExists(TempFilename))
			{
				IFileManager::Get().Delete(*TempFilename);
			}
			OutstandingAsyncWrites.Decrement();
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncWriteWorker, STATGROUP_ThreadPoolAsyncTasks);
		}
	};

	OutstandingAsyncWrites.Increment();
	(new FAutoDeleteAsyncTask<FAsyncWriteWorker>(Filename, &Data, TimeStamp))->StartBackgroundTask();
}

/** 
 * Helper object for all of the cross object state that is needed only while saving a package.
 */
class FSavePackageState
{
public:
	/** 
	 * Mark a FName as referenced.
	 * @param Name name to mark as referenced
	 */
	void MarkNameAsReferenced(const FName& Name)
	{
		// We need to store the FName without the number, as the number is stored separately by FLinkerSave 
		// and we don't want duplicate entries in the name table just because of the number
		const FName NameNoNumber(Name, 0);
		ReferencedNames.Add(NameNoNumber);
	}

	/** 
	 * Add the marked names to a linker
	 * @param Linker linker to save the marked names to
	 */
	void UpdateLinkerWithMarkedNames(FLinkerSave* Linker)
	{
		Linker->NameMap.Reserve(Linker->NameMap.Num() + ReferencedNames.Num());
		for (const FName& Name : ReferencedNames)
		{
			Linker->NameMap.Add(Name);
		}
	}

private:
	TSet<FName, FLinkerNamePairKeyFuncs> ReferencedNames;
};

static FSavePackageState* SavePackageState = NULL;

/** 
 * Helper object to scope the package state in SavePackage()
 */
class FScopeSavePackageState
{
public:
	FScopeSavePackageState()
	{
		check(!SavePackageState);
		SavePackageState = new FSavePackageState;
	}
	~FScopeSavePackageState()
	{
		check(SavePackageState);
		delete SavePackageState;
		SavePackageState = NULL;
	}
};


/**
 * Archive for tagging objects and names that must be exported
 * to the file.  It tags the objects passed to it, and recursively
 * tags all of the objects this object references.
 */
class FArchiveSaveTagExports : public FArchiveUObject
{
public:
	/**
	 * Constructor
	 * 
	 * @param	InOuter		the package to save
	 */
	FArchiveSaveTagExports( UPackage* InOuter )
	: Outer(InOuter)
	{
		ArIsSaving				= true;
		ArIsPersistent			= true;
		ArIsObjectReferenceCollector = true;
		ArShouldSkipBulkData	= true;
	}

	void ProcessBaseObject( UObject* BaseObject );
	FArchive& operator<<( UObject*& Obj );

	/**
	 * Package we're currently saving.  Only objects contained
	 * within this package will be tagged for serialization.
	 */
	UPackage* Outer;

	/**
	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const;

private:

	TArray<UObject*> TaggedObjects;

	void ProcessTaggedObjects();
};

/**
 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
 * is in when a loading error occurs.
 *
 * This is overridden for the specific Archive Types
 **/
FString FArchiveSaveTagExports::GetArchiveName() const
{
	return Outer != nullptr
		? *FString::Printf(TEXT("SaveTagExports (%s)"), *Outer->GetName())
		: TEXT("SaveTagExports");
}

FArchive& FArchiveSaveTagExports::operator<<( UObject*& Obj )
{
	check(Outer);
	CheckObjectPriorToSave(*this, Obj, Outer);
	if( Obj && Obj->IsIn(Outer) && !Obj->HasAnyFlags(RF_Transient) && !Obj->HasAnyMarks(OBJECTMARK_TagExp) )
	{
#if 0
		// the following line should be used to track down the cause behind
		// "Load flags don't match result of *" errors.  The most common reason
		// is a child component template is modifying the load flags of it parent
		// component template (that is, the component template that it is overriding
		// from a parent class).
		// @MISMATCHERROR
		if ( Obj->GetFName() == TEXT("BrushComponent0") && Obj->GetOuter()->GetFName() == TEXT("Default__Volume") )
		{
			UE_LOG(LogSavePackage, Log, TEXT(""));
		}
#endif
		// Set flags.
		Obj->Mark(OBJECTMARK_TagExp);

		// first, serialize this object's archetype so that if the archetype's load flags are set correctly if this object
		// is encountered by the SaveTagExports archive before its archetype.  This is necessary for the code below which verifies
		// that the load flags for the archetype and the load flags for the object match
		UObject* Template = Obj->GetArchetype();
		*this << Template;

		// class default objects should always be loaded
		if ( Obj->HasAnyFlags(RF_ClassDefaultObject) )
		{
			if ( Obj->GetClass()->HasAnyClassFlags(CLASS_Intrinsic) )
			{
				// if this class is an intrinsic class, its class default object
				// shouldn't be saved, as it does not contain any data that should be saved
				Obj->UnMark(OBJECTMARK_TagExp);
			}
		}
		else
		{
			// We always set up the context flags on save based on the virtual methods.
			// The base virtual methods will leave the flags unchanged.
			// Once should not read the flags directly (other than at load) because the virtual methods are authoritative.

			// if anything in the outer chain is NotFor, then we are also NotFor. Stop this search at public objects.

			UObject* Search = Obj;

			const EObjectMark ObjectMarks = UPackage::GetObjectMarksForTargetPlatform( CookingTarget(), IsCooking() );

			do
			{
				if( !Search->NeedsLoadForClient() ) 
				{
					Obj->Mark(OBJECTMARK_NotForClient);
				}

				if( !Search->NeedsLoadForServer() )
				{
					Obj->Mark(OBJECTMARK_NotForServer);
				}

				if (Search->HasAnyFlags(RF_Public))
				{
					break;
				}
				Search = Search->GetOuter();
			} while (Search && !Obj->HasAllMarks(ObjectMarks));

			{
				bool bNeedsLoadForEditorGame = false;
				for (UObject* OuterIt = Obj; OuterIt; OuterIt = OuterIt->GetOuter())
				{
					if (OuterIt->NeedsLoadForEditorGame())
					{
						bNeedsLoadForEditorGame = true;
						break;
					}
				}
				if (!bNeedsLoadForEditorGame)
				{
					Obj->Mark(OBJECTMARK_NotForEditorGame);
				}
			}

			// skip these checks if the Template object is the CDO for an intrinsic class, as they will never have any load flags set.
			if ( Template != NULL 
			&& (!Template->GetClass()->HasAnyClassFlags(CLASS_Intrinsic) || !Template->HasAnyFlags(RF_ClassDefaultObject)) )
			{
				// if the object's Template is not in the same package, we can't adjust its load flags to ensure that this object can be loaded
				// therefore make this a critical error so that we don't discover later that we won't be able to load this object.
				if ( !Template->IsIn(Obj->GetOutermost()) )
				{
					// if the component's archetype is not in the same package, we won't be able
					// to propagate the load flags to the archetype, so make sure that the
					// derived template's load flags are <= the parent's load flags.
					FString LoadFlagsString;
					if ( !Obj->HasAnyMarks(OBJECTMARK_NotForClient) && Template->HasAnyMarks(OBJECTMARK_NotForClient) )
					{
						if ( LoadFlagsString.Len() > 0 )
						{
							LoadFlagsString += TEXT(",");
						}
						LoadFlagsString += TEXT("OBJECTMARK_NotForClient");
					}
					if ( !Obj->HasAnyMarks(OBJECTMARK_NotForServer) && Template->HasAnyMarks(OBJECTMARK_NotForServer) )
					{
						if ( LoadFlagsString.Len() > 0 )
						{
							LoadFlagsString += TEXT(",");
						}
						LoadFlagsString += TEXT("OBJECTMARK_NotForServer");
					}

					if ( LoadFlagsString.Len() > 0 )
					{
						//@todo localize
						UE_LOG(LogSavePackage, Fatal,TEXT("Mismatched load flag/s (%s) on object archetype from different package.  Loading '%s' would fail because its archetype '%s' wouldn't be created."),
							*LoadFlagsString, *Obj->GetPathName(), *Template->GetPathName());
					}
				}

				// this is a normal object - it's template MUST be loaded anytime
				// the object is loaded, so make sure the load flags match the object's
				// load flags (note that it's OK for the template itself to be loaded
				// in situations where the object is not, but vice-versa is not OK)
				if( !Obj->HasAnyMarks(OBJECTMARK_NotForClient) )
				{
					Template->UnMark(OBJECTMARK_NotForClient);
				}

				if( !Obj->HasAnyMarks(OBJECTMARK_NotForServer) )
				{
					Template->UnMark(OBJECTMARK_NotForServer);
				}
			}
		}

		// Recurse with this object's class and package.
		UObject* Class  = Obj->GetClass();
		UObject* Parent = Obj->GetOuter();
		*this << Class << Parent;

		TaggedObjects.Add(Obj);
	}
	return *this;
}

/**
 * Serializes the specified object, tagging all objects it references.
 *
 * @param	BaseObject	the object that should be serialized; usually the package root or
 *						[in the case of a map package] the map's UWorld object.
 */
void FArchiveSaveTagExports::ProcessBaseObject(UObject* BaseObject )
{
	(*this) << BaseObject;
	ProcessTaggedObjects();
}

/**
 * Iterates over all objects which were encountered during serialization of the root object, serializing each one in turn.
 * Objects encountered during that serialization are then added to the array and iteration continues until no new objects are
 * added to the array.
 */
void FArchiveSaveTagExports::ProcessTaggedObjects()
{
	const int32 ArrayPreSize = 1024; // Was originally total number of objects, but this was unreasonably large
	TArray<UObject*> CurrentlyTaggedObjects;
	CurrentlyTaggedObjects.Empty(ArrayPreSize);
	while (TaggedObjects.Num())
	{
		CurrentlyTaggedObjects += TaggedObjects;
		TaggedObjects.Empty();

		for (int32 ObjIndex = 0; ObjIndex < CurrentlyTaggedObjects.Num(); ObjIndex++)
		{
			UObject* Obj = CurrentlyTaggedObjects[ObjIndex];

			// Recurse with this object's children.
			if (Obj->HasAnyFlags(RF_ClassDefaultObject))
			{
				Obj->GetClass()->SerializeDefaultObject(Obj, *this);
			}
			else
			{
				Obj->Serialize(*this);
			}
		}

		CurrentlyTaggedObjects.Empty(ArrayPreSize);
	}
}

/**
 * Archive for tagging objects and names that must be listed in the
 * file's imports table.
 */
class FArchiveSaveTagImports : public FArchiveUObject
{
public:
	FLinkerSave* Linker;
	TArray<UObject*> Dependencies;
	TArray<FString>& StringAssetReferencesMap;

	FArchiveSaveTagImports(FLinkerSave* InLinker, TArray<FString>& InStringAssetReferencesMap)
		: Linker(InLinker), StringAssetReferencesMap(InStringAssetReferencesMap)
	{
		ArIsSaving				= true;
		ArIsPersistent			= true;
		ArIsObjectReferenceCollector = true;
		ArShouldSkipBulkData	= true;
		if ( Linker != NULL )
		{
			ArPortFlags = Linker->GetPortFlags();
			SetCookingTarget(Linker->CookingTarget());
		}
	}
	FArchive& operator<<(UObject*& Obj) override;
	FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override;
	FArchive& operator<<(FAssetPtr& AssetPtr) override;
	FArchive& operator<<(FStringAssetReference& Value) override
	{
		if ( Value.IsValid() )
		{
			StringAssetReferencesMap.Add(Value.ToString());
		}
		return *this;
	}
	FArchive& operator<<(FName& Name) override
	{
		SavePackageState->MarkNameAsReferenced(Name);
		return *this;
	}

	/**
	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const override;

};


/**
 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
 * is in when a loading error occurs.
 *
 * This is overridden for the specific Archive Types
 **/
FString FArchiveSaveTagImports::GetArchiveName() const
{
	if ( Linker != NULL && Linker->LinkerRoot )
	{
		return FString::Printf(TEXT("SaveTagImports (%s)"), *Linker->LinkerRoot->GetName());
	}

	return TEXT("SaveTagImports");
}

FArchive& FArchiveSaveTagImports::operator<<( UObject*& Obj )
{
	CheckObjectPriorToSave(*this, Obj, NULL);

	const EObjectMark ObjectMarks = UPackage::GetObjectMarksForTargetPlatform( CookingTarget(), IsCooking() );
	
	// Skip PendingKill objects and objects that are both not for client and not for server when cooking.
	if( Obj && !Obj->IsPendingKill() && (!IsCooking() || !Obj->HasAllMarks(ObjectMarks)))
	{
		if( !Obj->HasAnyFlags(RF_Transient) || Obj->HasAllFlags(RF_Native) )
		{
			// remember it as a dependency, unless it's a top level package or native
			bool bIsTopLevelPackage = Obj->GetOuter() == NULL && dynamic_cast<UPackage*>(Obj);
			bool bIsNative = Obj->HasAnyFlags(RF_Native);
			UObject* Outer = Obj->GetOuter();
			
			// go up looking for native classes
			while (!bIsNative && Outer)
			{
				if (dynamic_cast<UClass*>(Outer) && Outer->HasAnyFlags(RF_Native))
				{
					bIsNative = true;
				}
				Outer = Outer->GetOuter();
			}

			// only add valid objects
			if (!bIsTopLevelPackage && !bIsNative)
			{
				Dependencies.AddUnique(Obj);
			}

			if( !Obj->HasAnyMarks(OBJECTMARK_TagExp) )  
			{
				// mark this object as an import
				Obj->Mark(OBJECTMARK_TagImp);

				if ( Obj->HasAnyFlags(RF_ClassDefaultObject) )
				{
					Obj->UnMark(OBJECTMARK_NotForClient);
					Obj->UnMark(OBJECTMARK_NotForServer);
				}

				UObject* Parent = Obj->GetOuter();
				if( Parent )
				{
					*this << Parent;
				}
			}
		}
	}
	return *this;
}

FArchive& FArchiveSaveTagImports::operator<<( FLazyObjectPtr& LazyObjectPtr)
{
	FUniqueObjectGuid ID;
	ID = LazyObjectPtr.GetUniqueID();
	return *this << ID;
}

FArchive& FArchiveSaveTagImports::operator<<( FAssetPtr& AssetPtr)
{
	FStringAssetReference ID;
	UObject *Object = AssetPtr.Get();

	if (Object)
	{
		// Use object in case name has changed. 
		ID = FStringAssetReference(Object);
	}
	else
	{
		ID = AssetPtr.GetUniqueID();
	}
	return *this << ID;
}


/**
 * Helper class for package compression.
 */
struct FFileCompressionHelper
{
public:
	/**
	 * Compresses the passed in src file and writes it to destination.
	 *
	 * @param	SrcFilename		Filename to compress
	 * @param	DstFilename		Output name of compressed file, cannot be the same as SrcFilename
	 * @param	SrcLinker		FLinkerSave object used to save src file
	 *
	 * @return true if sucessful, false otherwise
	 */
	bool CompressFile( const TCHAR* SrcFilename, const TCHAR* DstFilename, FLinkerSave* SrcLinker )
	{
		FString TmpFilename = FPaths::GetPath( DstFilename ) / ( FPaths::GetBaseFilename( DstFilename ) + TEXT( "_SaveCompressed.tmp" ));

		// Create file reader and writer...
		FArchive* FileReader = IFileManager::Get().CreateFileReader( SrcFilename );
		FArchive* FileWriter = IFileManager::Get().CreateFileWriter( *TmpFilename );
		// ... and abort if either operation wasn't successful.
		if( !FileReader || !FileWriter )
		{
			// Delete potentially created reader or writer.
			delete FileReader;
			delete FileWriter;
			// Delete temporary file.
			IFileManager::Get().Delete( *TmpFilename );
			// Failure.
			return false;
		}

		CompressArchive(FileReader,FileWriter,SrcLinker);

		// Tear down file reader and write. This will flush the writer first.
		delete FileReader;
		delete FileWriter;

		// Compression was successful, now move file.
		bool bMoveSucceded = IFileManager::Get().Move( DstFilename, *TmpFilename );
		return bMoveSucceded;
	}
	/**
	 * Compresses the passed in src file and writes it to destination. The file comes from the "Saver" in the FLinker
	 *
	 * @param	DstFilename		Output name of compressed file, cannot be the same as SrcFilename
	 * @param	SrcLinker		FLinkerSave object used to save src file
	 *
	 * @return true if sucessful, false otherwise
	 */
	bool CompressFile( const TCHAR* DstFilename, FLinkerSave* SrcLinker )
	{
		FString TmpFilename = FPaths::GetPath( DstFilename ) / ( FPaths::GetBaseFilename( DstFilename ) + TEXT( "_SaveCompressed.tmp" ));
		// Create file reader and writer...
		FMemoryReader Reader(*(FBufferArchive*)(SrcLinker->Saver),true);
		FArchive* FileWriter = IFileManager::Get().CreateFileWriter( *TmpFilename );
		// ... and abort if either operation wasn't successful.
		if( !FileWriter )
		{
			// Delete potentially created reader or writer.
			delete FileWriter;
			// Delete temporary file.
			IFileManager::Get().Delete( *TmpFilename );
			// Failure.
			return false;
		}


		CompressArchive(&Reader,FileWriter,SrcLinker);

		// Tear down file writer. This will flush the writer first.
		delete FileWriter;

		// Compression was successful, now move file.
		bool bMoveSucceded = IFileManager::Get().Move( DstFilename, *TmpFilename );
		return bMoveSucceded;
	}
	/**
	 * Compresses the passed in src archive and writes it to destination archive.
	 *
	 * @param	FileReader		archive to read from
	 * @param	FileWriter		archive to write to
	 * @param	SrcLinker		FLinkerSave object used to save src file
	 *
	 * @return true if sucessful, false otherwise
	 */
	void CompressArchive( FArchive* FileReader, FArchive* FileWriter, FLinkerSave* SrcLinker )
	{

		// Read package file summary from source file.
		FPackageFileSummary FileSummary;
		(*FileReader) << FileSummary;

		// Propagate byte swapping.
		FileWriter->SetByteSwapping( SrcLinker->ForceByteSwapping() );

		// We don't compress the package file summary but treat everything afterwards
		// till the first export as a single chunk. This basically lumps name and import 
		// tables into one compressed block.
		int32 StartOffset			= FileReader->Tell();
		int32 RemainingHeaderSize	= SrcLinker->Summary.TotalHeaderSize - StartOffset;
		CurrentChunk.UncompressedSize	= RemainingHeaderSize;
		CurrentChunk.UncompressedOffset	= StartOffset;

		// finish header chunk
		FinishCurrentAndCreateNewChunk( 0 );
		
		// Iterate over all exports and add them separately. The underlying code will take
		// care of merging small blocks.
		for( int32 ExportIndex=0; ExportIndex<SrcLinker->ExportMap.Num(); ExportIndex++ )
		{
			const FObjectExport& Export = SrcLinker->ExportMap[ExportIndex];		
			AddToChunk( Export.SerialSize );
		}
		
		// Finish chunk in flight and reset current chunk with size 0.
		FinishCurrentAndCreateNewChunk( 0 );

		ECompressionFlags BaseCompressionMethod = COMPRESS_Default;
		if (FileWriter->IsCooking())
		{
			BaseCompressionMethod = FileWriter->CookingTarget()->GetBaseCompressionMethod();
		}

		// Write base version of package file summary after updating compressed chunks array and compression flags.
		FileSummary.CompressionFlags = BaseCompressionMethod;
		FileSummary.CompressedChunks = CompressedChunks;
		(*FileWriter) << FileSummary;

		// Reset internal state so subsequent calls will work.
		CompressedChunks.Empty();
		CurrentChunk = FCompressedChunk();

		// Allocate temporary buffers for reading and compression.
		int32	SrcBufferSize	= RemainingHeaderSize;
		void*	SrcBuffer		= FMemory::Malloc( SrcBufferSize );

		// Iterate over all chunks, read the data, compress and write it out to destination file.
		for( int32 ChunkIndex=0; ChunkIndex<FileSummary.CompressedChunks.Num(); ChunkIndex++ )
		{
			FCompressedChunk& Chunk = FileSummary.CompressedChunks[ChunkIndex];

			// Increase temporary buffer sizes if they are too small.
			if( SrcBufferSize < Chunk.UncompressedSize )
			{
				SrcBufferSize = Chunk.UncompressedSize;
				SrcBuffer = FMemory::Realloc( SrcBuffer, SrcBufferSize );
			}
			
			// Verify that we're not skipping any data.
			check( Chunk.UncompressedOffset == FileReader->Tell() );

			// Read src/ uncompressed data.
			FileReader->Serialize( SrcBuffer, Chunk.UncompressedSize );

			// Keep track of offset.
			Chunk.CompressedOffset	= FileWriter->Tell();
			// Serialize compressed. This is compatible with async LoadCompressedData.
			FileWriter->SerializeCompressed( SrcBuffer, Chunk.UncompressedSize, (ECompressionFlags) FileSummary.CompressionFlags );
			// Keep track of compressed size.
			Chunk.CompressedSize	= FileWriter->Tell() - Chunk.CompressedOffset;		
		}

		// get the start of the bulkdata and update it in the summary
		FileSummary.BulkDataStartOffset = FileWriter->Tell();
		
		// serialize the bulkdata directly, as bulkdata is handling the compression already
		int64 SizeToCopy = FileReader->TotalSize() - FileReader->Tell();
		if (SrcBufferSize < SizeToCopy)
		{
			SrcBufferSize = SizeToCopy;
			SrcBuffer = FMemory::Realloc( SrcBuffer, SizeToCopy );
		}

		// Read src/ uncompressed data.
		FileReader->Serialize( SrcBuffer, SizeToCopy );
		FileWriter->Serialize( SrcBuffer, SizeToCopy );

		// Verify that we've compressed everything.
		check( FileReader->AtEnd() );

		// Serialize file summary again - this time CompressedChunks array is going to contain compressed size and offsets.
		FileWriter->Seek( 0 );
		(*FileWriter) << FileSummary;

		// Free intermediate buffers.
		FMemory::Free( SrcBuffer );

	}

	/**
	 * Compresses the passed in src file and writes it to destination.
	 *
	 * @param	SrcFilename			Filename to compress
	 * @param	DstFilename			Output name of compressed file, cannot be the same as SrcFilename
	 * @param	bForceByteswapping	Should the output file be force-byteswapped (we can't tell as there's no Source Linker with the info)
	 *
	 * @return true if successful, false otherwise
	 */
	bool FullyCompressFile( const TCHAR* SrcFilename, const TCHAR* DstFilename, bool bForceByteSwapping )
	{
		FString TmpFilename = FPaths::GetPath( DstFilename ) / TEXT( "SaveCompressed.tmp" );

		// Create file reader and writer...
		FArchive* FileReader = IFileManager::Get().CreateFileReader( SrcFilename );
		FArchive* FileWriter = IFileManager::Get().CreateFileWriter( *TmpFilename );

		// ... and abort if either operation wasn't successful.
		if( !FileReader || !FileWriter )
		{
			// Delete potentially created reader or writer.
			delete FileReader;
			delete FileWriter;
			// Delete temporary file.
			IFileManager::Get().Delete( *TmpFilename );
			// Failure.
			return false;
		}

		// read in source file
		const int32 FileSize = FileReader->TotalSize();
		
		// Force byte swapping if needed
		FileWriter->SetByteSwapping(bForceByteSwapping);
		
		// write it out compressed (passing in the FileReader so that the writer can read in small chunks to avoid 
		// single huge allocation of the entire source package size)
		FileWriter->SerializeCompressed(FileReader, FileSize, COMPRESS_Default, true);
		
		delete FileReader;
		delete FileWriter;
		
		// make the 
		// Compression was successful, now move file.
		bool bMoveSucceded = IFileManager::Get().Move( DstFilename, *TmpFilename );

		// if the move succeeded, create a .uncompressed_size file in the cooked directory so that the 
		// cooker frontend can create the table of contents
		if (bMoveSucceded)
		{
			// write out the size of the original file to the string
			FString SizeString = FString::Printf(TEXT("%d%s"), FileSize, LINE_TERMINATOR);
			FFileHelper::SaveStringToFile(SizeString, *(FString(DstFilename) + TEXT(".uncompressed_size")));
		}
		return bMoveSucceded;
	}

private:
	/**
	 * Tries to add bytes to current chunk and creates a new one if there is not enough space.
	 *
	 * @param	Size	Number of bytes to try to add to current chunk
	 */
	void AddToChunk( int32 Size )
	{
		// Resulting chunk would be too big.
		if( CurrentChunk.UncompressedSize > 0 && CurrentChunk.UncompressedSize + Size > MAX_MERGED_COMPRESSION_CHUNKSIZE )
		{
			// Finish up current chunk and create a new one of passed in size.
			FinishCurrentAndCreateNewChunk( Size );
		}
		// Add bytes to existing chunk.
		else
		{
			CurrentChunk.UncompressedSize += Size;
		}
	}

	/**
	 * Finish current chunk and add it to the CompressedChunks array. This also creates a new
	 * chunk with a base size passed in.
	 *
	 * @param	Size	Size in bytes of new chunk to create.
	 */
	void FinishCurrentAndCreateNewChunk( int32 Size )
	{
		CompressedChunks.Add( CurrentChunk );
		int32 Offset = CurrentChunk.UncompressedOffset + CurrentChunk.UncompressedSize;
		CurrentChunk = FCompressedChunk();
		CurrentChunk.UncompressedOffset	= Offset;
		CurrentChunk.UncompressedSize	= Size;
	}
	
	/** Compressed chunks, populated by AddToChunk/ FinishCurrentAndCreateNewChunk.	*/
	TArray<FCompressedChunk>	CompressedChunks;
	/** Current chunk, used by merging code.										*/
	FCompressedChunk			CurrentChunk;
};


/**
 * Find most likely culprit that caused the objects in the passed in array to be considered for saving.
 *
 * @param	BadObjects	array of objects that are considered "bad" (e.g. non- RF_Public, in different map package, ...)
 * @return	UObject that is considered the most likely culprit causing them to be referenced or NULL
 */
static void FindMostLikelyCulprit( TArray<UObject*> BadObjects, UObject*& MostLikelyCulprit, const UProperty*& PropertyRef )
{
	MostLikelyCulprit	= NULL;

	// Iterate over all objects that are marked as unserializable/ bad and print out their referencers.
	for( int32 BadObjIndex=0; BadObjIndex<BadObjects.Num(); BadObjIndex++ )
	{
		UObject* Obj = BadObjects[BadObjIndex];

		UE_LOG(LogSavePackage, Warning, TEXT("\r\nReferencers of %s:"), *Obj->GetFullName() );
		
		FReferencerInformationList Refs;

		if ( IsReferenced( Obj,RF_Native | RF_Public, true, &Refs) )
		{	
			for (int32 i = 0; i < Refs.ExternalReferences.Num(); i++)
			{
				UObject* RefObj = Refs.ExternalReferences[i].Referencer;
				if( RefObj->HasAnyMarks(EObjectMark(OBJECTMARK_TagExp|OBJECTMARK_TagImp)) )
				{
					if ( RefObj->GetFName() == NAME_PersistentLevel || RefObj->GetClass()->GetFName() == WorldClassName )
					{
						// these types of references should be ignored
						continue;
					}

					UE_LOG(LogSavePackage, Warning, TEXT("\t%s (%i refs)"), *RefObj->GetFullName(), Refs.ExternalReferences[i].TotalReferences );
					for ( int32 j = 0; j < Refs.ExternalReferences[i].ReferencingProperties.Num(); j++ )
					{
						const UProperty* Prop = Refs.ExternalReferences[i].ReferencingProperties[j];
						UE_LOG(LogSavePackage, Warning, TEXT("\t\t%i) %s"), j, *Prop->GetFullName());
						PropertyRef = Prop;
					}
			
					MostLikelyCulprit = Obj;
				}
			}
		}
	}
}

/**
 * Helper structure encapsulating functionality to sort a linker's name map according to the order of the names a package being conformed against.
 */
struct FObjectNameSortHelper
{
private:
	/** the linker that we're sorting names for */
	friend struct TDereferenceWrapper<FName, FObjectNameSortHelper>;

	/** Comparison function used by Sort */
	FORCEINLINE bool operator()( const FName& A, const FName& B ) const
	{
		return A.Compare(B) < 0;
	}

public:


	/**
	 * Sorts names according to the order in which they occur in the list of NameIndices.  If a package is specified to be conformed against, ensures that the order
	 * of the names match the order in which the corresponding names occur in the old package.
	 *
	 * @param	Linker				linker containing the names that need to be sorted
	 * @param	LinkerToConformTo	optional linker to conform against.
	 */
	void SortNames( FLinkerSave* Linker, FLinkerLoad* LinkerToConformTo=NULL )
	{
		int32 SortStartPosition = 0;

		if ( LinkerToConformTo != NULL )
		{
			SortStartPosition = LinkerToConformTo->NameMap.Num();
			TArray<FName> ConformedNameMap = LinkerToConformTo->NameMap;
			for ( int32 NameIndex = 0; NameIndex < Linker->NameMap.Num(); NameIndex++ )
			{
				FName& CurrentName = Linker->NameMap[NameIndex];
				if ( !ConformedNameMap.Contains(CurrentName) )
				{
					ConformedNameMap.Add(CurrentName);
				}
			}

			Linker->NameMap = ConformedNameMap;
			for ( int32 NameIndex = 0; NameIndex < Linker->NameMap.Num(); NameIndex++ )
			{
				FName& CurrentName = Linker->NameMap[NameIndex];
				SavePackageState->MarkNameAsReferenced(CurrentName);
			}
		}

		if ( SortStartPosition < Linker->NameMap.Num() )
		{
			Sort( &Linker->NameMap[SortStartPosition], Linker->NameMap.Num() - SortStartPosition, FObjectNameSortHelper() );
		}
	}
};

/**
 * Helper structure to encapsulate sorting a linker's import table according to the of the import table of the package being conformed against.
 */
struct FObjectImportSortHelper
{
private:
	/**
	 * Allows Compare access to the object full name lookup map
	 */
	static FObjectImportSortHelper* Sorter;

	/**
	 * Map of UObject => full name; optimization for sorting.
	 */
	TMap<UObject*,FString>			ObjectToFullNameMap;

	/** the linker that we're sorting names for */
	friend struct TDereferenceWrapper<FObjectImport,FObjectImportSortHelper>;

	/** Comparison function used by Sort */
	bool operator()( const FObjectImport& A, const FObjectImport& B ) const
	{
		checkSlow(Sorter);

		int32 Result = 0;
		if ( A.XObject == NULL )
		{
			Result = 1;
		}
		else if ( B.XObject == NULL )
		{
			Result = -1;
		}
		else
		{
			FString* FullNameA = Sorter->ObjectToFullNameMap.Find(A.XObject);
			FString* FullNameB = Sorter->ObjectToFullNameMap.Find(B.XObject);
			checkSlow(FullNameA);
			checkSlow(FullNameB);

			Result = FCString::Stricmp(**FullNameA, **FullNameB);
		}

		return Result < 0;
	}

public:

	/**
	 * Sorts imports according to the order in which they occur in the list of imports.  If a package is specified to be conformed against, ensures that the order
	 * of the imports match the order in which the corresponding imports occur in the old package.
	 *
	 * @param	Linker				linker containing the imports that need to be sorted
	 * @param	LinkerToConformTo	optional linker to conform against.
	 */
	void SortImports( FLinkerSave* Linker, FLinkerLoad* LinkerToConformTo=NULL )
	{
		int32 SortStartPosition=0;
		TArray<FObjectImport>& Imports = Linker->ImportMap;
		if ( LinkerToConformTo )
		{
			// intended to be a copy
			TArray<FObjectImport> Orig = Imports;
			Imports.Empty(Imports.Num());

			// this array tracks which imports from the new package exist in the old package
			TArray<uint8> Used;
			Used.AddZeroed(Orig.Num());

			TMap<FString,int32> OriginalImportIndexes;
			for ( int32 i = 0; i < Orig.Num(); i++ )
			{
				FObjectImport& Import = Orig[i];
				FString ImportFullName = Import.XObject->GetFullName();

				OriginalImportIndexes.Add( *ImportFullName, i );
				ObjectToFullNameMap.Add(Import.XObject, *ImportFullName);
			}

			for( int32 i=0; i<LinkerToConformTo->ImportMap.Num(); i++ )
			{
				// determine whether the new version of the package contains this export from the old package
				int32* OriginalImportPosition = OriginalImportIndexes.Find( *LinkerToConformTo->GetImportFullName(i) );
				if( OriginalImportPosition )
				{
					// this import exists in the new package as well,
					// create a copy of the FObjectImport located at the original index and place it
					// into the matching position in the new package's import map
					FObjectImport* NewImport = new(Imports) FObjectImport( Orig[*OriginalImportPosition] );
					check(NewImport->XObject == Orig[*OriginalImportPosition].XObject);
					Used[ *OriginalImportPosition ] = 1;
				}
				else
				{
					// this import no longer exists in the new package
					new(Imports)FObjectImport( NULL );
				}
			}

			SortStartPosition = LinkerToConformTo->ImportMap.Num();
			for( int32 i=0; i<Used.Num(); i++ )
			{
				if( !Used[i] )
				{
					// the FObjectImport located at pos "i" in the original import table did not
					// exist in the old package - add it to the end of the import table
					new(Imports) FObjectImport( Orig[i] );
				}
			}
		}
		else
		{
			for ( int32 ImportIndex = 0; ImportIndex < Linker->ImportMap.Num(); ImportIndex++ )
			{
				const FObjectImport& Import = Linker->ImportMap[ImportIndex];
				if ( Import.XObject )
				{
					ObjectToFullNameMap.Add(Import.XObject, *Import.XObject->GetFullName());
				}
			}
		}

		if ( SortStartPosition < Linker->ImportMap.Num() )
		{
			Sorter = this;
			Sort( &Linker->ImportMap[SortStartPosition], Linker->ImportMap.Num() - SortStartPosition, FObjectImportSortHelper() );
			Sorter = NULL;
		}
	}
};
FObjectImportSortHelper* FObjectImportSortHelper::Sorter = NULL;

/**
 * Helper structure to encapsulate sorting a linker's export table alphabetically, taking into account conforming to other linkers.
 */
struct FObjectExportSortHelper
{
private:
	/**
	 * Allows Compare access to the object full name lookup map
	 */
	static FObjectExportSortHelper* Sorter;

	/**
	 * Map of UObject => full name; optimization for sorting.
	 */
	TMap<UObject*,FString>			ObjectToFullNameMap;

	/** the linker that we're sorting exports for */
	friend struct TDereferenceWrapper<FObjectExport,FObjectExportSortHelper>;

	/** Comparison function used by Sort */
	bool operator()( const FObjectExport& A, const FObjectExport& B ) const
	{
		checkSlow(Sorter);

		int32 Result = 0;
		if ( A.Object == NULL )
		{
			Result = 1;
		}
		else if ( B.Object == NULL )
		{
			Result = -1;
		}
		else
		{
			FString* FullNameA = Sorter->ObjectToFullNameMap.Find(A.Object);
			FString* FullNameB = Sorter->ObjectToFullNameMap.Find(B.Object);
			checkSlow(FullNameA);
			checkSlow(FullNameB);

			Result = FCString::Stricmp(**FullNameA, **FullNameB);
		}

		return Result < 0;
	}

public:
	/**
	 * Sorts exports alphabetically.  If a package is specified to be conformed against, ensures that the order
	 * of the exports match the order in which the corresponding exports occur in the old package.
	 *
	 * @param	Linker				linker containing the exports that need to be sorted
	 * @param	LinkerToConformTo	optional linker to conform against.
	 */
	void SortExports( FLinkerSave* Linker, FLinkerLoad* LinkerToConformTo=NULL )
	{
		int32 SortStartPosition=0;
		if ( LinkerToConformTo )
		{
			// build a map of object full names to the index into the new linker's export map prior to sorting.
			// we need to do a little trickery here to generate an object path name that will match what we'll get back
			// when we call GetExportFullName on the LinkerToConformTo's exports, due to localized packages and forced exports.
			const FString LinkerName = Linker->LinkerRoot->GetName();
			const FString PathNamePrefix = LinkerName + TEXT(".");

			// Populate object to current index map.
			TMap<FString,int32> OriginalExportIndexes;
			for( int32 ExportIndex=0; ExportIndex < Linker->ExportMap.Num(); ExportIndex++ )
			{
				const FObjectExport& Export = Linker->ExportMap[ExportIndex];
				if( Export.Object )
				{
					// get the path name for this object; if the object is contained within the package we're saving,
					// we don't want the returned path name to contain the package name since we'll be adding that on
					// to ensure that forced exports have the same outermost name as the non-forced exports
					FString ObjectPathName = Export.Object != Linker->LinkerRoot
						? Export.Object->GetPathName(Linker->LinkerRoot)
						: LinkerName;
						
					FString ExportFullName = Export.Object->GetClass()->GetName() + TEXT(" ") + PathNamePrefix + ObjectPathName;

					// Set the index (key) in the map to the index of this object into the export map.
					OriginalExportIndexes.Add( *ExportFullName, ExportIndex );
					ObjectToFullNameMap.Add(Export.Object, *ExportFullName);
				}
			}

			// backup the existing export list so we can empty the linker's actual list
			TArray<FObjectExport> OldExportMap = Linker->ExportMap;
			Linker->ExportMap.Empty(Linker->ExportMap.Num());

			// this array tracks which exports from the new package exist in the old package
			TArray<uint8> Used;
			Used.AddZeroed(OldExportMap.Num());

			for( int32 i = 0; i<LinkerToConformTo->ExportMap.Num(); i++ )
			{
				// determine whether the new version of the package contains this export from the old package
				FString ExportFullName = LinkerToConformTo->GetExportFullName(i, *LinkerName);
				int32* OriginalExportPosition = OriginalExportIndexes.Find( *ExportFullName );
				if( OriginalExportPosition )
				{
					// this export exists in the new package as well,
					// create a copy of the FObjectExport located at the original index and place it
					// into the matching position in the new package's export map
					FObjectExport* NewExport = new(Linker->ExportMap) FObjectExport( OldExportMap[*OriginalExportPosition] );
					check(NewExport->Object == OldExportMap[*OriginalExportPosition].Object);
					Used[ *OriginalExportPosition ] = 1;
				}
				else
				{

					// this export no longer exists in the new package; to ensure that the _LinkerIndex matches, add an empty entry to pad the list
					new(Linker->ExportMap)FObjectExport( NULL );
					UE_LOG(LogSavePackage, Log, TEXT("No matching export found in new package for original export %i: %s"), i, *ExportFullName);
				}
			}



			SortStartPosition = LinkerToConformTo->ExportMap.Num();
			for( int32 i=0; i<Used.Num(); i++ )
			{
				if( !Used[i] )
				{
					// the FObjectExport located at pos "i" in the original export table did not
					// exist in the old package - add it to the end of the export table
					new(Linker->ExportMap) FObjectExport( OldExportMap[i] );
				}
			}

#if DO_GUARD_SLOW

			// sanity-check: make sure that all exports which existed in the linker before we sorted exist in the linker's export map now
			{
				TSet<UObject*> ExportObjectList;
				for( int32 ExportIndex=0; ExportIndex<Linker->ExportMap.Num(); ExportIndex++ )
				{
					ExportObjectList.Add(Linker->ExportMap[ExportIndex].Object);
				}

				for( int32 OldExportIndex=0; OldExportIndex<OldExportMap.Num(); OldExportIndex++ )
				{
					check(ExportObjectList.Contains(OldExportMap[OldExportIndex].Object));
				}
			}
#endif
		}
		else
		{
			for ( int32 ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ExportIndex++ )
			{
				const FObjectExport& Export = Linker->ExportMap[ExportIndex];
				if ( Export.Object )
				{
					ObjectToFullNameMap.Add(Export.Object, *Export.Object->GetFullName());
				}
			}
		}

		if ( SortStartPosition < Linker->ExportMap.Num() )
		{
			Sorter = this;
			Sort( &Linker->ExportMap[SortStartPosition], Linker->ExportMap.Num() - SortStartPosition, FObjectExportSortHelper() );
			Sorter = NULL;
		}
	}
};
FObjectExportSortHelper* FObjectExportSortHelper::Sorter = NULL;


class FExportReferenceSorter : public FArchiveUObject
{
	/**
	 * Verifies that all objects which will be force-loaded when the export at RelativeIndex is created and/or loaded appear in the sorted list of exports
	 * earlier than the export at RelativeIndex.
	 *
	 * Used for tracking down the culprit behind dependency sorting bugs.
	 *
	 * @param	RelativeIndex	the index into the sorted export list to check dependencies for
	 * @param	CheckObject		the object that will be force-loaded by the export at RelativeIndex
	 * @param	ReferenceType	the relationship between the object at RelativeIndex and CheckObject (archetype, class, etc.)
	 * @param	out_ErrorString	if incorrect sorting is detected, receives data containing more information about the incorrectly sorted object.
	 *
	 * @param	true if the export at RelativeIndex appears later than the exports associated with any objects that it will force-load; false otherwise.
	 */
	bool VerifyDependency( const int32 RelativeIndex, UObject* CheckObject, const FString& ReferenceType, FString& out_ErrorString )
	{
		bool bResult = false;

		checkf(ReferencedObjects.IsValidIndex(RelativeIndex), TEXT("Invalid index specified: %i (of %i)"), RelativeIndex, ReferencedObjects.Num());

		UObject* SourceObject = ReferencedObjects[RelativeIndex];
		checkf(SourceObject, TEXT("NULL Object at location %i in ReferencedObjects list"), RelativeIndex);
		checkf(CheckObject, TEXT("CheckObject is NULL for %s (%s)"), *SourceObject->GetFullName(), *ReferenceType);

		if ( SourceObject->GetOutermost() != CheckObject->GetOutermost() )
		{
			// not in the same package; therefore we can assume that the dependent object will exist
			bResult = true;
		}
		else
		{
			int32 OtherIndex = ReferencedObjects.Find(CheckObject);
			if ( OtherIndex != INDEX_NONE )
			{
				if ( OtherIndex < RelativeIndex )
				{
					bResult = true;
				}
				else
				{
					out_ErrorString = FString::Printf(TEXT("Sorting error detected (%s appears later in ReferencedObjects list)!  %i) %s   =>  %i) %s"), *ReferenceType, RelativeIndex,
						*SourceObject->GetFullName(), OtherIndex, *CheckObject->GetFullName());

					bResult = false;
				}
			}
			else
			{
				// the object isn't in the list of ReferencedObjects, which means it wasn't processed as a result of processing the source object; this
				// might indicate a bug, but might also just mean that the CheckObject was first referenced by an earlier export
				int32 ProcessedIndex = ProcessedObjects.Find(CheckObject);
				if ( ProcessedIndex != INDEX_NONE )
				{
					OtherIndex = ProcessedIndex;
					int32 SourceIndex = ProcessedObjects.Find(SourceObject);

					if ( OtherIndex < SourceIndex )
					{
						bResult = true;
					}
					else
					{
						out_ErrorString = FString::Printf(TEXT("Sorting error detected (%s was processed but not added to ReferencedObjects list)!  %i/%i) %s   =>  %i) %s"),
							*ReferenceType, RelativeIndex, SourceIndex, *SourceObject->GetFullName(), OtherIndex, *CheckObject->GetFullName());
						bResult = false;
					}
				}
				else
				{
					int32 SourceIndex = ProcessedObjects.Find(SourceObject);

					out_ErrorString = FString::Printf(TEXT("Sorting error detected (%s has not yet been processed)!  %i/%i) %s   =>  %s"),
						*ReferenceType, RelativeIndex, SourceIndex, *SourceObject->GetFullName(), *CheckObject->GetFullName());

					bResult = false;
				}
			}
		}

		return bResult;
	}

	/**
	 * Pre-initializes the list of processed objects with the boot-strap classes.
	 */
	void InitializeCoreClasses()
	{
		// initialize the tracking maps with the core classes
		UClass* CoreClassList[]=
		{
			UObject::StaticClass(),
			UField::StaticClass(),
			UStruct::StaticClass(),
			UScriptStruct::StaticClass(),
			UFunction::StaticClass(),
			UEnum::StaticClass(),
			UClass::StaticClass(),
			UProperty::StaticClass(),
			UByteProperty::StaticClass(),
			UIntProperty::StaticClass(),
			UBoolProperty::StaticClass(),
			UFloatProperty::StaticClass(),
			UDoubleProperty::StaticClass(),
			UObjectProperty::StaticClass(),
			UClassProperty::StaticClass(),
			UInterfaceProperty::StaticClass(),
			UNameProperty::StaticClass(),
			UStrProperty::StaticClass(),
			UArrayProperty::StaticClass(),
			UTextProperty::StaticClass(),
			UStructProperty::StaticClass(),
			UDelegateProperty::StaticClass(),
			UInterface::StaticClass(),
			UMulticastDelegateProperty::StaticClass(),
			UWeakObjectProperty::StaticClass(),
			UObjectPropertyBase::StaticClass(),
			ULazyObjectProperty::StaticClass(),
			UAssetObjectProperty::StaticClass(),
			UAssetClassProperty::StaticClass()
		};

		for ( int32 CoreClassIndex = 0; CoreClassIndex < ARRAY_COUNT(CoreClassList); CoreClassIndex++ )
		{
			UClass* CoreClass = CoreClassList[CoreClassIndex];
			CoreClasses.AddUnique(CoreClass);

			ReferencedObjects.Add(CoreClass);
			ReferencedObjects.Add(CoreClass->GetDefaultObject());
		}

		for ( int32 CoreClassIndex = 0; CoreClassIndex < CoreClasses.Num(); CoreClassIndex++ )
		{
			UClass* CoreClass = CoreClasses[CoreClassIndex];
			ProcessStruct(CoreClass);
		}

		CoreReferencesOffset = ReferencedObjects.Num();
	}

	/**
	 * Adds an object to the list of referenced objects, ensuring that the object is not added more than one.
	 *
	 * @param	Object			the object to add to the list
	 * @param	InsertIndex		the index to insert the object into the export list
	 */
	void AddReferencedObject( UObject* Object, int32 InsertIndex )
	{
		if ( Object != NULL && !ReferencedObjects.Contains(Object) )
		{
			ReferencedObjects.Insert(Object, InsertIndex);
		}
	}

	/**
	 * Handles serializing and calculating the correct insertion point for an object that will be force-loaded by another object (via an explicit call to Preload).
	 * If the RequiredObject is a UStruct or true is specified for bProcessObject, the RequiredObject will be inserted into the list of exports just before the object
	 * that has a dependency on this RequiredObject.
	 *
	 * @param	RequiredObject		the object which must be created and loaded first
	 * @param	bProcessObject		normally, only the class and archetype for non-UStruct objects are inserted into the list;  specify true to override this behavior
	 *								if RequiredObject is going to be force-loaded, rather than just created
	 */
	void HandleDependency( UObject* RequiredObject, bool bProcessObject=false )
	{
		if ( RequiredObject != NULL )
		{
			check(CurrentInsertIndex!=INDEX_NONE);

			const int32 PreviousReferencedObjectCount = ReferencedObjects.Num();
			const int32 PreviousInsertIndex = CurrentInsertIndex;

			if (UStruct* RequiredObjectStruct = dynamic_cast<UStruct*>(RequiredObject))
			{
				// if this is a struct/class/function/state, it may have a super that needs to be processed first
				ProcessStruct(RequiredObjectStruct);
			}
			else if ( bProcessObject )
			{
				// this means that RequiredObject is being force-loaded by the referencing object, rather than simply referenced
				ProcessObject(RequiredObject);
			}
			else
			{
				// only the object's class and archetype are force-loaded, so only those objects need to be in the list before
				// whatever object was referencing RequiredObject
				if ( ProcessedObjects.Find(RequiredObject->GetOuter()) == INDEX_NONE )
				{
					HandleDependency(RequiredObject->GetOuter());
				}

				// class is needed before archetype, but we need to process these in reverse order because we are inserting into the list.
				ProcessObject(RequiredObject->GetArchetype());
				ProcessStruct(RequiredObject->GetClass());
			}

			// InsertIndexOffset is the amount the CurrentInsertIndex was incremented during the serialization of SuperField; we need to
			// subtract out this number to get the correct location of the new insert index
			const int32 InsertIndexOffset = CurrentInsertIndex - PreviousInsertIndex;
			const int32 InsertIndexAdvanceCount = (ReferencedObjects.Num() - PreviousReferencedObjectCount) - InsertIndexOffset;
			if ( InsertIndexAdvanceCount > 0 )
			{
				// if serializing SuperField added objects to the list of ReferencedObjects, advance the insertion point so that
				// subsequence objects are placed into the list after the SuperField and its dependencies.
				CurrentInsertIndex += InsertIndexAdvanceCount;
			}
		}
	}

public:
	/**
	 * Constructor
	 */
	FExportReferenceSorter()
	: FArchiveUObject(), CurrentInsertIndex(INDEX_NONE)
	{
		ArIsObjectReferenceCollector = true;
		ArIsPersistent = true;
		ArIsSaving = true;

		InitializeCoreClasses();
	}

	/**
	 * Verifies that the sorting algorithm is working correctly by checking all objects in the ReferencedObjects array to make sure that their
	 * required objects appear in the list first
	 */
	void VerifySortingAlgorithm()
	{
		FString ErrorString;
		for ( int32 VerifyIndex = CoreReferencesOffset; VerifyIndex < ReferencedObjects.Num(); VerifyIndex++ )
		{
			UObject* Object = ReferencedObjects[VerifyIndex];
			
			// first, make sure that the object's class and archetype appear earlier in the list
			UClass* ObjectClass = Object->GetClass();
			if ( !VerifyDependency(VerifyIndex, ObjectClass, TEXT("Class"), ErrorString) )
			{
				UE_LOG(LogSavePackage, Log, TEXT("%s"), *ErrorString);
			}

			UObject* ObjectArchetype = Object->GetArchetype();
			if ( ObjectArchetype != NULL && !VerifyDependency(VerifyIndex, ObjectArchetype, TEXT("Archetype"), ErrorString) )
			{
				UE_LOG(LogSavePackage, Log, TEXT("%s"), *ErrorString);
			}

			// UObjectRedirectors are always force-loaded as the loading code needs immediate access to the object pointed to by the Redirector
			UObjectRedirector* Redirector = dynamic_cast<UObjectRedirector*>(Object);
			if ( Redirector != NULL && Redirector->DestinationObject != NULL )
			{
				// the Redirector does not force-load the destination object, so we only need its class and archetype.
				UClass* RedirectorDestinationClass = Redirector->DestinationObject->GetClass();
				if ( !VerifyDependency(VerifyIndex, RedirectorDestinationClass, TEXT("Redirector DestinationObject Class"), ErrorString) )
				{
					UE_LOG(LogSavePackage, Log, TEXT("%s"), *ErrorString);
				}

				UObject* RedirectorDestinationArchetype = Redirector->DestinationObject->GetArchetype();
				if ( RedirectorDestinationArchetype != NULL 
				&& !VerifyDependency(VerifyIndex, RedirectorDestinationArchetype, TEXT("Redirector DestinationObject Archetype"), ErrorString) )
				{
					UE_LOG(LogSavePackage, Log, TEXT("%s"), *ErrorString);
				}
			}
		}
	}

	/**
	 * Clears the list of encountered objects; should be called if you want to re-use this archive.
	 */
	void Clear()
	{
		ReferencedObjects.RemoveAt(CoreReferencesOffset, ReferencedObjects.Num() - CoreReferencesOffset);
	}

	/**
	 * Get the list of new objects which were encountered by this archive; excludes those objects which were passed into the constructor
	 */
	void GetExportList( TArray<UObject*>& out_Exports, bool bIncludeCoreClasses=false )
	{
		if ( !bIncludeCoreClasses )
		{
			const int32 NumReferencedObjects = ReferencedObjects.Num() - CoreReferencesOffset;
			if ( NumReferencedObjects > 0 )
			{
				int32 OutputIndex = out_Exports.Num();

				out_Exports.AddUninitialized(NumReferencedObjects);
				for ( int32 RefIndex = CoreReferencesOffset; RefIndex < ReferencedObjects.Num(); RefIndex++ )
				{
					out_Exports[OutputIndex++] = ReferencedObjects[RefIndex];
				}
			}
		}
		else
		{
			out_Exports += ReferencedObjects;
		}
	}

	/** 
	 * UObject serialization operator
	 *
	 * @param	Object	an object encountered during serialization of another object
	 *
	 * @return	reference to instance of this class
	 */
	FArchive& operator<<( UObject*& Object )
	{
		// we manually handle class default objects, so ignore those here
		if ( Object != NULL && !Object->HasAnyFlags(RF_ClassDefaultObject) )
		{
			if ( ProcessedObjects.Find(Object) == INDEX_NONE )
			{
				// if this object is not a UField, it is an object instance that is referenced through script or defaults (when processing classes) or
				// through an normal object reference (when processing the non-class exports).  Since classes and class default objects
				// are force-loaded (and thus, any objects referenced by the class's script or defaults will be created when the class
				// is force-loaded), we'll need to be sure that the referenced object's class and archetype are inserted into the list
				// of exports before the class, so that when CreateExport is called for this object reference we don't have to seek.
				// Note that in the non-UField case, we don't actually need the object itself to appear before the referencing object/class because it won't
				// be force-loaded (thus we don't need to add the referenced object to the ReferencedObject list)
				if (dynamic_cast<UField*>(Object))
				{
					// when field processing is enabled, ignore any referenced classes since a class's class and CDO are both intrinsic and
					// attempting to deal with them here will only cause problems
					if ( !bIgnoreFieldReferences && !dynamic_cast<UClass*>(Object) )
					{
						if ( CurrentClass == NULL || Object->GetOuter() != CurrentClass )
						{
							if ( UStruct* StructObject = dynamic_cast<UStruct*>(Object) )
							{
								// if this is a struct/class/function/state, it may have a super that needs to be processed first (Preload force-loads UStruct::SuperField)
								ProcessStruct(StructObject);
							}
							else
							{
								// byte properties that are enum references need their enums loaded first so that config importing works
								UByteProperty* ByteProp = dynamic_cast<UByteProperty*>(Object);
								if (ByteProp != NULL && ByteProp->Enum != NULL)
								{
									HandleDependency(ByteProp->Enum, /*bProcessObject =*/true);
								}
								// a normal field - property, enum, const; just insert it into the list and keep going
								ProcessedObjects.Add(Object);
								
								AddReferencedObject(Object, CurrentInsertIndex);
								if ( !SerializedObjects.Contains(Object) )
								{
									SerializedObjects.Add(Object);
									Object->Serialize(*this);
								}
							}
						}
					}
				}
				else
				{
					// since normal references to objects aren't force-loaded, 
					// we do not need to pass true for bProcessObject by default
					// (true would indicate that Object must be inserted into 
					// the sorted export list before the object that contains 
					// this object reference - i.e. the object we're currently
					// serializing)
					// 
					// sometimes (rarely) this is the case though, so we use 
					// ForceLoadObjects to determine if the object we're 
					// serializing would force load Object (if so, it'll come 
					// first in the ExportMap)
					bool const bProcessObject = ForceLoadObjects.Contains(Object);
					HandleDependency(Object, bProcessObject);
				}
			}
		}

		return *this;
	}

	/**
	 * Adds a normal object to the list of sorted exports.  Ensures that any objects which will be force-loaded when this object is created or loaded are inserted into
	 * the list before this object.
	 *
	 * @param	Object	the object to process.
	 */
	void ProcessObject( UObject* Object )
	{
		// we manually handle class default objects, so ignore those here
		if ( Object != NULL )
		{
			if ( !Object->HasAnyFlags(RF_ClassDefaultObject) )
			{
				if ( ProcessedObjects.Find(Object) == INDEX_NONE )
				{
					ProcessedObjects.Add(Object);

					const bool bRecursiveCall = CurrentInsertIndex != INDEX_NONE;
					if ( !bRecursiveCall )
					{
						CurrentInsertIndex = ReferencedObjects.Num();
					}

					// when an object is created (CreateExport), its class and archetype will be force-loaded, so we'll need to make sure that those objects
					// are placed into the list before this object so that when CreateExport calls Preload on these objects, no seeks occur
					// The object's Outer isn't force-loaded, but it will be created before the current object, so we'll need to ensure that its archetype & class
					// are placed into the list before this object.
					HandleDependency(Object->GetClass(), true);
					HandleDependency(Object->GetOuter());
					HandleDependency(Object->GetArchetype(), true);

					// UObjectRedirectors are always force-loaded as the loading code needs immediate access to the object pointed to by the Redirector
					UObjectRedirector* Redirector = dynamic_cast<UObjectRedirector*>(Object);
					if ( Redirector != NULL && Redirector->DestinationObject != NULL )
					{
						// the Redirector does not force-load the destination object, so we only need its class and archetype.
						HandleDependency(Redirector->DestinationObject);
					}

					// now we add this object to the list
					AddReferencedObject(Object, CurrentInsertIndex);

					// then serialize the object - any required references encountered during serialization will be inserted into the list before this object, but after this object's
					// class and archetype
					if ( !SerializedObjects.Contains(Object) )
					{
						SerializedObjects.Add(Object);
						Object->Serialize(*this);
					}

					if ( !bRecursiveCall )
					{
						CurrentInsertIndex = INDEX_NONE;
					}
				}
			}
		}
	}

	/**
	 * Adds a UStruct object to the list of sorted exports.  Handles serialization and insertion for any objects that will be force-loaded by this struct (via an explicit call to Preload).
	 *
	 * @param	StructObject	the struct to process
	 */
	void ProcessStruct( UStruct* StructObject )
	{
		if ( StructObject != NULL )
		{
			if ( ProcessedObjects.Find(StructObject) == INDEX_NONE )
			{
				ProcessedObjects.Add(StructObject);

				const bool bRecursiveCall = CurrentInsertIndex != INDEX_NONE;
				if ( !bRecursiveCall )
				{
					CurrentInsertIndex = ReferencedObjects.Num();
				}

				// this must be done after we've established a CurrentInsertIndex
				HandleDependency(StructObject->GetInheritanceSuper());

				// insert the class/function/state/struct into the list
				AddReferencedObject(StructObject, CurrentInsertIndex);
				if ( !SerializedObjects.Contains(StructObject) )
				{
					const bool bPreviousIgnoreFieldReferences = bIgnoreFieldReferences;

					// first thing to do is collect all actual objects referenced by this struct's script or defaults
					// so we turn off field serialization so that we don't have to worry about handling this struct's fields just yet
					bIgnoreFieldReferences = true;

					// most often, we don't want/need object references getting  
					// recorded as dependencies, but some structs (classes) 
					// require certain non-field objects be prioritized in the 
					// ExportMap earlier (see UClass::GetRequiredPreloadDependencies() 
					// for more details)... this array records/holds those 
					// required sub-objects
					TArray<UObject*> StructForceLoadObjects;

					bool const bIsClassObject = (dynamic_cast<UClass*>(StructObject) != nullptr);
					if (bIsClassObject)
					{
						UClass* AsClass = (UClass*)StructObject;
						AsClass->GetRequiredPreloadDependencies(StructForceLoadObjects);
					}
					// append rather than replace (in case we're nested in a 
					// recursive call)... adding these to ForceLoadObjects 
					// ensures that any reference to a StructForceLoadObjects
					// object gets recorded in the ExportMap before StructObject
					// (see operator<<, where we utilize ForceLoadObjects)
					ForceLoadObjects.Append(StructForceLoadObjects);
					int32 const ForceLoadCount = StructForceLoadObjects.Num();

					SerializedObjects.Add(StructObject);
					StructObject->Serialize(*this);

					// remove (pop) rather than empty, in case ClassForceLoadObjects
					// had entries from a previous call to this function up that chain
					ForceLoadObjects.RemoveAt(ForceLoadObjects.Num() - ForceLoadCount, ForceLoadCount);

					// at this point, any objects which were referenced through this struct's script or defaults will be in the list of exports, and 
					// the CurrentInsertIndex will have been advanced so that the object processed will be inserted just before this struct in the array
					// (i.e. just after class/archetypes for any objects which were referenced by this struct's script)

					// now re-enable field serialization and process the struct's properties, functions, enums, structs, etc.  They will be inserted into the list
					// just ahead of the struct itself, so that those objects are created first during seek-free loading.
					bIgnoreFieldReferences = false;
					
					// invoke the serialize operator rather than calling Serialize directly so that the object is handled correctly (i.e. if it is a struct, then we should
					// call ProcessStruct, etc. and all this logic is already contained in the serialization operator)
					if (!bIsClassObject)
					{
						// before processing the Children reference, set the CurrentClass to the class which contains this StructObject so that we
						// don't inadvertently serialize other fields of the owning class too early.
						CurrentClass = StructObject->GetOwnerClass();
					}					

					(*this) << (UObject*&)StructObject->Children;
					CurrentClass = NULL;

					(*this) << (UObject*&)StructObject->Next;

					bIgnoreFieldReferences = bPreviousIgnoreFieldReferences;
				}

				// Preload will force-load the class default object when called on a UClass object, so make sure that the CDO is always immediately after its class
				// in the export list; we can't resolve this circular reference, but hopefully we the CDO will fit into the same memory block as the class during 
				// seek-free loading.
				UClass* ClassObject = dynamic_cast<UClass*>(StructObject);
				if ( ClassObject != NULL )
				{
					UObject* CDO = ClassObject->GetDefaultObject();
					ensureMsgf(nullptr != CDO, TEXT("Error: Invalid CDO in class %s"), *GetPathNameSafe(ClassObject));
					if ((ProcessedObjects.Find(CDO) == INDEX_NONE) && (nullptr != CDO))
					{
						ProcessedObjects.Add(CDO);

						if ( !SerializedObjects.Contains(CDO) )
						{
							SerializedObjects.Add(CDO);
							CDO->Serialize(*this);
						}

						int32 ClassIndex = ReferencedObjects.Find(ClassObject);
						check(ClassIndex != INDEX_NONE);

						// we should be the only one adding CDO's to the list, so this assertion is to catch cases where someone else
						// has added the CDO to the list (as it will probably be in the wrong spot).
						check(!ReferencedObjects.Contains(CDO) || CoreClasses.Contains(ClassObject));
						AddReferencedObject(CDO, ClassIndex + 1);
					}
				}

				if ( !bRecursiveCall )
				{
					CurrentInsertIndex = INDEX_NONE;
				}
			}
		}
	}

private:

	/**
	 * The index into the ReferencedObjects array to insert new objects
	 */
	int32 CurrentInsertIndex;

	/**
	 * The index into the ReferencedObjects array for the first object not referenced by one of the core classes
	 */
	int32 CoreReferencesOffset;

	/**
	 * The classes which are pre-added to the array of ReferencedObjects.  Used for resolving a number of circular dependecy issues between
	 * the boot-strap classes.
	 */
	TArray<UClass*> CoreClasses;

	/**
	 * The list of objects that have been evaluated by this archive so far.
	 */
	TArray<UObject*> ProcessedObjects;

	/**
	 * The list of objects that have been serialized; used to prevent calling Serialize on an object more than once.
	 */
	TSet<UObject*> SerializedObjects;

	/**
	 * The list of new objects that were encountered by this archive
	 */
	TArray<UObject*> ReferencedObjects;

	/**
	 * Controls whether to process UField objects encountered during serialization of an object.
	 */
	bool bIgnoreFieldReferences;

	/**
	 * The UClass currently being processed.  This is used to prevent serialization of a UStruct's Children member causing other fields of the same class to be processed too early due
	 * to being referenced (directly or indirectly) by that field.  For example, if a class has two functions which both have a struct parameter of a struct type which is declared in the same class,
	 * the struct would be inserted into the list immediately before the first function processed.  The second function would be inserted into the list just before the struct.  At runtime,
	 * the "second" function would be created first, which would end up force-loading the struct.  This would cause an unacceptible seek because the struct appears later in the export list, thus
	 * hasn't been created yet.
	 */
	UClass* CurrentClass;

	/** 
	 * This is a list of objects that would be force loaded by a struct/class 
	 * currently being handled by ProcessStruct() (meaning that they should be
	 * prioritized in the target ExportMap, before the struct).
	 */
	TArray<UObject*> ForceLoadObjects;
};

#define EXPORT_SORTING_DETAILED_LOGGING 0

/**
 * Helper structure encapsulating functionality to sort a linker's export map to allow seek free
 * loading by creating the exports in the order they are in the export map.
 */
struct FObjectExportSeekFreeSorter
{
	/**
	 * Sorts exports in passed in linker in order to avoid seeking when creating them in order and also
	 * conform the order to an already existing linker if non- NULL.
	 *
	 * @param	Linker				LinkerSave to sort export map
	 * @param	LinkerToConformTo	LinkerLoad to conform LinkerSave to if non- NULL
	 */
	void SortExports( FLinkerSave* Linker, FLinkerLoad* LinkerToConformTo )
	{
		SortArchive.SetCookingTarget(Linker->CookingTarget());

		int32					FirstSortIndex = LinkerToConformTo ? LinkerToConformTo->ExportMap.Num() : 0;
		TMap<UObject*,int32>	OriginalExportIndexes;

		// Populate object to current index map.
		for( int32 ExportIndex=FirstSortIndex; ExportIndex<Linker->ExportMap.Num(); ExportIndex++ )
		{
			const FObjectExport& Export = Linker->ExportMap[ExportIndex];
			if( Export.Object )
			{
				// Set the index (key) in the map to the index of this object into the export map.
				OriginalExportIndexes.Add( Export.Object, ExportIndex );
			}
		}

		bool bRetrieveInitialReferences = true;

		// Now we need to sort the export list according to the order in which objects will be loaded.  For the sake of simplicity, 
		// process all classes first so they appear in the list first (along with any objects those classes will force-load) 
		for( int32 ExportIndex=FirstSortIndex; ExportIndex<Linker->ExportMap.Num(); ExportIndex++ )
		{
			const FObjectExport& Export = Linker->ExportMap[ExportIndex];
			if( UClass* ExportObjectClass = dynamic_cast<UClass*>(Export.Object) )
			{
				SortArchive.Clear();
				SortArchive.ProcessStruct(ExportObjectClass);
#if EXPORT_SORTING_DETAILED_LOGGING
				TArray<UObject*> ReferencedObjects;
				SortArchive.GetExportList(ReferencedObjects, bRetrieveInitialReferences);

				UE_LOG(LogSavePackage, Log, TEXT("Referenced objects for (%i) %s in %s"), ExportIndex, *Export.Object->GetFullName(), *Linker->LinkerRoot->GetName());
				for ( int32 RefIndex = 0; RefIndex < ReferencedObjects.Num(); RefIndex++ )
				{
					UE_LOG(LogSavePackage, Log, TEXT("\t%i) %s"), RefIndex, *ReferencedObjects(RefIndex)->GetFullName());
				}
				if ( ReferencedObjects.Num() > 1 )
				{
					// insert a blank line to make the output more readable
					UE_LOG(LogSavePackage, Log, TEXT(""));
				}

				SortedExports += ReferencedObjects;
#else
				SortArchive.GetExportList(SortedExports, bRetrieveInitialReferences);
#endif
				bRetrieveInitialReferences = false;
			}

		}

#if EXPORT_SORTING_DETAILED_LOGGING
		UE_LOG(LogSavePackage, Log, TEXT("*************   Processed %i classes out of %i possible exports for package %s.  Beginning second pass...   *************"), SortedExports.Num(), Linker->ExportMap.Num() - FirstSortIndex, *Linker->LinkerRoot->GetName());
#endif

		// All UClasses, CDOs, functions, properties, etc. are now in the list - process the remaining objects now
		for ( int32 ExportIndex = FirstSortIndex; ExportIndex < Linker->ExportMap.Num(); ExportIndex++ )
		{
			const FObjectExport& Export = Linker->ExportMap[ExportIndex];
			if ( Export.Object )
			{
				SortArchive.Clear();
				SortArchive.ProcessObject(Export.Object);
#if EXPORT_SORTING_DETAILED_LOGGING
				TArray<UObject*> ReferencedObjects;
				SortArchive.GetExportList(ReferencedObjects, bRetrieveInitialReferences);

				UE_LOG(LogSavePackage, Log, TEXT("Referenced objects for (%i) %s in %s"), ExportIndex, *Export.Object->GetFullName(), *Linker->LinkerRoot->GetName());
				for ( int32 RefIndex = 0; RefIndex < ReferencedObjects.Num(); RefIndex++ )
				{
					UE_LOG(LogSavePackage, Log, TEXT("\t%i) %s"), RefIndex, *ReferencedObjects(RefIndex)->GetFullName());
				}
				if ( ReferencedObjects.Num() > 1 )
				{
					// insert a blank line to make the output more readable
					UE_LOG(LogSavePackage, Log, TEXT(""));
				}

				SortedExports += ReferencedObjects;
#else
				SortArchive.GetExportList(SortedExports, bRetrieveInitialReferences);
#endif
				bRetrieveInitialReferences = false;
			}
		}
#if EXPORT_SORTING_DETAILED_LOGGING
		SortArchive.VerifySortingAlgorithm();
#endif
		// Back up existing export map and empty it so we can repopulate it in a sorted fashion.
		TArray<FObjectExport> OldExportMap = Linker->ExportMap;
		Linker->ExportMap.Empty( OldExportMap.Num() );

		// Add exports that can't be re-jiggled as they are part of the exports of the to be
		// conformed to Linker.
		for( int32 ExportIndex=0; ExportIndex<FirstSortIndex; ExportIndex++ )
		{
			Linker->ExportMap.Add( OldExportMap[ExportIndex] );
		}

		// Create new export map from sorted exports.
		for( int32 ObjectIndex=0; ObjectIndex<SortedExports.Num(); ObjectIndex++ )
		{
			// See whether this object was part of the to be sortable exports map...
			UObject* Object		= SortedExports[ObjectIndex];
			int32* ExportIndexPtr	= OriginalExportIndexes.Find( Object );
			if( ExportIndexPtr )
			{
				// And add it if it has been.
				Linker->ExportMap.Add( OldExportMap[*ExportIndexPtr] );
			}
		}

		// Manually add any new NULL exports last as they won't be in the SortedExportsObjects list. 
		// A NULL Export.Object can occur if you are e.g. saving an object in the game that is 
		// OBJECTMARK_NotForClient.
		for( int32 ExportIndex=FirstSortIndex; ExportIndex<OldExportMap.Num(); ExportIndex++ )
		{
			const FObjectExport& Export = OldExportMap[ExportIndex];
			if( Export.Object == NULL )
			{
				Linker->ExportMap.Add( Export );
			}
		}
	}

private:
	/**
	 * Archive for sorting an objects references according to the order in which they'd be loaded.
	 */
	FExportReferenceSorter SortArchive;

	/** Array of regular objects encountered by CollectExportsInOrderOfUse					*/
	TArray<UObject*>	SortedExports;
};

// helper class for clarification, encapsulation, and elimination of duplicate code
struct FPackageExportTagger
{
	UObject*		Base;
	EObjectFlags	TopLevelFlags;
	UObject*		Outer;

	FPackageExportTagger( UObject* CurrentBase, EObjectFlags CurrentFlags, UObject* InOuter )
	:	Base(CurrentBase)
	,	TopLevelFlags(CurrentFlags)
	,	Outer(InOuter)
	{}

	void TagPackageExports( FArchiveSaveTagExports& ExportTagger, bool bRoutePresave )
	{
		// Route PreSave on Base and serialize it for export tagging.
		if( Base )
		{
			if ( bRoutePresave )
			{
				Base->PreSave();
			}

			ExportTagger.ProcessBaseObject(Base);
		}
		TArray<UObject *> ObjectsInOuter;
		GetObjectsWithOuter(Outer, ObjectsInOuter);
		// Serialize objects to tag them as OBJECTMARK_TagExp.
		for( int32 Index = 0; Index < ObjectsInOuter.Num(); Index++ )
		{
			UObject* Obj = ObjectsInOuter[Index];
			if( Obj->HasAnyFlags(TopLevelFlags) && Obj->IsIn(Outer) )
			{
				ExportTagger.ProcessBaseObject(Obj);
			}
		}
		if ( bRoutePresave )
		{
			// Route PreSave.
			{
				TArray<UObject*> TagExpObjects;
				GetObjectsWithAnyMarks(TagExpObjects, OBJECTMARK_TagExp);
				for(int32 Index = 0; Index < TagExpObjects.Num(); Index++)
				{
					UObject* Obj = TagExpObjects[Index];
					check(Obj->HasAnyMarks(OBJECTMARK_TagExp));
					//@warning: Objects created from within PreSave will NOT have PreSave called on them!!!
					Obj->PreSave();
				}
			}
		}
	}
};


/** checks whether it is valid to conform NewPackage to OldPackage
 * i.e, that there are no incompatible changes between the two
 * @warning: this function needs to load objects from the old package to do the verification
 * it's very important that it cleans up after itself to avoid conflicts with e.g. script compilation
 * @param NewPackage - the new package being saved
 * @param OldLinker - linker of the old package to conform against
 * @param Error - log device to send any errors
 * @return whether NewPackage can be conformed
 */
static bool ValidateConformCompatibility(UPackage* NewPackage, FLinkerLoad* OldLinker, FOutputDevice* Error)
{
	// various assumptions made about Core and its contents prevent loading a version mapped to a different name from working correctly
	// Core script typically doesn't have any replication related definitions in it anyway
	if (NewPackage->GetFName() == NAME_CoreUObject || NewPackage->GetFName() == GLongCoreUObjectPackageName)
	{
		return true;
	}

	// save the RF_TagGarbageTemp flag for all objects so our use of it doesn't clobber anything
	TMap<UObject*, uint8> ObjectFlagMap;
	for (TObjectIterator<UObject> It; It; ++It)
	{
		ObjectFlagMap.Add(*It, (It->GetFlags() & RF_TagGarbageTemp) ? 1 : 0);
	}

	// this is needed to successfully find intrinsic classes/properties
	OldLinker->LoadFlags |= LOAD_NoWarn | LOAD_Quiet | LOAD_FindIfFail;

	// unfortunately, to get at the classes and their properties we will also need to load the default objects
	// but the remapped package won't be bound to its native instance, so we need to manually copy the constructors
	// so that classes with their own Serialize() implementations are loaded correctly
	BeginLoad();
	for (int32 i = 0; i < OldLinker->ExportMap.Num(); i++)
	{
		UClass* NewClass = (UClass*)StaticFindObjectFast(UClass::StaticClass(), NewPackage, OldLinker->ExportMap[i].ObjectName, true, false);
		UClass* OldClass = static_cast<UClass*>(OldLinker->Create(UClass::StaticClass(), OldLinker->ExportMap[i].ObjectName, OldLinker->LinkerRoot, LOAD_None, false));
		if (OldClass != NULL && NewClass != NULL && OldClass->HasAnyFlags(RF_Native) && NewClass->HasAnyFlags(RF_Native))
		{
			OldClass->ClassConstructor = NewClass->ClassConstructor;
#if WITH_HOT_RELOAD_CTORS
			OldClass->ClassVTableHelperCtorCaller = NewClass->ClassVTableHelperCtorCaller;
#endif // WITH_HOT_RELOAD_CTORS
			OldClass->ClassAddReferencedObjects = NewClass->ClassAddReferencedObjects;
		}
	}
	EndLoad();

	bool bHadCompatibilityErrors = false;
	// check for illegal change of networking flags on class fields
	for (int32 i = 0; i < OldLinker->ExportMap.Num(); i++)
	{
		if (OldLinker->GetExportClassName(i) == NAME_Class)
		{
			// load the object so we can analyze it
			BeginLoad();
			UClass* OldClass = static_cast<UClass*>(OldLinker->Create(UClass::StaticClass(), OldLinker->ExportMap[i].ObjectName, OldLinker->LinkerRoot, LOAD_None, false));
			EndLoad();
			if (OldClass != NULL)
			{
				UClass* NewClass = FindObjectFast<UClass>(NewPackage, OldClass->GetFName(), true, false);
				if (NewClass != NULL)
				{
					for (TFieldIterator<UField> OldFieldIt(OldClass,EFieldIteratorFlags::ExcludeSuper); OldFieldIt; ++OldFieldIt)
					{
						for (TFieldIterator<UField> NewFieldIt(NewClass,EFieldIteratorFlags::ExcludeSuper); NewFieldIt; ++NewFieldIt)
						{
							if (OldFieldIt->GetFName() == NewFieldIt->GetFName())
							{
								UProperty* OldProp = dynamic_cast<UProperty*>(*OldFieldIt);
								UProperty* NewProp = dynamic_cast<UProperty*>(*NewFieldIt);
								if (OldProp != NULL && NewProp != NULL)
								{
									if ((OldProp->PropertyFlags & CPF_Net) != (NewProp->PropertyFlags & CPF_Net))
									{
										Error->Logf(ELogVerbosity::Error, TEXT("Network flag mismatch for property %s"), *NewProp->GetPathName());
										bHadCompatibilityErrors = true;
									}
								}
								else
								{
									UFunction* OldFunc = dynamic_cast<UFunction*>(*OldFieldIt);
									UFunction* NewFunc = dynamic_cast<UFunction*>(*NewFieldIt);
									if (OldFunc != NULL && NewFunc != NULL)
									{
										if ((OldFunc->FunctionFlags & (FUNC_Net | FUNC_NetServer | FUNC_NetClient)) != (NewFunc->FunctionFlags & (FUNC_Net | FUNC_NetServer | FUNC_NetClient)))
										{
											Error->Logf(ELogVerbosity::Error, TEXT("Network flag mismatch for function %s"), *NewFunc->GetPathName());
											bHadCompatibilityErrors = true;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	// delete all of the newly created objects from the old package by marking everything else and deleting all unmarked objects
	for (TObjectIterator<UObject> It; It; ++It)
	{
		It->SetFlags(RF_TagGarbageTemp);
	}
	for (int32 i = 0; i < OldLinker->ExportMap.Num(); i++)
	{
		if (OldLinker->ExportMap[i].Object != NULL)
		{
			OldLinker->ExportMap[i].Object->ClearFlags(RF_TagGarbageTemp);
		}
	}
	CollectGarbage(RF_TagGarbageTemp, true);

	// restore RF_TagGarbageTemp flag value
	for (TMap<UObject*, uint8>::TIterator It(ObjectFlagMap); It; ++It)
	{
		UObject* Obj = It.Key();
		check(Obj->IsValidLowLevel()); // if this crashes we deleted something we shouldn't have
		if (It.Value())
		{
			Obj->SetFlags(RF_TagGarbageTemp);
		}
		else
		{
			Obj->ClearFlags(RF_TagGarbageTemp);
		}
	}
	// verify that we cleaned up after ourselves
	for (int32 i = 0; i < OldLinker->ExportMap.Num(); i++)
	{
		checkf(OldLinker->ExportMap[i].Object == NULL, TEXT("Conform validation code failed to clean up after itself! Surviving object: %s"), *OldLinker->ExportMap[i].Object->GetPathName());
	}

	// finally, abort if there were any errors
	return !bHadCompatibilityErrors;
}

EObjectMark UPackage::GetObjectMarksForTargetPlatform( const class ITargetPlatform* TargetPlatform, const bool bIsCooking )
{
	EObjectMark ObjectMarks = EObjectMark(OBJECTMARK_NotForClient|OBJECTMARK_NotForServer);

	if( TargetPlatform && bIsCooking )
	{
		const bool bIsServerOnly = TargetPlatform->IsServerOnly();
		const bool bIsClientOnly = TargetPlatform->IsClientOnly();

		if( bIsServerOnly )
		{
			ObjectMarks = OBJECTMARK_NotForServer;
		}
		else if( bIsClientOnly )
		{
			ObjectMarks = OBJECTMARK_NotForClient;
		}
	}

	return ObjectMarks;
}

#if WITH_EDITOR
/**
 * Helper function to sort export objects by fully qualified names.
 */
bool ExportObjectSorter(const UObject& Lhs, const UObject& Rhs)
{
	// Check names first.
	if (Lhs.GetFName() < Rhs.GetFName())
	{
		return true;
	}

	if (Lhs.GetFName() > Rhs.GetFName())
	{
		return false;
	}

	// Names equal, compare class names.
	if (Lhs.GetClass()->GetFName() < Rhs.GetClass()->GetFName())
	{
		return true;
	}

	if (Lhs.GetClass()->GetFName() > Rhs.GetClass()->GetFName())
	{
		return false;
	}

	// Compare by outers if they exist.
	if (Lhs.GetOuter() && Rhs.GetOuter())
	{
		return Lhs.GetOuter()->GetFName() < Rhs.GetOuter()->GetFName();
	}

	if (Lhs.GetOuter())
	{
		return true;
	}

	return false;
}

/**
* Helper equality comparator for export objects. Compares by names, class names and outer names.
*/
bool ExportEqualityComparator(UObject* Lhs, UObject* Rhs)
{
	check(Lhs && Rhs);
	return Lhs->GetOuter() == Rhs->GetOuter()
		&& Lhs->GetClass() == Rhs->GetClass()
		&& Lhs->GetFName() == Rhs->GetFName();
}

/**
 * Remove OBJECTMARK_TagExp from duplicated objects.
 */
TMap<UObject*, UObject*> UnmarkExportTagFromDuplicates()
{
	TMap<UObject*, UObject*> RedirectDuplicatesToOriginals;
	TArray<UObject*> Objects;
	GetObjectsWithAnyMarks(Objects, OBJECTMARK_TagExp);

	Objects.Sort(ExportObjectSorter);

	int32 LastUniqueObjectIndex = 0;
	for (int32 CurrentObjectIndex = 1; CurrentObjectIndex < Objects.Num(); ++CurrentObjectIndex)
	{
		UObject* LastUniqueObject = Objects[LastUniqueObjectIndex];
		UObject* CurrentObject = Objects[CurrentObjectIndex];

		// Check if duplicates with different pointers
		if (LastUniqueObject != CurrentObject
			// but matching names
			&& ExportEqualityComparator(LastUniqueObject, CurrentObject))
		{
			// Don't export duplicates.
			CurrentObject->UnMark(OBJECTMARK_TagExp);
			RedirectDuplicatesToOriginals.Add(CurrentObject, LastUniqueObject);
		}
		else
		{
			LastUniqueObjectIndex = CurrentObjectIndex;
		}
	}

	return RedirectDuplicatesToOriginals;
}
#endif // WITH_EDITOR

/**
 * Save one specific object (along with any objects it references contained within the same Outer) into an Unreal package.
 * 
 * @param	InOuter							the outer to use for the new package
 * @param	Base							the object that should be saved into the package
 * @param	TopLevelFlags					For all objects which are not referenced [either directly, or indirectly] through Base, only objects
 *											that contain any of these flags will be saved.  If 0 is specified, only objects which are referenced
 *											by Base will be saved into the package.
 * @param	Filename						the name to use for the new package file
 * @param	Error							error output
 * @param	Conform							if non-NULL, all index tables for this will be sorted to match the order of the corresponding index table
 *											in the conform package
 * @param	bForceByteSwapping				whether we should forcefully byte swap before writing to disk
 * @param	bWarnOfLongFilename				[opt] If true (the default), warn when saving to a long filename.
 * @param	SaveFlags						Flags to control saving
 * @param	TargetPlatform					The platform being saved for
 * @param	FinalTimeStamp					If not FDateTime::MinValue(), the timestamp the saved file should be set to. (Intended for cooking only...)
 *
 * @return	true if the package was saved successfully.
 */
#define UE_PROFILE_COOKSAVE 0
#if UE_PROFILE_COOKSAVE

#define UE_START_LOG_COOK_TIME(InFilename) double PreviousTime; double StartTime; PreviousTime = StartTime = FPlatformTime::Seconds(); \
	UE_LOG( LogSavePackage, Log, TEXT("Starting save package for %s"), InFilename);

#define UE_LOG_COOK_TIME(TimeType) \
	{\
		double CurrentTime = FPlatformTime::Seconds(); \
		UE_LOG( LogSavePackage, Log, TEXT("TIMING: %s took %fms"), TimeType, (CurrentTime - PreviousTime) * 1000.0f ); \
		PreviousTime = CurrentTime; \
	}

#define UE_FINISH_LOG_COOK_TIME()\
	UE_LOG_COOK_TIME( TEXT("Unaccounted time")); \
	{\
		double CurrentTime = FPlatformTime::Seconds(); \
		UE_LOG( LogSavePackage, Log, TEXT("Total save time: %fms"), (CurrentTime - StartTime) * 1000.0f ); \
	}

#else
// don't do anything
#define UE_START_LOG_COOK_TIME(InFilename)
#define UE_LOG_COOK_TIME(TimeType) 
#define UE_FINISH_LOG_COOK_TIME()
#endif

#if WITH_EDITOR
COREUOBJECT_API extern bool GOutputCookingWarnings;
#endif

bool UPackage::SavePackage( UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags, const TCHAR* Filename, 
	FOutputDevice* Error, FLinkerLoad* Conform, bool bForceByteSwapping, bool bWarnOfLongFilename, uint32 SaveFlags, 
	const class ITargetPlatform* TargetPlatform, const FDateTime&  FinalTimeStamp, bool bSlowTask)
{
	UE_START_LOG_COOK_TIME( Filename );
	
	if (FPlatformProperties::HasEditorOnlyData())
	{
		if (GIsSavingPackage)
		{
			ensureMsgf(false, TEXT("Recursive SavePackage() is not supported"));
			return false;
		}
		UE_LOG_COOK_TIME(TEXT("First"));


		
		bool bIsCooking = TargetPlatform != NULL;

		// if we are cooking we should be doing it in the editor
		// otherwise some other assumptions are bad
		check( !bIsCooking || WITH_EDITOR );
	#if WITH_EDITOR
		if ( !bIsCooking )
		{
			// Attempt to create a backup of this package before it is saved, if applicable
			if (FCoreUObjectDelegates::AutoPackageBackupDelegate.IsBound())
			{
				FCoreUObjectDelegates::AutoPackageBackupDelegate.Execute(*InOuter);
			}

			UE_LOG_COOK_TIME(TEXT("AutoPackageBackupDelegate"));
		}
		
	#endif	// #if WITH_EDITOR

		// do any path replacements on the source DestFile
		const FString NewPath = FString(Filename);

		// point to the new version of the path
		Filename = *NewPath;

		// We need to fulfill all pending streaming and async loading requests to then allow us to lock the global IO manager. 
		// The latter implies flushing all file handles which is a pre-requisite of saving a package. The code basically needs 
		// to be sure that we are not reading from a file that is about to be overwritten and that there is no way we might 
		// start reading from the file till we are done overwriting it.

		FlushAsyncLoading();

		UE_LOG_COOK_TIME(TEXT("Flush Async loading"));
		(*GFlushStreamingFunc)();
		UE_LOG_COOK_TIME(TEXT("Flush streaming func"));
		FIOSystem::Get().BlockTillAllRequestsFinishedAndFlushHandles();

		UE_LOG_COOK_TIME(TEXT("Block till all requests finished and flush handles"));

		check(InOuter);
		check(Filename);
		uint32 Time=0; CLOCK_CYCLES(Time);

		// Make sure package is fully loaded before saving. 
		if( !Base && !InOuter->IsFullyLoaded() )
		{
			if (!(SaveFlags & SAVE_NoError))
			{
				// We cannot save packages that aren't fully loaded as it would clobber existing not loaded content.
				FText ErrorText;
				if( InOuter->ContainsMap() )
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("Name"), FText::FromString( NewPath ));
					ErrorText = FText::Format( NSLOCTEXT( "SavePackage", "CannotSaveMapPartiallyLoaded", "Map '{Name}' cannot be saved as it has only been partially loaded" ), Arguments );
				}
				else
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("Name"), FText::FromString( NewPath ));
					ErrorText = FText::Format( NSLOCTEXT( "SavePackage", "CannotSaveAssetPartiallyLoaded", "Asset '{Name}' cannot be saved as it has only been partially loaded" ), Arguments );
				}
				Error->Logf(ELogVerbosity::Warning, *ErrorText.ToString());
			}
			return false;
		}

		// Make sure package is allowed to be saved.
		if( !TargetPlatform && FCoreUObjectDelegates::IsPackageOKToSaveDelegate.IsBound() )
		{
			bool bIsOKToSave = FCoreUObjectDelegates::IsPackageOKToSaveDelegate.Execute(InOuter, Filename, Error);
			if (!bIsOKToSave)
			{
				if (!(SaveFlags & SAVE_NoError))
				{
					FText ErrorText;
					if( InOuter->ContainsMap() )
					{
						FFormatNamedArguments Arguments;
						Arguments.Add(TEXT("Name"), FText::FromString( NewPath ));
						ErrorText = FText::Format( NSLOCTEXT( "SavePackage", "MapSaveNotAllowed", "Map '{Name}' is not allowed to save (see log for reason)" ), Arguments );
					}
					else
					{
						FFormatNamedArguments Arguments;
						Arguments.Add(TEXT("Name"), FText::FromString( NewPath ));
						ErrorText = FText::Format( NSLOCTEXT( "SavePackage", "AssetSaveNotAllowed", "Asset '{Name}' is not allowed to save (see log for reason)" ), Arguments );
					}
					Error->Logf(ELogVerbosity::Warning, *ErrorText.ToString());
				}
				return false;
			}
		}

		// if we're conforming, validate that the packages are compatible
		if (Conform != NULL && !ValidateConformCompatibility(InOuter, Conform, Error))
		{
			if (!(SaveFlags & SAVE_NoError))
			{
				FText ErrorText;
				if( InOuter->ContainsMap() )
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("Name"), FText::FromString( NewPath ));
					ErrorText = FText::Format( NSLOCTEXT( "SavePackage", "CannotSaveMapConformIncompatibility", "Conformed Map '{Name}' cannot be saved as it is incompatible with the original" ), Arguments );
				}
				else
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("Name"), FText::FromString( NewPath ));
					ErrorText = FText::Format( NSLOCTEXT( "SavePackage", "CannotSaveAssetConformIncompatibility", "Conformed Asset '{Name}' cannot be saved as it is incompatible with the original" ), Arguments );
				}
				Error->Logf(ELogVerbosity::Error, *ErrorText.ToString());
			}
			return false;
		}

		bool FilterEditorOnly = (InOuter->PackageFlags & PKG_FilterEditorOnly) != 0;
		// store a list of additional packages to cook when this is cooked (ie streaming levels)
		TArray<FString> AdditionalPackagesToCook;

	
		// Route PreSaveRoot to allow e.g. the world to attach components for the persistent level.
		bool bCleanupIsRequired = false;
		if( Base )
		{
			bCleanupIsRequired = Base->PreSaveRoot(Filename, AdditionalPackagesToCook);
		}

		const FString BaseFilename = FPaths::GetBaseFilename( Filename );
		// Make temp file. CreateTempFilename guarantees unique, non-existing filename.
		// The temp file will be saved in the game save folder to not have to deal with potentially too long paths.
		FString TempFilename;	
		TempFilename = FPaths::CreateTempFilename(*FPaths::GameSavedDir(), *BaseFilename);

		// Init.
		FString CleanFilename = FPaths::GetCleanFilename( Filename );

		FFormatNamedArguments Args;
		Args.Add( TEXT("CleanFilename"), FText::FromString( CleanFilename ) );

		FText StatusMessage = FText::Format( NSLOCTEXT("Core", "SavingFile", "Saving file: {CleanFilename}..."), Args );

		const int32 TotalSaveSteps = 31;
		FScopedSlowTask SlowTask(TotalSaveSteps, StatusMessage, bSlowTask);
		SlowTask.MakeDialog(SaveFlags & SAVE_FromAutosave ? true : false);

		SlowTask.EnterProgressFrame();

		bool Success = true;

		ResetLoadersForSave(InOuter,Filename);

		SlowTask.EnterProgressFrame();

	
		// Untag all objects and names.
		UnMarkAllObjects();


		UE_LOG_COOK_TIME(TEXT("ResetLoadersForSave UnMarkAllObjects"));

		// Size of serialized out package in bytes. This is before compression.
		int32 PackageSize = INDEX_NONE;
		{
			FScopeSavePackageState ScopeSavePackageState; // allocates the save package state and deletes it when this goes out of scope

			uint32 ComparisonFlags = PPF_DeepCompareInstances;

			// Export objects (tags them as OBJECTMARK_TagExp).
			FArchiveSaveTagExports ExportTaggerArchive( InOuter );
			ExportTaggerArchive.SetPortFlags( ComparisonFlags );
			ExportTaggerArchive.SetCookingTarget(TargetPlatform);
			
			check( ExportTaggerArchive.IsCooking() == !!TargetPlatform );
			check( ExportTaggerArchive.IsCooking() == bIsCooking );

			// Tag exports and route presave.
			FPackageExportTagger PackageExportTagger( Base, TopLevelFlags, InOuter );
			PackageExportTagger.TagPackageExports( ExportTaggerArchive, true );
			ExportTaggerArchive.SetFilterEditorOnly(FilterEditorOnly);

			UE_LOG_COOK_TIME(TEXT("TagPackageExports"));
		
			{
				// set GIsSavingPackage here as it is now illegal to create any new object references; they potentially wouldn't be saved correctly
				struct FScopedSavingFlag
				{
					FScopedSavingFlag() { GIsSavingPackage = true; }
					~FScopedSavingFlag() { GIsSavingPackage = false; }
				} IsSavingFlag;

			
				// Clear OBJECTMARK_TagExp again as we need to redo tagging below.
				UnMarkAllObjects(OBJECTMARK_TagExp);
			

				// We need to serialize objects yet again to tag objects that were created by PreSave as OBJECTMARK_TagExp.
				PackageExportTagger.TagPackageExports( ExportTaggerArchive, false );


				UE_LOG_COOK_TIME(TEXT("TagPackageExports"));



				// Kick off any Precaching required for the target platform to save these objects
				// only need to do this if we are cooking a different platform then the one which is currently running
				// TODO: if save package is cancelled then call ClearCache on each object
				TArray<UObject*> CachedObjects;

#if WITH_EDITOR
				if ( bIsCooking )
				{
					TArray<UObject*> TagExpObjects;
					GetObjectsWithAnyMarks(TagExpObjects, OBJECTMARK_TagExp);
					for ( int Index =0; Index < TagExpObjects.Num(); ++Index)
					{
						UObject *ExpObject = TagExpObjects[Index];
						if ( ExpObject->HasAnyMarks( OBJECTMARK_TagExp ) )
						{
							ExpObject->BeginCacheForCookedPlatformData( TargetPlatform );
							CachedObjects.Add( ExpObject );
						}
					}
				}
#endif



				UE_LOG_COOK_TIME(TEXT("BeginCacheForCookedPlatformData"));


				SlowTask.EnterProgressFrame();


				// structure to track what ever export needs to import
				TMap<UObject*, TArray<UObject*> > ObjectDependencies;

				// and a structure to track non-redirector references
				TSet<UObject*> DependenciesReferencedByNonRedirectors;
		
				/** If true, we are going to compress the package to memory to save a little time */
				bool bCompressFromMemory = !!(InOuter->PackageFlags & PKG_StoreCompressed);

				/** If true, we are going to save to disk async to save time. */
				bool bSaveAsync = !!(SaveFlags & SAVE_Async);

				bool bSaveUnversioned = !!(SaveFlags & SAVE_Unversioned);

				FLinkerSave* Linker = nullptr;
				
				if (bCompressFromMemory || bSaveAsync)
				{
					// Allocate the linker with a memory writer, forcing byte swapping if wanted.
					Linker = new FLinkerSave(InOuter, bForceByteSwapping, bSaveUnversioned);
				}
				else
				{
					// Allocate the linker, forcing byte swapping if wanted.
					Linker = new FLinkerSave(InOuter, *TempFilename, bForceByteSwapping, bSaveUnversioned);
				}

				// Use the custom versions we had previously gleaned from the export tag pass
				Linker->Summary.SetCustomVersionContainer(ExportTaggerArchive.GetCustomVersions());

				Linker->SetPortFlags(ComparisonFlags);
				Linker->SetFilterEditorOnly( FilterEditorOnly );
				Linker->SetCookingTarget(TargetPlatform);

				// Make sure the package has the same version as the linker
				InOuter->LinkerPackageVersion = Linker->UE4Ver();
				InOuter->LinkerLicenseeVersion = Linker->LicenseeUE4Ver();
				InOuter->LinkerCustomVersion = Linker->GetCustomVersions();

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();
			
				// keep a list of objects that would normally have gone into the dependency map, but since they are from cross-level dependencies, won't be found in the import map
				TArray<UObject*> DependenciesToIgnore;

				// When cooking, strip export objects that 
				//	are both not for client and not for server by default
				//	are for client if target is client only
				//	are for server if target is server only
				if (Linker->IsCooking())
				{
					const EObjectMark ObjectMarks = UPackage::GetObjectMarksForTargetPlatform( TargetPlatform, Linker->IsCooking() );

					TArray<UObject*> TagExpObjects;
					GetObjectsWithAnyMarks(TagExpObjects, OBJECTMARK_TagExp);
					for(int32 Index = 0; Index < TagExpObjects.Num(); Index++)
					{
						UObject* Obj = TagExpObjects[Index];	

						if (Obj->HasAllMarks(ObjectMarks))
						{
							Obj->UnMark(EObjectMark(OBJECTMARK_TagExp|OBJECTMARK_TagImp));
						}
					}

					// Obj->UnMark will delete object if it stripped all its tags.
					// If all exportable objects are deleted, an attempt to save
					// package will cause a crash.
					GetObjectsWithAnyMarks(TagExpObjects, OBJECTMARK_TagExp);
					if (TagExpObjects.Num() == 0)
					{
						if (!(SaveFlags & SAVE_NoError))
						{
							UE_LOG(LogSavePackage, Display, TEXT("No exports found (or all exports are editor-only) for %s. Package will not be saved."), *BaseFilename);
						}

						return false;
					}
				}

				// Import objects & names.
				TArray<FString> StringAssetReferencesMap;
				{
					TArray<UObject*> TagExpObjects;
					GetObjectsWithAnyMarks(TagExpObjects, OBJECTMARK_TagExp);
					for(int32 Index = 0; Index < TagExpObjects.Num(); Index++)
					{
						UObject* Obj = TagExpObjects[Index];
						check(Obj->HasAnyMarks(OBJECTMARK_TagExp));
						// Build list.
						FArchiveSaveTagImports ImportTagger(Linker, StringAssetReferencesMap);
						ImportTagger.SetPortFlags(ComparisonFlags);
						ImportTagger.SetFilterEditorOnly(FilterEditorOnly);

						UClass* Class = Obj->GetClass();

						if ( Obj->HasAnyFlags(RF_ClassDefaultObject) )
						{
							Class->SerializeDefaultObject(Obj, ImportTagger);
						}
						else
						{
							Obj->Serialize( ImportTagger );
						}

						ImportTagger << Class;

						UObject* Template = Obj->GetArchetype();
						if ( Template && Template != Class->GetDefaultObject() )
						{
							ImportTagger << Template;
						}

						if( Obj->IsIn(GetTransientPackage()) )
						{
							UE_LOG(LogSavePackage, Fatal, TEXT("%s"), *FString::Printf( TEXT("Transient object imported: %s"), *Obj->GetFullName() ) );
						}

						// add the list of dependencies to the dependency map
						ObjectDependencies.Add(Obj, ImportTagger.Dependencies);

						if (Obj->GetClass() != UObjectRedirector::StaticClass())
						{
							for ( auto DepIt = ImportTagger.Dependencies.CreateConstIterator(); DepIt; ++DepIt )
							{
								UObject* DependencyObject = *DepIt;
								if ( DependencyObject )
								{
									DependenciesReferencedByNonRedirectors.Add(DependencyObject);
								}
							}
						}
					}
				}

#if WITH_EDITOR
				// Remove TagExp from duplicate objects.
				TMap<UObject*, UObject*> DuplicateRedirects = UnmarkExportTagFromDuplicates();
#endif // WITH_EDITOR

				UE_LOG_COOK_TIME(TEXT("Serialize Imports"));
				
				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				TArray<UObject*> PrivateObjects;
				TArray<UObject*> ObjectsInOtherMaps;
				TArray<UObject*> LevelObjects;

				// Tag the names for all relevant object, classes, and packages.
				{
					TArray<UObject*> TagExpImpObjects;
					GetObjectsWithAnyMarks(TagExpImpObjects, EObjectMark(OBJECTMARK_TagExp|OBJECTMARK_TagImp));
					for(int32 Index = 0; Index < TagExpImpObjects.Num(); Index++)
					{
						UObject* Obj = TagExpImpObjects[Index];
						check(Obj->HasAnyMarks(EObjectMark(OBJECTMARK_TagExp|OBJECTMARK_TagImp)));

						SavePackageState->MarkNameAsReferenced(Obj->GetFName());
						if( Obj->GetOuter() )
						{
							SavePackageState->MarkNameAsReferenced(Obj->GetOuter()->GetFName());
						}

						if( Obj->HasAnyMarks(OBJECTMARK_TagImp) )
						{
							SavePackageState->MarkNameAsReferenced(Obj->GetClass()->GetFName());
							check(Obj->GetClass()->GetOuter());
							SavePackageState->MarkNameAsReferenced(Obj->GetClass()->GetOuter()->GetFName());
						
							// if a private object was marked by the cooker, it will be in memory on load, and will be found. helps with some objects
							// from a package in a package being moved into Startup_int.xxx, but not all
							// Imagine this:
							// Package P:
							//   - A (private)
							//   - B (public, references A)
							//   - C (public, references A)
							// Map M:
							//   - MObj (references B)
							// Startup Package S:
							//   - SObj (references C)
							// When Startup is cooked, it will pull in C and A. When M is cooked, it will pull in B, but not A, because
							// A was already marked by the cooker. M.xxx now has a private import to A, which is normally illegal, hence
							// the OBJECTMARK_MarkedByCooker check below
							if( !Obj->HasAnyFlags(RF_Public) )
							{
								PrivateObjects.Add(Obj);
							}

							// See whether the object we are referencing is in another map package.
							UPackage* ObjPackage = dynamic_cast<UPackage*>(Obj->GetOutermost());
							if( ObjPackage && ObjPackage->ContainsMap() )
							{
								if ( ObjPackage != Obj && Obj->GetFName() != NAME_PersistentLevel && Obj->GetClass()->GetFName() != WorldClassName )
								{
									ObjectsInOtherMaps.Add(Obj);

									if ( DependenciesReferencedByNonRedirectors.Contains(Obj) )
									{
										UE_LOG(LogSavePackage, Warning, TEXT( "Obj in another map: %s"), *Obj->GetFullName() );

										if (!(SaveFlags & SAVE_NoError))
										{
											Error->Logf(ELogVerbosity::Warning, *FText::Format( NSLOCTEXT( "Core", "SavePackageObjInAnotherMap", "Object '{0}' is in another map" ), FText::FromString( *Obj->GetFullName() ) ).ToString() );
										}
									}
								}
								else
								{
									LevelObjects.Add(Obj);
								}
							}

						}
					}
				}

				UE_LOG_COOK_TIME(TEXT("Mark Names"));

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				if ( LevelObjects.Num() > 0 && ObjectsInOtherMaps.Num() == 0 )
				{
					ObjectsInOtherMaps = LevelObjects;
				}

				// It is allowed for redirectors to reference objects in other maps.
				// Form the list of objects that erroneously reference another map.
				TArray<UObject*> IllegalObjectsInOtherMaps;
				for ( auto ObjIt = ObjectsInOtherMaps.CreateConstIterator(); ObjIt; ++ObjIt )
				{
					if ( DependenciesReferencedByNonRedirectors.Contains(*ObjIt) )
					{
						IllegalObjectsInOtherMaps.Add(*ObjIt);
					}
				}

				// The graph is linked to objects in a different map package!
				if( IllegalObjectsInOtherMaps.Num() )
				{
					UObject* MostLikelyCulprit = NULL;
					const UProperty* PropertyRef = NULL;

					// construct a string containing up to the first 5 objects problem objects
					FString ObjectNames;
					int32 MaxNamesToDisplay = 5;
					bool DisplayIsLimited = true;

					if (IllegalObjectsInOtherMaps.Num() < MaxNamesToDisplay)
					{
						MaxNamesToDisplay = IllegalObjectsInOtherMaps.Num();
						DisplayIsLimited = false;
					}

					for (int32 Idx = 0; Idx < MaxNamesToDisplay; Idx++)
					{
						ObjectNames += IllegalObjectsInOtherMaps[Idx]->GetName() + TEXT("\n");
					}
					
					// if there are more than 5 items we indicated this by adding "..." at the end of the list
					if (DisplayIsLimited)
					{
						ObjectNames += TEXT("...\n");
					}

					Args.Empty();
					Args.Add( TEXT("FileName"), FText::FromString( Filename ) );
					Args.Add( TEXT("ObjectNames"), FText::FromString( ObjectNames ) );
					const FText Message = FText::Format( NSLOCTEXT("Core", "LinkedToObjectsInOtherMap_FindCulpritQ", "Can't save {FileName}: Graph is linked to object(s) in external map.\nExternal Object(s):\n{ObjectNames}  \nTry to find the chain of references to that object (may take some time)?"), Args );

					FString CulpritString = TEXT( "Unknown" );
					FMessageDialog::Open(EAppMsgType::Ok, Message);

					FindMostLikelyCulprit( IllegalObjectsInOtherMaps, MostLikelyCulprit, PropertyRef );
					if( MostLikelyCulprit != NULL && PropertyRef != NULL )
					{
						CulpritString = FString::Printf(TEXT("%s (%s)"), *MostLikelyCulprit->GetFullName(), *PropertyRef->GetName());
					}
					else if (MostLikelyCulprit != NULL)
					{
						CulpritString = FString::Printf(TEXT("%s (Unknown property)"), *MostLikelyCulprit->GetFullName());
					}

					// Free the file handle and delete the temporary file
					Linker->Detach();
					IFileManager::Get().Delete( *TempFilename );
					if (!(SaveFlags & SAVE_NoError))
					{
						Error->Logf(ELogVerbosity::Warning, TEXT("Can't save %s: Graph is linked to object %s in external map"), Filename, *CulpritString);
					}
					return false;
				}

				// The graph is linked to private objects!
				if ( PrivateObjects.Num() )
				{

					UObject* MostLikelyCulprit = NULL;
					const UProperty* PropertyRef = NULL;
					
					// construct a string containing up to the first 5 objects problem objects
					FString ObjectNames;
					int32 MaxNamesToDisplay = 5;
					bool DisplayIsLimited = true;

					if (PrivateObjects.Num() < MaxNamesToDisplay)
					{
						MaxNamesToDisplay = PrivateObjects.Num();
						DisplayIsLimited = false;
					}

					for (int32 Idx = 0; Idx < MaxNamesToDisplay; Idx++)
					{
						ObjectNames += PrivateObjects[Idx]->GetName() + TEXT("\n");
					}

					// if there are more than 5 items we indicated this by adding "..." at the end of the list
					if (DisplayIsLimited)
					{
						ObjectNames += TEXT("...\n");
					}

					Args.Empty();
					Args.Add( TEXT("FileName"), FText::FromString( Filename ) );
					Args.Add( TEXT("ObjectNames"), FText::FromString( ObjectNames ) );
					const FText Message = FText::Format( NSLOCTEXT("Core", "LinkedToPrivateObjectsInOtherPackage_FindCulpritQ", "Can't save {FileName}: Graph is linked to private object(s) in an external package.\nExternal Object(s):\n{ObjectNames}  \nTry to find the chain of references to that object (may take some time)?"), Args );

					FString CulpritString = TEXT( "Unknown" );
					FMessageDialog::Open(EAppMsgType::Ok, Message);

					FindMostLikelyCulprit( PrivateObjects, MostLikelyCulprit, PropertyRef );
					CulpritString = FString::Printf(TEXT("%s (%s)"),
						(MostLikelyCulprit != NULL) ? *MostLikelyCulprit->GetFullName() : TEXT("(unknown culprit)"),
						(PropertyRef != NULL) ? *PropertyRef->GetName() : TEXT("unknown property ref"));

					// free the file handle and delete the temporary file
					Linker->Detach();
					IFileManager::Get().Delete( *TempFilename );
					if (!(SaveFlags & SAVE_NoError))
					{
						Error->Logf(ELogVerbosity::Warning, TEXT("Can't save %s: Graph is linked to external private object %s"), Filename, *CulpritString);
					}
					return false;
				}


				// store the additional packages for cooking
				Linker->Summary.AdditionalPackagesToCook = AdditionalPackagesToCook;

				// Write fixed-length file summary to overwrite later.
				if( Conform )
				{
					// Conform to previous generation of file.
					UE_LOG(LogSavePackage, Log,  TEXT("Conformal save, relative to: %s, Generation %i"), *Conform->Filename, Conform->Summary.Generations.Num()+1 );
					Linker->Summary.Guid        = Conform->Summary.Guid;
					Linker->Summary.Generations = Conform->Summary.Generations;
				}
				else if (SaveFlags & SAVE_KeepGUID)
				{
					// First generation file, keep existing GUID
					Linker->Summary.Guid = InOuter->Guid;
					Linker->Summary.Generations = TArray<FGenerationInfo>();
				}
				else
				{
					// First generation file.
					Linker->Summary.Guid = FGuid::NewGuid();
					Linker->Summary.Generations = TArray<FGenerationInfo>();

					// make sure the UPackage's copy of the GUID is up to date
					InOuter->Guid = Linker->Summary.Guid;
				}
				new(Linker->Summary.Generations)FGenerationInfo(0, 0);
				*Linker << Linker->Summary;
				int32 OffsetAfterPackageFileSummary = Linker->Tell();
		
				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();


				// Build NameMap.
				Linker->Summary.NameOffset = Linker->Tell();
				SavePackageState->UpdateLinkerWithMarkedNames(Linker);

#if WITH_EDITOR
				if ( GOutputCookingWarnings )
				{
					// check the name list for uniqueobjectnamefor cooking
					static FName NAME_UniqueObjectNameForCooking(TEXT("UniqueObjectNameForCooking"));

					for (const auto& NameInUse : Linker->NameMap)
					{
						if (NameInUse.GetComparisonIndex() == NAME_UniqueObjectNameForCooking.GetComparisonIndex())
						{
							//UObject *Object = FindObject<UObject>( ANY_PACKAGE, *NameInUse.ToString());

							// error
							// check(Object);
							UE_LOG(LogSavePackage, Warning, TEXT("Saving object into cooked package which was created at cook time, Object Name %s"), *NameInUse.ToString());
						}
					}
				}
#endif

				UE_LOG_COOK_TIME(TEXT("Serialize Summary"));

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				// Sort names.
				FObjectNameSortHelper NameSortHelper;
				NameSortHelper.SortNames( Linker, Conform );

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				// Save names.
				Linker->Summary.NameCount = Linker->NameMap.Num();
				for( int32 i=0; i<Linker->NameMap.Num(); i++ )
				{
					*Linker << *const_cast<FNameEntry*>(Linker->NameMap[i].GetDisplayNameEntry());
					Linker->NameIndices.Add(Linker->NameMap[i], i);
				}

				
				UE_LOG_COOK_TIME(TEXT("Serialize Names"));

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				// Build ImportMap.
				{
					TArray<UObject*> TagImpObjects;
					GetObjectsWithAnyMarks(TagImpObjects, OBJECTMARK_TagImp);
					for(int32 Index = 0; Index < TagImpObjects.Num(); Index++)
					{
						UObject* Obj = TagImpObjects[Index];
						check(Obj->HasAnyMarks(OBJECTMARK_TagImp));
						new( Linker->ImportMap )FObjectImport( Obj );
					}
				}


				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				// sort and conform imports
				FObjectImportSortHelper ImportSortHelper;
				ImportSortHelper.SortImports( Linker, Conform );
				Linker->Summary.ImportCount = Linker->ImportMap.Num();

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				
				// Build ExportMap.
				{
					TArray<UObject*> TagExpObjects;
					GetObjectsWithAnyMarks(TagExpObjects, OBJECTMARK_TagExp);
					for(int32 Index = 0; Index < TagExpObjects.Num(); Index++)
					{
						UObject* Obj = TagExpObjects[Index];
						check(Obj->HasAnyMarks(OBJECTMARK_TagExp));
						new( Linker->ExportMap )FObjectExport( Obj );
					}
				}



#if WITH_EDITOR
				if (GOutputCookingWarnings)
				{
					// check the name list for uniqueobjectnamefor cooking
					static FName NAME_UniqueObjectNameForCooking(TEXT("UniqueObjectNameForCooking"));

					for (const auto& Export : Linker->ExportMap)
					{
						const auto& NameInUse = Export.ObjectName;
						if (NameInUse.GetComparisonIndex() == NAME_UniqueObjectNameForCooking.GetComparisonIndex())
						{
							UE_LOG(LogSavePackage, Warning, TEXT("Saving object into cooked package which was created at cook time, Object Name %s, Full Path %s, Class %s"), *NameInUse.ToString(), *Export.Object->GetFullName(), *Export.Object->GetClass()->GetName());
						}
					}
				}
#endif


				UE_LOG_COOK_TIME(TEXT("Build Export Map"));

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				// Sort exports alphabetically and conform the export table (if necessary)
				FObjectExportSortHelper ExportSortHelper;
				ExportSortHelper.SortExports( Linker, Conform );
				
				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				// Sort exports for seek-free loading.
				FObjectExportSeekFreeSorter SeekFreeSorter;
				SeekFreeSorter.SortExports( Linker, Conform );
				Linker->Summary.ExportCount = Linker->ExportMap.Num();
				
				UE_LOG_COOK_TIME(TEXT("Sort Exports"));

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				// Pre-size depends map.
				Linker->DependsMap.AddZeroed( Linker->ExportMap.Num() );

				// track import and export object linker index
				TMap<UObject*, FPackageIndex> ImportToIndexMap;
				TMap<UObject*, FPackageIndex> ExportToIndexMap;
				for (int32 ImpIndex = 0; ImpIndex < Linker->ImportMap.Num(); ImpIndex++)
				{
					ImportToIndexMap.Add(Linker->ImportMap[ImpIndex].XObject, FPackageIndex::FromImport(ImpIndex));
				}

				for (int32 ExpIndex = 0; ExpIndex < Linker->ExportMap.Num(); ExpIndex++)
				{
					ExportToIndexMap.Add(Linker->ExportMap[ExpIndex].Object, FPackageIndex::FromExport(ExpIndex));
				}

				
				// go back over the (now sorted) exports and fill out the DependsMap
				for (int32 ExpIndex = 0; ExpIndex < Linker->ExportMap.Num(); ExpIndex++)
				{
					UObject* Object = Linker->ExportMap[ExpIndex].Object;
					// sorting while conforming can create NULL export map entries, so skip those depends map
					if (Object == NULL)
					{
						UE_LOG(LogSavePackage, Warning, TEXT("Object is missing for an export, unable to save dependency map. Most likely this is caused my conforming against a package that is missing this object. See log for more info"));
						if (!(SaveFlags & SAVE_NoError))
						{
							Error->Logf(ELogVerbosity::Warning, *FText::Format( NSLOCTEXT( "Core", "SavePackageObjectIsMissingExport", "Object is missing for an export, unable to save dependency map for asset '{0}'. Most likely this is caused my conforming against a asset that is missing this object. See log for more info" ), FText::FromString( FString( Filename ) ) ).ToString() );
						}
						continue;
					}

					// add a dependency map entry also
					TArray<FPackageIndex>& DependIndices = Linker->DependsMap[ExpIndex];
					if ( Object != NULL )
					{
						// find all the objects needed by this export
						TArray<UObject*>* SrcDepends = ObjectDependencies.Find(Object);
						checkf(SrcDepends,TEXT("Couldn't find dependency map for %s"), *Object->GetFullName());

						// go through each object and...
						for (int32 DependIndex = 0; DependIndex < SrcDepends->Num(); DependIndex++)
						{
							UObject* DependentObject = (*SrcDepends)[DependIndex];

							FPackageIndex DependencyIndex;

							// if the dependency is in the same pacakge, we need to save an index into our ExportMap
							if (DependentObject->GetOutermost() == Linker->LinkerRoot)
							{
								// ... find the associated ExportIndex
								DependencyIndex = ExportToIndexMap.FindRef(DependentObject);
							}
							// otherwise we need to save an index into the ImportMap
							else
							{
								// ... find the associated ImportIndex
								DependencyIndex = ImportToIndexMap.FindRef(DependentObject);
							}
					
#if WITH_EDITOR
							// If we still didn't find index, maybe it was a duplicate export which got removed.
							// Check if we have a redirect to original.
							if (DependencyIndex.IsNull())
							{
								DependencyIndex = ExportToIndexMap.FindRef(DuplicateRedirects[DependentObject]);
							}
#endif
							// if we didn't find it (FindRef returns 0 on failure, which is good in this case), then we are in trouble, something went wrong somewhere
							checkf(!DependencyIndex.IsNull(), TEXT("Failed to find dependency index for %s (%s)"), *DependentObject->GetFullName(), *Object->GetFullName());

							// add the import as an import for this export
							DependIndices.Add(DependencyIndex);
						}
					}
				}



				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				// Set linker reverse mappings.
				// also set netplay required data for any UPackages in the export map
				for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
				{
					UObject* Object = Linker->ExportMap[i].Object;
					if( Object )
					{
						Linker->ObjectIndicesMap.Add(Object, FPackageIndex::FromExport(i));

						UPackage* Package = dynamic_cast<UPackage*>(Object);
						if (Package != NULL)
						{
							Linker->ExportMap[i].PackageFlags = Package->PackageFlags;
							if (!(Package->PackageFlags & PKG_ServerSideOnly))
							{
								Linker->ExportMap[i].PackageGuid = Package->GetGuid();
							}
						}
					}
				}

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				// If this is a map package, make sure there is a world or level in the export map.
				if ( InOuter->ContainsMap() )
				{
					bool bContainsMap = false;
					for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
					{
						UObject* Object = Linker->ExportMap[i].Object;
						if ( Object )
						{
							const FString ExportClassName = Object->GetClass()->GetName();
							if( ExportClassName == TEXT("World") || ExportClassName == TEXT("Level") )
							{
								bContainsMap = true;
								break;
							}
						}
					}
					if (!bContainsMap)
					{
						ensureMsgf(false, TEXT("Attempting to save a map package '%s' that does not contain a map object."), *InOuter->GetName());
						UE_LOG(LogSavePackage, Error, TEXT("Attempting to save a map package '%s' that does not contain a map object."), *InOuter->GetName());

						if (!(SaveFlags & SAVE_NoError))
						{
							Error->Logf(ELogVerbosity::Warning, *FText::Format( NSLOCTEXT( "Core", "SavePackageNoMap", "Attempting to save a map asset '{0}' that does not contain a map object" ), FText::FromString( FString( Filename ) ) ).ToString() );
						}
						Success = false;
					}
				}


				UE_LOG_COOK_TIME(TEXT("Build Dependency Map"));

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				for( int32 i=0; i<Linker->ImportMap.Num(); i++ )
				{
					UObject* Object = Linker->ImportMap[i].XObject;
					if( Object != NULL )
					{
						Linker->ObjectIndicesMap.Add(Object, FPackageIndex::FromImport(i));
					}
					else
					{
						// the only reason we should ever have a NULL object in the import is when this package is being conformed against another package, and the new
						// version of the package no longer has this import
						checkf(Conform != NULL, TEXT("NULL XObject for import %i - Object: %s Class: %s"), i, *Linker->ImportMap[i].ObjectName.ToString(), *Linker->ImportMap[i].ClassName.ToString());
					}
				}


				SlowTask.EnterProgressFrame();

				// Find components referenced by exports.

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				// Save dummy import map, overwritten later.
				Linker->Summary.ImportOffset = Linker->Tell();
				for( int32 i=0; i<Linker->ImportMap.Num(); i++ )
				{
					FObjectImport& Import = Linker->ImportMap[ i ];
					*Linker << Import;
				}
				int32 OffsetAfterImportMap = Linker->Tell();



				UE_LOG_COOK_TIME(TEXT("Serialize Import Map"));

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				// Save dummy export map, overwritten later.
				Linker->Summary.ExportOffset = Linker->Tell();
				for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
				{
					FObjectExport& Export = Linker->ExportMap[ i ];
					*Linker << Export;
				}
				int32 OffsetAfterExportMap = Linker->Tell();


				UE_LOG_COOK_TIME(TEXT("Serialize Export Map"));

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				// save depends map (no need for later patching)
				check(Linker->DependsMap.Num()==Linker->ExportMap.Num());
				Linker->Summary.DependsOffset = Linker->Tell();
				for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
				{
					TArray<FPackageIndex>& Depends = Linker->DependsMap[ i ];
					*Linker << Depends;
				}


				UE_LOG_COOK_TIME(TEXT("Serialize Dependency Map"));

				if (EndSavingIfCancelled(Linker, TempFilename)) { return false; }
				SlowTask.EnterProgressFrame();

				// Save string asset reference map
				Linker->Summary.StringAssetReferencesOffset = Linker->Tell();
				Linker->Summary.StringAssetReferencesCount = StringAssetReferencesMap.Num();
				for (int32 i = 0; i < StringAssetReferencesMap.Num(); i++)
				{
					*Linker << StringAssetReferencesMap[i];
				}

				UE_LOG_COOK_TIME(TEXT("Serialize StringAssetReference Map"));
	
				// Save thumbnails
				UPackage::SaveThumbnails( InOuter, Linker );

				
				// Save asset registry data so the editor can search for information about assets in this package
				UPackage::SaveAssetRegistryData( InOuter, Linker );

				// Save level information used by World browser
				UPackage::SaveWorldLevelInfo( InOuter, Linker );
				
				Linker->Summary.TotalHeaderSize	= Linker->Tell();

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame(1, NSLOCTEXT("Core", "ProcessingExports", "ProcessingExports..."));

				// look for this package in the list of packages to generate script SHA for 
				TArray<uint8>* ScriptSHABytes = FLinkerSave::PackagesToScriptSHAMap.Find(*FPaths::GetBaseFilename(Filename));

				// if we want to generate the SHA key, start tracking script writes
				if (ScriptSHABytes)
				{
					Linker->StartScriptSHAGeneration();
				}

				UE_LOG_COOK_TIME(TEXT("SaveThumbNails Save AssetRegistryData SaveWorldLevelInfo"));
				
				{
					FScopedSlowTask ExportScope(Linker->ExportMap.Num());

					// Save exports.
					int32 LastExportSaveStep = 0;
					for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
					{
						if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
						ExportScope.EnterProgressFrame();

						FObjectExport& Export = Linker->ExportMap[i];
						if( Export.Object )
						{
							// Set class index.
							// If this is *exactly* a UClass, store null instead; for anything else, including UClass-derived classes, map it
							if( Export.Object->GetClass() != UClass::StaticClass() )
							{
								Export.ClassIndex = Linker->MapObject(Export.Object->GetClass());
								check(!Export.ClassIndex.IsNull());
							}
							else
							{
								Export.ClassIndex = FPackageIndex();
							}

							// Set the parent index, if this export represents a UStruct-derived object
							if( UStruct* Struct = dynamic_cast<UStruct*>(Export.Object) )
							{
								if (Struct->GetSuperStruct() != NULL)
								{
									Export.SuperIndex = Linker->MapObject(Struct->GetSuperStruct());
									check(!Export.SuperIndex.IsNull());
								}
								else
								{
									Export.SuperIndex = FPackageIndex();
								}
							}
							else
							{
								Export.SuperIndex = FPackageIndex();
							}

							// Set FPackageIndex for this export's Outer. If the export's Outer
							// is the UPackage corresponding to this package's LinkerRoot, the
							if( Export.Object->GetOuter() != InOuter )
							{
								check( Export.Object->GetOuter() );
								checkf(Export.Object->GetOuter()->IsIn(InOuter),
									TEXT("Export Object (%s) Outer (%s) mismatch."),
									*(Export.Object->GetPathName()),
									*(Export.Object->GetOuter()->GetPathName()));
								Export.OuterIndex = Linker->MapObject(Export.Object->GetOuter());
								checkf(!Export.OuterIndex.IsImport(), 
									TEXT("Export Object (%s) Outer (%s) is an Import."),
									*(Export.Object->GetPathName()),
									*(Export.Object->GetOuter()->GetPathName()));
							}
							else
							{
								// this export's Outer is the LinkerRoot for this package
								Export.OuterIndex = FPackageIndex();
							}

							// Save the object data.
							Export.SerialOffset = Linker->Tell();
							// UE_LOG(LogSavePackage, Log, TEXT("export %s for %s"), *Export.Object->GetFullName(), *Linker->CookingTarget()->PlatformName());
							if ( Export.Object->HasAnyFlags(RF_ClassDefaultObject) )
							{
								Export.Object->GetClass()->SerializeDefaultObject(Export.Object, *Linker);
							}
							else
							{
								Export.Object->Serialize( *Linker );
							}
							Export.SerialSize = Linker->Tell() - Export.SerialOffset;

							// Mark object as having been saved.
							Export.Object->Mark(OBJECTMARK_Saved);
						}
					}
				}

				UE_LOG_COOK_TIME(TEXT("Serialize Exports"));

				// if we want to generate the SHA key, get it out now that the package has finished saving
				if (ScriptSHABytes && Linker->ContainsCode())
				{
					// make space for the 20 byte key
					ScriptSHABytes->Empty(20);
					ScriptSHABytes->AddUninitialized(20);

					// retrieve it
					Linker->GetScriptSHAKey(ScriptSHABytes->GetData());
				}

				
				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame(1, NSLOCTEXT("Core", "SerializingBulkData", "Serializing bulk data"));

				// now we write all the bulkdata that is supposed to be at the end of the package
				// and fix up the offset
				int64 StartOfBulkDataArea = Linker->Tell();
				Linker->Summary.BulkDataStartOffset = StartOfBulkDataArea;

				{
					FScopedSlowTask BulkDataFeedback(Linker->BulkDataToAppend.Num());

					for (int32 i=0; i < Linker->BulkDataToAppend.Num(); ++i)
					{
						BulkDataFeedback.EnterProgressFrame();

						FLinkerSave::FBulkDataStorageInfo& BulkDataStorageInfo = Linker->BulkDataToAppend[i];

						// Set bulk data flags to what they were during initial serialization (they might have changed after that)
						const uint32 OldBulkDataFlags = BulkDataStorageInfo.BulkData->GetBulkDataFlags();
						BulkDataStorageInfo.BulkData->SetBulkDataFlags(BulkDataStorageInfo.BulkDataFlags);

						int64 BulkStartOffset = Linker->Tell();
						int64 StoredBulkStartOffset = BulkStartOffset - StartOfBulkDataArea;

						BulkDataStorageInfo.BulkData->SerializeBulkData(*Linker, BulkDataStorageInfo.BulkData->Lock(LOCK_READ_ONLY));

						int64 BulkEndOffset = Linker->Tell();
						int32 SizeOnDisk = (int32)(BulkEndOffset - BulkStartOffset);
				
						Linker->Seek(BulkDataStorageInfo.BulkDataOffsetInFilePos);
						*Linker << StoredBulkStartOffset;

						Linker->Seek(BulkDataStorageInfo.BulkDataSizeOnDiskPos);
						*Linker << SizeOnDisk;

						Linker->Seek(BulkEndOffset);

						// Restore BulkData flags to before serialization started
						BulkDataStorageInfo.BulkData->ClearBulkDataFlags(0xFFFFFFFF);
						BulkDataStorageInfo.BulkData->SetBulkDataFlags(OldBulkDataFlags);

						BulkDataStorageInfo.BulkData->Unlock();
					}
				}

				Linker->BulkDataToAppend.Empty();
			
				// write the package post tag
				uint32 Tag = PACKAGE_FILE_TAG;
				*Linker << Tag;

				UE_LOG_COOK_TIME(TEXT("Serialize BulkData"));

				// We capture the package size before the first seek to work with archives that don't report the file
				// size correctly while the file is still being written to.
				PackageSize = Linker->Tell();

				// Save the import map.
				Linker->Seek( Linker->Summary.ImportOffset );
				for( int32 i=0; i<Linker->ImportMap.Num(); i++ )
				{
					FObjectImport& Import = Linker->ImportMap[ i ];
					if ( Import.XObject )
					{
					// Set the package index.
						if( Import.XObject->GetOuter() )
						{
							if ( Import.XObject->GetOuter()->IsIn(InOuter) )
							{
								if ( !Import.XObject->HasAllFlags(RF_Native|RF_Transient) )
								{
									UE_LOG(LogSavePackage, Warning, TEXT("Bad Object=%s"),*Import.XObject->GetFullName());
								}
								else
								{
									// if an object is marked RF_Transient|RF_Native, it is either an intrinsic class or
									// a property of an intrinsic class.  Only properties of intrinsic classes will have
									// an Outer that passes the check for "GetOuter()->IsIn(InOuter)" (thus ending up in this
									// block of code).  Just verify that the Outer for this property is also marked RF_Transient|RF_Native
									check(Import.XObject->GetOuter()->HasAllFlags(RF_Native|RF_Transient));
								}
							}
							check(!Import.XObject->GetOuter()->IsIn(InOuter)||Import.XObject->HasAllFlags(EObjectFlags(RF_Native|RF_Transient)));
							Import.OuterIndex = Linker->MapObject(Import.XObject->GetOuter());
						}
					}
					else
					{
						checkf(Conform != NULL, TEXT("NULL XObject for import %i - Object: %s Class: %s"), i, *Import.ObjectName.ToString(), *Import.ClassName.ToString());
					}

					// Save it.
					*Linker << Import;
				}
				

				UE_LOG_COOK_TIME(TEXT("Serialize Import Map"));

				check( Linker->Tell() == OffsetAfterImportMap );

				// Save the export map.
				Linker->Seek( Linker->Summary.ExportOffset );
				for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
				{
					*Linker << Linker->ExportMap[ i ];
				}
				check( Linker->Tell() == OffsetAfterExportMap );

				UE_LOG_COOK_TIME(TEXT("Serialize Export Map"));

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				FFormatNamedArguments NamedArgs;
				NamedArgs.Add( TEXT("CleanFilename"), FText::FromString( CleanFilename ) );
				SlowTask.DefaultMessage = FText::Format( NSLOCTEXT("Core", "Finalizing", "Finalizing: {CleanFilename}..."), NamedArgs );

				//@todo: remove ExportCount and NameCount - no longer used
				Linker->Summary.Generations.Last().ExportCount = Linker->Summary.ExportCount;
				Linker->Summary.Generations.Last().NameCount   = Linker->Summary.NameCount;	

				// create the package source (based on developer or user created)
	#if (UE_BUILD_SHIPPING && WITH_EDITOR)
				Linker->Summary.PackageSource = FMath::Rand() * FMath::Rand();
	#else
				Linker->Summary.PackageSource = FCrc::StrCrc_DEPRECATED(*FPaths::GetBaseFilename(Filename).ToUpper());
	#endif

				// Flag package as requiring localization gather if the archive requires localization gathering.
				Linker->LinkerRoot->ThisRequiresLocalizationGather(Linker->RequiresLocalizationGather());
				
				// Update package flags from package, in case serialization has modified package flags.
				Linker->Summary.PackageFlags  = Linker->LinkerRoot->PackageFlags & ~PKG_NewlyCreated;

				Linker->Seek(0);
				*Linker << Linker->Summary;
				check( Linker->Tell() == OffsetAfterPackageFileSummary );

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				// Detach archive used for saving, closing file handle.
				if( Linker && !bCompressFromMemory && !bSaveAsync)
				{
					Linker->Detach();
				}
				UNCLOCK_CYCLES(Time);
				UE_LOG(LogSavePackage, Log,  TEXT("Save=%fms"), FPlatformTime::ToMilliseconds(Time) );
		
				if ( EndSavingIfCancelled( Linker, TempFilename ) ) { return false; }
				SlowTask.EnterProgressFrame();

				if( Success == true )
				{
					// Compress the temporarily file to destination.
					if( bCompressFromMemory )
					{
						UE_LOG(LogSavePackage, Log,  TEXT("Compressing from memory to '%s'"), *NewPath );
						FFileCompressionHelper CompressionHelper;
						Success = CompressionHelper.CompressFile( *NewPath, Linker );
						// Detach archive used for memory saving.
						if( Linker )
						{
							Linker->Detach();
						}
					}
					// Compress the temporarily file to destination.
					else if( InOuter->PackageFlags & PKG_StoreCompressed )
					{
						UE_LOG(LogSavePackage, Log,  TEXT("Compressing '%s' to '%s'"), *TempFilename, *NewPath );
						FFileCompressionHelper CompressionHelper;
						Success = CompressionHelper.CompressFile( *TempFilename, *NewPath, Linker );
					}
					// Fully compress package in one "block".
					else if( InOuter->PackageFlags & PKG_StoreFullyCompressed )
					{
						UE_LOG(LogSavePackage, Log,  TEXT("Full-package compressing '%s' to '%s'"), *TempFilename, *NewPath );
						FFileCompressionHelper CompressionHelper;
						Success = CompressionHelper.FullyCompressFile( *TempFilename, *NewPath, Linker->ForceByteSwapping() );
					}
					else if (bSaveAsync)
					{
						UE_LOG(LogSavePackage, Log,  TEXT("Async saving from memory to '%s'"), *NewPath );

						// Detach archive used for memory saving.
						if( Linker )
						{
							AsyncWriteFile(*(FBufferArchive*)(Linker->Saver), *NewPath, FinalTimeStamp);
							Linker->Detach();
						}
					}
					// Move the temporary file.
					else
					{
						UE_LOG(LogSavePackage, Log,  TEXT("Moving '%s' to '%s'"), *TempFilename, *NewPath );
						Success = IFileManager::Get().Move( *NewPath, *TempFilename );
						if (FinalTimeStamp != FDateTime::MinValue())
						{
							IFileManager::Get().SetTimeStamp(*NewPath, FinalTimeStamp);
						}
					}

					// Make sure to clean up any stale .uncompressed_size files.
					if( !(InOuter->PackageFlags & PKG_StoreFullyCompressed) )
					{
						IFileManager::Get().Delete( *(NewPath + TEXT(".uncompressed_size")) );
					}
			
					if( Success == false )
					{
						if (SaveFlags & SAVE_NoError)
						{
							UE_LOG(LogSavePackage, Warning, TEXT("%s"), *FString::Printf( TEXT("Error saving '%s'"), Filename ) );
						}
						else
						{
							UE_LOG(LogSavePackage, Error, TEXT("%s"), *FString::Printf( TEXT("Error saving '%s'"), Filename ) );
							Error->Logf(ELogVerbosity::Warning, *FText::Format( NSLOCTEXT( "Core", "SaveWarning", "Error saving '{0}'" ), FText::FromString( FString( Filename ) ) ).ToString() );
						}
					}
					else
					{
						// Mark exports and the package as RF_Loaded after they've been serialized
						// This is to ensue that newly created packages are properly marked as loaded (since they now exist on disk and 
						// in memory in the exact same state).
						for (auto& Export : Linker->ExportMap)
						{
							if (Export.Object)
							{
								Export.Object->SetFlags(RF_WasLoaded);
							}
						}
						if (Linker->LinkerRoot)
						{
							// And finally set the flag on the package itself.
							Linker->LinkerRoot->SetFlags(RF_WasLoaded);
						}

						// Clear dirty flag if desired
						if (!(SaveFlags & SAVE_KeepDirty))
						{
							InOuter->SetDirtyFlag(false);
						}
						
						// Update package FileSize value
						InOuter->FileSize = IFileManager::Get().FileSize(Filename);

						// Warn about long package names, which may be bad for consoles with limited filename lengths.
						if( bWarnOfLongFilename == true )
						{
							int32 MaxFilenameLength = MAX_UNREAL_FILENAME_LENGTH;

							// If the name is of the form "_LOC_xxx.ext", remove the loc data before the length check
							FString CleanBaseFilename = BaseFilename;
							if( CleanBaseFilename.Find( "_LOC_" ) == BaseFilename.Len() - 8 )
							{
								CleanBaseFilename = BaseFilename.LeftChop( 8 );
							}

							if( CleanBaseFilename.Len() > MaxFilenameLength )
							{
								if (SaveFlags & SAVE_NoError)
								{
									UE_LOG(LogSavePackage, Warning, TEXT("%s"), *FString::Printf( TEXT("Filename '%s' is too long; this may interfere with cooking for consoles.  Unreal filenames should be no longer than %s characters."), *BaseFilename, MaxFilenameLength ) );
								}
								else
								{
									FFormatNamedArguments Arguments;
									Arguments.Add(TEXT("FileName"), FText::FromString( BaseFilename ));
									Arguments.Add(TEXT("MaxLength"), FText::AsNumber( MaxFilenameLength ));
									Error->Logf(ELogVerbosity::Warning, *FText::Format( NSLOCTEXT( "Core", "Error_FilenameIsTooLongForCooking", "Filename '{FileName}' is too long; this may interfere with cooking for consoles.  Unreal filenames should be no longer than {MaxLength} characters." ), Arguments ).ToString() );
								}
							}
						}
					}

					// Delete the temporary file.
					IFileManager::Get().Delete( *TempFilename );
				}


				SlowTask.EnterProgressFrame();

				// Route PostSaveRoot to allow e.g. the world to detach components for the persistent level that where
				// attached in PreSaveRoot.
				if( Base )
				{
					Base->PostSaveRoot( bCleanupIsRequired );
				}

				SlowTask.EnterProgressFrame();
			
#if WITH_EDITOR
				for ( int CachedObjectIndex = 0; CachedObjectIndex < CachedObjects.Num(); ++CachedObjectIndex )
				{
					CachedObjects[CachedObjectIndex]->ClearCachedCookedPlatformData(TargetPlatform);
				}
#endif
			}

		}
		if( Success == true )
		{
			// Package has been save, so unmark NewlyCreated flag.
			InOuter->PackageFlags &= ~PKG_NewlyCreated;

			// send a message that the package was saved
			UPackage::PackageSavedEvent.Broadcast(Filename, InOuter);
		}

		// We're done!
		SlowTask.EnterProgressFrame();

		UE_FINISH_LOG_COOK_TIME();

		return Success;
	}
	else
	{
		return false;
	}
}



/**
 * Static: Saves thumbnail data for the specified package outer and linker
 *
 * @param	InOuter							the outer to use for the new package
 * @param	Linker							linker we're currently saving with
 */
void UPackage::SaveThumbnails( UPackage* InOuter, FLinkerSave* Linker )
{
	Linker->Summary.ThumbnailTableOffset = 0;

	// Do we have any thumbnails to save?
	if( !(Linker->Summary.PackageFlags & PKG_FilterEditorOnly) && InOuter->HasThumbnailMap() )
	{
		const FThumbnailMap& PackageThumbnailMap = InOuter->GetThumbnailMap();


		// Figure out which objects have thumbnails.  Note that we only want to save thumbnails
		// for objects that are actually in the export map.  This is so that we avoid saving out
		// thumbnails that were cached for deleted objects and such.
		TArray< FObjectFullNameAndThumbnail > ObjectsWithThumbnails;
		for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
		{
			FObjectExport& Export = Linker->ExportMap[i];
			if( Export.Object )
			{
				const FName ObjectFullName( *Export.Object->GetFullName() );
				const FObjectThumbnail* ObjectThumbnail = PackageThumbnailMap.Find( ObjectFullName );
		
				// if we didn't find the object via full name, try again with ??? as the class name, to support having
				// loaded old packages without going through the editor (ie cooking old packages)
				if (ObjectThumbnail == NULL)
				{
					// can't overwrite ObjectFullName, so that we add it properly to the map
					FName OldPackageStyleObjectFullName = FName(*FString::Printf(TEXT("??? %s"), *Export.Object->GetPathName()));
					ObjectThumbnail = PackageThumbnailMap.Find(OldPackageStyleObjectFullName);
				}
				if( ObjectThumbnail != NULL )
				{
					// IMPORTANT: We save all thumbnails here, even if they are a shared (empty) thumbnail!
					// Empty thumbnails let us know that an asset is in a package without having to
					// make a linker for it.
					ObjectsWithThumbnails.Add( FObjectFullNameAndThumbnail( ObjectFullName, ObjectThumbnail ) );
				}
			}
		}

		// preserve thumbnail rendered for the level
		const FObjectThumbnail* ObjectThumbnail = PackageThumbnailMap.Find(FName(*InOuter->GetFullName()));
		if (ObjectThumbnail != NULL)
		{
			ObjectsWithThumbnails.Add( FObjectFullNameAndThumbnail(FName(*InOuter->GetFullName()), ObjectThumbnail ) );
		}
		
		// Do we have any thumbnails?  If so, we'll save them out along with a table of contents
		if( ObjectsWithThumbnails.Num() > 0 )
		{
			// Save out the image data for the thumbnails
			for( int32 CurObjectIndex = 0; CurObjectIndex < ObjectsWithThumbnails.Num(); ++CurObjectIndex )
			{
				FObjectFullNameAndThumbnail& CurObjectThumb = ObjectsWithThumbnails[ CurObjectIndex ];

				// Store the file offset to this thumbnail
				CurObjectThumb.FileOffset = Linker->Tell();

				// Serialize the thumbnail!
				FObjectThumbnail* SerializableThumbnail = const_cast< FObjectThumbnail* >( CurObjectThumb.ObjectThumbnail );
				SerializableThumbnail->Serialize( *Linker );
			}


			// Store the thumbnail table of contents
			{
				Linker->Summary.ThumbnailTableOffset = Linker->Tell();

				// Save number of thumbnails
				int32 ThumbnailCount = ObjectsWithThumbnails.Num();
				*Linker << ThumbnailCount;

				// Store a list of object names along with the offset in the file where the thumbnail is stored
				for( int32 CurObjectIndex = 0; CurObjectIndex < ObjectsWithThumbnails.Num(); ++CurObjectIndex )
				{
					const FObjectFullNameAndThumbnail& CurObjectThumb = ObjectsWithThumbnails[ CurObjectIndex ];

					// Object name
					const FString ObjectFullName = CurObjectThumb.ObjectFullName.ToString();

					// Break the full name into it's class and path name parts
					const int32 FirstSpaceIndex = ObjectFullName.Find( TEXT( " " ) );
					check( FirstSpaceIndex != INDEX_NONE && FirstSpaceIndex > 0 );
					FString ObjectClassName = ObjectFullName.Left( FirstSpaceIndex );
					const FString ObjectPath = ObjectFullName.Mid( FirstSpaceIndex + 1 );

					// Remove the package name from the object path since that will be implicit based
					// on the package file name
					FString ObjectPathWithoutPackageName = ObjectPath.Mid( ObjectPath.Find( TEXT( "." ) ) + 1 );

					// Store both the object's class and path name (relative to it's package)
					*Linker << ObjectClassName;
					*Linker << ObjectPathWithoutPackageName;

					// File offset for the thumbnail (already saved out.)
					int32 FileOffset = CurObjectThumb.FileOffset;
					*Linker << FileOffset;
				}
			}
		}
	}

	// if content browser isn't enabled, clear the thumbnail map so we're not using additional memory for nothing
	if ( !GIsEditor || IsRunningCommandlet() )
	{
		InOuter->ThumbnailMap.Reset();
	}
}

void UPackage::SaveAssetRegistryData( UPackage* InOuter, FLinkerSave* Linker )
{
	// Make a copy of the tag map
	TArray<UObject*> AssetObjects;

	if( !(Linker->Summary.PackageFlags & PKG_FilterEditorOnly) )
	{
		// Find any exports which are not in the tag map
		for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
		{
			FObjectExport& Export = Linker->ExportMap[i];
			if( Export.Object && Export.Object->IsAsset() )
			{
				AssetObjects.Add(Export.Object);
			}
		}
	}

	// Store the asset registry offser in the file
	Linker->Summary.AssetRegistryDataOffset = Linker->Tell();

	// Save the number of objects in the tag map
	int32 ObjectCount = AssetObjects.Num();
	*Linker << ObjectCount;

	// If there are any Asset Registry tags, add them to the summary
	for (int32 ObjectIdx = 0; ObjectIdx < AssetObjects.Num(); ++ObjectIdx)
	{
		const UObject* Object = AssetObjects[ObjectIdx];

		// Exclude the package name in the object path, we just need to know the path relative to the outermost
		FString ObjectPath = Object->GetPathName(Object->GetOutermost());
		FString ObjectClassName = Object->GetClass()->GetName();
		
		TArray<FAssetRegistryTag> Tags;
		Object->GetAssetRegistryTags(Tags);

		int32 TagCount = Tags.Num();

		*Linker << ObjectPath;
		*Linker << ObjectClassName;
		*Linker << TagCount;
				
		for (TArray<FAssetRegistryTag>::TConstIterator TagIter(Tags); TagIter; ++TagIter)
		{
			FString Key = TagIter->Name.ToString();
			FString Value = TagIter->Value;
			*Linker << Key;
			*Linker << Value;
		}
	}
}

void UPackage::SaveWorldLevelInfo( UPackage* InOuter, FLinkerSave* Linker )
{
	Linker->Summary.WorldTileInfoDataOffset = 0;
	
	if(InOuter->WorldTileInfo.IsValid())
	{
		Linker->Summary.WorldTileInfoDataOffset = Linker->Tell();
		*Linker << *(InOuter->WorldTileInfo);
	}
}

bool UPackage::IsEmptyPackage (UPackage* Package, const UObject* LastReferencer)
{
	// Don't count null or volatile packages as empty, just let them be NULL or get GCed
	if ( Package != NULL )
	{
		// Make sure the package is fully loaded before determining if it is empty
		if( !Package->IsFullyLoaded() )
		{
			Package->FullyLoad();
		}

		if (LastReferencer != NULL)
		{
			// See if there will be no remaining non-redirector, non-metadata objects within this package other than LastReferencer
			for (TObjectIterator<UObject> ObjIt; ObjIt; ++ObjIt)
			{
				UObject* Object = *ObjIt;
				if ( Object->IsIn(Package)								// Is inside this package
					&& Object->IsAsset()								// Is an asset
					&& Object != LastReferencer )						// Is not the object being deleted
				{
					// The package contains at least one more UObject
					return false;
				}
			}
		}
		else
		{
			// See if there are no more objects within this package other than redirectors and meta data objects
			for (TObjectIterator<UObject> ObjIt; ObjIt; ++ObjIt)
			{
				UObject* Object = *ObjIt;
				if ( Object->IsIn(Package)								// Is inside this package
					&& Object->IsAsset() )								// Is an asset
				{
					// The package contains at least one more UObject
					return false;
				}
			}
		}

		// There are no more valid UObjects with this package as an outer
		return true;
	}

	// Invalid package
	return false;
}
