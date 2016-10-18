// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "Persona.h"
#include "SPersonaToolbar.h"
#include "PersonaModule.h"
#include "AnimGraphDefinitions.h"
#include "IDetailsView.h"

#include "Toolkits/IToolkitHost.h"

// this one needed?
#include "SAnimationEditorViewport.h"
// end of questionable

#include "SSequenceEditor.h"
#include "SMontageEditor.h"
#include "SAnimCompositeEditor.h"
#include "SAnimationBlendSpace.h"
#include "SAnimationBlendSpace1D.h"
#include "SKismetInspector.h"
#include "SSkeletonWidget.h"
#include "SPoseEditor.h"

#include "Editor/Kismet/Public/BlueprintEditorTabs.h"
#include "Editor/Kismet/Public/BlueprintEditorModes.h"

#include "ScopedTransaction.h"
#include "Editor/UnrealEd/Public/EdGraphUtilities.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Editor/UnrealEd/Public/Kismet2/DebuggerCommands.h"

//#include "SkeletonMode/SkeletonMode.h"
#include "MeshMode/MeshMode.h"
#include "PhysicsMode/PhysicsMode.h"
#include "AnimationMode/AnimationMode.h"
#include "BlueprintMode/AnimBlueprintMode.h"
#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"

#include "Editor/Persona/Private/AnimationEditorViewportClient.h"

#include "ComponentAssetBroker.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_BlendSpaceEvaluator.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_PoseByName.h"
#include "AnimGraphNode_PoseBlendNode.h"
#include "AnimGraphNode_Slot.h"
#include "Customization/AnimGraphNodeSlotDetails.h"

#include "AnimPreviewInstance.h"

#include "Particles/ParticleSystemComponent.h"

#include "DesktopPlatformModule.h"
#include "MainFrame.h"
#include "AnimationCompressionPanel.h"
#include "FbxAnimUtils.h"

#include "SAnimationDlgs.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "FbxMeshUtils.h"
#include "AnimationEditorUtils.h"
#include "SAnimationSequenceBrowser.h"
#include "SDockTab.h"
#include "GenericCommands.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

#include "Editor/KismetWidgets/Public/SSingleObjectDetailsPanel.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/PoseAsset.h"

#include "MessageLog.h"
#include "SAdvancedPreviewDetailsTab.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include "Editor/AnimGraph/Public/AnimGraphCommands.h"

#define LOCTEXT_NAMESPACE "FPersona"

const FName FPersona::PreviewSceneSettingsTabId(TEXT("Persona_PreviewScene"));

/////////////////////////////////////////////////////
// FLocalCharEditorCallbacks

struct FLocalCharEditorCallbacks
{
	static FText GetObjectName(UObject* Object)
	{
		return FText::FromString( Object->GetName() );
	}
};

/////////////////////////////////////////////////////
// SPersonaPreviewPropertyEditor

class SPersonaPreviewPropertyEditor : public SSingleObjectDetailsPanel
{
public:
	SLATE_BEGIN_ARGS(SPersonaPreviewPropertyEditor) {}
	SLATE_END_ARGS()

private:
	// Pointer back to owning Persona editor instance (the keeper of state)
	TWeakPtr<FPersona> PersonaPtr;
public:
	void Construct(const FArguments& InArgs, TSharedPtr<FPersona> InPersona)
	{
		PersonaPtr = InPersona;

		SSingleObjectDetailsPanel::Construct(SSingleObjectDetailsPanel::FArguments().HostCommandList(InPersona->GetToolkitCommands()), /*bAutomaticallyObserveViaGetObjectToObserve*/ true, /*bAllowSearch*/ true);

		PropertyView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateStatic([] { return !GIntraFrameDebuggingGameThread; }));
	}

	// SSingleObjectDetailsPanel interface
	virtual UObject* GetObjectToObserve() const override
	{
		if (UDebugSkelMeshComponent* PreviewComponent = PersonaPtr.Pin()->GetPreviewMeshComponent())
		{
			if (PreviewComponent->GetAnimInstance() != nullptr)
			{
				return PreviewComponent->GetAnimInstance();
			}
		}

		return nullptr;
	}

	virtual TSharedRef<SWidget> PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget) override
	{
		return SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Persona.PreviewPropertiesWarning"))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AnimBlueprintEditPreviewText", "Changes to preview options are not saved in the asset."))
					.Font(FEditorStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.ShadowColorAndOpacity(FLinearColor::Black.CopyWithNewOpacity(0.3f))
					.ShadowOffset(FVector2D::UnitVector)
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1)
			[
				PropertyEditorWidget
			];
	}
	// End of SSingleObjectDetailsPanel interface
};

//////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// FSkeletonEditAppMode

class FSkeletonEditAppMode : public FPersonaAppMode
{
public:
	FSkeletonEditAppMode(TSharedPtr<FPersona> InPersona)
		: FPersonaAppMode(InPersona, FPersonaModes::SkeletonDisplayMode)
	{
		PersonaTabFactories.RegisterFactory(MakeShareable(new FSelectionDetailsSummoner(InPersona)));

		TabLayout = FTabManager::NewLayout( "Persona_SkeletonEditMode_Layout_v3" )
			->AddArea
			(
				FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
				->Split
				(
					// Top toolbar area
					FTabManager::NewStack()
					->SetSizeCoefficient(0.186721f)
					->SetHideTabWell(true)
					->AddTab( InPersona->GetToolbarTabId(), ETabState::OpenedTab )
				)
				->Split
				(
					// Rest of screen
					FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal)
					->Split
					(
						// Left 1/3rd - Skeleton tree and mesh panel
						FTabManager::NewSplitter()
						->SetOrientation(Orient_Vertical)
						->SetSizeCoefficient(0.3f)
						->Split
						(
							FTabManager::NewStack()
							->AddTab( FPersonaTabs::SkeletonTreeViewID, ETabState::OpenedTab )
						)
					)
					->Split
					(
						// Middle 1/3rd - Viewport
						FTabManager::NewSplitter()
						->SetOrientation(Orient_Vertical)
						->SetSizeCoefficient(0.5f)
						->Split
						(
							FTabManager::NewStack()
							->SetHideTabWell(true)
							->AddTab( FPersonaTabs::PreviewViewportID, ETabState::OpenedTab )
						)
					)
					->Split
					(
						// Right 1/3rd - Details panel 
						FTabManager::NewSplitter()
						->SetOrientation(Orient_Vertical)
						->SetSizeCoefficient(0.2f)
						->Split
						(
							FTabManager::NewStack()
							->AddTab(FPersonaTabs::AdvancedPreviewSceneSettingsID, ETabState::OpenedTab)
							->AddTab( FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab )	//@TODO: FPersonaTabs::AnimPropertiesID
						)
					)
				)
			);

	}
};


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

TArray<UObject*> GetEditorObjectsOfClass( const TArray< UObject* >& Objects, const UClass* ClassToFind )
{
	TArray<UObject*> ObjectsToReturn;
	for( auto ObjectIter = Objects.CreateConstIterator(); ObjectIter; ++ObjectIter )
	{
		// Don't allow user to perform certain actions on objects that aren't actually assets (e.g. Level Script blueprint objects)
		const auto EditingObject = *ObjectIter;
		if( EditingObject != NULL && EditingObject->IsAsset() && EditingObject->GetClass()->IsChildOf( ClassToFind ) )
		{
			ObjectsToReturn.Add(EditingObject);
		}
	}
	return ObjectsToReturn;
}

/////////////////////////////////////////////////////
// FPersona

FPersona::FPersona()
	: TargetSkeleton(NULL)
	, PreviewComponent(NULL)
	, PersonaMeshDetailLayout(NULL)
	, PreviewScene(FPreviewScene::ConstructionValues().AllowAudioPlayback(true).ShouldSimulatePhysics(true))
	, LastCachedLODForPreviewComponent(0)
{
	// Register to be notified when properties are edited
	OnPropertyChangedHandle = FCoreUObjectDelegates::FOnObjectPropertyChanged::FDelegate::CreateRaw(this, &FPersona::OnPropertyChanged);
	OnPropertyChangedHandleDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.Add(OnPropertyChangedHandle);

	GEditor->OnBlueprintPreCompile().AddRaw(this, &FPersona::OnBlueprintPreCompile);

	//Temporary fix for missing attached assets - MDW
	PreviewScene.GetWorld()->GetWorldSettings()->SetIsTemporarilyHiddenInEditor(false);

}

FPersona::~FPersona()
{
	GEditor->OnBlueprintPreCompile().RemoveAll(this);

	FEditorDelegates::OnAssetPostImport.RemoveAll(this);
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);


	if(TargetSkeleton)
	{
		TargetSkeleton->UnregisterOnSkeletonHierarchyChanged(this);
	}

	if(Viewport.IsValid())
	{
		Viewport.Pin()->CleanupPersonaReferences();
	}

	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnPropertyChangedHandleDelegateHandle);

	if(PreviewComponent)
	{
		PreviewComponent->RemoveFromRoot();
	}
	
	// NOTE: Any tabs that we still have hanging out when destroyed will be cleaned up by FBaseToolkit's destructor
}

TSharedPtr<SWidget> FPersona::CreateEditorWidgetForAnimDocument(UObject* InAnimAsset, FString& DocumentLink)
{
	TSharedPtr<SWidget> Result;
	if (InAnimAsset)
	{
		if (UAnimSequence* Sequence = Cast<UAnimSequence>(InAnimAsset))
		{
			Result = SNew(SSequenceEditor)
				.Persona(SharedThis(this))
				.Sequence(Sequence);

			DocumentLink = TEXT("Engine/Animation/Sequences");
		}
		else if (UAnimComposite* Composite = Cast<UAnimComposite>(InAnimAsset))
		{
			Result = SNew(SAnimCompositeEditor)
				.Persona(SharedThis(this))
				.Composite(Composite);

			DocumentLink = TEXT("Engine/Animation/AnimationComposite");
		}
		else if (UAnimMontage* Montage = Cast<UAnimMontage>(InAnimAsset))
		{
			Result = SNew(SMontageEditor)
				.Persona(SharedThis(this))
				.Montage(Montage);

			DocumentLink = TEXT("Engine/Animation/AnimMontage");
		}
		else if (UPoseAsset* PoseAsset = Cast<UPoseAsset>(InAnimAsset))
		{
			Result = SNew(SPoseEditor)
				.Persona(SharedThis(this))
				.PoseAsset(PoseAsset);

			DocumentLink = TEXT("Engine/Animation/Sequences");
		}
		else if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(InAnimAsset))
		{
			Result = SNew(SBlendSpaceEditor)
				.Persona(SharedThis(this))
				.BlendSpace(BlendSpace);

			if (Cast<UAimOffsetBlendSpace>(InAnimAsset))
			{
				DocumentLink = TEXT("Engine/Animation/AimOffset");
			}
			else
			{
				DocumentLink = TEXT("Engine/Animation/Blendspaces");
			}
		}
		else if (UBlendSpace1D* BlendSpace1D = Cast<UBlendSpace1D>(InAnimAsset))
		{
			Result = SNew(SBlendSpaceEditor1D)
				.Persona(SharedThis(this))
				.BlendSpace1D(BlendSpace1D);


			if (Cast<UAimOffsetBlendSpace1D>(InAnimAsset))
			{
				DocumentLink = TEXT("Engine/Animation/AimOffset");
			}
			else
			{
				DocumentLink = TEXT("Engine/Animation/Blendspaces");
			}
		}
	}

	if (Result.IsValid())
	{
		InAnimAsset->SetFlags(RF_Transactional);
	}

	return Result;
}

void FPersona::ReinitMode()
{
	FName CurrentMode = GetCurrentMode();

	if (CurrentMode == FPersonaModes::AnimationEditMode)
	{
		// if opening animation edit mode, open the animation editor
		OpenNewAnimationDocumentTab(GetPreviewAnimationAsset());
	}
}

TSharedPtr<SDockTab> FPersona::OpenNewAnimationDocumentTab(UObject* InAnimAsset)
{
	TSharedPtr<SDockTab> OpenedTab;

	if (InAnimAsset != NULL)
	{
		FString	DocumentLink;
		TSharedPtr<SWidget> TabContents = CreateEditorWidgetForAnimDocument(InAnimAsset, DocumentLink);
		if (TabContents.IsValid())
		{
			if ( SharedAnimAssetBeingEdited.IsValid() )
			{
				RemoveEditingObject(SharedAnimAssetBeingEdited.Get());
			}

			if ( InAnimAsset != NULL )
			{
				AddEditingObject(InAnimAsset);
			}

			SharedAnimAssetBeingEdited = InAnimAsset;
			TAttribute<FText> NameAttribute = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&FLocalCharEditorCallbacks::GetObjectName, (UObject*)InAnimAsset));

			if (SharedAnimDocumentTab.IsValid())
			{
				OpenedTab = SharedAnimDocumentTab.Pin();
				OpenedTab->SetContent(TabContents.ToSharedRef());
				OpenedTab->ActivateInParent(ETabActivationCause::SetDirectly);
				OpenedTab->SetLabel(NameAttribute);
				OpenedTab->SetLeftContent(IDocumentation::Get()->CreateAnchor(DocumentLink));
			}
			else
			{
				OpenedTab = SNew(SDockTab)
					.Label(NameAttribute)
					.OnTabClosed(this, &FPersona::OnEditTabClosed)
					.TabRole(ETabRole::DocumentTab)
					.TabColorScale(GetTabColorScale())
					[
						TabContents.ToSharedRef()
					];

				OpenedTab->SetLeftContent(IDocumentation::Get()->CreateAnchor(DocumentLink));
			
				TabManager->InsertNewDocumentTab("Document", FTabManager::ESearchPreference::RequireClosedTab, OpenedTab.ToSharedRef());

				SharedAnimDocumentTab = OpenedTab;
			}
		}

		if(SequenceBrowser.IsValid())
		{
			UAnimationAsset* NewAsset = CastChecked<UAnimationAsset>(InAnimAsset);
			SequenceBrowser.Pin()->SelectAsset(NewAsset);
		}
	}

	return OpenedTab;
}

TSharedPtr<SDockTab> FPersona::OpenNewDocumentTab(class UAnimationAsset* InAnimAsset)
{
	/// before opening new asset, clear the currently selected object
	SetDetailObject(NULL);

	TSharedPtr<SDockTab> NewTab;
	if (InAnimAsset)
	{
		// Are we allowed to open animation documents right now?
		//@TODO: Super-hacky check
		FName CurrentMode = GetCurrentMode();
		if (CurrentMode == FPersonaModes::AnimationEditMode)
		{
			NewTab = OpenNewAnimationDocumentTab(InAnimAsset);
		}

		SetPreviewAnimationAsset(InAnimAsset);
	}

	return NewTab;
}

void FPersona::SetViewport(TWeakPtr<class SAnimationEditorViewportTabBody> NewViewport)
{
	Viewport = NewViewport;
	if(Viewport.IsValid())
	{
		Viewport.Pin()->SetPreviewComponent(PreviewComponent);
	}
	OnViewportCreated.Broadcast(Viewport);
}

void FPersona::SetSequenceBrowser(class SAnimationSequenceBrowser* InSequenceBrowser)
{
	if (InSequenceBrowser)
	{
		SequenceBrowser = SharedThis(InSequenceBrowser);
	}
	else
	{
		SequenceBrowser = NULL;
	}
}

void FPersona::RefreshViewport()
{
	if (Viewport.IsValid())
	{
		Viewport.Pin()->RefreshViewport();
	}
}

USkeleton* FPersona::GetSkeleton() const
{
	return TargetSkeleton;
}

USkeletalMesh* FPersona::GetMesh() const
{
	return PreviewComponent->SkeletalMesh;
}

UPhysicsAsset* FPersona::GetPhysicsAsset() const
{
	return NULL;
}

