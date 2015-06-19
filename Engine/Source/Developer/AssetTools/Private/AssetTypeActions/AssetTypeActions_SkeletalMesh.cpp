// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "Toolkits/AssetEditorManager.h"
#include "Editor/UnrealEd/Public/ObjectTools.h"
#include "Editor/UnrealEd/Public/PackageTools.h"
#include "AssetRegistryModule.h"
#include "Editor/UnrealEd/Public/SSkeletonWidget.h"
#include "Editor/PhAT/Public/PhATModule.h"
#include "Editor/Persona/Public/PersonaModule.h"
#include "AnimationEditorUtils.h"

#include "ContentBrowserModule.h"
#include "AssetToolsModule.h"
#include "Editor/UnrealEd/Public/FbxMeshUtils.h"
#include "Editor/UnrealEd/Public/AssetNotifications.h"
#include "PhysicsEngine/PhysicsAsset.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

/** 
* FBoneCheckboxInfo
* 
* Context data for the SDlgMergeSkeleton panel check boxes
*/
struct FBoneCheckboxInfo
{
	FName BoneName;
	int32 BoneID;
	bool  bUsed;
};

/** 
* FDlgMergeSkeleton
* 
* Wrapper class for SDlgMergeSkeleton. This class creates and launches a dialog then awaits the
* result to return to the user. 
*/
class FDlgMergeSkeleton
{
public:
	enum EResult
	{
		Cancel = 0,			// No/Cancel, normal usage would stop the current action
		Confirm = 1,		// Yes/Ok/Etc, normal usage would continue with action
	};

	FDlgMergeSkeleton( USkeletalMesh* InMesh, USkeleton* InSkeleton );

	/**  Shows the dialog box and waits for the user to respond. */
	EResult ShowModal();

	// List of required bones for skeleton
	TArray<int32> RequiredBones;
private:

	/** Cached pointer to the modal window */
	TSharedPtr<SWindow> DialogWindow;

	/** Cached pointer to the merge skeleton widget */
	TSharedPtr<class SDlgMergeSkeleton> DialogWidget;

	/** The SkeletalMesh to merge bones from*/
	USkeletalMesh* Mesh;
	/** The Skeleton to merge bones to */
	USkeleton* Skeleton;
};

/** 
 * Slate panel for choosing which bones to merge into the skeleton
 */
