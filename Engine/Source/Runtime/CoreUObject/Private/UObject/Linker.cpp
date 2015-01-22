// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Linker.cpp: Unreal object linker.
=============================================================================*/

#include "CoreUObjectPrivate.h"
#include "SecureHash.h"
#include "MessageLog.h"
#include "UObjectToken.h"

DEFINE_LOG_CATEGORY(LogLinker);

#define LOCTEXT_NAMESPACE "Linker"

TMap<UPackage*, ULinkerLoad*>		GObjLoaders;
TMap<UPackage*, ULinkerLoad*>		GObjPendingLoaders;
TSet<ULinkerLoad*>					GObjLoadersWithNewImports;

/** Points to the main PackageLinker currently being serialized */
ULinkerLoad* GSerializedPackageLinker = NULL;
/** The main Import Index currently being used for serialization by CreateImports()		*/
int32 GSerializedImportIndex = INDEX_NONE;
/** Points to the main Linker currently being used for serialization by CreateImports()	*/
ULinkerLoad* GSerializedImportLinker = NULL;
/** Points to the main UObject currently being serialized								*/
UObject* GSerializedObject = NULL;


/*-----------------------------------------------------------------------------
	Helper functions.
-----------------------------------------------------------------------------*/
namespace Linker
{
	FORCEINLINE bool IsCorePackage(const FName& PackageName)
	{
		return PackageName == NAME_Core || PackageName == GLongCorePackageName;
	}
}

/**
 * Type hash implementation. 
 *
 * @param	Ref		Reference to hash
 * @return	hash value
 */
uint32 GetTypeHash( const FDependencyRef& Ref  )
{
	return PointerHash(Ref.Linker) ^ Ref.ExportIndex;
}

/*----------------------------------------------------------------------------
	FCompressedChunk.
----------------------------------------------------------------------------*/

FCompressedChunk::FCompressedChunk()
:	UncompressedOffset(0)
,	UncompressedSize(0)
,	CompressedOffset(0)
,	CompressedSize(0)
{}

/** I/O function */
FArchive& operator<<(FArchive& Ar,FCompressedChunk& Chunk)
{
	Ar << Chunk.UncompressedOffset;
	Ar << Chunk.UncompressedSize;
	Ar << Chunk.CompressedOffset;
	Ar << Chunk.CompressedSize;
	return Ar;
}


/*----------------------------------------------------------------------------
	Items stored in Unreal files.
----------------------------------------------------------------------------*/

FGenerationInfo::FGenerationInfo(int32 InExportCount, int32 InNameCount)
: ExportCount(InExportCount), NameCount(InNameCount)
{}

/** I/O function
 * we use a function instead of operator<< so we can pass in the package file summary for version tests, since archive version hasn't been set yet
 */
void FGenerationInfo::Serialize(FArchive& Ar, const struct FPackageFileSummary& Summary)
{
	Ar << ExportCount << NameCount;
}

FName FLinkerTables::GetExportClassName( int32 i )
{
	if (ExportMap.IsValidIndex(i))
	{
		FObjectExport& Export = ExportMap[i];
		if( !Export.ClassIndex.IsNull() )
		{
			return ImpExp(Export.ClassIndex).ObjectName;
		}
	}
	return NAME_Class;
}

/*----------------------------------------------------------------------------
	ULinker.
----------------------------------------------------------------------------*/
ULinker::ULinker(const FObjectInitializer& ObjectInitializer, UPackage* InRoot, const TCHAR* InFilename )
:	UObject(ObjectInitializer)
,	LinkerRoot( InRoot )
,	Summary()
,	Filename( InFilename )
,	FilterClientButNotServer(false)
,	FilterServerButNotClient(false)
{
	check(!HasAnyFlags(RF_ClassDefaultObject));

	check(LinkerRoot);
	check(InFilename);

	if( !GIsClient && GIsServer)
	{
		FilterClientButNotServer = true;
	}
	if( GIsClient && !GIsServer)
	{
		FilterServerButNotClient = true;
	}
}