UAnimBlueprint* FPersona::GetAnimBlueprint() const
{
	return Cast<UAnimBlueprint>(GetBlueprintObj());
}

UObject* FPersona::GetMeshAsObject() const
{
	return GetMesh();
}

UObject* FPersona::GetSkeletonAsObject() const
{
	return GetSkeleton();
}

UObject* FPersona::GetPhysicsAssetAsObject() const
{
	return GetPhysicsAsset();
}

UObject* FPersona::GetAnimBlueprintAsObject() const
{
	return GetAnimBlueprint();
}

void FPersona::ExtendMenu()
{
	// Add additional persona editor menus
	struct Local
	{
		static void AddPersonaSaveLoadMenu(FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(FPersonaCommands::Get().SaveAnimationAssets);
			MenuBuilder.AddMenuSeparator();
		}
		
		static void AddPersonaFileMenu(FMenuBuilder& MenuBuilder)
		{
			// View
			MenuBuilder.BeginSection("Persona", LOCTEXT("PersonaEditorMenu_File", "Blueprint"));
			{
			}
			MenuBuilder.EndSection();
		}

		static void AddPersonaAssetMenu(FMenuBuilder& MenuBuilder, FPersona* PersonaPtr)
		{
			// View
			MenuBuilder.BeginSection("Persona", LOCTEXT("PersonaAssetMenuMenu_Skeleton", "Skeleton"));
			{
				MenuBuilder.AddMenuEntry(FPersonaCommands::Get().ChangeSkeletonPreviewMesh);
				MenuBuilder.AddMenuEntry(FPersonaCommands::Get().RemoveUnusedBones);
				MenuBuilder.AddMenuEntry(FPersonaCommands::Get().UpdateSkeletonRefPose);
				MenuBuilder.AddMenuEntry(FPersonaCommands::Get().TestSkeletonCurveNamesForUse);
			}
			MenuBuilder.EndSection();

			//  Animation menu
			MenuBuilder.BeginSection("Persona", LOCTEXT("PersonaAssetMenuMenu_Animation", "Animation"));
			{
				MenuBuilder.AddMenuEntry(FPersonaCommands::Get().ApplyCompression);
				MenuBuilder.AddMenuEntry(FPersonaCommands::Get().ExportToFBX);
				MenuBuilder.AddMenuEntry(FPersonaCommands::Get().AddLoopingInterpolation);
			}
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection("Persona", LOCTEXT("PersonaAssetMenuMenu_Record", "Record"));
			{
				MenuBuilder.AddMenuEntry(FPersonaCommands::Get().RecordAnimation, 
					NAME_None, 
					TAttribute<FText>(PersonaPtr, &FPersona::GetRecordMenuLabel));
			}
			MenuBuilder.EndSection();
		}
	};

	if(MenuExtender.IsValid())
	{
		RemoveMenuExtender(MenuExtender);
		MenuExtender.Reset();
	}

	MenuExtender = MakeShareable(new FExtender);

	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	if(AnimBlueprint)
	{
		TSharedPtr<FExtender> AnimBPMenuExtender = MakeShareable(new FExtender);
		FKismet2Menu::SetupBlueprintEditorMenu(AnimBPMenuExtender, *this);
		AddMenuExtender(AnimBPMenuExtender);

		AnimBPMenuExtender->AddMenuExtension(
			"FileLoadAndSave",
			EExtensionHook::After,
			GetToolkitCommands(),
			FMenuExtensionDelegate::CreateStatic(&Local::AddPersonaFileMenu));
	}

	MenuExtender->AddMenuExtension(
		"FileLoadAndSave",
		EExtensionHook::First,
		GetToolkitCommands(),
		FMenuExtensionDelegate::CreateStatic(&Local::AddPersonaSaveLoadMenu));

	MenuExtender->AddMenuExtension(
			"AssetEditorActions",
			EExtensionHook::After,
			GetToolkitCommands(),
			FMenuExtensionDelegate::CreateStatic(&Local::AddPersonaAssetMenu, this)
			);

	AddMenuExtender(MenuExtender);

	// add extensible menu if exists
	FPersonaModule* PersonaModule = &FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	AddMenuExtender(PersonaModule->GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FPersona::InitPersona(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, USkeleton* InitSkeleton, UAnimBlueprint* InitAnimBlueprint, UAnimationAsset* InitAnimationAsset, class USkeletalMesh* InitMesh)
{
	FReimportManager::Instance()->OnPostReimport().AddRaw(this, &FPersona::OnPostReimport);

	AssetDirtyBrush = FEditorStyle::GetBrush("ContentBrowser.ContentDirty");

	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShareable(new FBlueprintEditorToolbar(SharedThis(this)));
	}
	if (!PersonaToolbar.IsValid())
	{
		PersonaToolbar = MakeShareable(new FPersonaToolbar);
	}

	GetToolkitCommands()->Append(FPlayWorldCommands::GlobalPlayWorldActions.ToSharedRef());

	// Create a list of available modes
	{
		TSharedPtr<FPersona> ThisPtr(SharedThis(this));

		TArray< TSharedRef<FApplicationMode> > TempModeList;
		TempModeList.Add(MakeShareable(new FSkeletonEditAppMode(ThisPtr)));
		TempModeList.Add(MakeShareable(new FMeshEditAppMode(ThisPtr)));
		//TempModeList.Add(MakeShareable(new FPhysicsEditAppMode(ThisPtr)));
		TempModeList.Add(MakeShareable(new FAnimEditAppMode(ThisPtr)));
		TempModeList.Add(MakeShareable(new FAnimBlueprintEditAppMode(ThisPtr)));

		for (auto ModeListIt = TempModeList.CreateIterator(); ModeListIt; ++ModeListIt)
		{
			TSharedRef<FApplicationMode> ApplicationMode = *ModeListIt;
			AddApplicationMode(ApplicationMode->GetModeName(), ApplicationMode);
		}
	}

	// Determine the initial mode the app will open up in
	FName InitialMode = NAME_None;
	if (InitAnimBlueprint != NULL)
	{
		InitialMode = FPersonaModes::AnimBlueprintEditMode;
		TargetSkeleton = InitAnimBlueprint->TargetSkeleton;
	}
	else if (InitAnimationAsset != NULL)
	{
		InitialMode = FPersonaModes::AnimationEditMode;
		TargetSkeleton = InitSkeleton;
	}
	else if (InitMesh != NULL)
	{
		InitialMode = FPersonaModes::MeshEditMode;
		TargetSkeleton = InitSkeleton;
	}
	else
	{
		InitialMode = FPersonaModes::SkeletonDisplayMode;
		TargetSkeleton = InitSkeleton;
	}

	// Build up a list of objects being edited in this asset editor
	TArray<UObject*> ObjectsBeingEdited;

	if (InitAnimBlueprint != NULL)
	{
		ObjectsBeingEdited.Add(InitAnimBlueprint);
	} 

	// Purposefully skipping adding InitAnimationAsset here because it will get added when opening a document tab for it
	//if (InitAnimationAsset != NULL)
	//{
	//	ObjectsBeingEdited.Add(InitAnimationAsset);
	//}

	// Purposefully skipping adding InitMesh here because it will get added when setting the preview mesh
	//if (InitMesh != NULL)
	//{
	//	ObjectsBeingEdited.Add(InitMesh);
	//}

	ObjectsBeingEdited.Add(TargetSkeleton);
	check(TargetSkeleton);

	TargetSkeleton->CollectAnimationNotifies();

	TargetSkeleton->RegisterOnSkeletonHierarchyChanged( USkeleton::FOnSkeletonHierarchyChanged::CreateSP( this, &FPersona::OnSkeletonHierarchyChanged ) );

	// We could modify the skeleton within Persona (add/remove sockets), so we need to enable undo/redo on it
	TargetSkeleton->SetFlags( RF_Transactional );

	// Initialize the asset editor and spawn tabs
	const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());
	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, PersonaAppName, DummyLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsBeingEdited);
	
	TArray<UBlueprint*> AnimBlueprints;
	if (InitAnimBlueprint)
	{
		AnimBlueprints.Add(InitAnimBlueprint);
	}
	CommonInitialization(AnimBlueprints);

	check(PreviewComponent == NULL);

	// Create the preview component
	PreviewComponent = NewObject<UDebugSkelMeshComponent>();

	// note: we add to root here (rather than using RF_Standalone) as all standalone objects will be cleaned up when we switch
	// preview worlds (in UWorld::CleanupWorld), but we want this to stick around while Persona exists
	PreviewComponent->AddToRoot();

	bool bSetMesh = false;

	// Set the mesh
	if (InitMesh != NULL)
	{
		SetPreviewMesh(InitMesh);
		bSetMesh = true;

		if (TargetSkeleton && !TargetSkeleton->GetPreviewMesh())
		{
			TargetSkeleton->SetPreviewMesh(InitMesh, false);
		}
	}
	else if (InitAnimationAsset != NULL)
	{
		USkeletalMesh* AssetMesh = InitAnimationAsset->GetPreviewMesh();
		if (AssetMesh)
		{
			SetPreviewMesh(AssetMesh);
			bSetMesh = true;
		}
	}

	if (!bSetMesh && TargetSkeleton)
	{
		//If no preview mesh set, just find the first mesh that uses this skeleton
		USkeletalMesh* PreviewMesh = TargetSkeleton->GetPreviewMesh(true);
		if ( PreviewMesh )
		{
			SetPreviewMesh( PreviewMesh );
		}
	}

	// Force validation of preview attached assets (catch case of never doing it if we dont have a valid preview mesh)
	ValidatePreviewAttachedAssets(NULL);

	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	PreviewComponent->SetAnimInstanceClass(AnimBlueprint ? AnimBlueprint->GeneratedClass : NULL);

	// We always want a preview instance unless we are using blueprints so that bone manipulation works
	if (AnimBlueprint == NULL)
	{
		PreviewComponent->EnablePreview(true, nullptr);
	}
	else
	{
		// Make sure the object being debugged is the preview instance
		AnimBlueprint->SetObjectBeingDebugged(PreviewComponent->GetAnimInstance());
	}

	ExtendMenu();
	ExtendDefaultPersonaToolbar();
	RegenerateMenusAndToolbars();

	// Activate the initial mode (which will populate with a real layout)
	SetCurrentMode(InitialMode);
	check(CurrentAppModePtr.IsValid());

	// Post-layout initialization
	PostLayoutBlueprintEditorInitialization();

	//@TODO: Push somewhere else?
	if (InitAnimationAsset != NULL)
	{
		OpenNewDocumentTab( InitAnimationAsset );
	}

	// register customization of Slot node for this Persona
	// this is so that you can open the manage window per Persona
	TWeakPtr<FPersona> PersonaPtr = SharedThis(this);
	Inspector->GetPropertyView()->RegisterInstancedCustomPropertyLayout(UAnimGraphNode_Slot::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FAnimGraphNodeSlotDetails::MakeInstance, PersonaPtr));

	// Register post import callback to catch animation imports when we have the asset open (we need to reinit)
	FEditorDelegates::OnAssetPostImport.AddRaw(this, &FPersona::OnPostImport);

	
}
	
void FPersona::CreateAnimation(const TArray<UObject*> NewAssets, int32 Option) 
{
	bool bResult = true;
	if (NewAssets.Num() > 0)
	{
		USkeletalMeshComponent* MeshComponent = GetPreviewMeshComponent();
		UAnimSequence* Sequence = Cast<UAnimSequence> (GetPreviewAnimationAsset());

		for (auto NewAsset : NewAssets)
		{
			UAnimSequence* NewAnimSequence = Cast<UAnimSequence>(NewAsset);
			if (NewAnimSequence)
			{
				switch (Option)
				{
				case 0:
					bResult &= NewAnimSequence->CreateAnimation(MeshComponent->SkeletalMesh);
					break;
				case 1:
					bResult &= NewAnimSequence->CreateAnimation(MeshComponent);
					break;
				case 2:
					bResult &= NewAnimSequence->CreateAnimation(Sequence);
					break;
				}
			}
		}

		// if it contains error, warn them
		if (bResult)
		{
			OnAssetCreated(NewAssets);

			// if it created based on current mesh component, 
			if (Option == 1)
			{
				PreviewComponent->PreviewInstance->ResetModifiedBone();
			}
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToCreateAsset", "Failed to create asset"));
		}
	}
}

void FPersona::CreatePoseAsset(const TArray<UObject*> NewAssets, int32 Option)
{
	bool bResult = false;
	if (NewAssets.Num() > 0)
	{
		USkeletalMeshComponent* MeshComponent = GetPreviewMeshComponent();
		UAnimSequence* Sequence = Cast<UAnimSequence>(GetPreviewAnimationAsset());

		for (auto NewAsset : NewAssets)
		{
			UPoseAsset* NewPoseAsset = Cast<UPoseAsset>(NewAsset);
			if (NewPoseAsset)
			{
				switch (Option)
				{
				case 0:
					NewPoseAsset->AddOrUpdatePoseWithUniqueName(MeshComponent);
					break;
				case 1:
					NewPoseAsset->CreatePoseFromAnimation(Sequence);
					break;
				}
				
				bResult = true;
			}
		}

		// if it contains error, warn them
		if (bResult)
		{
			OnAssetCreated(NewAssets);
			
			// if it created based on current mesh component, 
			if (Option == 0)
			{
				PreviewComponent->PreviewInstance->ResetModifiedBone();
			}
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToCreateAsset", "Failed to create asset"));
		}
	}
}

void FPersona::FillCreateAnimationMenu(FMenuBuilder& MenuBuilder) const
{
	TArray<TWeakObjectPtr<USkeleton>> Skeletons;

	Skeletons.Add(TargetSkeleton);

	// create rig
	MenuBuilder.BeginSection("CreateAnimationSubMenu", LOCTEXT("CreateAnimationSubMenuHeading", "Create Animation"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreateAnimation_RefPose", "From Reference Pose"),
			LOCTEXT("CreateAnimation_RefPose_Tooltip", "Create Animation from reference pose."),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateStatic(&AnimationEditorUtils::ExecuteNewAnimAsset<UAnimSequenceFactory, UAnimSequence>, Skeletons, FString("_Sequence"), FAnimAssetCreated::CreateSP(this, &FPersona::CreateAnimation, 0), false),
			FCanExecuteAction()
			)
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreateAnimation_CurrentPose", "From Current Pose"),
			LOCTEXT("CreateAnimation_CurrentPose_Tooltip", "Create Animation from current pose."),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateStatic(&AnimationEditorUtils::ExecuteNewAnimAsset<UAnimSequenceFactory, UAnimSequence>, Skeletons, FString("_Sequence"), FAnimAssetCreated::CreateSP(this, &FPersona::CreateAnimation, 1), false),
			FCanExecuteAction()
			)
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreateAnimation_CurrentAnimation", "From Current Animation"),
			LOCTEXT("CreateAnimation_CurrentAnimation_Tooltip", "Create Animation from current animation."),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateStatic(&AnimationEditorUtils::ExecuteNewAnimAsset<UAnimSequenceFactory, UAnimSequence>, Skeletons, FString("_Sequence"), FAnimAssetCreated::CreateSP(this, &FPersona::CreateAnimation, 2), false),
			FCanExecuteAction::CreateSP(this, &FPersona::HasValidAnimationSequencePlaying)
			)
			);
	}
	MenuBuilder.EndSection();
}