class SDlgMergeSkeleton : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDlgMergeSkeleton)
		{}
		/** Window in which this widget resides */
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()	

	/**
	 * Constructs this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& InArgs )
	{
		UserResponse = FDlgMergeSkeleton::Cancel;
		ParentWindow = InArgs._ParentWindow.Get();

		this->ChildSlot[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew( STextBlock )
				.Text( LOCTEXT("MergeSkeletonDlgDescription", "Would you like to add following bones to the skeleton?"))
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SSeparator)
			]
			+SVerticalBox::Slot()
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SBorder)
				[
					SNew(SScrollBox)
					+SScrollBox::Slot()
					[
						//Save this widget so we can populate it later with check boxes
						SAssignNew(CheckBoxContainer, SVerticalBox)
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton) 
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &SDlgMergeSkeleton::ChangeAllOptions, true)
					.Text(LOCTEXT("SkeletonMergeSelectAll", "Select All"))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton) 
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &SDlgMergeSkeleton::ChangeAllOptions, false)
					.Text(LOCTEXT("SkeletonMergeDeselectAll", "Deselect All"))
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SSeparator)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton) 
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked( this, &SDlgMergeSkeleton::OnButtonClick, FDlgMergeSkeleton::Confirm )
					.Text(LOCTEXT("SkeletonMergeOk", "OK"))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton) 
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked( this, &SDlgMergeSkeleton::OnButtonClick, FDlgMergeSkeleton::Cancel )
					.Text(LOCTEXT("SkeletonMergeCancel", "Cancel"))
				]
			]
		];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Creates a Slate check box 
	 *
	 * @param	Label		Text label for the check box
	 * @param	ButtonId	The ID for the check box
	 * @return				The created check box widget
	 */
	TSharedRef<SWidget> CreateCheckBox( const FString& Label, int32 ButtonId )
	{
		return
			SNew(SCheckBox)
			.IsChecked( this, &SDlgMergeSkeleton::IsCheckboxChecked, ButtonId )
			.OnCheckStateChanged( this, &SDlgMergeSkeleton::OnCheckboxChanged, ButtonId )
			[
				SNew(STextBlock).Text(FText::FromString(Label))
			];
	}

	/**
	 * Returns the state of the check box
	 *
	 * @param	ButtonId	The ID for the check box
	 * @return				The status of the check box
	 */
	ECheckBoxState IsCheckboxChecked( int32 ButtonId ) const
	{
		return CheckBoxInfoMap.FindChecked(ButtonId).bUsed ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/**
	 * Handler for all check box clicks
	 *
	 * @param	NewCheckboxState	The new state of the check box
	 * @param	CheckboxThatChanged	The ID of the radio button that has changed. 
	 */
	void OnCheckboxChanged( ECheckBoxState NewCheckboxState, int32 CheckboxThatChanged )
	{
		FBoneCheckboxInfo& Info = CheckBoxInfoMap.FindChecked(CheckboxThatChanged);
		Info.bUsed = !Info.bUsed;
	}

	/**
	 * Handler for the Select All and Deselect All buttons
	 *
	 * @param	bNewCheckedState	The new state of the check boxes
	 */
	FReply ChangeAllOptions(bool bNewCheckedState)
	{
		for(auto Iter = CheckBoxInfoMap.CreateIterator(); Iter; ++Iter)
		{
			FBoneCheckboxInfo& Info = Iter.Value();
			Info.bUsed = bNewCheckedState;
		}
		return FReply::Handled();
	}

	/**
	 * Populated the dialog with multiple check boxes, each corresponding to a bone
	 *
	 * @param	BoneInfos	The list of Bones to populate the dialog with
	 */
	void PopulateOptions(TArray<FBoneCheckboxInfo>& BoneInfos)
	{
		for(auto Iter = BoneInfos.CreateIterator(); Iter; ++Iter)
		{
			FBoneCheckboxInfo& Info = (*Iter);
			Info.bUsed = true;

			CheckBoxInfoMap.Add(Info.BoneID, Info);

			CheckBoxContainer->AddSlot()
			.AutoHeight()
			[
				CreateCheckBox(Info.BoneName.GetPlainNameString(), Info.BoneID)
			];
		}
	}

	/** 
	 * Returns the EResult of the button which the user pressed. Closing of the dialog
	 * in any other way than clicking "Ok" results in this returning a "Cancel" value
	 */
	FDlgMergeSkeleton::EResult GetUserResponse() const 
	{
		return UserResponse; 
	}

	/** 
	 * Returns whether the user selected that bone to be used (checked its respective check box)
	 */
	bool IsBoneIncluded(int32 BoneID)
	{
		auto* Item = CheckBoxInfoMap.Find(BoneID);
		return Item ? Item->bUsed : false;
	}

private:
	
	/** 
	 * Handles when a button is pressed, should be bound with appropriate EResult Key
	 * 
	 * @param ButtonID - The return type of the button which has been pressed.
	 */
	FReply OnButtonClick(FDlgMergeSkeleton::EResult ButtonID)
	{
		ParentWindow->RequestDestroyWindow();
		UserResponse = ButtonID;

		return FReply::Handled();
	}

	/** Stores the users response to this dialog */
	FDlgMergeSkeleton::EResult	 UserResponse;

	/** The slate container that the bone check boxes get added to */
	TSharedPtr<SVerticalBox>	 CheckBoxContainer;
	/** Store the check box state for each bone */
	TMap<int32,FBoneCheckboxInfo> CheckBoxInfoMap;

	/** Pointer to the window which holds this Widget, required for modal control */
	TSharedPtr<SWindow>			 ParentWindow;
};

FDlgMergeSkeleton::FDlgMergeSkeleton( USkeletalMesh* InMesh, USkeleton* InSkeleton )
{
	Mesh = InMesh;
	Skeleton = InSkeleton;

	if (FSlateApplication::IsInitialized())
	{
		DialogWindow = SNew(SWindow)
			.Title( LOCTEXT("MergeSkeletonDlgTitle", "Merge Bones") )
			.SupportsMinimize(false) .SupportsMaximize(false)
			.ClientSize(FVector2D(350, 500));

		TSharedPtr<SBorder> DialogWrapper = 
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SAssignNew(DialogWidget, SDlgMergeSkeleton)
				.ParentWindow(DialogWindow)
			];

		DialogWindow->SetContent(DialogWrapper.ToSharedRef());
	}
}

