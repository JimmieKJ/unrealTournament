// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "FbxImporter.h"
#include "Widgets/Input/STextComboBox.h"
#include "SFbxSceneListView.h"
#include "FbxSceneOptionWindow.h"
#include "ClassIconFinder.h"

#define LOCTEXT_NAMESPACE "SFbxSceneStaticMeshListView"

/** The item used for visualizing the class in the tree. */
class SFbxMeshItem : public STableRow< FbxMeshInfoPtr >
{
public:

	SLATE_BEGIN_ARGS(SFbxMeshItem)
		: _FbxMeshInfo(NULL)
		, _SceneMeshOverrideOptions(NULL)
	{}

	/** The item content. */
	SLATE_ARGUMENT(FbxMeshInfoPtr, FbxMeshInfo)
	SLATE_ARGUMENT(MeshInfoOverrideOptions*, SceneMeshOverrideOptions)
	SLATE_END_ARGS()

	/**
	* Construct the widget
	*
	* @param InArgs   A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		FbxMeshInfo = InArgs._FbxMeshInfo;
		SceneMeshOverrideOptions = InArgs._SceneMeshOverrideOptions;

		//This is suppose to always be valid
		check(FbxMeshInfo.IsValid());
		check(SceneMeshOverrideOptions);

		UClass *IconClass = FbxMeshInfo->GetType();
		const FSlateBrush* ClassIcon = FClassIconFinder::FindIconForClass(IconClass);

		TSharedRef<SOverlay> IconContent = SNew(SOverlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(ClassIcon)
			];

		
		// Add the component mobility icon
		IconContent->AddSlot()
			.HAlign(HAlign_Left)
			[
				SNew(SImage)
				.Image(this, &SFbxMeshItem::GetBrushForOverrideIcon)
			];

		this->ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SFbxMeshItem::OnItemCheckChanged)
				.IsChecked(this, &SFbxMeshItem::IsItemChecked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 2.0f, 6.0f, 2.0f)
			[
				IconContent
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 3.0f, 6.0f, 3.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FbxMeshInfo->Name))
				.ToolTipText(FText::FromString(FbxMeshInfo->Name))
			]
		];

		STableRow< FbxMeshInfoPtr >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true),
			InOwnerTableView
			);
	}
	const FSlateBrush* GetBrushForOverrideIcon() const
	{
		if (SceneMeshOverrideOptions->Contains(FbxMeshInfo))
		{
			return FEditorStyle::GetBrush("FBXIcon.ImportOptionsOverride");
		}
		return FEditorStyle::GetBrush("FBXIcon.ImportOptionsDefault");
	}

private:

	void OnItemCheckChanged(ECheckBoxState CheckType)
	{
		if (!FbxMeshInfo.IsValid())
			return;
		FbxMeshInfo->bImportAttribute = CheckType == ECheckBoxState::Checked;
	}

	ECheckBoxState IsItemChecked() const
	{
		return FbxMeshInfo->bImportAttribute ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/** The node info to build the tree view row from. */
	FbxMeshInfoPtr FbxMeshInfo;
	MeshInfoOverrideOptions *SceneMeshOverrideOptions;
};

//////////////////////////////////////////////////////////////////////////
// Static Mesh List

SFbxSceneStaticMeshListView::~SFbxSceneStaticMeshListView()
{
	FbxMeshesArray.Empty();
	SceneInfo = NULL;
	GlobalImportSettings = NULL;
	SceneMeshOverrideOptions = NULL;
	SceneImportOptionsStaticMeshDisplay = NULL;
}

void SFbxSceneStaticMeshListView::Construct(const SFbxSceneStaticMeshListView::FArguments& InArgs)
{
	SceneInfo = InArgs._SceneInfo;
	GlobalImportSettings = InArgs._GlobalImportSettings;
	SceneMeshOverrideOptions = InArgs._SceneMeshOverrideOptions;
	SceneImportOptionsStaticMeshDisplay = InArgs._SceneImportOptionsStaticMeshDisplay;

	check(SceneInfo.IsValid());
	check(GlobalImportSettings != NULL);
	check(SceneMeshOverrideOptions != NULL)
	check(SceneImportOptionsStaticMeshDisplay != NULL);

	SFbxSceneOptionWindow::CopyStaticMeshOptionsToFbxOptions(GlobalImportSettings, SceneImportOptionsStaticMeshDisplay);
	//Set the default options to the current global import settings
	GlobalImportSettings->bTransformVertexToAbsolute = false;
	GlobalImportSettings->StaticMeshLODGroup = NAME_None;
	CurrentStaticMeshImportOptions = GlobalImportSettings;
	
	for (auto MeshInfoIt = SceneInfo->MeshInfo.CreateIterator(); MeshInfoIt; ++MeshInfoIt)
	{
		FbxMeshInfoPtr MeshInfo = (*MeshInfoIt);

		if (!MeshInfo->bIsSkelMesh)
		{
			FbxMeshesArray.Add(MeshInfo);
		}
	}

	SListView::Construct
	(
		SListView::FArguments()
		.ListItemsSource(&FbxMeshesArray)
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SFbxSceneStaticMeshListView::OnGenerateRowFbxSceneListView)
		.OnContextMenuOpening(this, &SFbxSceneStaticMeshListView::OnOpenContextMenu)
		.OnSelectionChanged(this, &SFbxSceneStaticMeshListView::OnSelectionChanged)
	);
}

