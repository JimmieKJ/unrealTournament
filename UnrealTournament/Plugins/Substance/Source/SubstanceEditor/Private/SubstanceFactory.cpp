//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceEditorPrivatePCH.h"

#include "SubstanceEditorHelpers.h"
#include "SubstanceEditorClasses.h"
#include "SubstanceOptionWindow.h"

#include "SubstanceCorePreset.h"
#include "SubstanceCoreTypedefs.h"
#include "SubstanceCoreClasses.h"
#include "SubstanceCoreHelpers.h"
#include "SubstanceFPackage.h"
#include "SubstanceFGraph.h"
#include "SubstanceInput.h"

#include "IPluginManager.h"
#include "AssetToolsModule.h"
#include "ObjectTools.h"
#include "ContentBrowserModule.h"
#include "IMainFrameModule.h"

bool USubstanceFactory::bSuppressImportOverwriteDialog = false;


struct InstanceBackup
{
	preset_t Preset;
	USubstanceGraphInstance* InstanceParent;
	TMap<uint32, FString> UidToName;
};

namespace local
{
	bool bIsPerformingReimport = false;
}

typedef Substance::List<output_inst_t> Outputs_t;
typedef Substance::List<output_inst_t>::TIterator itOutputs_t;

void Substance::ApplyImportUIToImportOptions(USubstanceImportOptionsUi* ImportUI, FSubstanceImportOptions& InOutImportOptions)
{
	static FName AssetToolsModuleName = FName("AssetTools");
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(AssetToolsModuleName);
	FString PkgName;
	FString AssetName;

	InOutImportOptions.bCreateInstance = ImportUI->bCreateInstance;
	InOutImportOptions.bCreateMaterial = ImportUI->bCreateMaterial;

	AssetToolsModule.Get().CreateUniqueAssetName(ImportUI->InstanceDestinationPath + TEXT("/"), ImportUI->InstanceName, PkgName, AssetName);

	InOutImportOptions.InstanceDestinationPath = PkgName;
	InOutImportOptions.InstanceName = AssetName;

	AssetToolsModule.Get().CreateUniqueAssetName(ImportUI->MaterialDestinationPath + TEXT("/"), ImportUI->MaterialName, PkgName, AssetName);

	InOutImportOptions.MaterialName = AssetName;
	InOutImportOptions.MaterialDestinationPath = PkgName;
}

