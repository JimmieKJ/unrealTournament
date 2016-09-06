//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceEditorPrivatePCH.h"
#include "SubstanceEditorModule.h"
#include "AssetTypeActions_Base.h"
#include "ModuleManager.h"
#include "Factories.h"
#include "EdGraphUtilities.h"

#include "SubstanceCoreModule.h"
#include "SubstanceEditor.h"
#include "SubstanceEditorClasses.h"

#include "AssetTypeActions_SubstanceGraphInstance.h"
#include "AssetTypeActions_SubstanceImageInput.h"
#include "AssetTypeActions_SubstanceInstanceFactory.h"
#include "AssetTypeActions_SubstanceTexture2D.h"


namespace SubstanceEditorModule
{
const FName SubstanceEditorAppIdentifier = FName(TEXT("SubstanceEditorApp"));
static FEditorDelegates::FOnPIEEvent::FDelegate OnBeginPIEDelegate;
static FDelegateHandle OnBeginPIEDelegateHandle;
}


/*-----------------------------------------------------------------------------
   FSubstanceEditorModule
-----------------------------------------------------------------------------*/

class FSubstanceEditorModule : public ISubstanceEditorModule
{
public:
	TSharedPtr<FAssetTypeActions_SubstanceGraphInstance> SubstanceGraphInstanceAssetTypeActions;
	TSharedPtr<FAssetTypeActions_SubstanceTexture2D> SubstanceTextureAssetTypeActions;
	TSharedPtr<FAssetTypeActions_SubstanceInstanceFactory> SubstanceInstanceFactoryAssetTypeActions;
	TSharedPtr<FAssetTypeActions_SubstanceImageInput> SubstanceImageInputAssetTypeActions;

	/** Constructor, set up console commands and variables **/
	FSubstanceEditorModule()
	{

	}

	/** Called right after the module DLL has been loaded and the module object has been created */
	virtual void StartupModule() override
	{
		MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
		ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

		FModuleManager::LoadModuleChecked<FSubstanceCoreModule>("SubstanceCore");
		FModuleManager::LoadModuleChecked<FSubstanceCoreModule>("AssetTools");

		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		SubstanceGraphInstanceAssetTypeActions = MakeShareable(new FAssetTypeActions_SubstanceGraphInstance);
		SubstanceTextureAssetTypeActions = MakeShareable(new FAssetTypeActions_SubstanceTexture2D);
		SubstanceInstanceFactoryAssetTypeActions = MakeShareable(new FAssetTypeActions_SubstanceInstanceFactory);
		SubstanceImageInputAssetTypeActions = MakeShareable(new FAssetTypeActions_SubstanceImageInput);

		AssetTools.RegisterAssetTypeActions(SubstanceGraphInstanceAssetTypeActions.ToSharedRef());
		AssetTools.RegisterAssetTypeActions(SubstanceTextureAssetTypeActions.ToSharedRef());
		AssetTools.RegisterAssetTypeActions(SubstanceInstanceFactoryAssetTypeActions.ToSharedRef());
		AssetTools.RegisterAssetTypeActions(SubstanceImageInputAssetTypeActions.ToSharedRef());

		// Register the thumbnail renderers
		UThumbnailManager::Get().RegisterCustomRenderer(USubstanceTexture2D::StaticClass(), USubstanceTextureThumbnailRenderer::StaticClass());
		UThumbnailManager::Get().RegisterCustomRenderer(USubstanceImageInput::StaticClass(), USubstanceImageInputThumbnailRenderer::StaticClass());

		SubstanceEditorModule::OnBeginPIEDelegate = FEditorDelegates::FOnPIEEvent::FDelegate::CreateStatic(&FSubstanceEditorModule::OnPieStart);
		SubstanceEditorModule::OnBeginPIEDelegateHandle = FEditorDelegates::BeginPIE.Add(SubstanceEditorModule::OnBeginPIEDelegate);
	}

	static void OnPieStart(const bool bIsSimulating)
	{
		Substance::Helpers::StartPIE();
	}

	/** Called before the module is unloaded, right before the module object is destroyed */
	virtual void ShutdownModule() override
	{
		FEditorDelegates::BeginPIE.Remove(SubstanceEditorModule::OnBeginPIEDelegateHandle);
	
		MenuExtensibilityManager.Reset();
		ToolBarExtensibilityManager.Reset();

		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

			AssetTools.UnregisterAssetTypeActions(
				SubstanceGraphInstanceAssetTypeActions.ToSharedRef());

			AssetTools.UnregisterAssetTypeActions(
				SubstanceTextureAssetTypeActions.ToSharedRef());

			AssetTools.UnregisterAssetTypeActions(
				SubstanceInstanceFactoryAssetTypeActions.ToSharedRef());

			AssetTools.UnregisterAssetTypeActions(
				SubstanceImageInputAssetTypeActions.ToSharedRef());
		}
	}
	

	virtual TSharedRef<ISubstanceEditor> CreateSubstanceEditor(const TSharedPtr< IToolkitHost >& InitToolkitHost, USubstanceGraphInstance* Graph) override
	{
		TSharedRef<FSubstanceEditor> NewSubstanceEditor(new FSubstanceEditor());
		NewSubstanceEditor->InitSubstanceEditor(InitToolkitHost, Graph);
		return NewSubstanceEditor;
	}

	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() {return ToolBarExtensibilityManager;}

private:
	
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};

IMPLEMENT_MODULE( FSubstanceEditorModule, SubstanceEditor );