void FPersona::FillCreatePoseAssetMenu(FMenuBuilder& MenuBuilder) const
{
	TArray<TWeakObjectPtr<USkeleton>> Skeletons;

	Skeletons.Add(TargetSkeleton);

	// create rig
	MenuBuilder.BeginSection("CreatePoseAssetSubMenu", LOCTEXT("CreatePoseAssetSubMenuHeading", "Create PoseAsset"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreatePoseAsset_CurrentPose", "From Current Pose"),
			LOCTEXT("CreatePoseAsset_CurrentPose_Tooltip", "Create PoseAsset from current pose."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&AnimationEditorUtils::ExecuteNewAnimAsset<UPoseAssetFactory, UPoseAsset>, Skeletons, FString("_PoseAsset"), FAnimAssetCreated::CreateSP(this, &FPersona::CreatePoseAsset, 0), false),
				FCanExecuteAction()
				)
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreatePoseAsset_CurrentAnimation", "From Current Animation"),
			LOCTEXT("CreatePoseAsset_CurrentAnimation_Tooltip", "Create Animation from current animation."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&AnimationEditorUtils::ExecuteNewAnimAsset<UPoseAssetFactory, UPoseAsset>, Skeletons, FString("_PoseAsset"), FAnimAssetCreated::CreateSP(this, &FPersona::CreatePoseAsset, 1), false),
				FCanExecuteAction::CreateSP(this, &FPersona::HasValidAnimationSequencePlaying)
				)
			);
	}
	MenuBuilder.EndSection();
}

TSharedRef< SWidget > FPersona::GenerateCreateAssetMenu( USkeleton* Skeleton ) const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, NULL);

	// Create Animation menu
	MenuBuilder.BeginSection("CreateAnimation", LOCTEXT("CreateAnimationMenuHeading", "Animation"));
	{
		// create menu
		MenuBuilder.AddSubMenu(
				LOCTEXT("CreateAnimationSubmenu", "Create Animation"),
				LOCTEXT("CreateAnimationSubmenu_ToolTip", "Create Animation for this skeleton"),
				FNewMenuDelegate::CreateSP(this, &FPersona::FillCreateAnimationMenu),
				false,
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.AnimSequence")
				);

		MenuBuilder.AddSubMenu(
			LOCTEXT("CreatePoseAssetSubmenu", "Create PoseAsset"),
			LOCTEXT("CreatePoseAsssetSubmenu_ToolTip", "Create PoseAsset for this skeleton"),
			FNewMenuDelegate::CreateSP(this, &FPersona::FillCreatePoseAssetMenu),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.PoseAsset")
			);
	}
	MenuBuilder.EndSection();

	TArray<TWeakObjectPtr<USkeleton>> Skeletons;

	Skeletons.Add(Skeleton);

	AnimationEditorUtils::FillCreateAssetMenu(MenuBuilder, Skeletons, FAnimAssetCreated::CreateSP(this, &FPersona::OnAssetCreated), false);

	return MenuBuilder.MakeWidget();
}

void FPersona::OnAssetCreated(const TArray<UObject*> NewAssets) 
{
	if ( NewAssets.Num() > 0 )
	{
		FAssetRegistryModule::AssetCreated(NewAssets[0]);
		OpenNewDocumentTab(CastChecked<UAnimationAsset>(NewAssets[0]));
	}
}

void FPersona::ExtendDefaultPersonaToolbar()
{
	// If the ToolbarExtender is valid, remove it before rebuilding it
	if(ToolbarExtender.IsValid())
	{
		RemoveToolbarExtender(ToolbarExtender);
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShareable(new FExtender);

	PersonaToolbar->SetupPersonaToolbar(ToolbarExtender, SharedThis(this));

	// extend extra menu/toolbars
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, USkeleton* Skeleton, FPersona* PersonaPtr)
		{
			ToolbarBuilder.BeginSection("Skeleton");
			{
				ToolbarBuilder.AddToolBarButton(FPersonaCommands::Get().TogglePreviewAsset, NAME_None, LOCTEXT("Toolbar_PreviewAsset", "Preview"), TAttribute<FText>(PersonaPtr, &FPersona::GetPreviewAssetTooltip));
				ToolbarBuilder.AddToolBarButton(FPersonaCommands::Get().ToggleReferencePose, NAME_None, LOCTEXT("Toolbar_ToggleReferencePose", "Ref Pose"), LOCTEXT("Toolbar_ToggleReferencePoseTooltip", "Show Reference Pose"));
				ToolbarBuilder.AddSeparator();
				ToolbarBuilder.AddToolBarButton(FPersonaCommands::Get().AnimNotifyWindow);
				ToolbarBuilder.AddToolBarButton(FPersonaCommands::Get().RetargetManager, NAME_None, LOCTEXT("Toolbar_RetargetManager", "Retarget Manager"));
				ToolbarBuilder.AddToolBarButton(FPersonaCommands::Get().ImportMesh);
				ToolbarBuilder.AddToolBarButton(FPersonaCommands::Get().ReimportMesh);

				// animation import menu
				ToolbarBuilder.AddToolBarButton(FPersonaCommands::Get().ImportAnimation, NAME_None, LOCTEXT("Toolbar_ImportAnimation", "Import"));
				ToolbarBuilder.AddToolBarButton(FPersonaCommands::Get().ReimportAnimation, NAME_None, LOCTEXT("Toolbar_ReimportAnimation", "Reimport"));
				ToolbarBuilder.AddToolBarButton(FPersonaCommands::Get().ExportToFBX, NAME_None, LOCTEXT("Toolbar_ExportToFBX", "Export"));

				ToolbarBuilder.AddToolBarButton(FPersonaCommands::Get().RecordAnimation, 
											NAME_None,
											 TAttribute<FText>(PersonaPtr, &FPersona::GetRecordStatusLabel),
											 TAttribute<FText>(PersonaPtr, &FPersona::GetRecordStatusTooltip),
											 TAttribute<FSlateIcon>(PersonaPtr, &FPersona::GetRecordStatusImage),
											 NAME_None);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("Animation");
			{
				// create button
				{
					ToolbarBuilder.AddComboButton(
						FUIAction(
							FExecuteAction(),
							FCanExecuteAction(),
							FIsActionChecked(),
							FIsActionButtonVisible::CreateSP(PersonaPtr, &FPersona::IsInPersonaMode, FPersonaModes::AnimationEditMode)
							),
						FOnGetContent::CreateSP(PersonaPtr, &FPersona::GenerateCreateAssetMenu, Skeleton),
						LOCTEXT("OpenBlueprint_Label", "Create Asset"),
						LOCTEXT("OpenBlueprint_ToolTip", "Create Assets for this skeleton."),
						FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.CreateAsset")
						);
				}

				ToolbarBuilder.AddToolBarButton(FPersonaCommands::Get().ApplyCompression, NAME_None, LOCTEXT("Toolbar_ApplyCompression", "Compression"));
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("Editing");
			{
				ToolbarBuilder.AddToolBarButton(FPersonaCommands::Get().SetKey, NAME_None, LOCTEXT("Toolbar_SetKey", "Key"));
				ToolbarBuilder.AddToolBarButton(FPersonaCommands::Get().ApplyAnimation, NAME_None, LOCTEXT("Toolbar_ApplyAnimation", "Apply"));
			}
			ToolbarBuilder.EndSection();
		}
	};


	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar, TargetSkeleton, this)
		);

	AddToolbarExtender(ToolbarExtender);

	FPersonaModule* PersonaModule = &FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	AddToolbarExtender(PersonaModule->GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

UBlueprint* FPersona::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for (int32 i = 0; i < EditingObjs.Num(); ++i)
	{
		if (EditingObjs[i]->IsA<UAnimBlueprint>()) {return (UBlueprint*)EditingObjs[i];}
	}
	return nullptr;
}

UObject* FPersona::GetPreviewAnimationAsset() const
{
	if (Viewport.IsValid())
	{
		if (PreviewComponent)
		{
			// if same, do not overwrite. It will reset time and everything
			if (PreviewComponent->PreviewInstance != NULL)
			{
				return PreviewComponent->PreviewInstance->GetCurrentAsset();
			}
		}
	}

	return NULL;
}

UObject* FPersona::GetAnimationAssetBeingEdited() const
{
	if (!SharedAnimDocumentTab.IsValid())
	{
		SharedAnimAssetBeingEdited = NULL;
	}

	return SharedAnimAssetBeingEdited.Get();
}

void FPersona::SetPreviewAnimationAsset(UAnimationAsset* AnimAsset, bool bEnablePreview)
{
	if (Viewport.IsValid() && !Viewport.Pin()->bPreviewLockModeOn)
	{
		if (PreviewComponent)
		{
			RemoveAttachedComponent(false);

			if (AnimAsset != NULL)
			{
				// Early out if the new preview asset is the same as the current one, to avoid replaying from the beginning, etc...
				if (AnimAsset == GetPreviewAnimationAsset() && PreviewComponent->IsPreviewOn())
				{
					return;
				}

				// Treat it as invalid if it's got a bogus skeleton pointer
				if (AnimAsset->GetSkeleton() != TargetSkeleton)
				{
					return;
				}
			}

			PreviewComponent->EnablePreview(bEnablePreview, AnimAsset);
		}

		OnAnimChanged.Broadcast(AnimAsset);
	}
}

void FPersona::UpdateSelectionDetails(UObject* Object, const FText& ForcedTitle)
{
	Inspector->ShowDetailsForSingleObject(Object, SKismetInspector::FShowDetailsOptions(ForcedTitle));
}

void FPersona::SetDetailObject(UObject* Obj)
{
	FText ForcedTitle = (Obj != NULL) ? FText::FromString(Obj->GetName()) : FText::GetEmpty();
	UpdateSelectionDetails(Obj, ForcedTitle);
}

UDebugSkelMeshComponent* FPersona::GetPreviewMeshComponent() const
{
	return PreviewComponent;
}

UAnimInstance* FPersona::GetPreviewAnimInstance() const
{
	UDebugSkelMeshComponent* DebugSkelComp = GetPreviewMeshComponent();
	return (DebugSkelComp != nullptr) ? DebugSkelComp->GetAnimInstance() : nullptr;
}


void FPersona::OnPostReimport(UObject* InObject, bool bSuccess)
{
	if (bSuccess)
	{
		ConditionalRefreshEditor(InObject);
	}
}

void FPersona::OnPostImport(UFactory* InFactory, UObject* InObject)
{
	ConditionalRefreshEditor(InObject);
}

void FPersona::ConditionalRefreshEditor(UObject* InObject)
{
	bool bInterestingAsset = true;
	// Ignore if this is regarding a different object
	if(InObject != TargetSkeleton && InObject != TargetSkeleton->GetPreviewMesh() && InObject != GetAnimationAssetBeingEdited())
	{
		bInterestingAsset = false;
	}

	// Check that we aren't a montage that uses an incoming animation
	if(UAnimMontage* Montage = Cast<UAnimMontage>(GetAnimationAssetBeingEdited()))
	{
		for(FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
		{
			if(bInterestingAsset)
			{
				break;
			}

			for(FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
			{
				if(Segment.AnimReference == InObject)
				{
					bInterestingAsset = true;
					break;
				}
			}
		}
	}

	if(bInterestingAsset)
	{
		RefreshViewport();
		ReinitMode();
		
		OnPersonaRefresh.Broadcast();
	}
}

/** Called when graph editor focus is changed */
void FPersona::OnGraphEditorFocused(const TSharedRef<class SGraphEditor>& InGraphEditor)
{
	// in the future, depending on which graph editor is this will act different
	FBlueprintEditor::OnGraphEditorFocused(InGraphEditor);
}

/** Create Default Tabs **/
void FPersona::CreateDefaultCommands() 
{
	if (GetBlueprintObj())
	{
		FBlueprintEditor::CreateDefaultCommands();
	}
	else
	{
		ToolkitCommands->MapAction( FGenericCommands::Get().Undo, 
			FExecuteAction::CreateSP( this, &FPersona::UndoAction ));
		ToolkitCommands->MapAction( FGenericCommands::Get().Redo, 
			FExecuteAction::CreateSP( this, &FPersona::RedoAction ));
	}

	// now add default commands
	FPersonaCommands::Register();

	// save all animation assets
	ToolkitCommands->MapAction(FPersonaCommands::Get().SaveAnimationAssets,
		FExecuteAction::CreateSP(this, &FPersona::SaveAnimationAssets_Execute),
		FCanExecuteAction::CreateSP(this, &FPersona::CanSaveAnimationAssets)
		);

	// record animation
	ToolkitCommands->MapAction( FPersonaCommands::Get().RecordAnimation,
		FExecuteAction::CreateSP( this, &FPersona::RecordAnimation ),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP( this, &FPersona::IsRecordAvailable )
		);

	ToolkitCommands->MapAction(FPersonaCommands::Get().ApplyCompression,
		FExecuteAction::CreateSP(this, &FPersona::OnApplyCompression),
		FCanExecuteAction::CreateSP(this, &FPersona::HasValidAnimationSequencePlaying),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FPersona::IsInPersonaMode, FPersonaModes::AnimationEditMode)
		);

	ToolkitCommands->MapAction(FPersonaCommands::Get().SetKey,
		FExecuteAction::CreateSP(this, &FPersona::OnSetKey),
		FCanExecuteAction::CreateSP(this, &FPersona::CanSetKey),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FPersona::IsInPersonaMode, FPersonaModes::AnimationEditMode)
		);

	ToolkitCommands->MapAction(FPersonaCommands::Get().ApplyAnimation,
			FExecuteAction::CreateSP(this, &FPersona::OnApplyRawAnimChanges),
			FCanExecuteAction::CreateSP(this, &FPersona::CanApplyRawAnimChanges),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateSP(this, &FPersona::IsInPersonaMode, FPersonaModes::AnimationEditMode)
			);

	ToolkitCommands->MapAction(FPersonaCommands::Get().ExportToFBX,
		FExecuteAction::CreateSP(this, &FPersona::OnExportToFBX),
		FCanExecuteAction::CreateSP(this, &FPersona::HasValidAnimationSequencePlaying),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FPersona::IsInPersonaMode, FPersonaModes::AnimationEditMode)
		);

	ToolkitCommands->MapAction(FPersonaCommands::Get().AddLoopingInterpolation,
		FExecuteAction::CreateSP(this, &FPersona::OnAddLoopingInterpolation),
		FCanExecuteAction::CreateSP(this, &FPersona::HasValidAnimationSequencePlaying),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FPersona::IsInPersonaMode, FPersonaModes::AnimationEditMode)
		);

	ToolkitCommands->MapAction( FPersonaCommands::Get().ChangeSkeletonPreviewMesh,
		FExecuteAction::CreateSP( this, &FPersona::ChangeSkeletonPreviewMesh ),
		FCanExecuteAction::CreateSP( this, &FPersona::CanChangeSkeletonPreviewMesh )
		);

	ToolkitCommands->MapAction(FPersonaCommands::Get().ToggleReferencePose,
	FExecuteAction::CreateSP(this, &FPersona::ShowReferencePose, true),
	FCanExecuteAction::CreateSP(this, &FPersona::CanShowReferencePose),
	FIsActionChecked::CreateSP(this, &FPersona::IsShowReferencePoseEnabled)
	);

	ToolkitCommands->MapAction(FPersonaCommands::Get().TogglePreviewAsset,
	FExecuteAction::CreateSP(this, &FPersona::ShowReferencePose, false),
	FCanExecuteAction::CreateSP(this, &FPersona::CanPreviewAsset),
	FIsActionChecked::CreateSP(this, &FPersona::IsPreviewAssetEnabled)
	);

	ToolkitCommands->MapAction(FPersonaCommands::Get().TogglePlay,
		FExecuteAction::CreateSP(this, &FPersona::TogglePlayback)
	);

	ToolkitCommands->MapAction( FPersonaCommands::Get().RemoveUnusedBones,
		FExecuteAction::CreateSP( this, &FPersona::RemoveUnusedBones ),
		FCanExecuteAction::CreateSP( this, &FPersona::CanRemoveBones )
		);
	ToolkitCommands->MapAction(FPersonaCommands::Get().TestSkeletonCurveNamesForUse,
		FExecuteAction::CreateSP(this, &FPersona::TestSkeletonCurveNamesForUse),
		FCanExecuteAction()
		);
	ToolkitCommands->MapAction(FPersonaCommands::Get().UpdateSkeletonRefPose,
		FExecuteAction::CreateSP(this, &FPersona::UpdateSkeletonRefPose)
		);

	ToolkitCommands->MapAction(FPersonaCommands::Get().AnimNotifyWindow,
		FExecuteAction::CreateSP(this, &FPersona::OnAnimNotifyWindow),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FPersona::IsInPersonaMode, FPersonaModes::SkeletonDisplayMode)
		);

	ToolkitCommands->MapAction(FPersonaCommands::Get().RetargetManager,
		FExecuteAction::CreateSP(this, &FPersona::OnRetargetManager),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FPersona::IsInPersonaMode, FPersonaModes::SkeletonDisplayMode)
		);

	ToolkitCommands->MapAction(FPersonaCommands::Get().ImportMesh,
		FExecuteAction::CreateSP(this, &FPersona::OnImportAsset, FBXIT_SkeletalMesh),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FPersona::IsInPersonaMode, FPersonaModes::SkeletonDisplayMode)
		);

	ToolkitCommands->MapAction(FPersonaCommands::Get().ReimportMesh,
		FExecuteAction::CreateSP(this, &FPersona::OnReimportMesh),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FPersona::IsInPersonaMode, FPersonaModes::MeshEditMode)
		);

