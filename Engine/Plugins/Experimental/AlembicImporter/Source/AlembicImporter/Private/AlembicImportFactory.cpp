// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AlembicImporterPrivatePCH.h"

#include "UnrealEd.h"
#include "MainFrame.h"

#include "AlembicImportFactory.h"
#include "AlembicImportOptions.h"

#include "AlembicLibraryModule.h"
#include "AbcImporter.h"
#include "AbcImportLogger.h"
#include "AbcImportSettings.h"

#include "GeometryCache.h"

#define LOCTEXT_NAMESPACE "AlembicImportFactory"

DEFINE_LOG_CATEGORY_STATIC(LogAlembic, Log, All);

UAlembicImportFactory::UAlembicImportFactory(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditAfterNew = true;
	SupportedClass = nullptr;

	bEditorImport = true;
	bText = false;

	Formats.Add(TEXT("abc;Alembic"));
}

void UAlembicImportFactory::PostInitProperties()
{
	Super::PostInitProperties();

	ImportSettings = UAbcImportSettings::Get();
}

FText UAlembicImportFactory::GetDisplayName() const
{
	return LOCTEXT("AlembicImportFactoryDescription", "Alembic");
}

bool UAlembicImportFactory::DoesSupportClass(UClass * Class)
{
	return (Class == UStaticMesh::StaticClass() || Class == UGeometryCache::StaticClass() || Class == USkeletalMesh::StaticClass());
}

UClass* UAlembicImportFactory::ResolveSupportedClass()
{
	return UStaticMesh::StaticClass();
}

UObject* UAlembicImportFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

	TSharedPtr<SAlembicImportOptions> Options;

	FAbcImporter Importer;
	EAbcImportError ErrorCode = Importer.OpenAbcFileForImport(UFactory::CurrentFilename);
	
	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the import
		FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
	}

	ImportSettings->SamplingSettings.FrameEnd = Importer.GetNumFrames();	
	ShowImportOptionsWindow(Options, UFactory::CurrentFilename, Importer);

	// Set whether or not the user canceled
	bOutOperationCanceled = !Options->ShouldImport();

	// Set up message log page name to separate different assets
	const FString PageName = "Importing " + InName.ToString() + ".abc";

	TArray<UObject*> ResultAssets;
	if (!bOutOperationCanceled)
	{
		FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);
		
		if (Options->ShouldImport())
		{
			int32 NumThreads = 1;
			if (FPlatformProcess::SupportsMultithreading())
			{
				NumThreads = FPlatformMisc::NumberOfCores();
			}

			// Import file		
			ErrorCode = Importer.ImportTrackData(NumThreads, ImportSettings);

			if (ErrorCode != AbcImportError_NoError)
			{
				// Failed to read the file info, fail the import
				FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
				FAbcImportLogger::OutputMessages(PageName);
				return nullptr;
			}
			else
			{
				if (ImportSettings->ImportType == EAlembicImportType::StaticMesh)
				{
					const TArray<UObject*> ResultStaticMeshes = ImportStaticMesh(Importer, InParent, Flags);
					ResultAssets.Append(ResultStaticMeshes);
				}
				else if (ImportSettings->ImportType == EAlembicImportType::GeometryCache)
				{
					UObject* GeometryCache = ImportGeometryCache(Importer, InParent, Flags);
					if (GeometryCache)
					{
						ResultAssets.Add(GeometryCache);
					}
				}
				else if (ImportSettings->ImportType == EAlembicImportType::Skeletal)
				{
					UObject* SkeletalMesh = ImportSkeletalMesh(Importer, InParent, Flags);
					if (SkeletalMesh)
					{
						ResultAssets.Add(SkeletalMesh);
					}
				}
			}
		}

		for (UObject* Object : ResultAssets)
		{
			if (Object)
			{
				FEditorDelegates::OnAssetPostImport.Broadcast(this, Object);
				Object->MarkPackageDirty();
				Object->PostEditChange();
			}
		}

		FAbcImportLogger::OutputMessages(PageName);
	}
	
	// Determine out parent according to the generated assets outer
	UObject* OutParent = (ResultAssets.Num() > 0 && InParent != ResultAssets[0]->GetOutermost()) ? ResultAssets[0]->GetOutermost() : InParent;
	return (ResultAssets.Num() > 0) ? OutParent : nullptr;
}

