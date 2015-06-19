// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinkerManager.h: Unreal object linker manager
=============================================================================*/
#include "CoreUObjectPrivate.h"
#include "LinkerManager.h"
#include "UObject/UObjectThreadContext.h"

FLinkerManager& FLinkerManager::Get()
{
	static TAutoPtr<FLinkerManager> Singleton(new FLinkerManager());
	return *Singleton.GetOwnedPointer();
}

bool FLinkerManager::Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("LinkerLoadList")))
	{
		UE_LOG(LogLinker, Display, TEXT("ObjectLoaders: %d"), ObjectLoaders.Num());
		UE_LOG(LogLinker, Display, TEXT("LoadersWithNewImports: %d"), LoadersWithNewImports.Num());

#if UE_BUILD_DEBUG
		UE_LOG(LogLinker, Display, TEXT("LiveLinkers: %d"), LiveLinkers.Num());
		for (auto Linker : LiveLinkers)
		{
			UE_LOG(LogLinker, Display, TEXT("%s"), *Linker->Filename);
		}
#endif
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("LINKERS")))
	{
		Ar.Logf(TEXT("Linkers:"));
		for (auto Linker : ObjectLoaders)
		{
			int32 NameSize = 0;
			for (int32 j = 0; j < Linker->NameMap.Num(); j++)
			{
				if (Linker->NameMap[j] != NAME_None)
				{
					NameSize += FNameEntry::GetSize(*Linker->NameMap[j].ToString());
				}
			}
			Ar.Logf
				(
				TEXT("%s (%s): Names=%i (%iK/%iK) Imports=%i (%iK) Exports=%i (%iK) Gen=%i Bulk=%i"),
				*Linker->Filename,
				*Linker->LinkerRoot->GetFullName(),
				Linker->NameMap.Num(),
				Linker->NameMap.Num() * sizeof(FName) / 1024,
				NameSize / 1024,
				Linker->ImportMap.Num(),
				Linker->ImportMap.Num() * sizeof(FObjectImport) / 1024,
				Linker->ExportMap.Num(),
				Linker->ExportMap.Num() * sizeof(FObjectExport) / 1024,
				Linker->Summary.Generations.Num(),
#if WITH_EDITOR
				Linker->BulkDataLoaders.Num()
#else
				0
#endif // WITH_EDITOR
				);
		}

		return true;
	}
	return false;
}

void FLinkerManager::ResetLoaders(UObject* InPkg)
{
	// Top level package to reset loaders for.
	UObject*		TopLevelPackage = InPkg ? InPkg->GetOutermost() : NULL;

	// Find loader/ linker associated with toplevel package. We do this upfront as Detach resets LinkerRoot.
	if (TopLevelPackage)
	{
		// Linker to reset/ detach.
		auto LinkerToReset = FLinkerLoad::FindExistingLinkerForPackage(CastChecked<UPackage>(TopLevelPackage));
		if (LinkerToReset)
		{
			{
#if THREADSAFE_UOBJECTS
				FScopeLock ObjectLoadersLock(&ObjectLoadersCritical);
#endif
				for (auto Linker : ObjectLoaders)
				{
					// Detach LinkerToReset from other linker's import table.
					if (Linker->LinkerRoot != TopLevelPackage)
					{
						for (auto& Import : Linker->ImportMap)
						{
							if (Import.SourceLinker == LinkerToReset)
							{
								Import.SourceLinker = NULL;
								Import.SourceIndex = INDEX_NONE;
							}
						}
					}
					else
					{
						check(Linker == LinkerToReset);
					}
				}
			}
			// Detach linker, also removes from array and sets LinkerRoot to NULL.
			LinkerToReset->LoadAndDetachAllBulkData();
			delete LinkerToReset;
			LinkerToReset = nullptr;
		}
	}
	else
	{
		// We just want a copy here
		TSet<FLinkerLoad*> LinkersToDetach;
		GetLoaders(LinkersToDetach);
		for (auto Linker : LinkersToDetach)
		{
			// Detach linker, also removes from array and sets LinkerRoot to NULL.
			Linker->LoadAndDetachAllBulkData();
			delete Linker;
		}
	}
}

void FLinkerManager::DissociateImportsAndForcedExports()
{
	int32& ImportCount = FUObjectThreadContext::Get().ImportCount;
	if (ImportCount != 0)
	{
		TSet<FLinkerLoad*> LocalLoadersWithNewImports;
		GetLoadersWithNewImportsAndEmpty(LocalLoadersWithNewImports);
		if (LocalLoadersWithNewImports.Num())
		{
			for (auto Linker : LocalLoadersWithNewImports)
			{
				for (int32 ImportIndex = 0; ImportIndex < Linker->ImportMap.Num(); ImportIndex++)
				{
					FObjectImport& Import = Linker->ImportMap[ImportIndex];
					if (Import.XObject && !Import.XObject->HasAnyFlags(RF_Native))
					{
						Import.XObject = nullptr;
					}
					Import.SourceLinker = nullptr;
					// when the SourceLinker is reset, the SourceIndex must also be reset, or recreating
					// an import that points to a redirector will fail to find the redirector
					Import.SourceIndex = INDEX_NONE;
				}
			}
		}
		ImportCount = 0;
	}

	int32& ForcedExportCount = FUObjectThreadContext::Get().ForcedExportCount;
	if (ForcedExportCount)
	{		
		TSet<FLinkerLoad*> LocalLoaders;
		GetLoaders(LocalLoaders);
		for (auto Linker : LocalLoaders)
		{
			//@todo optimization: only dissociate exports for loaders that had forced exports created
			//@todo optimization: since the last time this function was called.
			for (auto& Export : Linker->ExportMap)
			{
				if (Export.Object && Export.bForcedExport)
				{
					Export.Object->SetLinker(nullptr, INDEX_NONE);
					Export.Object = nullptr;
				}
			}
		}	
		ForcedExportCount = 0;
	}	
}