void Substance::GetImportOptions( 
	FString Name,
	FString ParentName,
	FSubstanceImportOptions& InOutImportOptions,
	bool& OutOperationCanceled)
{
	USubstanceImportOptionsUi* ImportUI = NewObject<USubstanceImportOptionsUi>();

	ImportUI->bForceCreateInstance = InOutImportOptions.bForceCreateInstance;
	ImportUI->bCreateInstance = true; //! @todo: load default from settings
	ImportUI->bCreateMaterial = true;

	Name = ObjectTools::SanitizeObjectName(Name);

	ImportUI->bOverrideFullName = false;
	FString BasePath;
	ParentName.Split(TEXT("/"), &(BasePath), NULL, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	
	ImportUI->InstanceDestinationPath = BasePath + TEXT("/") + Name;

	FString AssetNameStr;
	FString PackageNameStr;

	static FName AssetToolsModuleName = FName("AssetTools");
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(AssetToolsModuleName);
	AssetToolsModule.Get().CreateUniqueAssetName(BasePath + TEXT("/") + Name + TEXT("_INST"), TEXT(""), PackageNameStr, AssetNameStr);

	ImportUI->InstanceName = AssetNameStr;
	ImportUI->InstanceDestinationPath = BasePath;

	AssetToolsModule.Get().CreateUniqueAssetName(BasePath + TEXT("/") + Name + TEXT("_MAT"), TEXT(""), PackageNameStr, AssetNameStr);

	ImportUI->MaterialName = AssetNameStr;
	ImportUI->MaterialDestinationPath = BasePath;

	TSharedPtr<SWindow> ParentWindow;
	// Check if the main frame is loaded.  When using the old main frame it may not be.
	if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
		ParentWindow = MainFrame.GetParentWindow();
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(NSLOCTEXT("UnrealEd", "SubstanceImportOpionsTitle", "Substance Import Options"))
		.SizingRule( ESizingRule::Autosized );

	TSharedPtr<SSubstanceOptionWindow> SubstanceOptionWindow;
	Window->SetContent
		(
		SAssignNew(SubstanceOptionWindow, SSubstanceOptionWindow)
		.ImportUI(ImportUI)
		.WidgetWindow(Window)
		);

	// @todo: we can make this slow as showing progress bar later
	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	if (SubstanceOptionWindow->ShouldImport())
	{
		// open dialog
		// see if it's canceled
		ApplyImportUIToImportOptions(ImportUI, InOutImportOptions);
		OutOperationCanceled = false;
	}
	else
	{
		OutOperationCanceled = true;
	}
}



USubstanceFactory::USubstanceFactory( const class FObjectInitializer& PCIP )
	: Super(PCIP)
{
	bEditAfterNew = true;
	bEditorImport = true;

	SupportedClass = USubstanceInstanceFactory::StaticClass();

	Formats.Empty(1);
	Formats.Add(TEXT("sbsar;Substance Texture"));
}


UObject* USubstanceFactory::FactoryCreateBinary( 
	UClass* Class, 
	UObject* InParent, 
	FName Name, 
	EObjectFlags Flags, 
	UObject* Context, 
	const TCHAR* Type, 
	const uint8*& Buffer, 
	const uint8* BufferEnd, 
	FFeedbackContext* Warn)
{
	USubstanceInstanceFactory* Factory = NULL;

/*	// check if an already existing instance factory with the same name exists,
	// its references would be replaced by the new one, which would break its 
	// instances. Offer the user to rename the new one
	if ( !local::bIsPerformingReimport && NULL != (Factory = FindObject<USubstanceInstanceFactory>(InParent, *Name.ToString())))
	{
		if (appMsgf(
			AMT_OKCancel,
			*FString::Printf( TEXT("An object with that name (%s) already exists. Do you want to rename the new ?" ), *Name.ToString())))
		{
			INT Count = 0;
			FString NewName = Name.ToString() + FString::Printf(TEXT("_%i"),Count);

			// increment the instance number as long as there is already a package with that name
			while (FindObject<USubstanceInstanceFactory>(InParent, *NewName))
			{
				NewName = Name.ToString() + FString::Printf(TEXT("_%i"),Count++);
			}
			Name = FName(*NewName);
		}
		else
		{
			return NULL;
		}
	}*/

	Factory = CastChecked<USubstanceInstanceFactory>(
		CreateOrOverwriteAsset(
			USubstanceInstanceFactory::StaticClass(),
			InParent,
			Name,
			RF_Standalone|RF_Public));

	const uint32 BufferLength = BufferEnd - Buffer;

	Factory->SubstancePackage = new Substance::FPackage();
	Factory->SubstancePackage->Parent = Factory;
	Factory->SubstancePackage->Guid = FGuid::NewGuid();

	// and load the data in its associated Package
	Factory->SubstancePackage->SetData(
		Buffer,
		BufferLength,
		GetCurrentFilename());

	// if the operation failed
	if (false == Factory->SubstancePackage->IsValid())
	{
		// mark the package for garbage collect
		Factory->ClearFlags(RF_Standalone);
		return NULL;
	}

	Substance::FSubstanceImportOptions ImportOptions;
	TArray<FString> Names;

	bool bAllCancel = true;

	for (auto GraphIt = Factory->SubstancePackage->Graphs.itfront(); GraphIt; ++GraphIt)
	{
		bool bOperationCanceled = false;

		if (false == local::bIsPerformingReimport)
		{
			Substance::GetImportOptions((*GraphIt)->Label, InParent->GetName(), ImportOptions, bOperationCanceled);
		}

		if (bOperationCanceled)
		{
			bAllCancel = bOperationCanceled && bAllCancel;
			continue;
		}
		else
		{
			bAllCancel = false;
		}

		if (ImportOptions.bCreateInstance && !local::bIsPerformingReimport)
		{
			UObject* InstanceParent = CreatePackage(NULL, *ImportOptions.InstanceDestinationPath);

			graph_inst_t* NewInstance = 
				Substance::Helpers::InstantiateGraph(
					*GraphIt,
					InstanceParent,
					ImportOptions.InstanceName,
					!local::bIsPerformingReimport // bCreateOutputs
					);

			Substance::Helpers::RenderSync(NewInstance);	
			
			{
				Substance::List<graph_inst_t*> Instances;
				Instances.AddUnique(NewInstance);
				Substance::Helpers::UpdateTextures(Instances);
			}

			if (ImportOptions.bCreateMaterial)
			{
				UObject* MaterialParent = CreatePackage(NULL, *ImportOptions.MaterialDestinationPath);

				SubstanceEditor::Helpers::CreateMaterial(
					NewInstance,
					ImportOptions.MaterialName,
					MaterialParent);
			}
		}
	}

	if (bAllCancel)
	{
		Factory->ClearFlags(RF_Standalone);
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		return NULL;
	}

	if (local::bIsPerformingReimport)
	{
		TArray<UObject*> AssetList;
		AssetList.AddUnique(Factory);

		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(AssetList, true);
	}

	Factory->MarkPackageDirty();

	return Factory;
}


UReimportSubstanceFactory::UReimportSubstanceFactory(class FObjectInitializer const & PCIP):Super(PCIP)
{

}


bool UReimportSubstanceFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	const USubstanceGraphInstance* GraphInstance = Cast<USubstanceGraphInstance>(Obj);
	const USubstanceInstanceFactory* InstanceFactory = Cast<USubstanceInstanceFactory>(Obj);

	if (NULL == GraphInstance && NULL == InstanceFactory)
	{
		return false;
	}

	FString Filename;
	
	if (GraphInstance)
	{
		if (NULL == GraphInstance->Parent)
		{
			UE_LOG(LogSubstanceEditor, Warning, TEXT("Cannot reimport: The Substance Graph Instance does not have any parent package."));
			return false;
		}

		Filename =  GraphInstance->Parent->SubstancePackage->SourceFilePath;
		InstanceFactory = GraphInstance->Parent;
	}
	else if (InstanceFactory)
	{
		Filename =  InstanceFactory->SubstancePackage->SourceFilePath;
	}
	
	OutFilenames.Add(Filename);

	return true;
}


void UReimportSubstanceFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	USubstanceInstanceFactory* Factory = Cast<USubstanceInstanceFactory>(Obj);
	if (Factory && ensure(NewReimportPaths.Num() == 1))
	{
		Factory->SubstancePackage->SourceFilePath = NewReimportPaths[0];
	}
}


//! @brief find an output matching another in a given list of outputs
//! @param WantedOutput, the output to match
//! @param CandidateOutputs, the container to search into
//! @param ExactMatch, does the match have to be exact ? 
//!        True will match by uid
//!        False will match by role
//! @return the index of a match in the outputs container
int32 FindMatchingOutput(itOutputs_t& WantedOutput, Outputs_t& CandidateOutputs, bool ExactMatch = true)
{
	itOutputs_t ItOther(CandidateOutputs.itfront());

	for (;ItOther;++ItOther)
	{
		if ((*WantedOutput).Uid == (*ItOther).Uid)
		{
			return ItOther.GetIndex();
		}
	}

	return -1;
}


void TransferOutput(itOutputs_t& ItNew, output_inst_t& PrevOutput)
{
	bool FormatChanged = false;
	if (PrevOutput.Format != (*ItNew).Format)
	{
		FormatChanged = true;
	}

	uint32 NewUid = (*ItNew).Uid;
	int NewFormat = (*ItNew).Format;

	*ItNew = PrevOutput;
	(*ItNew).Uid = NewUid;
	(*ItNew).Format = NewFormat;
	(*ItNew).bIsDirty = true;

	// the texture has to be rebuild if the format changed
	if (FormatChanged)
	{
		USubstanceTexture2D* Texture = *(*ItNew).Texture.get();
		Substance::Helpers::CreateSubstanceTexture2D(&(*ItNew), false, Texture->GetName(), Texture->GetOuter());
	}

	// make sure the texture gets flagged as modified after reimport
	USubstanceTexture2D* Texture = *(*ItNew).Texture.get();
	if (Texture)
	{
		Texture->MarkPackageDirty();
	}
}