TArray<UObject*> UAlembicImportFactory::ImportStaticMesh(FAbcImporter& Importer, UObject* InParent, EObjectFlags Flags)
{
	// Flush commands before importing
	FlushRenderingCommands();

	TArray<UObject*> Objects;

	const uint32 NumMeshes = Importer.GetNumMeshTracks();
	// Check if the alembic file contained any meshes
	if (NumMeshes > 0)
	{
		const TArray<UStaticMesh*>& StaticMeshes = Importer.ImportAsStaticMesh(InParent, Flags);

		for (UStaticMesh* StaticMesh : StaticMeshes)
		{
			if (StaticMesh)
			{
				// Setup asset import data
				if (!StaticMesh->AssetImportData)
				{
					StaticMesh->AssetImportData = NewObject<UAssetImportData>(StaticMesh);
				}
				StaticMesh->AssetImportData->Update(UFactory::CurrentFilename);
				Objects.Add(StaticMesh);
			}
		}	
	}

	return Objects;
}

UObject* UAlembicImportFactory::ImportGeometryCache(FAbcImporter& Importer, UObject* InParent, EObjectFlags Flags)
{
	// Flush commands before importing
	FlushRenderingCommands();

	const uint32 NumMeshes = Importer.GetNumMeshTracks();
	// Check if the alembic file contained any meshes
	if (NumMeshes > 0)
	{
		UGeometryCache* GeometryCache = Importer.ImportAsGeometryCache(InParent, Flags);

		if (!GeometryCache)
		{
			return nullptr;
		}

		// Setup asset import data
		if (!GeometryCache->AssetImportData)
		{
			GeometryCache->AssetImportData = NewObject<UAssetImportData>(GeometryCache);
		}
		GeometryCache->AssetImportData->Update(UFactory::CurrentFilename);

		return GeometryCache;
	}
	else
	{
		// Not able to import a static mesh
		FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
		return nullptr;
	}
}

UObject* UAlembicImportFactory::ImportSkeletalMesh(FAbcImporter& Importer, UObject* InParent, EObjectFlags Flags)
{
	// Flush commands before importing
	FlushRenderingCommands();

	const uint32 NumMeshes = Importer.GetNumMeshTracks();
	// Check if the alembic file contained any meshes
	if (NumMeshes > 0)
	{
		USkeletalMesh* SkeletalMesh = Importer.ImportAsSkeletalMesh(InParent, Flags);

		if (!SkeletalMesh)
		{
			return nullptr;
		}

		// Setup asset import data
		if (!SkeletalMesh->AssetImportData)
		{
			SkeletalMesh->AssetImportData = NewObject<UAssetImportData>(SkeletalMesh);
		}
		SkeletalMesh->AssetImportData->Update(UFactory::CurrentFilename);

		return SkeletalMesh;
	}
	else
	{
		// Not able to import a static mesh
		FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
		return nullptr;
	}
}

bool UAlembicImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UAssetImportData* ImportData = nullptr;
	if (Obj->GetClass() == UStaticMesh::StaticClass())
	{
		UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
		ImportData = Mesh->AssetImportData;
	}
	else if (Obj->GetClass() == UGeometryCache::StaticClass())
	{
		UGeometryCache* Cache = Cast<UGeometryCache>(Obj);
		ImportData = Cache->AssetImportData;
	}
	else if (Obj->GetClass() == USkeletalMesh::StaticClass())
	{
		USkeletalMesh* Cache = Cast<USkeletalMesh>(Obj);
		ImportData = Cache->AssetImportData;
	}
	
	if (ImportData)
	{
		if (FPaths::GetExtension(ImportData->GetFirstFilename()).ToLower() == "abc")
		{
			ImportData->ExtractFilenames(OutFilenames);
			return true;
		}
	}
	return false;
}

void UAlembicImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
	if (Mesh && Mesh->AssetImportData && ensure(NewReimportPaths.Num() == 1))
	{
		Mesh->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);			
	}
}

EReimportResult::Type UAlembicImportFactory::Reimport(UObject* Obj)
{
	if (Obj->GetClass() == UStaticMesh::StaticClass())
	{
		UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
		if (!Mesh)
		{
			return EReimportResult::Failed;
		}

		CurrentFilename = Mesh->AssetImportData->GetFirstFilename();

		return ReimportStaticMesh(Mesh);
	}
	else if (Obj->GetClass() == UGeometryCache::StaticClass())
	{
		UGeometryCache* GeometryCache = Cast<UGeometryCache>(Obj);
		if (!GeometryCache)
		{
			return EReimportResult::Failed;
		}

		CurrentFilename = GeometryCache->AssetImportData->GetFirstFilename();

		EReimportResult::Type Result = ReimportGeometryCache(GeometryCache);

		if (GeometryCache->GetOuter())
		{
			GeometryCache->GetOuter()->MarkPackageDirty();
		}
		else
		{
			GeometryCache->MarkPackageDirty();
		}

		return Result;
	}
	else if (Obj->GetClass() == USkeletalMesh::StaticClass())
	{
		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj);
		if (!SkeletalMesh)
		{
			return EReimportResult::Failed;
		}

		CurrentFilename = SkeletalMesh->AssetImportData->GetFirstFilename();

		EReimportResult::Type Result = ReimportSkeletalMesh(SkeletalMesh);

		if (SkeletalMesh->GetOuter())
		{
			SkeletalMesh->GetOuter()->MarkPackageDirty();
		}
		else
		{
			SkeletalMesh->MarkPackageDirty();
		}

		return Result;
	}

	return EReimportResult::Failed;
}

void UAlembicImportFactory::ShowImportOptionsWindow(TSharedPtr<SAlembicImportOptions>& Options, FString FilePath, const FAbcImporter& Importer)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Alembic Cache Import Options"))
		.SizingRule(ESizingRule::Autosized);
		
	Window->SetContent
		(
			SAssignNew(Options, SAlembicImportOptions).WidgetWindow(Window)
			.ImportSettings(ImportSettings)
			.WidgetWindow(Window)
			.PolyMeshes(Importer.GetPolyMeshes())
			.FullPath(FText::FromString(FilePath))
		);

	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
}

EReimportResult::Type UAlembicImportFactory::ReimportGeometryCache(UGeometryCache* Cache)
{
	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*CurrentFilename) == INDEX_NONE)
	{
		return EReimportResult::Failed;
	}

	FAbcImporter Importer;
	EAbcImportError ErrorCode = Importer.OpenAbcFileForImport(CurrentFilename);

	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the re importing process 
		return EReimportResult::Failed;
	}

	TSharedPtr<SAlembicImportOptions> Options;
	ImportSettings->ImportType = EAlembicImportType::GeometryCache;
	ShowImportOptionsWindow(Options, CurrentFilename, Importer);
	
	if (!Options->ShouldImport())
	{
		return EReimportResult::Cancelled;
	}

	int32 NumThreads = 1;
	if (FPlatformProcess::SupportsMultithreading())
	{
		NumThreads = FPlatformMisc::NumberOfCores();
	}
	
	// Import file	
	ErrorCode = Importer.ImportTrackData(NumThreads, ImportSettings);

	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the re importing process 
		return EReimportResult::Failed;
	}
	
	UGeometryCache* GeometryCache = Importer.ReimportAsGeometryCache(Cache);

	if (!GeometryCache)
	{
		return EReimportResult::Failed;
	}
	else
	{
		// Update file path/timestamp (Path could change if user has to browse for it manually)
		if (!GeometryCache->AssetImportData)
		{
			GeometryCache->AssetImportData = NewObject<UAssetImportData>(GeometryCache);
		}

		GeometryCache->AssetImportData->Update(CurrentFilename);
	}			

	return EReimportResult::Succeeded;
}