FDlgMergeSkeleton::EResult FDlgMergeSkeleton::ShowModal()
{
	RequiredBones.Empty();

	TMap<FName, int32> BoneIndicesMap;
	TArray<FBoneCheckboxInfo> BoneInfos;

	// Make a list of all skeleton bone list
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const TArray<FBoneNode>& BoneTree = Skeleton->GetBoneTree();
	for ( int32 BoneTreeId=0; BoneTreeId<RefSkeleton.GetNum(); ++BoneTreeId )
	{
		const FName& BoneName = RefSkeleton.GetBoneName(BoneTreeId);
		BoneIndicesMap.Add(BoneName, BoneTreeId);
	}

	for ( int32 RefBoneId=0 ; RefBoneId< Mesh->RefSkeleton.GetNum() ; ++RefBoneId )
	{
		const FName& BoneName = Mesh->RefSkeleton.GetBoneName(RefBoneId);
		// if I can't find this from Skeleton
		if (BoneIndicesMap.Find(BoneName)==NULL)
		{
			FBoneCheckboxInfo Info;
			Info.BoneID = RefBoneId;
			Info.BoneName = BoneName;
			BoneInfos.Add(Info);
		}
	}

	if (BoneInfos.Num() == 0)
	{
		// it's all identical, but still need to return RequiredBones
		// for the case, where they'd like to replace the one exactly same hierarchy but different skeleton 
		for ( int32 RefBoneId= 0 ; RefBoneId< Mesh->RefSkeleton.GetNum() ; ++RefBoneId )
		{
			const FName& BoneName = Mesh->RefSkeleton.GetBoneName(RefBoneId);
			RequiredBones.Add(RefBoneId);
		}

		return EResult::Confirm;
	}

	DialogWidget->PopulateOptions(BoneInfos);

	//Show Dialog
	GEditor->EditorAddModalWindow(DialogWindow.ToSharedRef());
	EResult UserResponse = (EResult)DialogWidget->GetUserResponse();

	if(UserResponse == EResult::Confirm)
	{
		for ( int32 RefBoneId= 0 ; RefBoneId< Mesh->RefSkeleton.GetNum() ; ++RefBoneId )
		{
			if ( DialogWidget->IsBoneIncluded(RefBoneId) )
			{
				TArray<int32> ParentList;
				
				// I need to make sure parent exists first
				int32 ParentIndex = Mesh->RefSkeleton.GetParentIndex(RefBoneId);
				
				// make sure RequiredBones already have ParentIndex
				while (ParentIndex >= 0 )
				{
					// if I don't have it yet
					if ( RequiredBones.Contains(ParentIndex)==false )
					{
						ParentList.Add(ParentIndex);
					}

					ParentIndex = Mesh->RefSkeleton.GetParentIndex(ParentIndex);
				}

				if ( ParentList.Num() > 0 )
				{
					// if we need to add parent list
					// add from back to front (since it's added from child to up
					for (int32 I=ParentList.Num()-1; I>=0; --I)
					{
						RequiredBones.Add(ParentList[I]);
					}
				}

				RequiredBones.Add(RefBoneId);
			}
		}
	}
	return UserResponse;
}

void FAssetTypeActions_SkeletalMesh::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto Meshes = GetTypedWeakObjectPtrs<USkeletalMesh>(InObjects);

	MenuBuilder.AddSubMenu(
			LOCTEXT("CreateSkeletalMeshSubmenu", "Create"),
			LOCTEXT("CreateSkeletalMeshSubmenu_ToolTip", "Create related assets"),
			FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_SkeletalMesh::FillCreateMenu, Meshes),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.CreateAnimAsset")
			);

	MenuBuilder.AddSubMenu(	
		LOCTEXT("SkeletalMesh_LODImport", "Import LOD"),
		LOCTEXT("SkeletalMesh_LODImportTooltip", "Select which LODs to import."),
		FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_SkeletalMesh::GetLODMenu, Meshes)
		);

	// Add actions that do not apply to destructible meshes
	GetNonDestructibleActions(Meshes, MenuBuilder);
}