TSharedRef< ITableRow > SFbxSceneStaticMeshListView::OnGenerateRowFbxSceneListView(FbxMeshInfoPtr Item, const TSharedRef< STableViewBase >& OwnerTable)
{
	TSharedRef< SFbxMeshItem > ReturnRow = SNew(SFbxMeshItem, OwnerTable)
		.FbxMeshInfo(Item)
		.SceneMeshOverrideOptions(SceneMeshOverrideOptions);
	return ReturnRow;
}

TSharedPtr<SWidget> SFbxSceneStaticMeshListView::OnOpenContextMenu()
{
	TArray<FbxMeshInfoPtr> SelectedItems;
	int32 SelectCount = GetSelectedItems(SelectedItems);
	// Build up the menu for a selection
	const bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder(bCloseAfterSelection, TSharedPtr<FUICommandList>());

	// We always create a section here, even if there is no parent so that clients can still extend the menu
	MenuBuilder.BeginSection("FbxScene_SM_ImportSection");
	{
		const FSlateIcon PlusIcon(FEditorStyle::GetStyleSetName(), "Plus");
		MenuBuilder.AddMenuEntry(LOCTEXT("CheckForImport", "Add Selection To Import"), FText(), PlusIcon, FUIAction(FExecuteAction::CreateSP(this, &SFbxSceneStaticMeshListView::AddSelectionToImport)));
		const FSlateIcon MinusIcon(FEditorStyle::GetStyleSetName(), "PropertyWindow.Button_RemoveFromArray");
		MenuBuilder.AddMenuEntry(LOCTEXT("UncheckForImport", "Remove Selection From Import"), FText(), MinusIcon, FUIAction(FExecuteAction::CreateSP(this, &SFbxSceneStaticMeshListView::RemoveSelectionFromImport)));
		MenuBuilder.AddMenuSeparator(TEXT("FbxImportScene_SM_Separator"));
	}
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("FbxScene_SM_OptionsSection", LOCTEXT("FbxScene_SM_Options", "Options:"));
	{
		for (auto OptionName : OverrideNameOptions)
		{
			MenuBuilder.AddMenuEntry(FText::FromString(*OptionName.Get()), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SFbxSceneStaticMeshListView::AssignToOptions, OptionName)));
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SFbxSceneStaticMeshListView::AssignToOptions(TSharedPtr<FString> OptionName)
{
	bool IsDefaultOptions = OptionName.Get() == DefaultOptionNamePtr.Get();
	if (!OverrideNameOptionsMap.Contains(OptionName) && !IsDefaultOptions)
	{
		return;
	}
	UnFbx::FBXImportOptions *FbxOptionsToAssign = IsDefaultOptions ? GlobalImportSettings : *OverrideNameOptionsMap.Find(OptionName);
	TArray<FbxMeshInfoPtr> SelectedItems;
	GetSelectedItems(SelectedItems);
	for (auto Item : SelectedItems)
	{
		FbxMeshInfoPtr ItemPtr = Item;
		if (SceneMeshOverrideOptions->Contains(ItemPtr))
		{
			SceneMeshOverrideOptions->Remove(ItemPtr);
		}
		if (!IsDefaultOptions)
		{
			SceneMeshOverrideOptions->Add(ItemPtr, FbxOptionsToAssign);
		}
	}
	OptionComboBox->SetSelectedItem(OptionName);
}

void SFbxSceneStaticMeshListView::AddSelectionToImport()
{
	SetSelectionImportState(true);
}

void SFbxSceneStaticMeshListView::RemoveSelectionFromImport()
{
	SetSelectionImportState(false);
}

void SFbxSceneStaticMeshListView::SetSelectionImportState(bool MarkForImport)
{
	TArray<FbxMeshInfoPtr> SelectedItems;
	GetSelectedItems(SelectedItems);
	for (auto Item : SelectedItems)
	{
		FbxMeshInfoPtr ItemPtr = Item;
		ItemPtr->bImportAttribute = MarkForImport;
	}
}

void SFbxSceneStaticMeshListView::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	SFbxSceneOptionWindow::CopyStaticMeshOptionsToFbxOptions(CurrentStaticMeshImportOptions, SceneImportOptionsStaticMeshDisplay);
}