EReimportResult::Type UAlembicImportFactory::ReimportSkeletalMesh(USkeletalMesh* SkeletalMesh)
{
	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*CurrentFilename) == INDEX_NONE)
	{
		return EReimportResult::Failed;
	}

	FAbcImporter Importer;
	EAbcImportError ErrorCode = Importer.OpenAbcFileForImport(CurrentFilename);

	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the re importing process 
		return EReimportResult::Failed;
	}

	TSharedPtr<SAlembicImportOptions> Options;
	ImportSettings->ImportType = EAlembicImportType::Skeletal;
	ShowImportOptionsWindow(Options, CurrentFilename, Importer);

	if (!Options->ShouldImport())
	{
		return EReimportResult::Cancelled;
	}

	int32 NumThreads = 1;
	if (FPlatformProcess::SupportsMultithreading())
	{
		NumThreads = FPlatformMisc::NumberOfCores();
	}

	// Import file	
	ErrorCode = Importer.ImportTrackData(NumThreads, ImportSettings);

	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the re importing process 
		return EReimportResult::Failed;
	}

	USkeletalMesh* NewSkeletalMesh = Importer.ReimportAsSkeletalMesh(SkeletalMesh);

	if (!NewSkeletalMesh)
	{
		return EReimportResult::Failed;
	}
	else
	{
		// Update file path/timestamp (Path could change if user has to browse for it manually)
		if (!NewSkeletalMesh->AssetImportData)
		{
			NewSkeletalMesh->AssetImportData = NewObject<UAssetImportData>(NewSkeletalMesh);
		}

		NewSkeletalMesh->AssetImportData->Update(CurrentFilename);
	}

	return EReimportResult::Succeeded;
}

EReimportResult::Type UAlembicImportFactory::ReimportStaticMesh(UStaticMesh* Mesh)
{	
	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*CurrentFilename) == INDEX_NONE)
	{
		return EReimportResult::Failed;
	}		

	FAbcImporter Importer;
	EAbcImportError ErrorCode = Importer.OpenAbcFileForImport(CurrentFilename);

	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the re importing process 
		return EReimportResult::Failed;
	}

	TSharedPtr<SAlembicImportOptions> Options;
	ImportSettings->ImportType = EAlembicImportType::StaticMesh;
	ShowImportOptionsWindow(Options, CurrentFilename, Importer);

	if (!Options->ShouldImport())
	{
		return EReimportResult::Cancelled;
	}

	int32 NumThreads = 1;
	if (FPlatformProcess::SupportsMultithreading())
	{
		NumThreads = FPlatformMisc::NumberOfCores();
	}

	ErrorCode = Importer.ImportTrackData(NumThreads, ImportSettings);

	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the re importing process 
		return EReimportResult::Failed;
	}
	else
	{
		const TArray<UStaticMesh*>& StaticMeshes = Importer.ReimportAsStaticMesh(Mesh);
		for (UStaticMesh* StaticMesh : StaticMeshes)
		{
			if (StaticMesh)
			{
				// Setup asset import data
				if (!StaticMesh->AssetImportData)
				{
					StaticMesh->AssetImportData = NewObject<UAssetImportData>(StaticMesh);
				}
				StaticMesh->AssetImportData->Update(UFactory::CurrentFilename);
			}
		}

		if (!StaticMeshes.Num())
		{
			return EReimportResult::Failed;
		}		
	}

	return EReimportResult::Succeeded;
}

int32 UAlembicImportFactory::GetPriority() const
{
	return ImportPriority;
}

#undef LOCTEXT_NAMESPACE