void FAssetTypeActions_SkeletalMesh::FillCreateMenu(FMenuBuilder& MenuBuilder, const TArray<TWeakObjectPtr<USkeletalMesh>> Meshes) const
{
	MenuBuilder.BeginSection("CreatePhysicsAsset", LOCTEXT("CreatePhysicsAssetMenuHeading", "Physics Asset"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SkeletalMesh_NewPhysicsAsset", "Physics Asset"),
			LOCTEXT("SkeletalMesh_NewPhysicsAssetTooltip", "Creates a new physics asset for each of the selected meshes."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_SkeletalMesh::ExecuteNewPhysicsAsset, Meshes),
				FCanExecuteAction()
				)
			);
	}
	MenuBuilder.EndSection();

	// Get the skeleton for each selected skeletal mesh
	TArray<TWeakObjectPtr<USkeleton>> Skeletons;
	Skeletons.Reserve(Meshes.Num());
	for (int32 i = 0; i < Meshes.Num(); i++)
	{
		Skeletons.Add(Meshes[i]->Skeleton);
	}
	
	AnimationEditorUtils::FillCreateAssetMenu(MenuBuilder, Skeletons, FAnimAssetCreated::CreateSP(this, &FAssetTypeActions_SkeletalMesh::OnAssetCreated));

}

void FAssetTypeActions_SkeletalMesh::GetNonDestructibleActions( const TArray<TWeakObjectPtr<USkeletalMesh>>& Meshes, FMenuBuilder& MenuBuilder)
{
	// skeleton menu
	MenuBuilder.AddSubMenu(
			LOCTEXT("SkeletonSubmenu", "Skeleton"),
			LOCTEXT("SkeletonSubmenu_ToolTip", "Skeleton related actions"),
			FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_SkeletalMesh::FillSkeletonMenu, Meshes)
			);
}

void FAssetTypeActions_SkeletalMesh::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Mesh = Cast<USkeletalMesh>(*ObjIt);
		if (Mesh != NULL)
		{
			if (Mesh->Skeleton == NULL)
			{
				if ( FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("MissingSkeleton", "This mesh currently has no valid Skeleton. Would you like to create a new Skeleton?")) == EAppReturnType::Yes )
				{
					const FString DefaultSuffix = TEXT("_Skeleton");

					// Determine an appropriate name
					FString Name;
					FString PackageName;
					CreateUniqueAssetName(Mesh->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

					USkeletonFactory* Factory = NewObject<USkeletonFactory>();
					Factory->TargetSkeletalMesh = Mesh;

					FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
					AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USkeleton::StaticClass(), Factory);
				}
				else
				{
					AssignSkeletonToMesh(Mesh);
				}

				if( Mesh->Skeleton == NULL )
				{
					// error message
					FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("CreateSkeletonOrAssign", "You need to create a Skeleton or assign one in order to open this in Persona."));
				}
			}

			if ( Mesh->Skeleton != NULL )
			{
				const bool bBringToFrontIfOpen = false;
				if( IAssetEditorInstance* EditorInstance =  FAssetEditorManager::Get().FindEditorForAsset( Mesh->Skeleton, bBringToFrontIfOpen ) )
				{
					// The skeleton is already open in an editor.
					// Tell persona that a mesh was requested
					EditorInstance->FocusWindow(Mesh);
				}
				else
				{
					FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>( "Persona" );
					PersonaModule.CreatePersona( Mode, EditWithinLevelEditor, Mesh->Skeleton, NULL, NULL, Mesh );
				}
			}
		}
	}
}

UThumbnailInfo* FAssetTypeActions_SkeletalMesh::GetThumbnailInfo(UObject* Asset) const
{
	USkeletalMesh* SkeletalMesh = CastChecked<USkeletalMesh>(Asset);
	UThumbnailInfo* ThumbnailInfo = SkeletalMesh->ThumbnailInfo;
	if ( ThumbnailInfo == NULL )
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(SkeletalMesh);
		SkeletalMesh->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

void FAssetTypeActions_SkeletalMesh::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto SkeletalMesh = CastChecked<USkeletalMesh>(Asset);
		if (SkeletalMesh->AssetImportData)
		{
			OutSourceFilePaths.Add(FReimportManager::ResolveImportFilename(SkeletalMesh->AssetImportData->SourceFilePath, SkeletalMesh));
		}
	}
}