// 	ToolkitCommands->MapAction(FPersonaCommands::Get().ImportLODs,
// 		FExecuteAction::CreateSP(this, &FPersona::OnImportLODs),
// 		FCanExecuteAction(),
// 		FIsActionChecked(),
// 		FIsActionButtonVisible::CreateSP(this, &FPersona::IsInPersonaMode, FPersonaModes::MeshEditMode)
// 		);

// 	ToolkitCommands->MapAction(FPersonaCommands::Get().AddBodyPart,
// 		FExecuteAction::CreateSP(this, &FPersona::OnAddBodyPart),
// 		FCanExecuteAction(),
// 		FIsActionChecked(),
// 		FIsActionButtonVisible::CreateSP(this, &FPersona::IsInPersonaMode, FPersonaModes::MeshEditMode)
// 		);


	// animation menu options
	// import animation
	ToolkitCommands->MapAction(FPersonaCommands::Get().ImportAnimation,
		FExecuteAction::CreateSP(this, &FPersona::OnImportAsset, FBXIT_Animation),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FPersona::IsInPersonaMode, FPersonaModes::AnimationEditMode)
		);

	ToolkitCommands->MapAction(FPersonaCommands::Get().ReimportAnimation,
		FExecuteAction::CreateSP(this, &FPersona::OnReimportAnimation),
		FCanExecuteAction::CreateSP(this, &FPersona::HasValidAnimationSequencePlaying),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FPersona::IsInPersonaMode, FPersonaModes::AnimationEditMode)
		);
}

void FPersona::OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList)
{
	GraphEditorCommandsList->MapAction(FAnimGraphCommands::Get().TogglePoseWatch,
		FExecuteAction::CreateSP(this, &FPersona::OnTogglePoseWatch));
}

bool FPersona::CanSelectBone() const
{
	return true;
}

void FPersona::OnSelectBone()
{
	//@TODO: A2REMOVAL: This doesn't do anything 
}

void FPersona::OnAddPosePin()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UObject* Node = *NodeIt;

			if (UAnimGraphNode_BlendListByInt* BlendNode = Cast<UAnimGraphNode_BlendListByInt>(Node))
			{
				BlendNode->AddPinToBlendList();
				break;
			}
			else if (UAnimGraphNode_LayeredBoneBlend* FilterNode = Cast<UAnimGraphNode_LayeredBoneBlend>(Node))
			{
				FilterNode->AddPinToBlendByFilter();
				break;
			}
		}
	}
}

bool FPersona::CanAddPosePin() const
{
	return true;
}

void FPersona::OnRemovePosePin()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	UAnimGraphNode_BlendListByInt* BlendListIntNode = NULL;
	UAnimGraphNode_LayeredBoneBlend* BlendByFilterNode = NULL;

	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			if (UAnimGraphNode_BlendListByInt* BlendNode = Cast<UAnimGraphNode_BlendListByInt>(*NodeIt))
			{
				BlendListIntNode = BlendNode;
				break;
			}
			else if (UAnimGraphNode_LayeredBoneBlend* LayeredBlendNode = Cast<UAnimGraphNode_LayeredBoneBlend>(*NodeIt))
			{
				BlendByFilterNode = LayeredBlendNode;
				break;
			}		
		}
	}


	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		// @fixme: I think we can make blendlistbase to have common functionality
		// and each can implement the common function, but for now, we separate them
		// each implement their menu, so we still can use listbase as the root
		if (BlendListIntNode)
		{
			// make sure we at least have BlendListNode selected
			UEdGraphPin* SelectedPin = FocusedGraphEd->GetGraphPinForMenu();

			BlendListIntNode->RemovePinFromBlendList(SelectedPin);

			// Update the graph so that the node will be refreshed
			FocusedGraphEd->NotifyGraphChanged();
		}

		if (BlendByFilterNode)
		{
			// make sure we at least have BlendListNode selected
			UEdGraphPin* SelectedPin = FocusedGraphEd->GetGraphPinForMenu();

			BlendByFilterNode->RemovePinFromBlendByFilter(SelectedPin);

			// Update the graph so that the node will be refreshed
			FocusedGraphEd->NotifyGraphChanged();
		}
	}
}

void FPersona::OnConvertToSequenceEvaluator()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_SequencePlayer* OldNode = Cast<UAnimGraphNode_SequencePlayer>(*NodeIter);

			// see if sequence player
			if ( OldNode && OldNode->Node.Sequence )
			{
				//const FScopedTransaction Transaction( LOCTEXT("ConvertToSequenceEvaluator", "Convert to Single Frame Animation") );

				// convert to sequence evaluator
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new evaluator
				FGraphNodeCreator<UAnimGraphNode_SequenceEvaluator> NodeCreator(*TargetGraph);
				UAnimGraphNode_SequenceEvaluator* NewNode = NodeCreator.CreateNode();
				NewNode->Node.Sequence = OldNode->Node.Sequence;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("Pose"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->CopyPersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}

void FPersona::OnConvertToSequencePlayer()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_SequenceEvaluator* OldNode = Cast<UAnimGraphNode_SequenceEvaluator>(*NodeIter);

			// see if sequence player
			if ( OldNode && OldNode->Node.Sequence )
			{
				//const FScopedTransaction Transaction( LOCTEXT("ConvertToSequenceEvaluator", "Convert to Single Frame Animation") );
				// convert to sequence player
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new player
				FGraphNodeCreator<UAnimGraphNode_SequencePlayer> NodeCreator(*TargetGraph);
				UAnimGraphNode_SequencePlayer* NewNode = NodeCreator.CreateNode();
				NewNode->Node.Sequence = OldNode->Node.Sequence;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("Pose"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->CopyPersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}

void FPersona::OnConvertToBlendSpaceEvaluator() 
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_BlendSpacePlayer* OldNode = Cast<UAnimGraphNode_BlendSpacePlayer>(*NodeIter);

			// see if sequence player
			if ( OldNode && OldNode->Node.BlendSpace )
			{
				//const FScopedTransaction Transaction( LOCTEXT("ConvertToSequenceEvaluator", "Convert to Single Frame Animation") );

				// convert to sequence evaluator
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new evaluator
				FGraphNodeCreator<UAnimGraphNode_BlendSpaceEvaluator> NodeCreator(*TargetGraph);
				UAnimGraphNode_BlendSpaceEvaluator* NewNode = NodeCreator.CreateNode();
				NewNode->Node.BlendSpace = OldNode->Node.BlendSpace;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("X"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("X"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->CopyPersistentDataFromOldPin(*OldPosePin);
				}

				OldPosePin = OldNode->FindPin(TEXT("Y"));
				NewPosePin = NewNode->FindPin(TEXT("Y"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->CopyPersistentDataFromOldPin(*OldPosePin);
				}


				OldPosePin = OldNode->FindPin(TEXT("Pose"));
				NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->CopyPersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}
void FPersona::OnConvertToBlendSpacePlayer() 
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_BlendSpaceEvaluator* OldNode = Cast<UAnimGraphNode_BlendSpaceEvaluator>(*NodeIter);

			// see if sequence player
			if ( OldNode && OldNode->Node.BlendSpace )
			{
				//const FScopedTransaction Transaction( LOCTEXT("ConvertToSequenceEvaluator", "Convert to Single Frame Animation") );
				// convert to sequence player
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new player
				FGraphNodeCreator<UAnimGraphNode_BlendSpacePlayer> NodeCreator(*TargetGraph);
				UAnimGraphNode_BlendSpacePlayer* NewNode = NodeCreator.CreateNode();
				NewNode->Node.BlendSpace = OldNode->Node.BlendSpace;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("X"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("X"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->CopyPersistentDataFromOldPin(*OldPosePin);
				}

				OldPosePin = OldNode->FindPin(TEXT("Y"));
				NewPosePin = NewNode->FindPin(TEXT("Y"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->CopyPersistentDataFromOldPin(*OldPosePin);
				}


				OldPosePin = OldNode->FindPin(TEXT("Pose"));
				NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->CopyPersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}

void FPersona::OnConvertToPoseBlender()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_PoseByName* OldNode = Cast<UAnimGraphNode_PoseByName>(*NodeIter);

			// see if sequence player
			if (OldNode && OldNode->Node.PoseAsset)
			{
				// convert to sequence player
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new player
				FGraphNodeCreator<UAnimGraphNode_PoseBlendNode> NodeCreator(*TargetGraph);
				UAnimGraphNode_PoseBlendNode* NewNode = NodeCreator.CreateNode();
				NewNode->Node.PoseAsset = OldNode->Node.PoseAsset;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("Pose"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->CopyPersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}

void FPersona::OnConvertToPoseByName()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UAnimGraphNode_PoseBlendNode* OldNode = Cast<UAnimGraphNode_PoseBlendNode>(*NodeIter);

			// see if sequence player
			if (OldNode && OldNode->Node.PoseAsset)
			{
				//const FScopedTransaction Transaction( LOCTEXT("ConvertToSequenceEvaluator", "Convert to Single Frame Animation") );
				// convert to sequence player
				UEdGraph* TargetGraph = OldNode->GetGraph();
				// create new player
				FGraphNodeCreator<UAnimGraphNode_PoseByName> NodeCreator(*TargetGraph);
				UAnimGraphNode_PoseByName* NewNode = NodeCreator.CreateNode();
				NewNode->Node.PoseAsset = OldNode->Node.PoseAsset;
				NodeCreator.Finalize();

				// get default data from old node to new node
				FEdGraphUtilities::CopyCommonState(OldNode, NewNode);

				UEdGraphPin* OldPosePin = OldNode->FindPin(TEXT("Pose"));
				UEdGraphPin* NewPosePin = NewNode->FindPin(TEXT("Pose"));

				if (ensure(OldPosePin && NewPosePin))
				{
					NewPosePin->CopyPersistentDataFromOldPin(*OldPosePin);
				}

				// remove from selection and from graph
				NodeIter.RemoveCurrent();
				TargetGraph->RemoveNode(OldNode);

				NewNode->Modify();
			}
		}

		// @todo fixme: below code doesn't work
		// because of SetAndCenterObject kicks in after new node is added
		// will need to disable that first
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
		// It's possible to leave invalid objects in the selection set if they get GC'd, so clear it out
		FocusedGraphEd->ClearSelectionSet();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}

void FPersona::OnTogglePoseWatch()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	UAnimBlueprint* AnimBP = GetAnimBlueprint();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		if (UAnimGraphNode_Base* SelectedNode = Cast<UAnimGraphNode_Base>(*NodeIt))
		{
			UPoseWatch* PoseWatch = AnimationEditorUtils::FindPoseWatchForNode(SelectedNode, AnimBP);
			if (PoseWatch)
			{
				AnimationEditorUtils::RemovePoseWatch(PoseWatch, AnimBP);
			}
			else
			{
				AnimationEditorUtils::MakePoseWatchForNode(AnimBP, SelectedNode, FColor::Red);
			}
		}
	}
}

void FPersona::OnOpenRelatedAsset()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	
	EToolkitMode::Type Mode = EToolkitMode::Standalone;
	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>( "Persona" );

	if (SelectedNodes.Num() > 0)
	{
		for (auto NodeIter = SelectedNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			if(UAnimGraphNode_Base* Node = Cast<UAnimGraphNode_Base>(*NodeIter))
			{
				UAnimationAsset* AnimAsset = Node->GetAnimationAsset();
				
				if(AnimAsset)
				{
					PersonaModule.CreatePersona( Mode, TSharedPtr<IToolkitHost>(), AnimAsset->GetSkeleton(), NULL, AnimAsset, NULL );
				}
			}
		}
	}
}

bool FPersona::CanRemovePosePin() const
{
	return true;
}

void FPersona::RecompileAnimBlueprintIfDirty()
{
	if (UBlueprint* Blueprint = GetBlueprintObj())
	{
		if (!Blueprint->IsUpToDate())
		{
			Compile();
		}
	}
}

void FPersona::Compile()
{
	// Note if we were debugging the preview
	UObject* CurrentDebugObject = GetBlueprintObj()->GetObjectBeingDebugged();
	const bool bIsDebuggingPreview = (PreviewComponent != NULL) && PreviewComponent->IsAnimBlueprintInstanced() && (PreviewComponent->GetAnimInstance() == CurrentDebugObject);

	if (PreviewComponent != NULL)
	{
		// Force close any asset editors that are using the AnimScriptInstance (such as the Property Matrix), the class will be garbage collected
		FAssetEditorManager::Get().CloseOtherEditors(PreviewComponent->GetAnimInstance(), nullptr);
	}

	// Compile the blueprint
	FBlueprintEditor::Compile();

	if (PreviewComponent != NULL)
	{
		if (PreviewComponent->GetAnimInstance() == NULL)
		{
			// try reinitialize animation if it doesn't exist
			PreviewComponent->InitAnim(true);
		}

		if (bIsDebuggingPreview)
		{
			GetBlueprintObj()->SetObjectBeingDebugged(PreviewComponent->GetAnimInstance());
		}
	}

	// calls PostCompile to copy proper values between anim nodes
	if (Viewport.IsValid())
	{
		Viewport.Pin()->GetAnimationViewportClient()->PostCompile();
	}
}

FName FPersona::GetToolkitContextFName() const
{
	return GetToolkitFName();
}

FName FPersona::GetToolkitFName() const
{
	return FName("Persona");
}

FText FPersona::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Persona");
}

FText FPersona::GetToolkitName() const
{
	FFormatNamedArguments Args;

	if (IsEditingSingleBlueprint())
	{
		const bool bDirtyState = GetBlueprintObj()->GetOutermost()->IsDirty();

		Args.Add( TEXT("BlueprintName"), FText::FromString( GetBlueprintObj()->GetName() ) );
		Args.Add( TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty() );
		return FText::Format( LOCTEXT("KismetToolkitName_SingleBlueprint", "{BlueprintName}{DirtyState}"), Args );
	}
	else
	{
		check(TargetSkeleton != NULL);

		const bool bDirtyState = TargetSkeleton->GetOutermost()->IsDirty();

		Args.Add( TEXT("SkeletonName"), FText::FromString( TargetSkeleton->GetName() ) );
		Args.Add( TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty() );
		return FText::Format( LOCTEXT("KismetToolkitName_SingleSkeleton", "{SkeletonName}{DirtyState}"), Args );
	}
}

FText FPersona::GetToolkitToolTipText() const
{
	if (IsEditingSingleBlueprint())
	{
		return FAssetEditorToolkit::GetToolTipTextForObject(GetBlueprintObj());
	}
	else
	{
		check(TargetSkeleton != NULL);
		return FAssetEditorToolkit::GetToolTipTextForObject(TargetSkeleton);
	}
}

FString FPersona::GetWorldCentricTabPrefix() const
{
	return NSLOCTEXT("Persona", "WorldCentricTabPrefix", "Persona ").ToString();
}


FLinearColor FPersona::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.5f, 0.25f, 0.35f, 0.5f );
}