void ULinker::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if( Ar.IsCountingMemory() )
	{
		// Can't use CountBytes as ExportMap is array of structs of arrays.
		Ar << ImportMap;
		Ar << ExportMap;
		Ar << DependsMap;

		if (Ar.IsSaving() || Ar.UE4Ver() >= VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP)
		{
			Ar << StringAssetReferencesMap;
		}
	}

	// Prevent garbage collecting of linker's names and package.
	Ar << NameMap << LinkerRoot;
	{
		for( int32 i=0; i<ExportMap.Num(); i++ )
		{
			FObjectExport& E = ExportMap[i];
			Ar << E.ObjectName;
		}
	}
	{
		for( int32 i=0; i<ImportMap.Num(); i++ )
		{
			FObjectImport& I = ImportMap[i];
			Ar << (UObject*&)I.SourceLinker;
			Ar << I.ClassPackage << I.ClassName;
		}
	}
}

void ULinker::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	ULinker* This = CastChecked<ULinker>(InThis);
#if WITH_EDITOR
	if (GIsEditor)
	{
		Collector.AddReferencedObject( *(UObject**)&This->LinkerRoot, This );
	}
#endif
	Super::AddReferencedObjects( This, Collector );
}

// ULinker interface.
/**
 * Return the path name of the UObject represented by the specified import. 
 * (can be used with StaticFindObject)
 * 
 * @param	ImportIndex	index into the ImportMap for the resource to get the name for
 *
 * @return	the path name of the UObject represented by the resource at ImportIndex
 */
FString ULinker::GetImportPathName(int32 ImportIndex)
{
	ULinkerLoad* Loader = dynamic_cast<ULinkerLoad*>(this);

	FString Result;
	for (FPackageIndex LinkerIndex = FPackageIndex::FromImport(ImportIndex); !LinkerIndex.IsNull();)
	{
		FObjectResource Resource = ImpExp(LinkerIndex);
		bool bSubobjectDelimiter=false;

		if (Result.Len() > 0 && Loader != NULL && Loader->GetClassName(LinkerIndex) != NAME_Package
			&& (Resource.OuterIndex.IsNull() || Loader->GetClassName(Resource.OuterIndex) == NAME_Package) )
		{
			bSubobjectDelimiter = true;
		}

		// don't append a dot in the first iteration
		if ( Result.Len() > 0 )
		{
			if ( bSubobjectDelimiter )
			{
				Result = FString(SUBOBJECT_DELIMITER) + Result;
			}
			else
			{
				Result = FString(TEXT(".")) + Result;
			}
		}

		Result = Resource.ObjectName.ToString() + Result;
		LinkerIndex = Resource.OuterIndex;
	}
	return Result;
}

/**
 * Return the path name of the UObject represented by the specified export.
 * (can be used with StaticFindObject)
 * 
 * @param	ExportIndex				index into the ExportMap for the resource to get the name for
 * @param	FakeRoot				Optional name to replace use as the root package of this object instead of the linker
 * @param	bResolveForcedExports	if true, the package name part of the return value will be the export's original package,
 *									not the name of the package it's currently contained within.
 *
 * @return	the path name of the UObject represented by the resource at ExportIndex
 */
FString ULinker::GetExportPathName(int32 ExportIndex, const TCHAR* FakeRoot,bool bResolveForcedExports/*=false*/)
{
	FString Result;
	ULinkerLoad* Loader = dynamic_cast<ULinkerLoad*>(this);

	bool bForcedExport = false;
	for ( FPackageIndex LinkerIndex = FPackageIndex::FromExport(ExportIndex); !LinkerIndex.IsNull(); LinkerIndex = Exp(LinkerIndex).OuterIndex )
	{ 
		const FObjectExport Export = Exp(LinkerIndex);

		// don't append a dot in the first iteration
		if ( Result.Len() > 0 )
		{
			// if this export is not a UPackage but this export's Outer is a UPackage, we need to use subobject notation
			if (Loader != NULL
			&&	(	Export.OuterIndex.IsNull()
				||	Loader->GetExportClassName(Export.OuterIndex) == NAME_Package)
			&&	Loader->GetExportClassName(LinkerIndex) != NAME_Package)
			{
				Result = FString(SUBOBJECT_DELIMITER) + Result;
			}
			else
			{
				Result = FString(TEXT(".")) + Result;
			}
		}
		Result = Export.ObjectName.ToString() + Result;
		bForcedExport = bForcedExport || Export.bForcedExport;
	}

	if ( bForcedExport && FakeRoot == NULL && bResolveForcedExports )
	{
		// Result already contains the correct path name for this export
		return Result;
	}

	return (FakeRoot ? FakeRoot : LinkerRoot->GetPathName()) + TEXT(".") + Result;
}

FString ULinker::GetImportFullName(int32 ImportIndex)
{
	return ImportMap[ImportIndex].ClassName.ToString() + TEXT(" ") + GetImportPathName(ImportIndex);
}

