// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MergeActorsPrivatePCH.h"
#include "SMeshMergingDialog.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "STextComboBox.h"
#include "RawMesh.h"
#include "MeshMergingTool/MeshMergingTool.h"

#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "SMeshMergingDialog"

//////////////////////////////////////////////////////////////////////////
// SMeshMergingDialog

SMeshMergingDialog::SMeshMergingDialog()
{
	bRefreshListView = false;
	NumSelectedMeshComponents = 0;
}

SMeshMergingDialog::~SMeshMergingDialog()
{
	// Remove all delegates
	USelection::SelectionChangedEvent.RemoveAll(this);
	USelection::SelectObjectEvent.RemoveAll(this);	
	FEditorDelegates::MapChange.RemoveAll(this);
	FEditorDelegates::NewCurrentLevel.RemoveAll(this);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMeshMergingDialog::Construct(const FArguments& InArgs, FMeshMergingTool* InTool)
{
	checkf(InTool != nullptr, TEXT("Invalid owner tool supplied"));
	Tool = InTool;

	NumSelectedMeshComponents = 0;
	UpdateSelectedStaticMeshComponents();
	CreateSettingsView();
	
	// Create widget layout
	this->ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				// Static mesh component selection
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MergeStaticMeshComponentsLabel", "Mesh Components to be incorporated in the merge:"))						
					]
				]
				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					[
						SAssignNew(MeshComponentsListView, SListView<TSharedPtr<FMeshComponentData>>)
						.ListItemsSource(&SelectedMeshComponents)
						.OnGenerateRow(this, &SMeshMergingDialog::MakeComponentListItemWidget)
						.ToolTipText(LOCTEXT("SelectedComponentsListBoxToolTip", "The selected mesh components will be incorporated into the merged mesh"))
					]
			]
		]

		+ SVerticalBox::Slot()
		.Padding(0, 10, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				// Static mesh component selection
				+ SVerticalBox::Slot()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SettingsView->AsShared()
					]
				]
			]
		]

		// Replace source actors
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
		[
			SNew(SCheckBox)
			.Type(ESlateCheckBoxType::CheckBox)
			.IsChecked(this, &SMeshMergingDialog::GetReplaceSourceActors)
			.OnCheckStateChanged(this, &SMeshMergingDialog::SetReplaceSourceActors)
			.ToolTipText(LOCTEXT("ReplaceSourceActorsToolTip", "When enabled the Source Actors will be replaced with the newly generated merged mesh"))
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ReplaceSourceActorsLabel", "Replace Source Actors"))
				.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor::Yellow)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Visibility_Lambda([this]()->EVisibility { return this->GetContentEnabledState() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DeleteUndo", "Insufficient mesh components found for merging."))
			]
		]
	];	


	// Selection change
	USelection::SelectionChangedEvent.AddRaw(this, &SMeshMergingDialog::OnLevelSelectionChanged);
	USelection::SelectObjectEvent.AddRaw(this, &SMeshMergingDialog::OnLevelSelectionChanged);
	FEditorDelegates::MapChange.AddSP(this, &SMeshMergingDialog::OnMapChange);
	FEditorDelegates::NewCurrentLevel.AddSP(this, &SMeshMergingDialog::OnNewCurrentLevel);

	MergeSettings = UMeshMergingSettingsObject::Get();
	SettingsView->SetObject(MergeSettings);
}

void SMeshMergingDialog::OnMapChange(uint32 MapFlags)
{
	Reset();
}

void SMeshMergingDialog::OnNewCurrentLevel()
{
	Reset();
}

void SMeshMergingDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Check if we need to update selected components and the listbox
	if (bRefreshListView == true)
	{
		StoreCheckBoxState();
		NumSelectedMeshComponents = 0;
		UpdateSelectedStaticMeshComponents();
		MeshComponentsListView->ClearSelection();
		MeshComponentsListView->RequestListRefresh();
		bRefreshListView = false;		
	}
}

void SMeshMergingDialog::Reset()
{	
	bRefreshListView = true;
}

ECheckBoxState SMeshMergingDialog::GetReplaceSourceActors() const
{
	return (Tool->bReplaceSourceActors ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SMeshMergingDialog::SetReplaceSourceActors(ECheckBoxState NewValue)
{
	Tool->bReplaceSourceActors = (ECheckBoxState::Checked == NewValue);
}

bool SMeshMergingDialog::GetContentEnabledState() const
{
	return (GetNumSelectedMeshComponents() >= 1);
}

void SMeshMergingDialog::UpdateSelectedStaticMeshComponents()
{	
	// Retrieve selected actors
	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<AActor*> Actors;
	TArray<ULevel*> UniqueLevels;
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			Actors.Add(Actor);
			UniqueLevels.AddUnique(Actor->GetLevel());
		}
	}

	// Retrieve static mesh components from selected actors
	SelectedMeshComponents.Empty();
	for (AActor* Actor : Actors)
	{
		check(Actor != nullptr);

		TArray<UStaticMeshComponent*> StaticMeshComponents;
		Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
		for (UStaticMeshComponent* MeshComponent : StaticMeshComponents)
		{
			SelectedMeshComponents.Add(TSharedPtr<FMeshComponentData>(new FMeshComponentData(MeshComponent)));
		}		
	}
}