void FPersona::SaveAnimationAssets_Execute()
{
	// only save animation assets related to skeletons
	TArray<UClass*> SaveClasses;
	SaveClasses.Add(UAnimationAsset::StaticClass());
	SaveClasses.Add(UAnimBlueprint::StaticClass());
	SaveClasses.Add(USkeletalMesh::StaticClass());
	SaveClasses.Add(USkeleton::StaticClass());

	const bool bPromptUserToSave = true;
	const bool bFastSave = false;
	FEditorFileUtils::SaveDirtyContentPackages(SaveClasses, bPromptUserToSave, bFastSave);
}

bool FPersona::CanSaveAnimationAssets() const
{
	return true;
}

void FPersona::OnActiveTabChanged( TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated )
{
	if (!NewlyActivated.IsValid())
	{
		TArray<UObject*> ObjArray;
		Inspector->ShowDetailsForObjects(ObjArray);
	}
	else if (SharedAnimDocumentTab.IsValid() && NewlyActivated.Get() == SharedAnimDocumentTab.Pin().Get())
	{
		if (UAnimationAsset* SharedAsset = Cast<UAnimationAsset>(SharedAnimAssetBeingEdited.Get()))
		{
			SetPreviewAnimationAsset(SharedAsset);
		}
	}
	else if (SharedAnimDocumentTab.IsValid() && PreviouslyActive.Get() == SharedAnimDocumentTab.Pin().Get())
	{
		//@TODO: The flash focus makes it impossible to do this and still edit montages
		//SetDetailObject(NULL);
	}
	else 
	{
		FBlueprintEditor::OnActiveTabChanged(PreviouslyActive, NewlyActivated);
	}
}

void FPersona::SetPreviewMeshInternal(USkeletalMesh* NewPreviewMesh)
{
	ValidatePreviewAttachedAssets(NewPreviewMesh);
	if(NewPreviewMesh != PreviewComponent->SkeletalMesh)
	{
		if(PreviewComponent->SkeletalMesh != NULL)
		{
			RemoveEditingObject(PreviewComponent->SkeletalMesh);
		}

		if(NewPreviewMesh != NULL)
		{
			AddEditingObject(NewPreviewMesh);
		}

		// setting skeletalmesh unregister/re-register, 
		// so I have to save the animation settings and resetting after setting mesh
		UAnimationAsset* AnimAssetToPlay = NULL;
		float PlayPosition = 0.f;
		bool bPlaying = false;
		bool bNeedsToCopyAnimationData = PreviewComponent->GetAnimInstance() && PreviewComponent->GetAnimInstance() == PreviewComponent->PreviewInstance;
		if(bNeedsToCopyAnimationData)
		{
			AnimAssetToPlay = PreviewComponent->PreviewInstance->GetCurrentAsset();
			PlayPosition = PreviewComponent->PreviewInstance->GetCurrentTime();
			bPlaying = PreviewComponent->PreviewInstance->IsPlaying();
		}

		PreviewComponent->SetSkeletalMesh(NewPreviewMesh);

		if(bNeedsToCopyAnimationData)
		{
			SetPreviewAnimationAsset(AnimAssetToPlay);
			PreviewComponent->PreviewInstance->SetPosition(PlayPosition);
			PreviewComponent->PreviewInstance->SetPlaying(bPlaying);
		}
	}
	else
	{
		PreviewComponent->InitAnim(true);
	}

	if(NewPreviewMesh != NULL)
	{
		PreviewScene.AddComponent(PreviewComponent, FTransform::Identity);
		for(auto Iter = AdditionalMeshes.CreateIterator(); Iter; ++Iter)
		{
			PreviewScene.AddComponent((*Iter), FTransform::Identity);
		}

		// Set up the mesh for transactions
		NewPreviewMesh->SetFlags(RF_Transactional);

		AddPreviewAttachedObjects();

		if(Viewport.IsValid())
		{
			Viewport.Pin()->SetPreviewComponent(PreviewComponent);
		}
	}

	for(auto Iter = AdditionalMeshes.CreateIterator(); Iter; ++Iter)
	{
		(*Iter)->SetMasterPoseComponent(PreviewComponent);
		(*Iter)->UpdateMasterBoneMap();
	}

	OnPreviewMeshChanged.Broadcast(NewPreviewMesh);
}

// Sets the current preview mesh
void FPersona::SetPreviewMesh(USkeletalMesh* NewPreviewMesh)
{
	if(!TargetSkeleton->IsCompatibleMesh(NewPreviewMesh))
	{
		// message box, ask if they'd like to regenerate skeleton
		if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("RenerateSkeleton", "The preview mesh hierarchy doesn't match with Skeleton anymore. Would you like to regenerate skeleton?")) == EAppReturnType::Yes)
		{
			TargetSkeleton->RecreateBoneTree( NewPreviewMesh );
			SetPreviewMeshInternal(NewPreviewMesh);
		}
		else
		{
			// Send a notification that the skeletal mesh cannot work with the skeleton
			FFormatNamedArguments Args;
			Args.Add(TEXT("PreviewMeshName"), FText::FromString(NewPreviewMesh->GetName()));
			Args.Add(TEXT("TargetSkeletonName"), FText::FromString(TargetSkeleton->GetName()));
			FNotificationInfo Info(FText::Format(LOCTEXT("SkeletalMeshIncompatible", "Skeletal Mesh \"{PreviewMeshName}\" incompatible with Skeleton \"{TargetSkeletonName}\""), Args));
			Info.ExpireDuration = 3.0f;
			Info.bUseLargeFont = false;
			TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if(Notification.IsValid())
			{
				Notification->SetCompletionState(SNotificationItem::CS_Fail);
			}
		}
	}
	else
	{
		SetPreviewMeshInternal(NewPreviewMesh);
	}
}

void FPersona::OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	// Re-initialize the preview when a skeletal control is being edited
	//@TODO: Should we still do this?
}

void FPersona::RefreshPreviewInstanceTrackCurves()
{
	// need to refresh the preview mesh
	if(PreviewComponent->PreviewInstance)
	{
		PreviewComponent->PreviewInstance->RefreshCurveBoneControllers();
	}
}

void FPersona::PostUndo(bool bSuccess)
{
	DocumentManager->CleanInvalidTabs();
	DocumentManager->RefreshAllTabs();

	FBlueprintEditor::PostUndo(bSuccess);

	// If we undid a node creation that caused us to clean up a tab/graph we need to refresh the UI state
	RefreshEditors();

	// PostUndo broadcast
	OnPostUndo.Broadcast();	

	// Re-init preview instance
	if (PreviewComponent && PreviewComponent->AnimScriptInstance)
	{
		PreviewComponent->AnimScriptInstance->InitializeAnimation();
	}

	RefreshPreviewInstanceTrackCurves();

	// clear up preview anim notify states
	// animnotify states are saved in AnimInstance
	// if those are undoed or redoed, they have to be 
	// cleared up, otherwise, they might have invalid data
	ClearupPreviewMeshAnimNotifyStates();
}

void FPersona::ClearupPreviewMeshAnimNotifyStates()
{
	USkeletalMeshComponent* PreviewMeshComponent = GetPreviewMeshComponent();
	if ( PreviewMeshComponent )
	{
		UAnimInstance* AnimInstantace = PreviewMeshComponent->GetAnimInstance();

		if (AnimInstantace)
		{
			// empty this because otherwise, it can have corrupted data
			// this will cause state to be interrupted, but that is better
			// than crashing
			AnimInstantace->ActiveAnimNotifyState.Empty();
		}
	}
}

void FPersona::OnSkeletonHierarchyChanged()
{
	OnSkeletonTreeChanged.Broadcast();
}

bool FPersona::IsInAScriptingMode() const
{
	return IsModeCurrent(FPersonaModes::AnimBlueprintEditMode);
}

void FPersona::GetCustomDebugObjects(TArray<FCustomDebugObject>& DebugList) const
{
	if (PreviewComponent->IsAnimBlueprintInstanced())
	{
		new (DebugList) FCustomDebugObject(PreviewComponent->GetAnimInstance(), LOCTEXT("PreviewObjectLabel", "Preview Instance").ToString());
	}
}

void FPersona::CreateDefaultTabContents(const TArray<UBlueprint*>& InBlueprints)
{
	FBlueprintEditor::CreateDefaultTabContents(InBlueprints);

	PreviewEditor = SNew(SPersonaPreviewPropertyEditor, SharedThis(this));
}

FGraphAppearanceInfo FPersona::GetGraphAppearance(UEdGraph* InGraph) const
{
	FGraphAppearanceInfo AppearanceInfo = FBlueprintEditor::GetGraphAppearance(InGraph);

	if ( GetBlueprintObj()->IsA(UAnimBlueprint::StaticClass()) )
	{
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Animation", "ANIMATION");
	}

	return AppearanceInfo;
}

void FPersona::FocusWindow(UObject* ObjectToFocusOn)
{
	FBlueprintEditor::FocusWindow(ObjectToFocusOn);
	if (ObjectToFocusOn != NULL)
	{
		// we should be already in our desired window, now just update mode
		if (ObjectToFocusOn->IsA(UAnimBlueprint::StaticClass()))
		{
			SetCurrentMode(FPersonaModes::AnimBlueprintEditMode);
		}
		else if (UAnimationAsset* AnimationAssetToFocus = Cast<UAnimationAsset>(ObjectToFocusOn))
		{
			SetCurrentMode(FPersonaModes::AnimationEditMode);
			OpenNewDocumentTab(AnimationAssetToFocus);
		}
		else if (USkeletalMesh* SkeletalMeshToFocus = Cast<USkeletalMesh>(ObjectToFocusOn))
		{
			SetCurrentMode(FPersonaModes::MeshEditMode);
			SetPreviewMesh(SkeletalMeshToFocus);
		}
		else if (ObjectToFocusOn->IsA(USkeleton::StaticClass()))
		{
			SetCurrentMode(FPersonaModes::SkeletonDisplayMode);
		}
	}
}

void FPersona::ClearSelectedBones()
{
	if (UDebugSkelMeshComponent* Preview = GetPreviewMeshComponent())
	{
		Preview->BonesOfInterest.Empty();
	}
}

void FPersona::SetSelectedBone(USkeleton* InTargetSkeleton, const FName& BoneName, bool bRebroadcast /* = true */ )
{
	const int32 BoneIndex = InTargetSkeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
	if (BoneIndex != INDEX_NONE)
	{
		if (UDebugSkelMeshComponent* Preview = GetPreviewMeshComponent())
		{
			Preview->BonesOfInterest.Empty();
			ClearSelectedSocket();

			// Add in bone of interest only if we have a preview instance set-up
			if (Preview->PreviewInstance != NULL)
			{
				// need to get mesh bone base since BonesOfInterest is saved in SkeletalMeshComponent
				// and it is used by renderer. It is not Skeleton base
				const int32 MeshBoneIndex = Preview->GetBoneIndex(BoneName);

				if (MeshBoneIndex != INDEX_NONE)
				{
					Preview->BonesOfInterest.Add(MeshBoneIndex);
				}

				if ( bRebroadcast )
				{
					// Broadcast that a bone has been selected
					OnBoneSelected.Broadcast( BoneName );
				}

				if( Viewport.IsValid() )
				{
					Viewport.Pin()->GetLevelViewportClient().Invalidate();
				}
			}
		}
	}
}

void FPersona::SetSelectedSocket( const FSelectedSocketInfo& SocketInfo, bool bRebroadcast /* = true */ )
{
	if ( UDebugSkelMeshComponent* Preview = GetPreviewMeshComponent() )
	{
		Preview->SocketsOfInterest.Empty();
		Preview->BonesOfInterest.Empty();

		Preview->SocketsOfInterest.Add( SocketInfo );

		if ( bRebroadcast )
		{
			OnSocketSelected.Broadcast( SocketInfo );
		}

		// This populates the details panel with the information for the socket
		SetDetailObject( SocketInfo.Socket );

		if( Viewport.IsValid() )
		{
			Viewport.Pin()->GetLevelViewportClient().Invalidate();
		}
	}
}

void FPersona::ClearSelectedSocket()
{
	if (UDebugSkelMeshComponent* Preview = GetPreviewMeshComponent())
	{
		Preview->SocketsOfInterest.Empty();
		Preview->BonesOfInterest.Empty();

		SetDetailObject( NULL );
	}
}

void FPersona::ClearSelectedWindActor()
{
	if (Viewport.IsValid())
	{
		Viewport.Pin()->GetAnimationViewportClient()->ClearSelectedWindActor();
	}
}

void FPersona::ClearSelectedAnimGraphNode()
{
	if(Viewport.IsValid())
	{
		Viewport.Pin()->GetAnimationViewportClient()->ClearSelectedAnimGraphNode();
	}
}

void FPersona::RenameSocket( USkeletalMeshSocket* Socket, const FName& NewSocketName )
{
	Socket->SocketName = NewSocketName;

	// Broadcast that the skeleton tree has changed due to the new name
	OnSkeletonTreeChanged.Broadcast();

}

void FPersona::ChangeSocketParent( USkeletalMeshSocket* Socket, const FName& NewSocketParent )
{
	// Update the bone name.
	Socket->BoneName = NewSocketParent;

	// Broadcast that the skeleton tree has changed due to the new parent
	OnSkeletonTreeChanged.Broadcast();
}

void FPersona::DuplicateAndSelectSocket( const FSelectedSocketInfo& SocketInfoToDuplicate, const FName& NewParentBoneName /*= FName()*/ )
{
	check( SocketInfoToDuplicate.Socket );

	USkeletalMesh* Mesh = GetMesh();

	const FScopedTransaction Transaction( LOCTEXT( "CopySocket", "Copy Socket" ) );

	USkeletalMeshSocket* NewSocket;

	bool bModifiedSkeleton = false;

	if ( SocketInfoToDuplicate.bSocketIsOnSkeleton )
	{
		TargetSkeleton->Modify();
		bModifiedSkeleton = true;

		NewSocket = NewObject<USkeletalMeshSocket>(TargetSkeleton);
		check(NewSocket);
	}
	else if ( Mesh )
	{
		Mesh->Modify();

		NewSocket = NewObject<USkeletalMeshSocket>(Mesh);
		check(NewSocket);
	}
	else 
	{
		// Original socket was on the mesh, but we have no mesh. Huh?
		check( 0 );
		return;
	}

	NewSocket->SocketName = GenerateUniqueSocketName( SocketInfoToDuplicate.Socket->SocketName );
	NewSocket->BoneName = NewParentBoneName != NAME_None ? NewParentBoneName : SocketInfoToDuplicate.Socket->BoneName;
	NewSocket->RelativeLocation = SocketInfoToDuplicate.Socket->RelativeLocation;
	NewSocket->RelativeRotation = SocketInfoToDuplicate.Socket->RelativeRotation;
	NewSocket->RelativeScale = SocketInfoToDuplicate.Socket->RelativeScale;

	if ( SocketInfoToDuplicate.bSocketIsOnSkeleton )
	{
		TargetSkeleton->Sockets.Add( NewSocket );
	}
	else if ( Mesh )
	{
		Mesh->GetMeshOnlySocketList().Add( NewSocket );
	}

	// Duplicated attached assets
	int32 NumExistingAttachedObjects = TargetSkeleton->PreviewAttachedAssetContainer.Num();
	for(int AttachedObjectIndex = 0; AttachedObjectIndex < NumExistingAttachedObjects; ++AttachedObjectIndex)
	{
		FPreviewAttachedObjectPair& Pair = TargetSkeleton->PreviewAttachedAssetContainer[AttachedObjectIndex];
		if(Pair.AttachedTo == SocketInfoToDuplicate.Socket->SocketName)
		{
			if(!bModifiedSkeleton)
			{
				bModifiedSkeleton = true;
				TargetSkeleton->Modify();
			}
			FPreviewAttachedObjectPair NewPair = Pair;
			NewPair.AttachedTo = NewSocket->SocketName;
			AttachObjectToPreviewComponent( NewPair.GetAttachedObject(), NewPair.AttachedTo, &TargetSkeleton->PreviewAttachedAssetContainer );
		}
	}

	// Broadcast that the skeleton tree has changed due to the new socket
	OnSkeletonTreeChanged.Broadcast();

	SetSelectedSocket( FSelectedSocketInfo( NewSocket, SocketInfoToDuplicate.bSocketIsOnSkeleton ) );
}