void GiveOutputsBack(graph_inst_t* NewInstance, 
	Substance::List<output_inst_t>& PrevOutputs)
{
	// for each output of the new instance
	// look for a matching previous one
	Substance::List<output_inst_t> NoMatch;
	itOutputs_t ItNew(NewInstance->Outputs.itfront());
	
	for (;ItNew;++ItNew)
	{
		int32 IdxMatch = FindMatchingOutput(ItNew, PrevOutputs);

		if (-1 == IdxMatch)
		{
			NoMatch.push(*ItNew);
		}
		else
		{
			TransferOutput(ItNew, PrevOutputs[IdxMatch]);
			PrevOutputs.RemoveAt(IdxMatch);
		}
	}
}


//! @brief Rebuild an instance based on a desc and
//! @note used when the desc has been changed for exampled
graph_inst_t* RebuildInstance(
	graph_desc_t* Desc,
	InstanceBackup& Backup)
{
	check(Desc);
	check(Backup.InstanceParent);

	preset_t& Preset = Backup.Preset;
	USubstanceGraphInstance* Parent = Backup.InstanceParent;

	// save the outputs of the previous instance
	Substance::List<output_inst_t> PrevOutputs = Parent->Instance->Outputs;

	Substance::List< TSharedPtr<input_inst_t> >::TIterator
		ItInput(Parent->Instance->Inputs.itfront());

	// remove previous address from the list of consumers in SubstanceImageInput
	for (;ItInput;++ItInput)
	{
		if (false == (*ItInput)->IsNumerical())
		{
			Substance::FImageInputInstance* ImgInput = (Substance::FImageInputInstance*)ItInput->Get();
			USubstanceImageInput* SrcImageInput = Cast<USubstanceImageInput>(ImgInput->ImageSource);

			if (SrcImageInput && SrcImageInput->Consumers.Contains(Parent))
			{
				SrcImageInput->Consumers.Remove(Parent);
			}
		}
	}

	// empty the previous instance container
	delete Parent->Instance;
	Parent->SetFlags(RF_Standalone); // reset the standalone flag, deleted in the destructor
	Parent->Instance = 0;
	Parent->Parent = 0;

	// recreate an instance
	graph_inst_t* NewInstance = Desc->Instantiate(Parent, false /*bCreateOutputs*/);
	NewInstance->Desc = Desc;

	// apply the preset
	Preset.Apply(NewInstance);
	
	GiveOutputsBack(NewInstance, PrevOutputs);
	Parent->Instance = NewInstance;

	Parent->MarkPackageDirty();

	//delete the old outputs no longer belonging to the instance
	for (auto itOut = PrevOutputs.itfront(); itOut; ++itOut)
	{
		if ((*itOut).bIsEnabled)
		{
			USubstanceTexture2D* Texture = *((*itOut).Texture);
			UE_LOG(LogSubstanceEditor, Log, TEXT("An output no longer belongs to the re-imported substance, it will be deleted: %s"), *Texture->GetName());
			Substance::Helpers::RegisterForDeletion(Texture);
		}
	}

	return NewInstance;
}