void FAssetTypeActions_SkeletalMesh::GetLODMenu(class FMenuBuilder& MenuBuilder,TArray<TWeakObjectPtr<USkeletalMesh>> Objects)
{
	check(Objects.Num() > 0);
	auto First = Objects[0];
	USkeletalMesh* SkeletalMesh = First.Get();

	for(int32 LOD = 0;LOD<=First->LODInfo.Num();++LOD)
	{
		const FText Description = FText::Format( LOCTEXT("LODLevel", "LOD {0}"), FText::AsNumber( LOD ) );
		const FText ToolTip = ( LOD == First->LODInfo.Num() ) ? LOCTEXT("NewImportTip", "Import new LOD") : LOCTEXT("ReimportTip", "Reimport over existing LOD");

		MenuBuilder.AddMenuEntry(	Description, 
									ToolTip, FSlateIcon(),
									FUIAction(FExecuteAction::CreateStatic( &FbxMeshUtils::ImportMeshLODDialog, Cast<UObject>(SkeletalMesh), LOD) )) ;
	}
}

void FAssetTypeActions_SkeletalMesh::ExecuteNewPhysicsAsset(TArray<TWeakObjectPtr<USkeletalMesh>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			CreatePhysicsAssetFromMesh(Object);
		}
	}
}

void FAssetTypeActions_SkeletalMesh::ExecuteNewSkeleton(TArray<TWeakObjectPtr<USkeletalMesh>> Objects)
{
	const FString DefaultSuffix = TEXT("_Skeleton");

	if ( Objects.Num() == 1 )
	{
		auto Object = Objects[0].Get();

		if ( Object )
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			USkeletonFactory* Factory = NewObject<USkeletonFactory>();
			Factory->TargetSkeletalMesh = Object;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USkeleton::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;
		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if ( Object )
			{
				// Determine an appropriate name
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				USkeletonFactory* Factory = NewObject<USkeletonFactory>();
				Factory->TargetSkeletalMesh = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USkeleton::StaticClass(), Factory);

				if ( NewAsset )
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if ( ObjectsToSync.Num() > 0 )
		{
			FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

void FAssetTypeActions_SkeletalMesh::ExecuteAssignSkeleton(TArray<TWeakObjectPtr<USkeletalMesh>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			AssignSkeletonToMesh(Object);
		}
	}
}

void FAssetTypeActions_SkeletalMesh::ExecuteFindSkeleton(TArray<TWeakObjectPtr<USkeletalMesh>> Objects)
{
	TArray<UObject*> ObjectsToSync;
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			USkeleton* Skeleton = Object->Skeleton;
			if (Skeleton)
			{
				ObjectsToSync.AddUnique(Skeleton);
			}
		}
	}

	if ( ObjectsToSync.Num() > 0 )
	{
		FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
	}
}


void FAssetTypeActions_SkeletalMesh::FillSkeletonMenu(FMenuBuilder& MenuBuilder, const TArray<TWeakObjectPtr<USkeletalMesh>> Meshes) const
{
	MenuBuilder.BeginSection("SkeletonMenu", LOCTEXT("SkeletonMenuHeading", "Skeleton"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("SkeletalMesh_NewSkeleton", "Create Skeleton"),
		LOCTEXT("SkeletalMesh_NewSkeletonTooltip", "Creates a new skeleton for each of the selected meshes."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "AssetIcons.Skeleton"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_SkeletalMesh::ExecuteNewSkeleton, Meshes),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SkeletalMesh_AssignSkeleton", "Assign Skeleton"),
		LOCTEXT("SkeletalMesh_AssignSkeletonTooltip", "Assigns a skeleton to the selected meshes."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.AssignSkeleton"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_SkeletalMesh::ExecuteAssignSkeleton, Meshes),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SkeletalMesh_FindSkeleton", "Find Skeleton"),
		LOCTEXT("SkeletalMesh_FindSkeletonTooltip", "Finds the skeleton used by the selected meshes in the content browser."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.FindSkeleton"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_SkeletalMesh::ExecuteFindSkeleton, Meshes),
			FCanExecuteAction()
			)
		);
}

void FAssetTypeActions_SkeletalMesh::CreatePhysicsAssetFromMesh(USkeletalMesh* SkelMesh) const
{
	// Get a unique package and asset name
	FString Name;
	FString PackageName;
	CreateUniqueAssetName(SkelMesh->GetOutermost()->GetName(), TEXT("_Physics"), PackageName, Name);

	// Then find/create it.
	UPackage* Package = CreatePackage(NULL, *PackageName);
	if ( !ensure(Package) )
	{
		// There was a problem creating the package
		return;
	}

	IPhATModule* PhATModule = &FModuleManager::LoadModuleChecked<IPhATModule>( "PhAT" );
	FPhysAssetCreateParams NewBodyData;
	EAppReturnType::Type NewBodyResponse;

	// Now show the 'asset creation' options dialog
	PhATModule->OpenNewBodyDlg(&NewBodyData, &NewBodyResponse);
	bool bWasOkClicked = (NewBodyResponse == EAppReturnType::Ok);

	if( bWasOkClicked )
	{			
		UPhysicsAsset* NewAsset = NewObject<UPhysicsAsset>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
		if(NewAsset)
		{
			// Do automatic asset generation.
			FText ErrorMessage;
			bool bSuccess = FPhysicsAssetUtils::CreateFromSkeletalMesh(NewAsset, SkelMesh, NewBodyData, ErrorMessage);
			if(bSuccess)
			{
				NewAsset->MarkPackageDirty();
				PhATModule->CreatePhAT(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), NewAsset);

				// Notify the asset registry
				FAssetRegistryModule::AssetCreated(NewAsset);

				// auto-link source skelmesh to the new physasset and recreate physics state if needed
				RefreshSkelMeshOnPhysicsAssetChange(SkelMesh);
				SkelMesh->MarkPackageDirty();
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
				NewAsset->ClearFlags( RF_Public| RF_Standalone );
			}
		}
		else
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("CreatePhysicsAsset", "CreatePhysicsAssetFailed", "Failed to create new Physics Asset.") );
		}
	}
}