FName FPersona::GenerateUniqueSocketName( FName InName )
{
	USkeletalMesh* Mesh = GetMesh();

	int32 NewNumber = InName.GetNumber();

	// Increment NewNumber until we have a unique name (potential infinite loop if *all* int32 values are used!)
	while ( DoesSocketAlreadyExist( NULL, FText::FromName( FName( InName, NewNumber ) ), TargetSkeleton->Sockets ) ||
		( Mesh ? DoesSocketAlreadyExist( NULL, FText::FromName( FName( InName, NewNumber ) ), Mesh->GetMeshOnlySocketList() ) : false ) )
	{
		++NewNumber;
	}

	return FName( InName, NewNumber );
}

bool FPersona::DoesSocketAlreadyExist( const USkeletalMeshSocket* InSocket, const FText& InSocketName, const TArray< USkeletalMeshSocket* >& InSocketArray ) const
{
	for ( auto SocketIt = InSocketArray.CreateConstIterator(); SocketIt; ++SocketIt )
	{
		USkeletalMeshSocket* Socket = *( SocketIt );

		if ( InSocket != Socket && Socket->SocketName.ToString() == InSocketName.ToString() )
		{
			return true;
		}
	}

	return false;
}

USkeletalMeshComponent*	FPersona::CreateNewSkeletalMeshComponent()
{
	USkeletalMeshComponent* NewComp = NewObject<USkeletalMeshComponent>();
	AdditionalMeshes.Add(NewComp);
	NewComp->SetMasterPoseComponent(GetPreviewMeshComponent());
	return NewComp;
}

void FPersona::RemoveAdditionalMesh(USkeletalMeshComponent* MeshToRemove)
{
	PreviewScene.RemoveComponent(MeshToRemove);
	AdditionalMeshes.Remove(MeshToRemove);
}

void FPersona::ClearAllAdditionalMeshes()
{
	for(auto Iter = AdditionalMeshes.CreateIterator(); Iter; ++Iter)
	{
		PreviewScene.RemoveComponent( *Iter );
	}

	AdditionalMeshes.Empty();
}

void FPersona::AddPreviewAttachedObjects()
{
	// Load up mesh attachments...
	USkeletalMesh* Mesh = GetMesh();

	if ( Mesh )
	{
		for(int32 i = 0; i < Mesh->PreviewAttachedAssetContainer.Num(); i++)
		{
			FPreviewAttachedObjectPair& PreviewAttachedObject = Mesh->PreviewAttachedAssetContainer[i];

			AttachObjectToPreviewComponent(PreviewAttachedObject.GetAttachedObject(), PreviewAttachedObject.AttachedTo);
		}
	}

	// ...and then skeleton attachments
	for(int32 i = 0; i < TargetSkeleton->PreviewAttachedAssetContainer.Num(); i++)
	{
		FPreviewAttachedObjectPair& PreviewAttachedObject = TargetSkeleton->PreviewAttachedAssetContainer[i];

		AttachObjectToPreviewComponent(PreviewAttachedObject.GetAttachedObject(), PreviewAttachedObject.AttachedTo);
	}
}

bool FPersona::AttachObjectToPreviewComponent( UObject* Object, FName AttachTo, FPreviewAssetAttachContainer* PreviewAssetContainer /* = NULL */ )
{
	if(GetComponentForAttachedObject(Object, AttachTo))
	{
		return false; // Already have this asset attached at this location
	}

	TSubclassOf<UActorComponent> ComponentClass = FComponentAssetBrokerage::GetPrimaryComponentForAsset(Object->GetClass());
	if(PreviewComponent && *ComponentClass && ComponentClass->IsChildOf(USceneComponent::StaticClass()))
	{
		if ( PreviewAssetContainer )
		{
			// Set up the attached object for transactions
			Object->SetFlags(RF_Transactional);
			PreviewAssetContainer->AddAttachedObject( Object, AttachTo );
		}

		//set up world info for undo
		AWorldSettings* WorldSettings = PreviewScene.GetWorld()->GetWorldSettings();
		WorldSettings->SetFlags(RF_Transactional);
		WorldSettings->Modify();

		USceneComponent* SceneComponent = NewObject<USceneComponent>(WorldSettings, ComponentClass, NAME_None, RF_Transactional);

		FComponentAssetBrokerage::AssignAssetToComponent(SceneComponent, Object);

		if(UParticleSystemComponent* NewPSysComp = Cast<UParticleSystemComponent>(SceneComponent))
		{
			//maybe this should happen in FComponentAssetBrokerage::AssignAssetToComponent?
			NewPSysComp->SetTickGroup(TG_PostUpdateWork);
		}

		//set up preview component for undo
		PreviewComponent->SetFlags(RF_Transactional);
		PreviewComponent->Modify();

		// Attach component to the preview component
		SceneComponent->SetupAttachment(PreviewComponent, AttachTo);
		SceneComponent->RegisterComponent();
		return true;
	}
	return false;
}

void FPersona::RemoveAttachedObjectFromPreviewComponent(UObject* Object, FName AttachedTo)
{
	// clean up components	
	if (PreviewComponent)
	{
		AWorldSettings* WorldSettings = PreviewScene.GetWorld()->GetWorldSettings();
		WorldSettings->SetFlags(RF_Transactional);
		WorldSettings->Modify();

		//set up preview component for undo
		PreviewComponent->SetFlags(RF_Transactional);
		PreviewComponent->Modify();

		for (int32 I=PreviewComponent->GetAttachChildren().Num()-1; I >= 0; --I) // Iterate backwards because CleanupComponent will remove from AttachChildren
		{
			USceneComponent* ChildComponent = PreviewComponent->GetAttachChildren()[I];
			UObject* Asset = FComponentAssetBrokerage::GetAssetFromComponent(ChildComponent);

			if( Asset == Object && ChildComponent->GetAttachSocketName() == AttachedTo)
			{
				// PreviewComponet will be cleaned up by PreviewScene, 
				// but if anything is attached, it won't be cleaned up, 
				// so we'll need to clean them up manually
				CleanupComponent(PreviewComponent->GetAttachChildren()[I]);
				break;
			}
		}
	}
}

USceneComponent* FPersona::GetComponentForAttachedObject(UObject* Object, FName AttachedTo)
{
	if (PreviewComponent)
	{
		for (USceneComponent* ChildComponent : PreviewComponent->GetAttachChildren())
		{
			UObject* Asset = FComponentAssetBrokerage::GetAssetFromComponent(ChildComponent);

			if( Asset == Object && ChildComponent->GetAttachSocketName() == AttachedTo)
			{
				return ChildComponent;
			}
		}
	}
	return NULL;
}

void FPersona::RemoveAttachedComponent( bool bRemovePreviewAttached /* = true */ )
{
	TMap<UObject*, TArray<FName>> PreviewAttachedObjects;

	if( !bRemovePreviewAttached )
	{
		for(auto Iter = TargetSkeleton->PreviewAttachedAssetContainer.CreateConstIterator(); Iter; ++Iter)
		{
			PreviewAttachedObjects.FindOrAdd(Iter->GetAttachedObject()).Add(Iter->AttachedTo);
		}

		if ( USkeletalMesh* PreviewMesh = GetMesh() )
		{
			for(auto Iter = PreviewMesh->PreviewAttachedAssetContainer.CreateConstIterator(); Iter; ++Iter)
			{
				PreviewAttachedObjects.FindOrAdd(Iter->GetAttachedObject()).Add(Iter->AttachedTo);
			}
		}
	}

	// clean up components	
	if (PreviewComponent)
	{
		for (int32 I=PreviewComponent->GetAttachChildren().Num()-1; I >= 0; --I) // Iterate backwards because CleanupComponent will remove from AttachChildren
		{
			USceneComponent* ChildComponent = PreviewComponent->GetAttachChildren()[I];
			UObject* Asset = FComponentAssetBrokerage::GetAssetFromComponent(ChildComponent);

			bool bRemove = true;

			//are we supposed to leave assets that came from the skeleton
			if(	!bRemovePreviewAttached )
			{
				//could this asset have come from the skeleton
				if(PreviewAttachedObjects.Contains(Asset))
				{
					if(PreviewAttachedObjects.Find(Asset)->Contains(ChildComponent->GetAttachSocketName()))
					{
						bRemove = false;
					}
				}
			}

			if(bRemove)
			{
				// PreviewComponet will be cleaned up by PreviewScene, 
				// but if anything is attached, it won't be cleaned up, 
				// so we'll need to clean them up manually
				CleanupComponent(PreviewComponent->GetAttachChildren()[I]);
			}
		}

		if( bRemovePreviewAttached )
		{
			check(PreviewComponent->GetAttachChildren().Num() == 0);
		}
	}
}

void FPersona::CleanupComponent(USceneComponent* Component)
{
	if (Component)
	{
		for (int32 I = Component->GetAttachChildren().Num() - 1; I >= 0; --I) // Iterate backwards because CleanupComponent will remove from AttachChildren
		{
			CleanupComponent(Component->GetAttachChildren()[I]);
		}

		check(Component->GetAttachChildren().Num() == 0);
		Component->DestroyComponent();
	}
}

void FPersona::DeselectAll()
{
	ClearSelectedBones();
	ClearSelectedSocket();
	ClearSelectedWindActor();
	ClearSelectedAnimGraphNode();

	OnAllDeselected.Broadcast();
}

FReply FPersona::OnClickEditMesh()
{
	USkeletalMesh* PreviewMesh = TargetSkeleton->GetPreviewMesh();
	if(PreviewMesh)
	{
		UpdateSelectionDetails(PreviewMesh, FText::FromString(PreviewMesh->GetName()));
	}
	return FReply::Handled();
}


void FPersona::PostRedo(bool bSuccess)
{
	DocumentManager->RefreshAllTabs();

	FBlueprintEditor::PostRedo(bSuccess);

	// PostUndo broadcast, OnPostRedo
	OnPostUndo.Broadcast();

	if (PreviewComponent && PreviewComponent->AnimScriptInstance)
	{
		PreviewComponent->AnimScriptInstance->InitializeAnimation();
	}

	// clear up preview anim notify states
	// animnotify states are saved in AnimInstance
	// if those are undoed or redoed, they have to be 
	// cleared up, otherwise, they might have invalid data
	ClearupPreviewMeshAnimNotifyStates();
}

TArray<UObject*> FPersona::GetEditorObjectsForMode(FName Mode) const
{
	if( Mode == FPersonaModes::AnimationEditMode )
	{
		return GetEditorObjectsOfClass( GetEditingObjects(), UAnimationAsset::StaticClass() );
	}
	else if( Mode == FPersonaModes::AnimBlueprintEditMode )
	{
		return GetEditorObjectsOfClass( GetEditingObjects(), UAnimBlueprint::StaticClass() );
	}
	else if( Mode == FPersonaModes::MeshEditMode )
	{
		return GetEditorObjectsOfClass( GetEditingObjects(), USkeletalMesh::StaticClass() );
	}
	else if( Mode == FPersonaModes::PhysicsEditMode )
	{
		return GetEditorObjectsOfClass( GetEditingObjects(), UPhysicsAsset::StaticClass() );
	}
	else if( Mode == FPersonaModes::SkeletonDisplayMode )
	{
		return GetEditorObjectsOfClass( GetEditingObjects(), USkeleton::StaticClass() );
	}
	return TArray<UObject*>();
}

void FPersona::GetSaveableObjects(TArray<UObject*>& OutObjects) const
{
	OutObjects = GetEditorObjectsForMode(GetCurrentMode());
}

const FSlateBrush* FPersona::GetDirtyImageForMode(FName Mode) const
{
	TArray<UObject*> EditorObjects = GetEditorObjectsForMode(Mode);
	for(auto Iter = EditorObjects.CreateIterator(); Iter; ++Iter)
	{
		if(UPackage* Package = (*Iter)->GetOutermost())
		{
			if(Package->IsDirty())
			{
				return AssetDirtyBrush;
			}
		}
	}
	return NULL;
}

void FPersona::FindInContentBrowser_Execute()
{
	FName CurrentMode = GetCurrentMode();
	TArray<UObject*> ObjectsToSyncTo = GetEditorObjectsForMode(CurrentMode);
	if (ObjectsToSyncTo.Num() > 0)
	{
		GEditor->SyncBrowserToObjects( ObjectsToSyncTo );
	}
}

void FPersona::OnCommandGenericDelete()
{
	OnGenericDelete.Broadcast();
}

void FPersona::UndoAction()
{
	GEditor->UndoTransaction();
}

void FPersona::RedoAction()
{
	GEditor->RedoTransaction();
}

FSlateIcon FPersona::GetRecordStatusImage() const
{
	if (IsRecording())
	{
		return FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.StopRecordAnimation");
	}

	return FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.StartRecordAnimation");
}

FText FPersona::GetRecordMenuLabel() const
{
	if(IsRecording())
	{
		return LOCTEXT("Persona_StopRecordAnimationMenuLabel", "Stop Record Animation");
	}

	return LOCTEXT("Persona_StartRecordAnimationMenuLabel", "Start Record Animation");
}

FText FPersona::GetRecordStatusLabel() const
{
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>(TEXT("Persona"));

	bool bInRecording = false;
	PersonaModule.OnIsRecordingActive().ExecuteIfBound(PreviewComponent, bInRecording);

	if (bInRecording)
	{
		return LOCTEXT("Persona_StopRecordAnimationLabel", "Stop");
	}

	return LOCTEXT("Persona_StartRecordAnimationLabel", "Record");
}

FText FPersona::GetRecordStatusTooltip() const
{
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>(TEXT("Persona"));

	bool bInRecording = false;
	PersonaModule.OnIsRecordingActive().ExecuteIfBound(PreviewComponent, bInRecording);

	if (bInRecording)
	{
		return LOCTEXT("Persona_StopRecordAnimation", "Stop Record Animation");
	}

	return LOCTEXT("Persona_StartRecordAnimation", "Start Record Animation");
}

void FPersona::RecordAnimation()
{
	if (!PreviewComponent || !PreviewComponent->SkeletalMesh)
	{
		// error
		return;
	}

	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>(TEXT("Persona"));

	bool bInRecording = false;
	PersonaModule.OnIsRecordingActive().ExecuteIfBound(PreviewComponent, bInRecording);

	if (bInRecording)
	{
		PersonaModule.OnStopRecording().ExecuteIfBound(PreviewComponent);
	}
	else
	{
		PersonaModule.OnRecord().ExecuteIfBound(PreviewComponent);
	}
}

void FPersona::OnSetKeyCompleted()
{
	OnTrackCurvesChanged.Broadcast();
}

bool FPersona::CanSetKey() const
{
	return ( HasValidAnimationSequencePlaying() && PreviewComponent->BonesOfInterest.Num() > 0);
}

void FPersona::OnSetKey()
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence> (GetAnimationAssetBeingEdited());
	if (AnimSequence)
	{
		UDebugSkelMeshComponent* Component = GetPreviewMeshComponent();
		Component->PreviewInstance->SetKey(FSimpleDelegate::CreateSP(this, &FPersona::OnSetKeyCompleted));
	}
}