void SFbxSceneStaticMeshListView::OnSelectionChanged(FbxMeshInfoPtr Item, ESelectInfo::Type SelectionType)
{
}


bool SFbxSceneStaticMeshListView::CanDeleteOverride()  const
{
	return CurrentStaticMeshImportOptions != GlobalImportSettings;
}

FReply SFbxSceneStaticMeshListView::OnDeleteOverride()
{
	if (!CanDeleteOverride())
	{
		return FReply::Unhandled();
	}

	TArray<TSharedPtr<FFbxMeshInfo>> RemoveKeys;
	//Find all asset that use the current option
	for (auto iter = SceneMeshOverrideOptions->CreateIterator(); iter; ++iter)
	{
		if (iter.Value() == CurrentStaticMeshImportOptions)
		{
			RemoveKeys.Add(iter.Key());
		}
	}
	for (auto Item : RemoveKeys)
	{
		SceneMeshOverrideOptions->Remove(Item);
	}

	UnFbx::FBXImportOptions *OldOverrideOption = CurrentStaticMeshImportOptions;
	delete OldOverrideOption;

	CurrentStaticMeshImportOptions = GlobalImportSettings;
	OptionComboBox->SetSelectedItem(OverrideNameOptions[0]);

	return FReply::Handled();
}

FReply SFbxSceneStaticMeshListView::OnSelectAssetUsing()
{
	ClearSelection();
	if (CurrentStaticMeshImportOptions == GlobalImportSettings)
	{
		for (auto MeshInfo : FbxMeshesArray)
		{
			if (!SceneMeshOverrideOptions->Contains(MeshInfo))
			{
				SetItemSelection(MeshInfo, true);
			}
		}
	}
	else
	{
		for (auto iter = SceneMeshOverrideOptions->CreateIterator(); iter; ++iter)
		{
			if (iter.Value() == CurrentStaticMeshImportOptions)
			{
				SetItemSelection(iter.Key(), true);
			}
		}
	}
	return FReply::Handled();
}

void SFbxSceneStaticMeshListView::OnChangedOverrideOptions(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo)
{
	if (ItemSelected->Compare("Default") == 0)
	{
		CurrentStaticMeshImportOptions = GlobalImportSettings;
	}
	else if(OverrideNameOptionsMap.Contains(ItemSelected))
	{
		CurrentStaticMeshImportOptions = *OverrideNameOptionsMap.Find(ItemSelected);
	}
	SFbxSceneOptionWindow::CopyFbxOptionsToStaticMeshOptions(CurrentStaticMeshImportOptions, SceneImportOptionsStaticMeshDisplay);
}

FReply SFbxSceneStaticMeshListView::OnCreateOverrideOptions()
{
	UnFbx::FBXImportOptions *OverrideOption = new UnFbx::FBXImportOptions();
	SFbxSceneOptionWindow::CopyFbxOptionsToFbxOptions(GlobalImportSettings, OverrideOption);
	TSharedPtr<FString> OverrideName = MakeShareable(new FString(FGuid::NewGuid().ToString()));
	OverrideNameOptions.Add(OverrideName);
	OverrideNameOptionsMap.Add(OverrideName, OverrideOption);
	//Update the selection to the new override
	OptionComboBox->SetSelectedItem(OverrideName);
	return FReply::Handled();
}

TSharedPtr<STextComboBox> SFbxSceneStaticMeshListView::CreateOverrideOptionComboBox()
{

	DefaultOptionNamePtr = MakeShareable(new FString("Default"));
	OverrideNameOptions.Add(DefaultOptionNamePtr);
	OptionComboBox = SNew(STextComboBox)
		.OptionsSource(&OverrideNameOptions)
		.InitiallySelectedItem(DefaultOptionNamePtr)
		.OnSelectionChanged(this, &SFbxSceneStaticMeshListView::OnChangedOverrideOptions);
	return OptionComboBox;
}

void SFbxSceneStaticMeshListView::OnToggleSelectAll(ECheckBoxState CheckType)
{
	for (auto MeshInfo : FbxMeshesArray)
	{
		if (!SceneMeshOverrideOptions->Contains(MeshInfo))
		{
			MeshInfo->bImportAttribute = CheckType == ECheckBoxState::Checked;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Skeletal Mesh List

#undef LOCTEXT_NAMESPACE