// look for a desc matching the preset
graph_desc_t* FindMatch(Substance::List<graph_desc_t*>& Desc,
	InstanceBackup* Backup)
{
	Substance::List<graph_desc_t*>::TIterator ItDesc(Desc.itfront());

	for (;ItDesc;++ItDesc)
	{
		if (Backup->Preset.mPackageUrl == (*ItDesc)->PackageUrl)
		{
			return *ItDesc;
		}
	}

	ItDesc.Reset();
	for (;ItDesc;++ItDesc)
	{
		FString Left;
		Backup->Preset.mLabel.Split(TEXT("_INST"), &Left, NULL, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		
		if (Left == (*ItDesc)->Label)
		{
			return *ItDesc;
		}
	}

	// try matching by output UIDs, in case the graph was moved or renamed
	ItDesc.Reset();
	for (; ItDesc; ++ItDesc)
	{
		int32 OutputMatching = 0;

		auto BackupOutputIt = Backup->UidToName.CreateConstIterator();

		for (; BackupOutputIt; ++BackupOutputIt)
		{
			for (auto DescOutputIt = (*ItDesc)->OutputDescs.itfront(); DescOutputIt; ++DescOutputIt)
			{
				if (DescOutputIt->Uid == BackupOutputIt.Key())
				{
					++OutputMatching;
				}
			}
		}
		
		if (Backup->UidToName.Num() == OutputMatching)
		{
			return *ItDesc;
		}
	}

	return NULL;
}


EReimportResult::Type UReimportSubstanceFactory::Reimport(UObject* Obj)
{
	USubstanceGraphInstance* GraphInstance = Cast<USubstanceGraphInstance>( Obj );
	USubstanceInstanceFactory* OriginalSubstance = Cast<USubstanceInstanceFactory>( Obj );

	if (GraphInstance)
	{
		if (NULL == GraphInstance->Parent)
		{
			UE_LOG(LogSubstanceEditor, Error, TEXT("Unable to reimport: Substance Graph Instance missing its parent Instance Factory."));
			return EReimportResult::Failed;
		}
		OriginalSubstance = GraphInstance->Parent;
	}

	const int32 TotalInstanceCount = 
		OriginalSubstance->SubstancePackage->GetInstanceCount();

	const int32 LoadedInstanceCount = 
		OriginalSubstance->SubstancePackage->LoadedInstancesCount;

	// check that all instances are loaded
	if (LoadedInstanceCount != TotalInstanceCount)
	{
		UE_LOG(LogSubstanceEditor, Error, TEXT("Unable to reimport: some Substance Graph Instances are missing."));
		return EReimportResult::Failed;
	}

	// backup the instances values before recreating the desc
	Substance::List<InstanceBackup> OriginalInstances;
	auto ItGraph = OriginalSubstance->SubstancePackage->Graphs.itfront();

	for (; ItGraph ; ++ItGraph)
	{
		for (auto ItInst = (*ItGraph)->LoadedInstances.itfront(); ItInst; ++ItInst)
		{
			InstanceBackup& Backup = 
				OriginalInstances[OriginalInstances.AddZeroed(1)];

			Backup.InstanceParent = (*ItInst)->ParentInstance;
			Backup.Preset.ReadFrom(*ItInst);

			for (auto it = (*ItInst)->Outputs.itfront(); it; ++it)
			{
				if (*(*it).Texture.get())
				{
					Backup.UidToName.Add((*it).Uid, (*(*it).Texture.get())->GetName());
				}
			}
		}
	}

	local::bIsPerformingReimport = true;

	USubstanceInstanceFactory* NewSubstance =
		Cast<USubstanceInstanceFactory>(UFactory::StaticImportObject(
			OriginalSubstance->GetClass(),
			OriginalSubstance->GetOuter(),
			*OriginalSubstance->GetName(),
			RF_Public|RF_Standalone,
			*(OriginalSubstance->SubstancePackage->SourceFilePath),
			NULL,
			this));

	if (NewSubstance != NULL)
	{
		NewSubstance->MarkPackageDirty();
	}

	local::bIsPerformingReimport = false;

	if (NULL == NewSubstance)
	{
		UE_LOG(LogSubstanceEditor, Error, TEXT("Substance reimport failed.") );
		return EReimportResult::Failed;
	}
		
	Substance::List<graph_inst_t*> Instances;

	for (auto ItBack = OriginalInstances.itfront() ; ItBack ; ++ItBack)
	{
		graph_desc_t* DescMatch = 
			FindMatch(
				NewSubstance->SubstancePackage->Graphs,
				&(*ItBack));

		if (!DescMatch)
		{
			FString InstanceName = (*ItBack).Preset.mLabel;
			(*ItBack).InstanceParent->MarkPackageDirty();
			(*ItBack).InstanceParent->Parent = NULL;
			UE_LOG(LogSubstanceEditor, Warning, TEXT("No match for the instance %s"), *InstanceName);
			continue;
		}

		Instances.push(RebuildInstance(DescMatch,*ItBack));
	}

	Substance::Helpers::RenderSync(Instances);
	Substance::Helpers::UpdateTextures(Instances);

	UE_LOG(LogSubstanceEditor, Log, TEXT("Re-imported successfully"), *GetName());
	return EReimportResult::Succeeded;
}