TSharedRef<ITableRow> SMeshMergingDialog::MakeComponentListItemWidget(TSharedPtr<FMeshComponentData> MeshComponentData, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(MeshComponentData->MeshComponent != nullptr);

	// Retrieve information about the mesh component
	const FString OwningActorName = MeshComponentData->MeshComponent->GetOwner()->GetName();
	const FString StaticMeshName = (MeshComponentData->MeshComponent->StaticMesh != nullptr) ? MeshComponentData->MeshComponent->StaticMesh->GetName() : TEXT("No Static Mesh Available");
	// Only static mesh components with valid static mesh data should be incorporated	
	MeshComponentData->bShouldIncorporate = (MeshComponentData->MeshComponent->StaticMesh != nullptr);
	const FString ComponentName = MeshComponentData->MeshComponent->GetName();

	// See if we stored a checkbox state for this mesh component, and set accordingly
	ECheckBoxState State = (MeshComponentData->MeshComponent->StaticMesh != nullptr) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	auto StoredState = StoredCheckBoxStates.Find(MeshComponentData->MeshComponent.Get());
	if (StoredState)
	{
		State = *StoredState;
		MeshComponentData->bShouldIncorporate = (State == ECheckBoxState::Checked);
		
		NumSelectedMeshComponents += (MeshComponentData->bShouldIncorporate) ? 1 : 0;
	}
	else if (MeshComponentData->bShouldIncorporate == true)
	{
		NumSelectedMeshComponents++;
	}

	return SNew(STableRow<TSharedPtr<FMeshComponentData>>, OwnerTable)
		[
			SNew(SBox)
			[
				// Disable UI element if this static mesh component has invalid static mesh data
				SNew(SHorizontalBox)				
				.IsEnabled((MeshComponentData->MeshComponent->StaticMesh != nullptr))
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked(State)
					.ToolTipText(LOCTEXT("IncorporateCheckBoxToolTip", "When ticked the Mesh Component will be incorporated into the merge"))
					
					.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
					{
						MeshComponentData->bShouldIncorporate = (NewState == ECheckBoxState::Checked);
						this->NumSelectedMeshComponents += (NewState == ECheckBoxState::Checked) ? 1 : -1;
					})
				]

				+ SHorizontalBox::Slot()
				.Padding(5.0, 0, 0, 0)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromString(OwningActorName + " - " + StaticMeshName + " - " + ComponentName))
				]
			]
		];

}

void SMeshMergingDialog::CreateSettingsView()
{
	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = true;
	DetailsViewArgs.bLockable = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ComponentsAndActorsUseNameArea;
	DetailsViewArgs.bCustomNameAreaLocation = false;
	DetailsViewArgs.bCustomFilterAreaLocation = true;
	DetailsViewArgs.DefaultsOnlyVisibility = FDetailsViewArgs::EEditDefaultsOnlyNodeVisibility::Hide;
	
		
	// Tiny hack to hide this setting, since we have no way / value to go off to 
	struct Local
	{
		/** Delegate to show all properties */
		static bool IsPropertyVisible(const FPropertyAndParent& PropertyAndParent, bool bInShouldShowNonEditable)
		{
			return (PropertyAndParent.Property.GetFName() != GET_MEMBER_NAME_CHECKED(FMaterialProxySettings, GutterSpace));
		}
	};

	SettingsView = EditModule.CreateDetailView(DetailsViewArgs);
	SettingsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateStatic(&Local::IsPropertyVisible, true));
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMeshMergingDialog::OnLevelSelectionChanged(UObject* Obj)
{
	Reset();
}

void SMeshMergingDialog::StoreCheckBoxState()
{
	StoredCheckBoxStates.Empty();

	// Loop over selected mesh component and store its checkbox state
	for (TSharedPtr<FMeshComponentData> SelectedComponent : SelectedMeshComponents )
	{
		UStaticMeshComponent* MeshComponent = SelectedComponent->MeshComponent.Get();
		const ECheckBoxState State = SelectedComponent->bShouldIncorporate ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		StoredCheckBoxStates.Add(MeshComponent, State);
	}
}

#undef LOCTEXT_NAMESPACE