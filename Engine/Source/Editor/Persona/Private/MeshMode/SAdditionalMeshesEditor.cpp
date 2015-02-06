// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"

#include "Persona.h"
#include "ModuleManager.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "AssetData.h"

#include "SAnimationEditorViewport.h"
#include "SAdditionalMeshesEditor.h"

#define LOCTEXT_NAMESPACE "PersonaAdditionalMeshesEditor"

/** Delegate used when the remove button is clicked on a mesh item*/
DECLARE_DELEGATE_OneParam(FOnMeshItemRemoved, TSharedRef<class SAdditionalMeshItem>  /*PanelWidget*/);

/** Delegate used when the user removes an additional mesh */
DECLARE_DELEGATE_OneParam(FOnMeshRemoved, class USkeletalMeshComponent* /*MeshToRemove*/);


/**
 * Implements an individual widget for an Additional Mesh in the display panel
 */
class SAdditionalMeshItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAdditionalMeshItem){}
		SLATE_EVENT(FOnMeshItemRemoved, OnRemoveItem)
	SLATE_END_ARGS()

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs	- The construction arguments.
	 * @param Mesh		- The mesh component that this item represents
	 */
	void Construct(const FArguments& InArgs, USkeletalMeshComponent* _MeshComponent);

	/**
	 * Handler for the widgets remove button
	 */
	FReply RemoveButtonHandler();

	USkeletalMeshComponent* MeshComponent;
private:

	FOnMeshItemRemoved OnRemoveItem;
};

FReply SAdditionalMeshItem::RemoveButtonHandler()
{
	OnRemoveItem.ExecuteIfBound(SharedThis(this));
	return FReply::Handled();
}

void SAdditionalMeshItem::Construct(const FArguments& InArgs, USkeletalMeshComponent* _MeshComponent)
{
	OnRemoveItem = InArgs._OnRemoveItem;
	MeshComponent = _MeshComponent;

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor( FLinearColor(.2f,.2f,.2f,.2f) )
		.BorderImage( FEditorStyle::GetBrush( "DetailsView.GroupSection" ) )
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(MeshComponent->SkeletalMesh->GetName()))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonColorAndOpacity(FLinearColor::Transparent)
				.ContentPadding(0)
				.OnClicked(this, &SAdditionalMeshItem::RemoveButtonHandler)
				[
					SNew(SImage)
					.Image( FEditorStyle::GetBrush("ExpandableButton.CloseButton") )
				]
			]
		]
	];

}

/**
 * Implements the additional meshes display panel
 */
class SAdditionalMeshesDisplayPanel : public SWrapBox
{
public:
	SLATE_BEGIN_ARGS(SAdditionalMeshesDisplayPanel)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}
		SLATE_EVENT(FOnMeshRemoved, OnRemoveAdditionalMesh)
	SLATE_END_ARGS()

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs	- The construction arguments.
	 */
	void Construct(const FArguments& InArgs); 

	/**
	 * Adds a mesh widget to the panel
	 *
	 * @param Mesh	- The mesh component to add.
	 */
	void AddMesh(USkeletalMeshComponent* Mesh);

	/**
	 * Handler - Called by a SAdditionalMeshItem when it is removed
	 *
	 * @param MeshItem	- The panels item that was removed.
	 */
	void OnMeshItemRemovedFromPanel(TSharedRef<SAdditionalMeshItem> PanelWidget);

private:

	// Called when a user chooses to remove a mesh from the panel
	FOnMeshRemoved OnRemoveAdditionalMesh;
};

void SAdditionalMeshesDisplayPanel::Construct(const FArguments& InArgs)
{
	SWrapBox::Construct(SWrapBox::FArguments().UseAllottedWidth(true));

	OnRemoveAdditionalMesh = InArgs._OnRemoveAdditionalMesh;
}

void SAdditionalMeshesDisplayPanel::AddMesh(USkeletalMeshComponent* Mesh)
{
	AddSlot()
	.Padding(1.f)
	[
		SNew(SAdditionalMeshItem, Mesh)
		.OnRemoveItem(this, &SAdditionalMeshesDisplayPanel::OnMeshItemRemovedFromPanel)
	];
}

void SAdditionalMeshesDisplayPanel::OnMeshItemRemovedFromPanel(TSharedRef<SAdditionalMeshItem> PanelWidget)
{
	OnRemoveAdditionalMesh.ExecuteIfBound(PanelWidget->MeshComponent);
	RemoveSlot(PanelWidget);
}