FString ULinker::GetExportFullName(int32 ExportIndex, const TCHAR* FakeRoot,bool bResolveForcedExports/*=false*/)
{
	FPackageIndex ClassIndex = ExportMap[ExportIndex].ClassIndex;
	FName ClassName = ClassIndex.IsNull() ? FName(NAME_Class) : ImpExp(ClassIndex).ObjectName;

	return ClassName.ToString() + TEXT(" ") + GetExportPathName(ExportIndex, FakeRoot, bResolveForcedExports);
}

/**
 * Tell this linker to start SHA calculations
 */
void ULinker::StartScriptSHAGeneration()
{
	// create it if needed
	if (ScriptSHA == NULL)
	{
		ScriptSHA = new FSHA1;
	}

	// make sure it's reset
	ScriptSHA->Reset();
}

/**
 * If generating a script SHA key, update the key with this script code
 *
 * @param ScriptCode Code to SHAify
 */
void ULinker::UpdateScriptSHAKey(const TArray<uint8>& ScriptCode)
{
	// if we are doing SHA, update it
	if (ScriptSHA && ScriptCode.Num())
	{
		ScriptSHA->Update((uint8*)ScriptCode.GetData(), ScriptCode.Num());
	}
}

/**
 * After generating the SHA key for all of the 
 *
 * @param OutKey Storage for the key bytes (20 bytes)
 */
void ULinker::GetScriptSHAKey(uint8* OutKey)
{
	check(ScriptSHA);

	// finish up the calculation, and return it
	ScriptSHA->Final();
	ScriptSHA->GetHash(OutKey);
}

void ULinker::BeginDestroy()
{
	Super::BeginDestroy();

	// free any SHA memory
	delete ScriptSHA;
}



/*-----------------------------------------------------------------------------
	Global functions
-----------------------------------------------------------------------------*/

//
// Empty the loaders.
//
void ResetLoaders( UObject* InPkg )
{
	// Make sure we're not in the middle of loading something in the background.
	FlushAsyncLoading();

	// Top level package to reset loaders for.
	UObject*		TopLevelPackage = InPkg ? InPkg->GetOutermost() : NULL;

	// Find loader/ linker associated with toplevel package. We do this upfront as Detach resets LinkerRoot.
	if( TopLevelPackage )
	{
		// Linker to reset/ detach.
		ULinkerLoad* LinkerToReset = ULinkerLoad::FindExistingLinkerForPackage(CastChecked<UPackage>(TopLevelPackage));
		if ( LinkerToReset )
		{
			for (TMap<UPackage*, ULinkerLoad*>::TIterator It(GObjLoaders); It; ++It)
			{
				ULinkerLoad* Linker = It.Value();
				// Detach LinkerToReset from other linker's import table.
				if( Linker->LinkerRoot != TopLevelPackage )
				{
					for( int32 j=0; j<Linker->ImportMap.Num(); j++ )
					{
						if( Linker->ImportMap[j].SourceLinker == LinkerToReset )
						{
							Linker->ImportMap[j].SourceLinker	= NULL;
							Linker->ImportMap[j].SourceIndex	= INDEX_NONE;
						}
					}
				}
				else
				{
					check(Linker == LinkerToReset);
				}
			}
			// Detach linker, also removes from array and sets LinkerRoot to NULL.
			LinkerToReset->Detach(true);
		}
	}
	else
	{
		TArray<ULinkerLoad *> LinkersToDetach;
		GObjLoaders.GenerateValueArray(LinkersToDetach);

		for (int32 Index = 0; Index < LinkersToDetach.Num(); Index++)
		{
			ULinkerLoad* Linker = LinkersToDetach[Index];
			// Detach linker, also removes from array and sets LinkerRoot to NULL.
			Linker->Detach(true);
		}
	}

}



/**
 * Dissociates all linker import and forced export object references. This currently needs to 
 * happen as the referred objects might be destroyed at any time.
 */