void FAssetTypeActions_SkeletalMesh::AssignSkeletonToMesh(USkeletalMesh* SkelMesh) const
{
	// Create a skeleton asset from the selected skeletal mesh. Defaults to being in the same package/group as the skeletal mesh.
	TSharedRef<SWindow> WidgetWindow = SNew(SWindow)
		.Title(LOCTEXT("ChooseSkeletonWindowTitle", "Choose Skeleton"))
		.ClientSize(FVector2D(400,600));
	TSharedPtr<SSkeletonSelectorWindow> SkeletonSelectorWindow;
	WidgetWindow->SetContent
		(
			SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(SkeletonSelectorWindow, SSkeletonSelectorWindow)
					.Object(SkelMesh)
					.WidgetWindow(WidgetWindow)
			]
		);

	GEditor->EditorAddModalWindow(WidgetWindow);
	USkeleton* SelectedSkeleton = SkeletonSelectorWindow->GetSelectedSkeleton();

	// only do this if not same
	if ( SelectedSkeleton )
	{
		FDlgMergeSkeleton AssetDlg( SkelMesh, SelectedSkeleton );
		if(AssetDlg.ShowModal() == FDlgMergeSkeleton::Confirm)
		{			
			TArray<int32> RequiredBones;
			RequiredBones.Append(AssetDlg.RequiredBones);

			if ( RequiredBones.Num() > 0 )
			{
				// Do automatic asset generation.
				bool bSuccess = SelectedSkeleton->MergeBonesToBoneTree( SkelMesh, RequiredBones );
				if ( bSuccess )
				{
					FAssetNotifications::SkeletonNeedsToBeSaved(SelectedSkeleton);
				}
				else
				{
					// if failed, ask if user would like to regenerate skeleton hierarchy
					if ( EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, 
						LOCTEXT("SkeletonMergeBones_Override", "FAILED TO MERGE BONES:  \n\nThis could happen if significant hierarchical change has been made\n - i.e. inserting bone between nodes \nWould you like to regenerate Skeleton from this mesh? \n\n***WARNING: THIS WILL INVALIDATE ALL ANIMATION DATA THAT IS LINKED TO THIS SKELETON***\n") ) ) 
					{
						if ( SelectedSkeleton->RecreateBoneTree( SkelMesh ) )
						{
							FAssetNotifications::SkeletonNeedsToBeSaved( SelectedSkeleton );
						}
					}
					else
					{
						FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("SkeletonMergeBonesFailure", "Failed to merge bones to Skeleton") );
					}
				}
			}
			else
			{
				FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("SkeletonMergeBonesFailure", "Failed to merge bones to Skeleton") );
			}
		}
	}
}

void FAssetTypeActions_SkeletalMesh::OnAssetCreated(TArray<UObject*> NewAssets) const
{
	if (NewAssets.Num() > 1)
	{
		FAssetTools::Get().SyncBrowserToAssets(NewAssets);
	}

}

#undef LOCTEXT_NAMESPACE
