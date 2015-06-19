// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorPrivatePCH.h"
#include "EdGraphUtilities.h"
#include "WorkspaceMenuStructureModule.h"
#include "LiveEditorConfigWindow.h"


#include "BlueprintEditor.h"
#include "BlueprintEditorModes.h"
#include "BlueprintEditorTabs.h"
#include "SDockTab.h"

DEFINE_LOG_CATEGORY_STATIC(LiveEditor, Log, All);

struct FLiveEditorPinFactory : public FGraphPanelPinFactory
{
public:
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* Pin) const
	{
		if ( Pin == NULL )
			return NULL;

		UK2Node_LiveEditObject *LiveEditObjectNode = Cast<UK2Node_LiveEditObject>(Pin->GetOwningNode());
		if ( LiveEditObjectNode == NULL )
			return NULL;

		if ( Pin->PinType.PinSubCategory != UK2Node_LiveEditObject::LiveEditableVarPinCategory )
			return NULL;

		return SNew(SGraphPinLiveEditVar, Pin, LiveEditObjectNode->GetClassToSpawn());
	}
};

class FComponentsEditorModeOverride : public FBlueprintEditorApplicationMode
{
public:
	FComponentsEditorModeOverride(TSharedPtr<FBlueprintEditor> InBlueprintEditor, FName InModeName)
		: FBlueprintEditorApplicationMode(InBlueprintEditor, InModeName, FBlueprintEditorApplicationModes::GetLocalizedMode)
	{
	}

	TWeakPtr<FBlueprintEditor> GetBlueprintEditor()
	{
		return MyBlueprintEditor;
	}
	UBlueprint* GetBlueprint() const
	{
		FBlueprintEditor* Editor = MyBlueprintEditor.Pin().Get();
		if ( Editor )
		{
			return Editor->GetBlueprintObj();
		}
		else
		{
			return NULL;
		}
	}
};

class FBlueprintEditorExtenderForLiveEditor
{
public:
	static TSharedRef<FApplicationMode> OnModeCreated(const FName ModeName, TSharedRef<FApplicationMode> InMode)
	{
		if (ModeName == FBlueprintEditorApplicationModes::BlueprintComponentsMode)
		{
			//@TODO: Bit of a lie - push GetBlueprint up, or pass in editor!
			auto LieMode = StaticCastSharedRef<FComponentsEditorModeOverride>(InMode);

			UBlueprint* BP = LieMode->GetBlueprint();
			if(	BP )
			{
				FLiveEditorManager::Get().InjectNewBlueprintEditor( LieMode->GetBlueprintEditor() );
			}
		}

		return InMode;
	}
};

class FLiveEditorNodeInjector : public TSharedFromThis<FLiveEditorNodeInjector>
{
public:
	FLiveEditorNodeInjector() {}
	void InstallHooks();
	void OnBeginPIE(const bool bIsSimulating);
	void OnEndPIE(const bool bIsSimulating);
};

void FLiveEditorNodeInjector::InstallHooks()
{
	FEditorDelegates::BeginPIE.AddRaw(this, &FLiveEditorNodeInjector::OnBeginPIE);
	FEditorDelegates::EndPIE.AddRaw(this, &FLiveEditorNodeInjector::OnEndPIE);
}

void FLiveEditorNodeInjector::OnBeginPIE(const bool bIsSimulating)
{
	FLiveEditorManager::Get().OnBeginPIE();
}

void FLiveEditorNodeInjector::OnEndPIE(const bool bIsSimulating)
{
	FLiveEditorManager::Get().OnEndPIE();
}

class FLiveEditorObjectCreateListener : public FUObjectArray::FUObjectCreateListener
{
public:
	FLiveEditorObjectCreateListener() {}
	~FLiveEditorObjectCreateListener() {}
	virtual void NotifyUObjectCreated(const class UObjectBase *Object, int32 Index)
	{
		FLiveEditorManager::Get().OnObjectCreation( Object );
	}
};
class FLiveEditorObjectDeleteListener : public FUObjectArray::FUObjectDeleteListener
{
public:
	FLiveEditorObjectDeleteListener() {}
	~FLiveEditorObjectDeleteListener() {}
	virtual void NotifyUObjectDeleted(const class UObjectBase *Object, int32 Index)
	{
		FLiveEditorManager::Get().OnObjectDeletion( Object );
	}
};

namespace LiveEditorModule
{
	static const FName LiveEditorApp = FName(TEXT("LiveEditorApp"));
}
class FLiveEditor : public ILiveEditor
{
public:
	FLiveEditor();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void InstallHooks();
	void RemoveHooks();
	static TSharedRef<SDockTab> SpawnLiveEditorTab(const FSpawnTabArgs& Args);
	static TSharedRef<SDockTab> SpawnDeviceSetupWindow(const FSpawnTabArgs& Args);