void DissociateImportsAndForcedExports()
{
	if( GImportCount && GObjLoadersWithNewImports.Num())
	{
		for (TSet< ULinkerLoad*>::TIterator It(GObjLoadersWithNewImports); It; ++It)
		{
			ULinkerLoad* Linker = *It;
			for( int32 ImportIndex=0; ImportIndex<Linker->ImportMap.Num(); ImportIndex++ )
			{
				FObjectImport& Import = Linker->ImportMap[ImportIndex];
				if( Import.XObject && !Import.XObject->HasAnyFlags(RF_Native) )
				{
					Import.XObject = NULL;
				}
				Import.SourceLinker = NULL;
				// when the SourceLinker is reset, the SourceIndex must also be reset, or recreating
				// an import that points to a redirector will fail to find the redirector
				Import.SourceIndex = INDEX_NONE;
			}
		}
	}
	GImportCount = 0;
	GObjLoadersWithNewImports.Empty();

	if( GForcedExportCount )
	{
		for (TMap<UPackage*, ULinkerLoad*>::TIterator It(GObjLoaders); It; ++It)
		{
			ULinkerLoad* Linker = It.Value();
			//@todo optimization: only dissociate exports for loaders that had forced exports created
			//@todo optimization: since the last time this function was called.
			for( int32 ExportIndex=0; ExportIndex<Linker->ExportMap.Num(); ExportIndex++ )
			{
				FObjectExport& Export = Linker->ExportMap[ExportIndex];
				if( Export.Object && Export.bForcedExport )
				{
					Export.Object->SetLinker( NULL, INDEX_NONE );
					Export.Object = NULL;
				}
			}
		}
	}
	GForcedExportCount = 0;
}

static void LogGetPackageLinkerError(FArchiveUObject* LinkerArchive, const TCHAR* InFilename, const FText& InFullErrorMessage, const FText& InSummaryErrorMessage, UObject* InOuter, uint32 LoadFlags)
{
	static FName NAME_LoadErrors("LoadErrors");
	struct Local
	{
		/** Helper function to output more detailed error info if available */
		static void OutputErrorDetail(FArchiveUObject* LinkerArchive, const FName& LogName)
		{
			if ( GSerializedObject && GSerializedImportLinker )
			{
				FMessageLog LoadErrors(LogName);

				TSharedRef<FTokenizedMessage> Message = LoadErrors.Info();
				Message->AddToken(FTextToken::Create(LOCTEXT("FailedLoad_Message", "Failed to load")));
				Message->AddToken(FAssetNameToken::Create(GSerializedImportLinker->GetImportPathName(GSerializedImportIndex)));
				Message->AddToken(FTextToken::Create(LOCTEXT("FailedLoad_Referenced", "Referenced by")));
				Message->AddToken(FUObjectToken::Create(GSerializedObject));
				auto SerializedProperty = LinkerArchive ? LinkerArchive->GetSerializedProperty() : nullptr;
				if (SerializedProperty != nullptr)
				{
					FString PropertyPathName = SerializedProperty->GetPathName();
					Message->AddToken(FTextToken::Create(LOCTEXT("FailedLoad_Property", "Property")));
					Message->AddToken(FAssetNameToken::Create(PropertyPathName, FText::FromString( PropertyPathName) ) );
				}
			}
		}
	};

	FMessageLog LoadErrors(NAME_LoadErrors);

	// Display log error regardless LoadFlag settings
	SET_WARN_COLOR(COLOR_RED);
	if (LoadFlags & LOAD_NoWarn)
	{
		UE_LOG(LogLinker, Log, TEXT("%s"), *InFullErrorMessage.ToString());
	}
	else 
	{
		UE_LOG(LogLinker, Warning, TEXT("%s"), *InFullErrorMessage.ToString());
	}
	CLEAR_WARN_COLOR();
	if( GIsEditor && !IsRunningCommandlet() )
	{
		// if we don't want to be warned, skip the load warning
		if (!(LoadFlags & LOAD_NoWarn))
		{
			// we only want to output errors that content creators will be able to make sense of,
			// so any errors we cant get links out of we will just let be output to the output log (above)
			// rather than clog up the message log

			if(InFilename != NULL && InOuter != NULL)
			{
				// Output the summary error & the filename link. This might be something like "..\Content\Foo.upk Out of Memory"
				TSharedRef<FTokenizedMessage> Message = LoadErrors.Error();
				Message->AddToken(FAssetNameToken::Create(FPackageName::FilenameToLongPackageName(InFilename)));
				Message->AddToken(FTextToken::Create(FText::FromString(TEXT(":"))));
				Message->AddToken(FTextToken::Create(InSummaryErrorMessage));
				Message->AddToken(FAssetNameToken::Create(FPackageName::FilenameToLongPackageName(InOuter->GetPathName())));
			}

			Local::OutputErrorDetail(LinkerArchive, NAME_LoadErrors);
		}
	}
	else
	{
		if (!(LoadFlags & LOAD_NoWarn))
		{
			Local::OutputErrorDetail(LinkerArchive, NAME_LoadErrors);
		}

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("FileName"), FText::FromString(InFilename ? InFilename : InOuter ? *InOuter->GetName() : TEXT("NULL")));
		Arguments.Add(TEXT("ErrorMessage"), InFullErrorMessage);
		const FText Error = FText::Format(LOCTEXT("FailedLoad", "Failed to load '{FileName}': {ErrorMessage}"), Arguments);

		// @see ResavePackagesCommandlet
		if( FParse::Param(FCommandLine::Get(),TEXT("SavePackagesThatHaveFailedLoads")) == true )
		{
			LoadErrors.Warning(Error);
		}
		else
		{
			// Gracefully handle missing packages
			SafeLoadError( InOuter, LoadFlags, *InFullErrorMessage.ToString(), *Error.ToString() );
		}
	}
}