bool FPersona::CanApplyRawAnimChanges() const
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence> (GetAnimationAssetBeingEdited());
	// ideally would be great if we can only show if something changed
	return (AnimSequence && (AnimSequence->DoesNeedRebake() || AnimSequence->DoesNeedRecompress()));
}

void FPersona::OnApplyRawAnimChanges()
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(GetAnimationAssetBeingEdited());
	if(AnimSequence)
	{
		UDebugSkelMeshComponent* Component = GetPreviewMeshComponent();
		// now bake
		if (AnimSequence->DoesNeedRebake())
		{
			FScopedTransaction ScopedTransaction(LOCTEXT("BakeAnimation", "Bake Animation"));
			AnimSequence->Modify(true);
			AnimSequence->BakeTrackCurvesToRawAnimation();
		}

		if (AnimSequence->DoesNeedRecompress())
		{
			FScopedTransaction ScopedTransaction(LOCTEXT("BakeAnimation", "Bake Animation"));
			AnimSequence->Modify(true);
			AnimSequence->RequestSyncAnimRecompression(false);
		}
	}
}

void FPersona::OnApplyCompression()
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence> (GetPreviewAnimationAsset());

	if (AnimSequence)
	{
		TArray<TWeakObjectPtr<UAnimSequence>> AnimSequences;
		AnimSequences.Add(AnimSequence);
		ApplyCompression(AnimSequences);
	}
}

void FPersona::OnExportToFBX()
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(GetPreviewAnimationAsset());

	if(AnimSequence)
	{
		TArray<TWeakObjectPtr<UAnimSequence>> AnimSequences;
		AnimSequences.Add(AnimSequence);
		ExportToFBX(AnimSequences);
	}
}

void FPersona::OnAddLoopingInterpolation()
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(GetPreviewAnimationAsset());

	if(AnimSequence)
	{
		TArray<TWeakObjectPtr<UAnimSequence>> AnimSequences;
		AnimSequences.Add(AnimSequence);
		AddLoopingInterpolation(AnimSequences);
	}
}

void FPersona::ApplyCompression(TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences)
{
	FDlgAnimCompression AnimCompressionDialog(AnimSequences);
	AnimCompressionDialog.ShowModal();
}

void FPersona::ExportToFBX(TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	if(DesktopPlatform)
	{
		USkeletalMesh* PreviewMesh = GetPreviewMeshComponent()->SkeletalMesh;
		if(PreviewMesh == NULL)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ExportToFBXExportMissingPreviewMesh", "ERROR: Missing preview mesh"));
			return;
		}

		if(AnimSequences.Num() > 0)
		{
			//Get parent window for dialogs
			IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
			const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();

			void* ParentWindowWindowHandle = NULL;

			if(MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid())
			{
				ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
			}

			//Cache anim file names
			TArray<FString> AnimFileNames;
			for(auto Iter = AnimSequences.CreateIterator(); Iter; ++Iter)
			{
				AnimFileNames.Add(Iter->Get()->GetName() + TEXT(".fbx"));
			}

			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			FString DestinationFolder;

			const FString Title = LOCTEXT("ExportFBXsToFolderTitle", "Choose a destination folder for the FBX file(s)").ToString();

			if(AnimSequences.Num() > 1)
			{
				bool bFolderValid = false;
				// More than one file, just ask for directory
				while(!bFolderValid)
				{
					const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
						ParentWindowWindowHandle,
						Title,
						FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT),
						DestinationFolder
						);

					if(!bFolderSelected)
					{
						// User canceled, return
						return;
					}

					FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, DestinationFolder);
					FPaths::NormalizeFilename(DestinationFolder);

					//Check whether there are any fbx filename conflicts in this folder
					for(auto Iter = AnimFileNames.CreateIterator(); Iter; ++Iter)
					{
						FString& AnimFileName = *Iter;
						FString FullPath = DestinationFolder + "/" + AnimFileName;

						bFolderValid = true;
						if(PlatformFile.FileExists(*FullPath))
						{
							FFormatNamedArguments Args;
							Args.Add(TEXT("DestinationFolder"), FText::FromString(DestinationFolder));
							const FText DialogMessage = FText::Format(LOCTEXT("ExportToFBXFileOverwriteMessage", "Exporting to '{DestinationFolder}' will cause one or more existing FBX files to be overwritten. Would you like to continue?"), Args);
							EAppReturnType::Type DialogReturn = FMessageDialog::Open(EAppMsgType::YesNo, DialogMessage);
							bFolderValid = (EAppReturnType::Yes == DialogReturn);
							break;
						}
					}
				}
			}
			else
			{
				// One file only, ask for full filename.
				// Can set bFolderValid from the SaveFileDialog call as the window will handle 
				// duplicate files for us.
				TArray<FString> TempDestinationNames;
				bool bSave = DesktopPlatform->SaveFileDialog(
					ParentWindowWindowHandle,
					Title,
					FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT),
					AnimSequences[0]->GetName(),
					"FBX  |*.fbx",
					EFileDialogFlags::None,
					TempDestinationNames
					);

				if(!bSave)
				{
					// Canceled
					return;
				}
				check(TempDestinationNames.Num() == 1);
				check(AnimFileNames.Num() == 1);

				DestinationFolder = FPaths::GetPath(TempDestinationNames[0]);
				AnimFileNames[0] = FPaths::GetCleanFilename(TempDestinationNames[0]);

				FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, DestinationFolder);
			}

			EAppReturnType::Type DialogReturn = FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("ExportToFBXExportPreviewMeshToo", "Would you like to export the current skeletal mesh with the animation(s)?"));
			bool bSaveSkeletalMesh = EAppReturnType::Yes == DialogReturn;

			const bool bShowCancel = false;
			const bool bShowProgressDialog = true;
			GWarn->BeginSlowTask(LOCTEXT("ExportToFBXProgress", "Exporting Animation(s) to FBX"), bShowProgressDialog, bShowCancel);

			// make sure to use PreviewMesh, when export inside of Persona
			const int32 NumberOfAnimations = AnimSequences.Num();
			for(int32 i = 0; i < NumberOfAnimations; ++i)
			{
				GWarn->UpdateProgress(i, NumberOfAnimations);

				UAnimSequence* AnimSequence = AnimSequences[i].Get();

				FString FileName = FString::Printf(TEXT("%s/%s"), *DestinationFolder, *AnimFileNames[i]);

				FbxAnimUtils::ExportAnimFbx(*FileName, AnimSequence, PreviewMesh, bSaveSkeletalMesh);
			}

			GWarn->EndSlowTask();
		}
	}
}

void FPersona::AddLoopingInterpolation(TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences)
{
	FText WarningMessage = LOCTEXT("AddLoopiingInterpolation", "This will add an extra first frame at the end of the animation to create a better looping interpolation. This action cannot be undone. Would you like to proceed?");

	if(FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage) == EAppReturnType::Yes)
	{
		for(auto Animation : AnimSequences)
		{
			// get first frame and add to the last frame and go through track
			// now calculating old animated space bases
			Animation->AddLoopingInterpolation();
		}
	}
}

bool FPersona::HasValidAnimationSequencePlaying() const
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence> (GetAnimationAssetBeingEdited());
	
	return (AnimSequence != NULL);
}

bool FPersona::IsInPersonaMode(const FName InPersonaMode) const
{
	return (GetCurrentMode() == InPersonaMode);
}

void FPersona::ChangeSkeletonPreviewMesh()
{
	// Menu option cannot be called unless this is valid
	if(TargetSkeleton->GetPreviewMesh() != PreviewComponent->SkeletalMesh)
	{
		const FScopedTransaction Transaction( LOCTEXT("ChangeSkeletonPreviewMesh", "Change Skeleton Preview Mesh") );
		TargetSkeleton->SetPreviewMesh(PreviewComponent->SkeletalMesh);

		FFormatNamedArguments Args;
		Args.Add( TEXT("PreviewMeshName"), FText::FromString( PreviewComponent->SkeletalMesh->GetName() ) );
		Args.Add( TEXT("TargetSkeletonName"), FText::FromString( TargetSkeleton->GetName() ) );
		FNotificationInfo Info( FText::Format( LOCTEXT("SaveSkeletonWarning", "Please save Skeleton {TargetSkeletonName} if you'd like to keep {PreviewMeshName} as default preview mesh" ), Args ) );
		Info.ExpireDuration = 5.0f;
		Info.bUseLargeFont = false;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if ( Notification.IsValid() )
		{
			Notification->SetCompletionState( SNotificationItem::CS_Fail );
		}
	}
}

void FPersona::UpdateSkeletonRefPose()
{
	// update ref pose with current preview mesh
	if (TargetSkeleton)
	{
		TargetSkeleton->UpdateReferencePoseFromMesh(GetPreviewMeshComponent()->SkeletalMesh);
	}
}

void FPersona::RemoveUnusedBones()
{
	// Menu option cannot be called unless this is valid
	if(TargetSkeleton)
	{
		TArray<FName> SkeletonBones;
		const FReferenceSkeleton& RefSkeleton = TargetSkeleton->GetReferenceSkeleton();

		for(int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
		{
			SkeletonBones.Add(RefSkeleton.GetBoneName(BoneIndex));
		}

		FARFilter Filter;
		Filter.ClassNames.Add(USkeletalMesh::StaticClass()->GetFName());

		FString SkeletonString = FAssetData(TargetSkeleton).GetExportTextName();
		Filter.TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(USkeletalMesh, Skeleton), SkeletonString);

		TArray<FAssetData> SkeletalMeshes;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().GetAssets(Filter, SkeletalMeshes);

		FText TimeTakenMessage = FText::Format( LOCTEXT("RemoveUnusedBones_TimeTakenWarning", "In order to verify bone use all Skeletal Meshes that use this skeleton will be loaded, this may take some time.\n\nProceed?\n\nNumber of Meshes: {0}"), FText::AsNumber(SkeletalMeshes.Num()) );
		
		if(FMessageDialog::Open( EAppMsgType::YesNo, TimeTakenMessage ) == EAppReturnType::Yes)
		{
			const FText StatusUpdate = FText::Format(LOCTEXT("RemoveUnusedBones_ProcessingAssetsFor", "Processing Skeletal Meshes for {0}"), FText::FromString(TargetSkeleton->GetName()) );
			GWarn->BeginSlowTask(StatusUpdate, true );

			// Loop through all SkeletalMeshes and remove the bones they use from our list
			for ( int32 MeshIdx = 0; MeshIdx < SkeletalMeshes.Num(); ++MeshIdx )
			{
				GWarn->StatusUpdate( MeshIdx, SkeletalMeshes.Num(), StatusUpdate );

				USkeletalMesh* Mesh = Cast<USkeletalMesh>(SkeletalMeshes[MeshIdx].GetAsset());
				const FReferenceSkeleton& MeshRefSkeleton = Mesh->RefSkeleton;

				for(int32 BoneIndex = 0; BoneIndex < MeshRefSkeleton.GetNum(); ++BoneIndex)
				{
					SkeletonBones.Remove(MeshRefSkeleton.GetBoneName(BoneIndex));
					if(SkeletonBones.Num() == 0)
					{
						break;
					}
				}
				if(SkeletonBones.Num() == 0)
				{
					break;
				}
			}

			GWarn->EndSlowTask();

			//Remove bones that are a parent to bones we aren't removing
			for(int32 BoneIndex = RefSkeleton.GetNum() -1; BoneIndex >= 0; --BoneIndex)
			{
				FName CurrBoneName = RefSkeleton.GetBoneName(BoneIndex);
				if(!SkeletonBones.Contains(CurrBoneName))
				{
					//We aren't removing this bone, so remove parent from list of bones to remove too
					int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
					if(ParentIndex != INDEX_NONE)
					{
						SkeletonBones.Remove(RefSkeleton.GetBoneName(ParentIndex));
					}
				}
			}

			// If we have any bones left they are unused
			if(SkeletonBones.Num() > 0)
			{
				const FText RemoveBoneMessage = FText::Format(LOCTEXT("RemoveBoneWarning", "Continuing will remove the following bones from the skeleton '{0}'. These bones are not being used by any of the SkeletalMeshes assigned to this skeleton\n\nOnce the bones have been removed all loaded animations for this skeleton will be recompressed (any that aren't loaded will be recompressed the next time they are loaded)."), FText::FromString(TargetSkeleton->GetName()) );

				// Ask User whether they would like to remove the bones from the skeleton
				if (SSkeletonBoneRemoval::ShowModal(SkeletonBones, RemoveBoneMessage))
				{
					//Remove these bones from the skeleton
					TargetSkeleton->RemoveBonesFromSkeleton(SkeletonBones, true);
				}
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoBonesToRemove", "No unused bones were found."));
			}
		}
	}
}

void FPersona::Tick(float DeltaTime)
{
	FBlueprintEditor::Tick(DeltaTime);

	if (Viewport.IsValid())
	{
		Viewport.Pin()->RefreshViewport();
	}

	if (PreviewComponent && LastCachedLODForPreviewComponent != PreviewComponent->PredictedLODLevel)
	{
		OnLODChanged.Broadcast();
		LastCachedLODForPreviewComponent = PreviewComponent->PredictedLODLevel;
	}
}

bool FPersona::CanChangeSkeletonPreviewMesh() const
{
	return PreviewComponent && PreviewComponent->SkeletalMesh;
}

bool FPersona::CanRemoveBones() const
{
	return PreviewComponent && PreviewComponent->SkeletalMesh;
}

bool FPersona::IsRecordAvailable() const
{
	// make sure mesh exists
	return (PreviewComponent && PreviewComponent->SkeletalMesh);
}

bool FPersona::IsEditable(UEdGraph* InGraph) const
{
	bool bEditable = FBlueprintEditor::IsEditable(InGraph);
	bEditable &= IsGraphInCurrentBlueprint(InGraph);

	return bEditable;
}

FText FPersona::GetGraphDecorationString(UEdGraph* InGraph) const
{
	if (!IsGraphInCurrentBlueprint(InGraph))
	{
		return LOCTEXT("PersonaExternalGraphDecoration", " Parent Graph Preview");
	}
	return FText::GetEmpty();
}

void FPersona::ValidatePreviewAttachedAssets(USkeletalMesh* PreviewSkeletalMesh)
{
	// Validate the skeleton/meshes attached objects and display a notification to the user if any were broken
	int32 NumBrokenAssets = TargetSkeleton->ValidatePreviewAttachedObjects();
	if (PreviewSkeletalMesh)
	{
		NumBrokenAssets += PreviewSkeletalMesh->ValidatePreviewAttachedObjects();
	}

	if (NumBrokenAssets > 0)
	{
		// Tell the user that there were assets that could not be loaded
		FFormatNamedArguments Args;
		Args.Add(TEXT("NumBrokenAssets"), NumBrokenAssets);
		FNotificationInfo Info(FText::Format(LOCTEXT("MissingPreviewAttachedAssets", "{NumBrokenAssets} attached assets could not be found on loading and were removed"), Args));

		Info.bUseLargeFont = false;
		Info.ExpireDuration = 5.0f;

		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if (Notification.IsValid())
		{
			Notification->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}
}

void FPersona::OnAnimNotifyWindow()
{
	TabManager->InvokeTab(FPersonaTabs::SkeletonAnimNotifiesID);
}

void FPersona::OnRetargetManager()
{
	TabManager->InvokeTab(FPersonaTabs::RetargetManagerID);
}

void FPersona::OnImportAsset(enum EFBXImportType DefaultImportType)
{
	// open dialog
	// get path
	FString AssetPath;

	TSharedRef<SImportPathDialog> NewAnimDlg =
	SNew(SImportPathDialog);

	if(NewAnimDlg->ShowModal() != EAppReturnType::Cancel)
	{
		AssetPath = NewAnimDlg->GetAssetPath();
	
		UFbxImportUI* ImportUI = NewObject<UFbxImportUI>();
		ImportUI->Skeleton = TargetSkeleton;
		ImportUI->MeshTypeToImport = DefaultImportType;

		FbxMeshUtils::SetImportOption(ImportUI);

		// now I have to set skeleton on it. 
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		TArray<UObject*> NewAssets = AssetToolsModule.Get().ImportAssets(AssetPath);

		if ( NewAssets.Num() > 0 )
		{

		}
		else
		{
			// error
		}
	}
}

void FPersona::OnReimportMesh()
{
	// Reimport the asset
	UDebugSkelMeshComponent* PreviewMeshComponent = GetPreviewMeshComponent();
	if(PreviewMeshComponent && PreviewMeshComponent->SkeletalMesh)
	{
		FReimportManager::Instance()->Reimport(PreviewMeshComponent->SkeletalMesh, true);
	}
}

void FPersona::OnReimportAnimation()
{
	// Reimport the asset
	UAnimSequence* AnimSequence = Cast<UAnimSequence> (GetAnimationAssetBeingEdited());
	if (AnimSequence)
	{
		FReimportManager::Instance()->Reimport(AnimSequence, true);
	}
}

TStatId FPersona::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FPersona, STATGROUP_Tickables);
}