	FWorkflowApplicationModeExtender BlueprintEditorExtenderDelegate;
	FDelegateHandle                  BlueprintEditorExtenderDelegateHandle;
	TSharedPtr<FLiveEditorNodeInjector> NodeInjector;
	TSharedPtr<FLiveEditorPinFactory> PinFactory;
	FLiveEditorObjectCreateListener *ObjectCreationListener;
	FLiveEditorObjectDeleteListener *ObjectDeletionListener;
};

IMPLEMENT_MODULE( FLiveEditor, LiveEditor )

FLiveEditor::FLiveEditor()
:	NodeInjector(NULL),
	PinFactory(NULL)
{
}

void FLiveEditor::StartupModule()
{
	FLiveEditorManager::Get().Initialize();
	InstallHooks();
}

void FLiveEditor::ShutdownModule()
{
	RemoveHooks();

	delete ObjectCreationListener;
	ObjectCreationListener = NULL;

	delete ObjectDeletionListener;
	ObjectDeletionListener = NULL;

	FLiveEditorManager::Shutdown();
}

DECLARE_DELEGATE_RetVal_OneParam(bool, FOnBeginBlueprintContextMenuCreated, FGraphContextMenuBuilder &);

void FLiveEditor::InstallHooks()
{
	BlueprintEditorExtenderDelegate = FWorkflowApplicationModeExtender::CreateStatic(&FBlueprintEditorExtenderForLiveEditor::OnModeCreated);
	auto& ExtenderList = FWorkflowCentricApplication::GetModeExtenderList();
	ExtenderList.Add(BlueprintEditorExtenderDelegate);
	BlueprintEditorExtenderDelegateHandle = ExtenderList.Last().GetHandle();

	NodeInjector = MakeShareable(new FLiveEditorNodeInjector());
	NodeInjector->InstallHooks();

	PinFactory = MakeShareable(new FLiveEditorPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(PinFactory);

	ObjectCreationListener = new FLiveEditorObjectCreateListener();
	GetUObjectArray().AddUObjectCreateListener(ObjectCreationListener);

	ObjectDeletionListener = new FLiveEditorObjectDeleteListener();
	GetUObjectArray().AddUObjectDeleteListener(ObjectDeletionListener);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(LiveEditorModule::LiveEditorApp, FOnSpawnTab::CreateStatic(&SpawnLiveEditorTab))
		.SetDisplayName(NSLOCTEXT("LiveEditorPlugin", "TabTitle", "Live Editor"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LiveEditor.TabIcon"));
}

void FLiveEditor::RemoveHooks()
{
	FWorkflowCentricApplication::GetModeExtenderList().RemoveAll([=](const FWorkflowApplicationModeExtender& Extender){ return BlueprintEditorExtenderDelegateHandle == Extender.GetHandle(); });

	FEdGraphUtilities::UnregisterVisualPinFactory(PinFactory);

	GetUObjectArray().RemoveUObjectCreateListener(ObjectCreationListener);
	GetUObjectArray().RemoveUObjectDeleteListener(ObjectDeletionListener);

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LiveEditorModule::LiveEditorApp);
}

TSharedRef<SDockTab> FLiveEditor::SpawnLiveEditorTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LiveEditor.TabIcon"))
		.TabRole(ETabRole::NomadTab)
		[
			SNew( SBorder )
			.BorderImage( FEditorStyle::GetBrush( "Docking.Tab.ContentAreaBrush" ) )
			[
				SNew(SLiveEditorConfigWindow)
			]
		];
}

	/**
	 * LOAD the property window with the LiveEditorManager object properties
	void FLevelEditorActionCallbacks::OnShowWorldProperties( TWeakPtr< SLevelEditor > LevelEditor )
	{
		//Remove any selected actor info from the Inspector and the Property views.
		GEditor->SelectNone( true, true );
	
		//Update the details page of the WorldSettings without officially selecting it
		TArray<UObject *> SelectedActors;
		SelectedActors.Add(LevelEditor.Pin()->GetWorld()->GetWorldSettings());

		FMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<FMainFrameModule>(TEXT("MainFrame"));

		bool bHasUnlockedViews = false;

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>( TEXT("PropertyEditor") );

		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
		LevelEditorModule.SummonSelectionDetails();

		GUnrealEd->UpdateFloatingPropertyWindowsFromActorList( SelectedActors );
	}
	*/