//
// Find or create the linker for a package.
//
ULinkerLoad* GetPackageLinker
(
	UPackage*		InOuter,
	const TCHAR*	InLongPackageName,
	uint32			LoadFlags,
	UPackageMap*	Sandbox,
	FGuid*			CompatibleGuid
)
{
	// See if there is already a linker for this package.
	ULinkerLoad* Result = ULinkerLoad::FindExistingLinkerForPackage(InOuter);

	// Try to load the linker.
	// See if the linker is already loaded.
	if( Result )
	{
		return Result;
	}

	FString NewFilename;
	if( !InLongPackageName )
	{
		// Resolve filename from package name.
		if( !InOuter )
		{
			// try to recover from this instead of throwing, it seems recoverable just by doing this
			FText ErrorText(LOCTEXT("PackageResolveFailed", "Can't resolve asset name"));
			LogGetPackageLinkerError(Result, InLongPackageName, ErrorText, ErrorText, InOuter, LoadFlags);
			return nullptr;
		}

		if( !FPackageName::DoesPackageExist(InOuter->GetName(), CompatibleGuid, &NewFilename) )
		{
			// Compiled in packages have no linker and this is ok
			if (!(LoadFlags & LOAD_AllowDll) && !(InOuter->PackageFlags & PKG_CompiledIn))
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("AssetName"), FText::FromString(InOuter->GetName()));
				Arguments.Add(TEXT("PackageName"), FText::FromString(GSerializedPackageLinker ? *(GSerializedPackageLinker->Filename) : TEXT("NULL")));
				LogGetPackageLinkerError(Result, GSerializedPackageLinker ? *GSerializedPackageLinker->Filename : nullptr,
											FText::Format(LOCTEXT("PackageNotFound", "Can't find file for asset '{AssetName}' while loading {PackageName}."), Arguments),
											LOCTEXT("PackageNotFoundShort", "Can't find file for asset."),
											InOuter,
											LoadFlags);
			}

			return nullptr;
		}
	}
	else
	{
		FString PackageName(InLongPackageName);
		if (!FPackageName::TryConvertFilenameToLongPackageName(InLongPackageName, PackageName))
		{
			// try to recover from this instead of throwing, it seems recoverable just by doing this
			FText ErrorText(LOCTEXT("PackageResolveFailed", "Can't resolve asset name"));
			LogGetPackageLinkerError(Result, InLongPackageName, ErrorText, ErrorText, InOuter, LoadFlags);
			return nullptr;
		}

		if (UPackage* ExistingPackage = FindObject<UPackage>(nullptr, *PackageName))
		{
			if (!ExistingPackage->GetOuter() && (ExistingPackage->PackageFlags & PKG_CompiledIn))
			{
				// this is a compiled in package and so it has no linker and this is ok
				return nullptr;
			}
		}

		// Verify that the file exists.
		if( !FPackageName::DoesPackageExist( PackageName, CompatibleGuid, &NewFilename ) )
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Filename"), FText::FromString(InLongPackageName));

			// try to recover from this instead of throwing, it seems recoverable just by doing this
			LogGetPackageLinkerError(Result, InLongPackageName, FText::Format(LOCTEXT("FileNotFound", "Can't find file '{Filename}'"), Arguments), LOCTEXT("FileNotFoundShort", "Can't find file"), InOuter, LoadFlags);
			return nullptr;
		}

		// Create the package with the provided long package name.
		UPackage* FilenamePkg = CreatePackage(nullptr, *PackageName);
		if (LoadFlags & LOAD_PackageForPIE)
		{
			FilenamePkg->PackageFlags |= PKG_PlayInEditor;
		}

		// If no package specified, use package from file.
		if (!InOuter)
		{
			if( !FilenamePkg )
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Filename"), FText::FromString(InLongPackageName));
				LogGetPackageLinkerError(Result, InLongPackageName, FText::Format(LOCTEXT("FilenameToPackage", "Can't convert filename '{Filename}' to asset name"), Arguments), LOCTEXT("FilenameToPackageShort", "Can't convert filename to asset name"), InOuter, LoadFlags);
				return nullptr;
			}
			InOuter = FilenamePkg;
			Result = ULinkerLoad::FindExistingLinkerForPackage(InOuter);
		}
		else if (InOuter != FilenamePkg) //!!should be tested and validated in new UnrealEd
		{
			// Loading a new file into an existing package, so reset the loader.
			//UE_LOG(LogLinker, Log,  TEXT("New File, Existing Package (%s, %s)"), *InOuter->GetFullName(), *FilenamePkg->GetFullName() );
			ResetLoaders( InOuter );
		}
	}

