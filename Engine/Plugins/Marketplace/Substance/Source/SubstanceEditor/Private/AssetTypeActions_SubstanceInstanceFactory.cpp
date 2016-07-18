//! @file SubstanceEditorPanel.cpp
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceEditorPrivatePCH.h"

#include "AssetTypeActions_Base.h"
#include "AssetTypeActions_SubstanceInstanceFactory.h"

#include "SubstanceCoreHelpers.h"
#include "SubstanceCoreClasses.h"
#include "SubstanceFPackage.h"
#include "SubstanceFactory.h"

#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "ReferencedAssetsUtils.h"
#include "Text.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"


void FAssetTypeActions_SubstanceInstanceFactory::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto InstanceFactory = GetTypedWeakObjectPtrs<USubstanceInstanceFactory>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SubstanceInstanceFactory_Edit","Edit"),
		LOCTEXT("SubstanceInstanceFactory_EditTooltip", "Opens the selected graph instances in the graph instance editor."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_SubstanceInstanceFactory::ExecuteEdit, InstanceFactory ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SubstanceInstanceFactory_Reimport","Reimport"),
		LOCTEXT("SubstanceInstanceFactory_ReimportTooltip", "Reimports the selected graph instances."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_SubstanceInstanceFactory::ExecuteReimport, InstanceFactory ),
			FCanExecuteAction()
			)
		);

	// show list of graph contained in instance factory when a single one is selected
	if (InObjects.Num() == 1)
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("SubstanceInstanceFactory_CreateInstance","Create Graph Instance"),
			LOCTEXT("SubstanceInstanceFactory_CreateInstance", "Create an instance of the specified graph."),
			FNewMenuDelegate::CreateRaw( this, &FAssetTypeActions_SubstanceInstanceFactory::CreateInstanceSubMenu, InstanceFactory )
			);
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SubstanceInstanceFactory_DeleteWithOutputs","Delete with instances and outputs"),
		LOCTEXT("SubstanceInstanceFactory_DeleteWithOutputs", "Delete this instanceFactory with all its instances and output textures."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_SubstanceInstanceFactory::ExecuteDeleteWithOutputs, InstanceFactory ),
			FCanExecuteAction()
			)
		);
}


void FAssetTypeActions_SubstanceInstanceFactory::CreateInstanceSubMenu(class FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USubstanceInstanceFactory>> InstanceFactory)
{
	// show list of graph contained in instance factory when a single one is selected
	TArray<graph_desc_t*>::TIterator ItGraphDesc(InstanceFactory[0]->SubstancePackage->Graphs.itfront());

	for (;ItGraphDesc;++ItGraphDesc)
	{
		FText text = FText::FromString(FString::Printf(
			*NSLOCTEXT("SubstanceInstanceFactory", "New Instance of ", "%s").ToString(), 
			*(*ItGraphDesc)->Label));

		MenuBuilder.AddMenuEntry(
			text,
			FText::FromString(TEXT("")), 
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_SubstanceInstanceFactory::ExecuteInstantiate, *ItGraphDesc ),
			FCanExecuteAction()
			));
	}
}


void FAssetTypeActions_SubstanceInstanceFactory::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{

}


void FAssetTypeActions_SubstanceInstanceFactory::ExecuteEdit(TArray<TWeakObjectPtr<USubstanceInstanceFactory>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if (Object)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(Object);
		}
	}
}


void FAssetTypeActions_SubstanceInstanceFactory::ExecuteReimport(TArray<TWeakObjectPtr<USubstanceInstanceFactory>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if (Object)
		{
			FReimportManager::Instance()->Reimport( Object, true, true );
		}
	}
}


void FAssetTypeActions_SubstanceInstanceFactory::ExecuteInstantiate(graph_desc_t* GraphDesc)
{
	bool bOperationCanceled = false;
	Substance::FSubstanceImportOptions ImportOptions;
	ImportOptions.bForceCreateInstance = true;

	Substance::GetImportOptions(GraphDesc->Label, GraphDesc->Parent->Parent->GetOuter()->GetName(), ImportOptions, bOperationCanceled);

	if (bOperationCanceled)
	{
		return;
	}

	UObject* InstanceOuter = CreatePackage(NULL, *ImportOptions.InstanceDestinationPath);

	graph_inst_t* Instance =
		Substance::Helpers::InstantiateGraph(
			GraphDesc,
			InstanceOuter,
			ImportOptions.InstanceName,
			true // bCreateOutputs
			);

	Substance::Helpers::RenderSync(Instance);		

	{
		Substance::List<graph_inst_t*> Instances;
		Instances.AddUnique(Instance);
		Substance::Helpers::UpdateTextures(Instances);
	}

	if (ImportOptions.bCreateMaterial)
	{
		UObject* MaterialParent = CreatePackage(NULL, *ImportOptions.MaterialDestinationPath);
		SubstanceEditor::Helpers::CreateMaterial(
			Instance,
			ImportOptions.MaterialName,
			MaterialParent);
	}

	TArray<UObject*> AssetList;
	AssetList.AddUnique(Instance->ParentInstance);
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets(AssetList, true);
}


void FAssetTypeActions_SubstanceInstanceFactory::ExecuteDeleteWithOutputs(TArray<TWeakObjectPtr<USubstanceInstanceFactory>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		USubstanceInstanceFactory* InstanceFactory = (USubstanceInstanceFactory *)((*ObjIt).Get());
		if (InstanceFactory)
		{
			Substance::Helpers::RegisterForDeletion(InstanceFactory);
		}
	}
}

#undef LOC_NAMESPACE