void FPersona::OnBlueprintPreCompile(UBlueprint* BlueprintToCompile)
{
	if(PreviewComponent && PreviewComponent->PreviewInstance)
	{
		// If we are compiling an anim notify state the class will soon be sanitized and 
		// if an anim instance is running a state when that happens it will likely
		// crash, so we end any states that are about to compile.
		UAnimPreviewInstance* Instance = PreviewComponent->PreviewInstance;
		USkeletalMeshComponent* SkelMeshComp = Instance->GetSkelMeshComponent();

		for(int32 Idx = Instance->ActiveAnimNotifyState.Num() - 1 ; Idx >= 0 ; --Idx)
		{
			FAnimNotifyEvent& Event = Instance->ActiveAnimNotifyState[Idx];
			if(Event.NotifyStateClass->GetClass() == BlueprintToCompile->GeneratedClass)
			{
				Event.NotifyStateClass->NotifyEnd(SkelMeshComp, Cast<UAnimSequenceBase>(Event.NotifyStateClass->GetOuter()));
				Instance->ActiveAnimNotifyState.RemoveAt(Idx);
			}
		}
	}
}

void FPersona::OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled /*= false*/)
{
	FBlueprintEditor::OnBlueprintChangedImpl(InBlueprint, bIsJustBeingCompiled);

	UObject* CurrentDebugObject = GetBlueprintObj()->GetObjectBeingDebugged();
	const bool bIsDebuggingPreview = (PreviewComponent != NULL) && PreviewComponent->IsAnimBlueprintInstanced() && (PreviewComponent->GetAnimInstance() == CurrentDebugObject);

	if(PreviewComponent != NULL)
	{
		// Reinitialize the animation, anything we reference could have changed triggering
		// the blueprint change
		PreviewComponent->InitAnim(true);

		if(bIsDebuggingPreview)
		{
			GetBlueprintObj()->SetObjectBeingDebugged(PreviewComponent->GetAnimInstance());
		}
	}

	// calls PostCompile to copy proper values between anim nodes
	if(Viewport.IsValid())
	{
		Viewport.Pin()->GetAnimationViewportClient()->PostCompile();
	}
}

void FPersona::ShowReferencePose(bool bReferencePose)
{
	if(PreviewComponent)
	{
		if(bReferencePose == false)
		{
			if(IsInPersonaMode(FPersonaModes::AnimBlueprintEditMode))
			{
				PreviewComponent->EnablePreview(false, nullptr);

				UAnimBlueprint* AnimBP = GetAnimBlueprint();
				if(AnimBP)
				{
					PreviewComponent->SetAnimInstanceClass(AnimBP->GeneratedClass);
				}
			}
			else
			{
				UObject* PreviewAsset = CachedPreviewAsset.IsValid()? CachedPreviewAsset.Get() : (GetAnimationAssetBeingEdited());
				PreviewComponent->EnablePreview(true, Cast<UAnimationAsset>(PreviewAsset));
			}
		}
		else
		{
			if (PreviewComponent->PreviewInstance && PreviewComponent->PreviewInstance->GetCurrentAsset())
			{
				CachedPreviewAsset = PreviewComponent->PreviewInstance->GetCurrentAsset();
			}
			
			PreviewComponent->EnablePreview(true, nullptr);
		}
	}
}

bool FPersona::ShouldDisplayAdditiveScaleErrorMessage()
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(GetAnimationAssetBeingEdited());
	if (AnimSequence)
	{
		if (AnimSequence->IsValidAdditive() && AnimSequence->RefPoseSeq)
		{
			if (RefPoseGuid != AnimSequence->RefPoseSeq->RawDataGuid)
			{
				RefPoseGuid = AnimSequence->RefPoseSeq->RawDataGuid;
				bDoesAdditiveRefPoseHaveZeroScale = AnimSequence->DoesSequenceContainZeroScale();
			}
			return bDoesAdditiveRefPoseHaveZeroScale;
		}
	}

	RefPoseGuid.Invalidate();
	return false;
}

void PopulateWithAssets(FName ClassName, FName SkeletonMemberName, const FString& SkeletonString, TArray<FAssetData>& OutAssets)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FARFilter Filter;
	Filter.ClassNames.Add(ClassName);
	Filter.TagsAndValues.Add(SkeletonMemberName, SkeletonString);

	AssetRegistryModule.Get().GetAssets(Filter, OutAssets);
}

void FPersona::TestSkeletonCurveNamesForUse() const
{
	if (TargetSkeleton)
	{
		if (const FSmartNameMapping* Mapping = TargetSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName))
		{
			const FString SkeletonString = FAssetData(TargetSkeleton).GetExportTextName();
			
			TArray<FAssetData> SkeletalMeshes;
			PopulateWithAssets(USkeletalMesh::StaticClass()->GetFName(), GET_MEMBER_NAME_CHECKED(USkeletalMesh, Skeleton), SkeletonString, SkeletalMeshes);
			TArray<FAssetData> Animations;
			PopulateWithAssets(UAnimSequence::StaticClass()->GetFName(), FName("Skeleton"), SkeletonString, Animations);

			FText TimeTakenMessage = FText::Format(LOCTEXT("VerifyCurves_TimeTakenWarning", "In order to verify curve usage all Skeletal Meshes and Animations that use this skeleton will be loaded, this may take some time.\n\nProceed?\n\nNumber of Meshes: {0}\nNumber of Animations: {1}"), FText::AsNumber(SkeletalMeshes.Num()), FText::AsNumber(Animations.Num()));

			if (FMessageDialog::Open(EAppMsgType::YesNo, TimeTakenMessage) == EAppReturnType::Yes)
			{
				const FText LoadingStatusUpdate = FText::Format(LOCTEXT("VerifyCurves_LoadingAllAnimations", "Loading all animations for skeleton '{0}'"), FText::FromString(TargetSkeleton->GetName()));
				{
					FScopedSlowTask LoadingAnimSlowTask(Animations.Num(), LoadingStatusUpdate);
					LoadingAnimSlowTask.MakeDialog();

					// Loop through all animations to load then, this makes sure smart names are all up to date
					for (const FAssetData& Anim : Animations)
					{
						LoadingAnimSlowTask.EnterProgressFrame();
						UAnimSequence* Seq = Cast<UAnimSequence>(Anim.GetAsset());
					}
				}

				// Grab all curve names for this skeleton
				TArray<FName> UnusedNames;
				Mapping->FillNameArray(UnusedNames);

				const FText ProcessingStatusUpdate = FText::Format(LOCTEXT("VerifyCurves_ProcessingSkeletalMeshes", "Looking at curve useage for each skeletal mesh of skeleton '{0}'"), FText::FromString(TargetSkeleton->GetName()));
				{
					FScopedSlowTask LoadingSkelMeshSlowTask(SkeletalMeshes.Num(), ProcessingStatusUpdate);
					LoadingSkelMeshSlowTask.MakeDialog();

					for (int32 MeshIdx = 0; MeshIdx < SkeletalMeshes.Num(); ++MeshIdx)
					{
						LoadingSkelMeshSlowTask.EnterProgressFrame();

						const USkeletalMesh* Mesh = Cast<USkeletalMesh>(SkeletalMeshes[MeshIdx].GetAsset());

						// Filter morph targets from curves
						const TArray<UMorphTarget*>& MorphTargets = Mesh->MorphTargets;
						for (int32 I = 0; I < MorphTargets.Num(); ++I)
						{
							const int32 CurveIndex = UnusedNames.RemoveSingleSwap(MorphTargets[I]->GetFName(), false);
						}

						// Filter material params from curves
						for (const FSkeletalMaterial& Mat : Mesh->Materials)
						{
							if (UnusedNames.Num() == 0)
							{
								break; // Done
							}

							UMaterial* Material = (Mat.MaterialInterface != nullptr) ? Mat.MaterialInterface->GetMaterial() : nullptr;
							if (Material)
							{
								TArray<FName> OutParameterNames;
								TArray<FGuid> OutParameterIds;

								// Retrieve all scalar parameter names from the material
								Material->GetAllScalarParameterNames(OutParameterNames, OutParameterIds);

								for (FName SPName : OutParameterNames)
								{
									UnusedNames.RemoveSingleSwap(SPName);
								}
							}
						}
					}
				}

				FMessageLog CurveOutput("Persona");
				CurveOutput.NewPage(LOCTEXT("PersonaMessageLogName", "Persona"));

				bool bFoundIssue = false;

				const FText ProcessingAnimStatusUpdate = FText::Format(LOCTEXT("VerifyCurves_FindAnimationsWithUnusedCurves", "Finding animations that reference unused curves on skeleton '{0}'"), FText::FromString(TargetSkeleton->GetName()));
				{
					FScopedSlowTask ProcessingAnimationsSlowTask(Animations.Num(), ProcessingAnimStatusUpdate);
					ProcessingAnimationsSlowTask.MakeDialog();

					for (const FAssetData& Anim : Animations)
					{
						ProcessingAnimationsSlowTask.EnterProgressFrame();
						UAnimSequence* Seq = Cast<UAnimSequence>(Anim.GetAsset());

						TSharedPtr<FTokenizedMessage> Message;
						for (FFloatCurve& Curve : Seq->RawCurveData.FloatCurves)
						{
							if (UnusedNames.Contains(Curve.Name.DisplayName))
							{
								bFoundIssue = true;
								if (!Message.IsValid())
								{
									Message = CurveOutput.Warning();
									Message->AddToken(FAssetNameToken::Create(Anim.ObjectPath.ToString(), FText::FromName(Anim.AssetName)));
									Message->AddToken(FTextToken::Create(LOCTEXT("VerifyCurves_FoundAnimationsWithUnusedReferences", "References the following curves that are not used for either morph targets or material parameters and so may be unneeded")));
								}
								CurveOutput.Info(FText::FromName(Curve.Name.DisplayName));
							}
						}
					}
				}

				if (bFoundIssue)
				{
					CurveOutput.Notify();
				}
			}
		}
	}
}

bool FPersona::CanShowReferencePose() const
{
	return PreviewComponent != NULL;
}

bool FPersona::IsShowReferencePoseEnabled() const
{
	if(PreviewComponent)
	{
		return PreviewComponent->IsPreviewOn() && PreviewComponent->PreviewInstance->GetCurrentAsset() == NULL;
	}
	return false;
}

bool FPersona::CanPreviewAsset() const
{
	return CanShowReferencePose();
}

bool FPersona::IsPreviewAssetEnabled() const
{
	return IsShowReferencePoseEnabled() == false;
}

FText FPersona::GetPreviewAssetTooltip() const
{
	// if already looking at ref pose
	if(IsShowReferencePoseEnabled())
	{
		FString AssetName = TEXT("None Available. Please select asset to preview.");

		if(IsInPersonaMode(FPersonaModes::AnimBlueprintEditMode))
		{
			UAnimBlueprint* AnimBP = GetAnimBlueprint();
			if(AnimBP)
			{
				AssetName = AnimBP->GetName();
			}
		}
		else
		{
			UObject* PreviewAsset = CachedPreviewAsset.IsValid()? CachedPreviewAsset.Get() : (GetAnimationAssetBeingEdited());
			if (PreviewAsset)
			{
				AssetName = PreviewAsset->GetName();
			}
		}
		return FText::FromString(FString::Printf(TEXT("Preview %s"), *AssetName));
	}
	else
	{
		return FText::FromString(FString::Printf(TEXT("Currently previewing %s"), *PreviewComponent->GetPreviewText()));
	}
}

void FPersona::SetSelectedBlendProfile(UBlendProfile* InBlendProfile)
{
	OnBlendProfileSelected.Broadcast(InBlendProfile);
}

bool FPersona::IsRecording() const
{
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");

	bool bInRecording = false;
	PersonaModule.OnIsRecordingActive().ExecuteIfBound(PreviewComponent, bInRecording);

	return bInRecording;
}

void FPersona::StopRecording()
{
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
	PersonaModule.OnStopRecording().ExecuteIfBound(PreviewComponent);
}

UAnimSequence* FPersona::GetCurrentRecording() const
{
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
	UAnimSequence* Recording = nullptr;
	PersonaModule.OnGetCurrentRecording().ExecuteIfBound(PreviewComponent, Recording);
	return Recording;
}

float FPersona::GetCurrentRecordingTime() const
{
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
	float RecordingTime = 0.0f;
	PersonaModule.OnGetCurrentRecordingTime().ExecuteIfBound(PreviewComponent, RecordingTime);
	return RecordingTime;
}

void FPersona::TogglePlayback()
{
	if (PreviewComponent && PreviewComponent->PreviewInstance)
	{
		PreviewComponent->PreviewInstance->SetPlaying(!PreviewComponent->PreviewInstance->IsPlaying());
	}
}

static class FMeshHierarchyCmd : private FSelfRegisteringExec
{
public:
	/** Console commands, see embeded usage statement **/
	virtual bool Exec( UWorld*, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		bool bResult = false;
		if(FParse::Command(&Cmd,TEXT("TMH")))
		{
			Ar.Log(TEXT("Starting Mesh Test"));
			FARFilter Filter;
			Filter.ClassNames.Add(USkeletalMesh::StaticClass()->GetFName());

			TArray<FAssetData> SkeletalMeshes;
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			AssetRegistryModule.Get().GetAssets(Filter, SkeletalMeshes);

			const FText StatusUpdate = LOCTEXT("RemoveUnusedBones_ProcessingAssets", "Processing Skeletal Meshes");
			GWarn->BeginSlowTask(StatusUpdate, true );

			// go through all assets try load
			for ( int32 MeshIdx = 0; MeshIdx < SkeletalMeshes.Num(); ++MeshIdx )
			{
				GWarn->StatusUpdate( MeshIdx, SkeletalMeshes.Num(), StatusUpdate );

				USkeletalMesh* Mesh = Cast<USkeletalMesh>(SkeletalMeshes[MeshIdx].GetAsset());
				if(Mesh->Skeleton)
				{
					FName MeshRoot = Mesh->RefSkeleton.GetBoneName(0);
					FName SkelRoot = Mesh->Skeleton->GetReferenceSkeleton().GetBoneName(0);

					if(MeshRoot != SkelRoot)
					{
						Ar.Logf( TEXT("Mesh Found '%s' %s->%s"), *Mesh->GetName(), *MeshRoot.ToString(), *SkelRoot.ToString());
					}
				}
			}

			GWarn->EndSlowTask();
			Ar.Log(TEXT("Mesh Test Finished"));
		}
		return bResult;
	}
} MeshHierarchyCmdExec;

#undef LOCTEXT_NAMESPACE