#if 0
	// Make sure the package is accessible in the sandbox.
	if( Sandbox && !Sandbox->SupportsPackage(InOuter) )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("AssetName"), FText::FromString(InOuter->GetName()));

		LogGetPackageLinkerError(Result, InLongPackageName, FText::Format(LOCTEXT("Sandbox", "Asset '{AssetName}' is not accessible in this sandbox"), Arguments), LOCTEXT("SandboxShort", "Asset is not accessible in this sandbox"), InOuter, LoadFlags);
		return nullptr;
	}
#endif

	// Create new linker.
	if( !Result )
	{
		check(IsLoading());

		// we will already have found the filename above
		check(NewFilename.Len() > 0);

		Result = ULinkerLoad::CreateLinker( InOuter, *NewFilename, LoadFlags );
	}

	// Verify compatibility.
	if( CompatibleGuid && Result->Summary.Guid!=*CompatibleGuid )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("AssetName"), FText::FromString(InOuter->GetName()));

		// This should never fire, because FindPackageFile should never return an incompatible file
		LogGetPackageLinkerError(Result, InLongPackageName, FText::Format(LOCTEXT("PackageVersion", "Asset '{AssetName}' version mismatch"), Arguments), LOCTEXT("PackageVersionShort", "Asset version mismatch"), InOuter, LoadFlags);
		return nullptr;
	}

	return Result;
}

/**
 * 
 * Ensure thumbnails are loaded and then reset the loader in preparation for a package save
 *
 * @param	InOuter			The outer for the package we are saving
 * @param	Filename		The filename we are saving too
 */
void ResetLoadersForSave(UObject* InOuter, const TCHAR *Filename)
{
	UPackage* Package = dynamic_cast<UPackage*>(InOuter);
	// If we have a loader for the package, unload it to prevent conflicts if we are resaving to the same filename
	ULinkerLoad* Loader = ULinkerLoad::FindExistingLinkerForPackage(Package);
	// This is the loader corresponding to the package we're saving.
	if( Loader )
	{
		// Before we save the package, make sure that we load up any thumbnails that aren't already
		// in memory so that they won't be wiped out during this save
		Loader->SerializeThumbnails();

		// Compare absolute filenames to see whether we're trying to save over an existing file.
		if( FPaths::ConvertRelativePathToFull(Filename) == FPaths::ConvertRelativePathToFull( Loader->Filename ) )
		{
			// Detach all exports from the linker and dissociate the linker.
			ResetLoaders( InOuter );
		}
	}
}

IMPLEMENT_CORE_INTRINSIC_CLASS(ULinker, UObject,
	{
		Class->ClassAddReferencedObjects = &ULinker::AddReferencedObjects;
		Class->EmitObjectReference(STRUCT_OFFSET(ULinker, LinkerRoot), TEXT("LinkerRoot"));
		const uint32 SkipIndexIndex = Class->EmitStructArrayBegin(STRUCT_OFFSET(ULinker, ImportMap), TEXT("ImportMap"), sizeof(FObjectImport));
		Class->EmitObjectReference(STRUCT_OFFSET(FObjectImport, SourceLinker), TEXT("SourceLinker"));
		Class->EmitStructArrayEnd( SkipIndexIndex );
	}
);

#undef LOCTEXT_NAMESPACE
