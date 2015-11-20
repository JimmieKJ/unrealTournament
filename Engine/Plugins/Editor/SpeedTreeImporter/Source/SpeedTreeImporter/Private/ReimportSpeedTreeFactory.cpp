// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SpeedTreeImporterPrivatePCH.h"
#include "ReimportSpeedTreeFactory.h"
#include "Engine/StaticMesh.h"



#define LOCTEXT_NAMESPACE "EditorFactories"
DEFINE_LOG_CATEGORY_STATIC(LogEditorFactories, Log, All);



UReimportSpeedTreeFactory::UReimportSpeedTreeFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	#if WITH_SPEEDTREE
		SupportedClass = UStaticMesh::StaticClass();
		Formats.Add(TEXT("srt;SpeedTree"));
	#endif

	bCreateNew = false;
	bText = false;
}

bool UReimportSpeedTreeFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
#if WITH_SPEEDTREE
	UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
	if (Mesh && Mesh->AssetImportData)
	{
		if (FPaths::GetExtension(Mesh->AssetImportData->GetFirstFilename()).ToLower() == "srt")
		{
			// SpeedTrees file extension matches with filepath			
			Mesh->AssetImportData->ExtractFilenames(OutFilenames);
			return true;
		}		
	}
#endif // #if WITH_SPEEDTREE
	return false;
}

void UReimportSpeedTreeFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
#if WITH_SPEEDTREE
	UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
	if (Mesh && Mesh->AssetImportData && ensure(NewReimportPaths.Num() == 1))
	{
		Mesh->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
#endif // #if WITH_SPEEDTREE
}

EReimportResult::Type UReimportSpeedTreeFactory::Reimport(UObject* Obj)
{
#if WITH_SPEEDTREE
	UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
	if (!Mesh)
	{
		return EReimportResult::Failed;
	}

	const FString Filename = Mesh->AssetImportData->GetFirstFilename();
	const FString FileExtension = FPaths::GetExtension(Filename);
	const bool bIsSRT = FCString::Stricmp(*FileExtension, TEXT("SRT")) == 0;

	if (!bIsSRT)
	{
		return EReimportResult::Failed;
	}

	if (!(Filename.Len()))
	{
		// Since this is a new system most static meshes don't have paths, so logging has been commented out
		//UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: static mesh resource does not have path stored."));
		return EReimportResult::Failed;
	}

	UE_LOG(LogEditorFactories, Log, TEXT("Performing atomic reimport of [%s]"), *Filename);

	if (UFactory::StaticImportObject(Mesh->GetClass(), Mesh->GetOuter(), *Mesh->GetName(), RF_Public | RF_Standalone, *Filename, NULL, this))
	{
		UE_LOG(LogEditorFactories, Log, TEXT("-- imported successfully"));

		Mesh->AssetImportData->Update(Filename);

		// Mark the package dirty after the successful import
		Mesh->MarkPackageDirty();

		return EReimportResult::Succeeded;
	}
	else
	{
		UE_LOG(LogEditorFactories, Warning, TEXT("-- import failed"));
	}
#endif // #if WITH_SPEEDTREE
	return EReimportResult::Failed;
}

int32 UReimportSpeedTreeFactory::GetPriority() const
{
	return ImportPriority;
}

#undef LOCTEXT_NAMESPACE