////////////////////////////////////////////////////////////
// SAdditionalMeshesEditor

void SAdditionalMeshesEditor::Construct(const FArguments& InArgs, TSharedPtr<FPersona> InPersona)
{
	PersonaPtr = InPersona;

	USkeleton* Skeleton = PersonaPtr.Pin()->GetSkeleton();
	SkeletonNameAssetFilter = FString::Printf(TEXT("%s'%s'"), *Skeleton->GetClass()->GetName(), *Skeleton->GetPathName());

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(PickerComboButton, SComboButton)
				.ContentPadding(3)
				.OnGetMenuContent(this, &SAdditionalMeshesEditor::MakeAssetPickerMenu)
				.ButtonContent()
				[
					SNew(STextBlock) .Text(LOCTEXT("AdditionalMeshesAddButtonLabel", "Add Mesh"))
					.ToolTip(IDocumentation::Get()->CreateToolTip(
					LOCTEXT("AdditionalMeshesAddButtonTooltip", "Add a body part which is a skeletal mesh sharing the same skeleton"),
					NULL,
					TEXT("Shared/Editors/Persona"),
					TEXT("AdditionalMeshes")))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SAdditionalMeshesEditor::OnClearAllClicked)
				.Text(LOCTEXT("AdditionalMeshesClearButtonLabel", "Clear All"))
			]
		]
		+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f, 4.0f)
			[
				SAssignNew(LoadedMeshButtonsHolder, SAdditionalMeshesDisplayPanel)
				.OnRemoveAdditionalMesh(this, &SAdditionalMeshesEditor::OnRemoveAdditionalMesh)
			]
	];

	for(auto Iter = PersonaPtr.Pin()->AdditionalMeshes.CreateIterator(); Iter; ++Iter)
	{
		LoadedMeshButtonsHolder->AddMesh((*Iter));
	}
}

FPreviewScene* SAdditionalMeshesEditor::GetPersonaPreviewScene()
{
	return &PersonaPtr.Pin()->GetPreviewScene();
}

FReply SAdditionalMeshesEditor::OnClearAllClicked()
{
	PersonaPtr.Pin()->ClearAllAdditionalMeshes();

	LoadedMeshButtonsHolder->ClearChildren();

	return FReply::Handled();
}

TSharedRef<SWidget> SAdditionalMeshesEditor::MakeAssetPickerMenu()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;

	UClass* FilterClass = USkeletalMesh::StaticClass();
	if (FilterClass != NULL)
	{
		AssetPickerConfig.Filter.ClassNames.Add(FilterClass->GetFName());
		AssetPickerConfig.Filter.bRecursiveClasses = true;
	}

	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SAdditionalMeshesEditor::OnAssetSelectedFromPicker);
	AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SAdditionalMeshesEditor::ShouldFilterAssetBasedOnSkeleton);;

	return SNew(SBox)
		.WidthOverride(384)
		.HeightOverride(768)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];
}

void SAdditionalMeshesEditor::OnAssetSelectedFromPicker(const FAssetData& AssetData)
{
	PickerComboButton->SetIsOpen(false);
	UObject* Asset = AssetData.GetAsset();

	if(USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset))
	{
		USkeletalMeshComponent* NewComp = PersonaPtr.Pin()->CreateNewSkeletalMeshComponent();

		NewComp->SetSkeletalMesh(Mesh);
		NewComp->UpdateMasterBoneMap();

		FPreviewScene* PreviewScene = GetPersonaPreviewScene();
		check(PreviewScene);

		PreviewScene->AddComponent(NewComp, FTransform::Identity);

		LoadedMeshButtonsHolder->AddMesh(NewComp);
	}
	
}

bool SAdditionalMeshesEditor::ShouldFilterAssetBasedOnSkeleton(const FAssetData& AssetData)
{
	const FString* SkeletonName = AssetData.TagsAndValues.Find(TEXT("Skeleton"));

	if ( SkeletonName )
	{
		if ( (*SkeletonName) == SkeletonNameAssetFilter )
		{
			return false;
		}
	}

	return true;
}

void SAdditionalMeshesEditor::OnRemoveAdditionalMesh(USkeletalMeshComponent* MeshToRemove)
{
	PersonaPtr.Pin()->RemoveAdditionalMesh(MeshToRemove);
}

#undef LOCTEXT_NAMESPACE
