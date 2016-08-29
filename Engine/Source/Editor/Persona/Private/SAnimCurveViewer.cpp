// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "SAnimCurveViewer.h"
#include "ObjectTools.h"
#include "AssetRegistryModule.h"
#include "ScopedTransaction.h"
#include "SSearchBox.h"
#include "SInlineEditableTextBlock.h"
#include "STextEntryPopup.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

#define LOCTEXT_NAMESPACE "SAnimCurveViewer"

static const FName ColumnId_AnimCurveNameLabel( "Curve Name" );
static const FName ColumnID_AnimCurveWeightLabel( "Weight" );
static const FName ColumnID_AnimCurveEditLabel( "Edit" );

const float MaxMorphWeight = 5.f;

//////////////////////////////////////////////////////////////////////////
// SAnimCurveListRow



void SAnimCurveListRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	Item = InArgs._Item;
	AnimCurveViewerPtr = InArgs._AnimCurveViewerPtr;

	check( Item.IsValid() );

	SMultiColumnTableRow< TSharedPtr<FDisplayedAnimCurveInfo> >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
}

TSharedRef< SWidget > SAnimCurveListRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	if ( ColumnName == ColumnId_AnimCurveNameLabel )
	{
		TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
		if (AnimCurveViewer.IsValid())
		{
			return
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				.VAlign(VAlign_Center)
				[
					SAssignNew(Item->EditableText, SInlineEditableTextBlock)
					.OnTextCommitted(AnimCurveViewer.Get(), &SAnimCurveViewer::OnNameCommitted, Item)
					.ColorAndOpacity(this, &SAnimCurveListRow::GetItemTextColor)
					.IsSelected(this, &SAnimCurveListRow::IsSelected)
					.Text(this, &SAnimCurveListRow::GetItemName)
					.HighlightText(this, &SAnimCurveListRow::GetFilterText)
				];
		}
		else
		{
			return SNullWidget::NullWidget;
		}
	}
	else if ( ColumnName == ColumnID_AnimCurveWeightLabel )
	{
		// Encase the SSpinbox in an SVertical box so we can apply padding. Setting ItemHeight on the containing SListView has no effect :-(
		return
			SNew( SVerticalBox )

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 0.0f, 1.0f )
			.VAlign( VAlign_Center )
			[
				SNew( SSpinBox<float> )
				.MinSliderValue(-1.f)
				.MaxSliderValue(1.f)
				.MinValue(-MaxMorphWeight)
				.MaxValue(MaxMorphWeight)
				.Value( this, &SAnimCurveListRow::GetWeight )
				.OnValueChanged( this, &SAnimCurveListRow::OnAnimCurveWeightChanged )
				.OnValueCommitted( this, &SAnimCurveListRow::OnAnimCurveWeightValueCommitted )
			];
	}
	else 
	{
		return
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 1.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SAnimCurveListRow::OnAnimCurveAutoFillChecked)
				.IsChecked(this, &SAnimCurveListRow::IsAnimCurveAutoFillChangedChecked)
			];
	}
}

void SAnimCurveListRow::OnAnimCurveAutoFillChecked(ECheckBoxState InState)
{
	Item->bAutoFillData = InState == ECheckBoxState::Checked;

	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		if (Item->bAutoFillData)
		{
			// clear value so that it can be filled up
			AnimCurveViewer->RemoveAnimCurveOverride(Item->SmartName.DisplayName);
		}
		else
		{
			AnimCurveViewer->AddAnimCurveOverride(Item->SmartName.DisplayName, Item->Weight);
		}
	}
}

ECheckBoxState SAnimCurveListRow::IsAnimCurveAutoFillChangedChecked() const
{
	return (Item->bAutoFillData)? ECheckBoxState::Checked: ECheckBoxState::Unchecked;
}

void SAnimCurveListRow::OnAnimCurveWeightChanged( float NewWeight )
{
	float NewValidWeight = FMath::Clamp(NewWeight, -MaxMorphWeight, MaxMorphWeight);
	Item->Weight = NewValidWeight;
	Item->bAutoFillData = false;

	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		// If adjusting an element that is not selected, make it the only selection
		if (!AnimCurveViewer->AnimCurveListView->IsItemSelected(Item))
		{
			AnimCurveViewer->AnimCurveListView->SetSelection(Item);
		}

		// Add override
		AnimCurveViewer->AddAnimCurveOverride(Item->SmartName.DisplayName, Item->Weight);

		// ...then any selected rows need changing by the same delta
		TArray< TSharedPtr< FDisplayedAnimCurveInfo > > SelectedRows = AnimCurveViewer->AnimCurveListView->GetSelectedItems();
		for (auto ItemIt = SelectedRows.CreateIterator(); ItemIt; ++ItemIt)
		{
			TSharedPtr< FDisplayedAnimCurveInfo > RowItem = (*ItemIt);

			if (RowItem != Item) // Don't do "this" row again if it's selected
			{
				RowItem->Weight = NewValidWeight;
				RowItem->bAutoFillData = false;
				AnimCurveViewer->AddAnimCurveOverride(RowItem->SmartName.DisplayName, RowItem->Weight);
			}
		}

		// Refresh viewport
		TSharedPtr<FPersona> Persona = AnimCurveViewer->PersonaPtr.Pin();
		if (Persona.IsValid())
		{
			Persona->RefreshViewport();
		}
	}
}

void SAnimCurveListRow::OnAnimCurveWeightValueCommitted( float NewWeight, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
	{
		OnAnimCurveWeightChanged(NewWeight);
	}
}

FText SAnimCurveListRow::GetItemName() const
{
	FName ItemName;

	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		const FSmartNameMapping* Mapping = Item->Skeleton->GetSmartNameContainer(AnimCurveViewer->ContainerName);
		check(Mapping);
		Mapping->GetName(Item->SmartName.UID, ItemName);
	}

	return FText::FromName(ItemName);
}

FText SAnimCurveListRow::GetFilterText() const
{
	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		return AnimCurveViewer->GetFilterText();
	}
	else
	{
		return FText::GetEmpty();
	}
}


bool SAnimCurveListRow::GetActiveWeight(float& OutWeight) const
{
	bool bFoundActive = false;

	// If anim viewer
	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		// If persona
		TSharedPtr<FPersona> Persona = AnimCurveViewer->PersonaPtr.Pin();
		if (Persona.IsValid())
		{
			// If anim instance
			UAnimInstance* AnimInstance = Persona->GetPreviewAnimInstance();
			if (AnimInstance)
			{
				// See if curve is in active set
				TMap<FName, float> CurveList;
				AnimInstance->GetAnimationCurveList(ACF_EditorPreviewCurves, CurveList);

				float* CurrentValue = CurveList.Find(Item->SmartName.DisplayName);
				if (CurrentValue)
				{
					OutWeight = *CurrentValue;
					// Remember we found it
					bFoundActive = true;
				}
			}
		}
	}

	return bFoundActive;
}


FSlateColor SAnimCurveListRow::GetItemTextColor() const
{
	// If row is selected, show text as black to make it easier to read
	if (IsSelected())
	{
		return FLinearColor(0, 0, 0);
	}

	// If not selected, show bright if active
	bool bItemActive = true;
	if (Item->bAutoFillData)
	{
		float Weight = 0.f;
		bItemActive = GetActiveWeight(Weight);
	}

	return bItemActive ? FLinearColor(1, 1, 1) : FLinearColor(0.5, 0.5, 0.5);
}

float SAnimCurveListRow::GetWeight() const 
{ 
	float Weight = Item->Weight;

	if (Item->bAutoFillData)
	{
		GetActiveWeight(Weight);
	}

	return Weight;
}

//////////////////////////////////////////////////////////////////////////
// SAnimCurveTypeList

class SAnimCurveTypeList : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SAnimCurveTypeList) {}

		/** The item for this row **/
		SLATE_ARGUMENT(int32, CurveFlags)

		/* The SAnimCurveViewer that we push the morph target weights into */
		SLATE_ARGUMENT(TWeakPtr<SAnimCurveViewer>, AnimCurveViewerPtr)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Auto fill check call back functions */
	void OnAnimCurveTypeShowChecked(ECheckBoxState InState);
	ECheckBoxState IsAnimCurveTypeShowChangedChecked() const;

	FText GetAnimCurveType() const;
private:

	/* The SAnimCurveViewer that we push the morph target weights into */
	TWeakPtr<SAnimCurveViewer> AnimCurveViewerPtr;

	/** The name and weight of the morph target */
	int32 CurveFlags;
};

void SAnimCurveTypeList::Construct(const FArguments& InArgs)
{
	CurveFlags = InArgs._CurveFlags;
	AnimCurveViewerPtr = InArgs._AnimCurveViewerPtr;

	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		ChildSlot
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 1.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SAnimCurveTypeList::OnAnimCurveTypeShowChecked)
				.IsChecked(this, &SAnimCurveTypeList::IsAnimCurveTypeShowChangedChecked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(3.0f, 1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAnimCurveTypeList::GetAnimCurveType)
				.HighlightText(AnimCurveViewer->GetFilterText())
			]
		];
	}

}

void SAnimCurveTypeList::OnAnimCurveTypeShowChecked(ECheckBoxState InState)
{
	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		// clear value so that it can be filled up
		if (InState == ECheckBoxState::Checked)
		{
			AnimCurveViewer->CurrentCurveFlag |= CurveFlags;
		}
		else
		{
			AnimCurveViewer->CurrentCurveFlag &= ~CurveFlags;
		}

		AnimCurveViewer->RefreshCurveList();
	}
}

ECheckBoxState SAnimCurveTypeList::IsAnimCurveTypeShowChangedChecked() const
{
	TSharedPtr<SAnimCurveViewer> AnimCurveViewer = AnimCurveViewerPtr.Pin();
	if (AnimCurveViewer.IsValid())
	{
		return (AnimCurveViewer->CurrentCurveFlag & CurveFlags) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}

FText SAnimCurveTypeList::GetAnimCurveType() const
{
	switch (CurveFlags)
	{
	case ACF_DriveMorphTarget:
		return LOCTEXT("AnimCurveType_Morphtarget", "Morph Target");
	case ACF_DriveAttribute:
		return LOCTEXT("AnimCurveType_Attribute", "Attribute");
	case ACF_DriveMaterial:
		return LOCTEXT("AnimCurveType_Material", "Material");
		// @fixme: when we move curve types to skeleton fix this
	case ACF_Metadata:
		return LOCTEXT("AnimCurveType_Metadata", "Meta Data");
	}

	return LOCTEXT("AnimCurveType_Unknown", "Unknown");
}
//////////////////////////////////////////////////////////////////////////
// SAnimCurveViewer

void SAnimCurveViewer::Construct(const FArguments& InArgs)
{
	PersonaPtr = InArgs._Persona;

	ContainerName = USkeleton::AnimCurveMappingName;

	CachedPreviewInstance = nullptr;

	bShowAllCurves = true;

	TSharedPtr<FPersona> Persona = PersonaPtr.Pin();
	if (Persona.IsValid())
	{
		CurrentSkeleton = Persona->GetSkeleton();

		Persona->RegisterOnPreviewMeshChanged(FPersona::FOnPreviewMeshChanged::CreateSP(this, &SAnimCurveViewer::OnPreviewMeshChanged));
		Persona->RegisterOnAnimChanged(FPersona::FOnAnimChanged::CreateSP(this, &SAnimCurveViewer::OnPreviewAssetChanged));
		Persona->RegisterOnPostUndo(FPersona::FOnPostUndo::CreateSP(this, &SAnimCurveViewer::OnPostUndo));
		Persona->RegisterOnChangeCurves(FPersona::FOnCurvesChanged::CreateSP(this, &SAnimCurveViewer::OnCurvesChanged));
	}

	// @todo fix this to be filtered
	CurrentCurveFlag = ACF_DriveMorphTarget | ACF_DriveMaterial | ACF_DriveAttribute;

	RefreshCachePreviewInstance();
	 
	TSharedPtr<SHorizontalBox> AnimTypeBoxContainer = SNew(SHorizontalBox);

	ChildSlot
	[
		SNew( SVerticalBox )
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0,2)
		[
			SNew(SHorizontalBox)
			// Filter entry
			+SHorizontalBox::Slot()
			.FillWidth( 1 )
			[
				SAssignNew( NameFilterBox, SSearchBox )
				.SelectAllTextWhenFocused( true )
				.OnTextChanged( this, &SAnimCurveViewer::OnFilterTextChanged )
				.OnTextCommitted( this, &SAnimCurveViewer::OnFilterTextCommitted )
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()		// This is required to make the scrollbar work, as content overflows Slate containers by default
		[
			SNew(SBox)
			.WidthOverride(150)
			.Content()
			[
				AnimTypeBoxContainer.ToSharedRef()
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight( 1.0f )		// This is required to make the scrollbar work, as content overflows Slate containers by default
		[
			SAssignNew( AnimCurveListView, SAnimCurveListType )
			.ListItemsSource( &AnimCurveList )
			.OnGenerateRow( this, &SAnimCurveViewer::GenerateAnimCurveRow )
			.OnContextMenuOpening( this, &SAnimCurveViewer::OnGetContextMenuContent )
			.ItemHeight( 22.0f )
			.SelectionMode(ESelectionMode::Multi)
			.HeaderRow
			(
				SNew( SHeaderRow )
				+ SHeaderRow::Column( ColumnId_AnimCurveNameLabel )
				.FillWidth(1.f)
				.DefaultLabel( LOCTEXT( "AnimCurveNameLabel", "Curve Name" ) )

				+ SHeaderRow::Column( ColumnID_AnimCurveWeightLabel )
				.FillWidth(1.f)
				.DefaultLabel( LOCTEXT( "AnimCurveWeightLabel", "Weight" ) )

				+ SHeaderRow::Column(ColumnID_AnimCurveEditLabel)
				.FillWidth(0.25f)
				.DefaultLabel(LOCTEXT("AnimCurveEditLabel", "Auto"))
			)
		]
	];

	CreateAnimCurveTypeList(AnimTypeBoxContainer.ToSharedRef());
	CreateAnimCurveList();
}

SAnimCurveViewer::~SAnimCurveViewer()
{
	TSharedPtr<FPersona> Persona = PersonaPtr.Pin();
	if (Persona.IsValid())
	{
		// if persona isn't there, we probably don't have the preview mesh either, so no valid anim instance
		if (CachedPreviewInstance && OnAddAnimationCurveDelegate.IsBound())
		{
			CachedPreviewInstance->RemoveDelegate_AddCustomAnimationCurve(OnAddAnimationCurveDelegate);
		}

		Persona->UnregisterOnPreviewMeshChanged(this);
		Persona->UnregisterOnAnimChanged(this);
		Persona->UnregisterOnPostUndo(this);
		Persona->UnregisterOnChangeCurves(this);
	}
}

bool SAnimCurveViewer::IsCurveFilterEnabled() const
{
	return !bShowAllCurves;
}

ECheckBoxState SAnimCurveViewer::IsShowingAllCurves() const
{
	return bShowAllCurves ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAnimCurveViewer::OnToggleShowingAllCurves(ECheckBoxState NewState)
{
	bShowAllCurves = (NewState == ECheckBoxState::Checked);

	RefreshCurveList();
}

void SAnimCurveViewer::RefreshCachePreviewInstance()
{
	if (CachedPreviewInstance && OnAddAnimationCurveDelegate.IsBound())
	{
		CachedPreviewInstance->RemoveDelegate_AddCustomAnimationCurve(OnAddAnimationCurveDelegate);
	}

	CachedPreviewInstance = Cast<UAnimInstance>(PersonaPtr.Pin()->GetPreviewAnimInstance());

	if (CachedPreviewInstance)
	{
		OnAddAnimationCurveDelegate.BindRaw(this, &SAnimCurveViewer::ApplyCustomCurveOverride);
		CachedPreviewInstance->AddDelegate_AddCustomAnimationCurve(OnAddAnimationCurveDelegate);
	}
}

void SAnimCurveViewer::OnPreviewMeshChanged(class USkeletalMesh* NewPreviewMesh)
{
	RefreshCachePreviewInstance();
	RefreshCurveList();
}

void SAnimCurveViewer::OnFilterTextChanged( const FText& SearchText )
{
	FilterText = SearchText;

	RefreshCurveList();
}

void SAnimCurveViewer::OnCurvesChanged()
{
	RefreshCurveList();
}

void SAnimCurveViewer::OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo )
{
	// Just do the same as if the user typed in the box
	OnFilterTextChanged( SearchText );
}

TSharedRef<ITableRow> SAnimCurveViewer::GenerateAnimCurveRow(TSharedPtr<FDisplayedAnimCurveInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	check( InInfo.IsValid() );

	return
		SNew( SAnimCurveListRow, OwnerTable )
		.Item( InInfo )
		.AnimCurveViewerPtr( SharedThis(this) );
}

TSharedPtr<SWidget> SAnimCurveViewer::OnGetContextMenuContent() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, NULL);

	MenuBuilder.BeginSection("AnimCurveAction", LOCTEXT( "MorphsAction", "Selected Item Actions" ) );

	{
		FUIAction Action;
		// Rename selection
		{
			Action.ExecuteAction = FExecuteAction::CreateSP(this, &SAnimCurveViewer::OnRenameClicked);
			Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &SAnimCurveViewer::CanRename);
			const FText Label = LOCTEXT("RenameSmartNameLabel", "Rename");
			const FText ToolTipText = LOCTEXT("RenameSmartNameToolTip", "Rename the selected item");
			MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
		}
		// Delete a name
		{
			Action.ExecuteAction = FExecuteAction::CreateSP(this, &SAnimCurveViewer::OnDeleteNameClicked);
			Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &SAnimCurveViewer::CanDelete);
			const FText Label = LOCTEXT("DeleteSmartNameLabel", "Delete");
			const FText ToolTipText = LOCTEXT("DeleteSmartNameToolTip", "Delete the selected item");
			MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
		}
		// Add a name
		MenuBuilder.AddMenuSeparator();
		{
			Action.ExecuteAction = FExecuteAction::CreateSP(this, &SAnimCurveViewer::OnAddClicked);
			Action.CanExecuteAction = nullptr;
			const FText Label = LOCTEXT("AddSmartNameLabel", "Add...");
			const FText ToolTipText = LOCTEXT("AddSmartNameToolTip", "Add an entry to the skeleton");
			MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
		}
	}

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SAnimCurveViewer::OnRenameClicked()
{
	TArray< TSharedPtr<FDisplayedAnimCurveInfo> > SelectedItems = AnimCurveListView->GetSelectedItems();

	SelectedItems[0]->EditableText->EnterEditingMode();
}

bool SAnimCurveViewer::CanRename()
{
	return AnimCurveListView->GetNumItemsSelected() == 1;
}

void SAnimCurveViewer::OnAddClicked()
{
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewSmartnameLabel", "New Name"))
		.OnTextCommitted(this, &SAnimCurveViewer::CreateNewNameEntry);

	FSlateApplication& SlateApp = FSlateApplication::Get();
	SlateApp.PushMenu(
		AsShared(),
		FWidgetPath(),
		TextEntry,
		SlateApp.GetCursorPos(),
		FPopupTransitionEffect::TypeInPopup
		);
}



void SAnimCurveViewer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (PersonaPtr.IsValid())
	{
		UAnimInstance* AnimInstance = PersonaPtr.Pin()->GetPreviewAnimInstance();
		if (AnimInstance)
		{
			RefreshCurveList();
		}
	}
}

void SAnimCurveViewer::CreateNewNameEntry(const FText& CommittedText, ETextCommit::Type CommitType)
{
	FSlateApplication::Get().DismissAllMenus();
	if (!CommittedText.IsEmpty() && CommitType == ETextCommit::OnEnter)
	{
		if (const FSmartNameMapping* NameMapping = GetAnimCurveMapping())
		{
			FName NewName = FName(*CommittedText.ToString());
			FSmartName NewCurveName;
			if (CurrentSkeleton->AddSmartNameAndModify(ContainerName, NewName, NewCurveName))
			{
				// Successfully added
				RefreshCurveList();
			}
		}
	}
}

const FSmartNameMapping* SAnimCurveViewer::GetAnimCurveMapping()
{
	const FSmartNameMapping* Mapping = nullptr;
	if (CurrentSkeleton)
	{
		Mapping = CurrentSkeleton->GetSmartNameContainer(ContainerName);
	}
	return Mapping;
}

int32 FindIndexOfAnimCurveInfo(TArray<TSharedPtr<FDisplayedAnimCurveInfo>>& AnimCurveInfos, const FName& CurveName)
{
	for (int32 CurveIdx = 0; CurveIdx < AnimCurveInfos.Num(); CurveIdx++)
	{
		if (AnimCurveInfos[CurveIdx]->SmartName.DisplayName == CurveName)
		{
			return CurveIdx;
		}
	}

	return INDEX_NONE;
}

void SAnimCurveViewer::CreateAnimCurveList( const FString& SearchText )
{
	TSharedPtr<FPersona> Persona = PersonaPtr.Pin();
	if (Persona.IsValid() && CurrentSkeleton)
	{
		const FSmartNameMapping* Mapping = GetAnimCurveMapping();
		if (Mapping)
		{
			TArray<FSmartNameMapping::UID> UidList;
			Mapping->FillUidArray(UidList);

			// Get set of active curves
			TMap<FName, float> ActiveCurves;
			UAnimInstance* AnimInstance = Persona->GetPreviewAnimInstance();
			if (!bShowAllCurves && AnimInstance)
			{
				AnimInstance->GetAnimationCurveList(CurrentCurveFlag, ActiveCurves);
			}

			// Iterate through all curves..
			for (FSmartNameMapping::UID Uid : UidList)
			{
				bool bAddToList = true;

				FSmartName SmartName;
				Mapping->FindSmartNameByUID(Uid, SmartName);

				// See if we pass the search filter
				if (!FilterText.IsEmpty())
				{
					if (!SmartName.DisplayName.ToString().Contains(*FilterText.ToString()))
					{
						bAddToList = false;
					}
				}

				// If we passed that, see if we are filtering to only active
				if (bAddToList && !bShowAllCurves)
				{
					bAddToList = ActiveCurves.Contains(SmartName.DisplayName);
				}

				// If we still want to add
				if (bAddToList)
				{
					// If not already in list, add it
					if (FindIndexOfAnimCurveInfo(AnimCurveList, SmartName.DisplayName) == INDEX_NONE)
					{
						AnimCurveList.Add(FDisplayedAnimCurveInfo::Make(CurrentSkeleton, ContainerName, SmartName));
					}
				}
				// Don't want in list
				else
				{
					// If already in list, remove it
					int32 CurrentIndex = FindIndexOfAnimCurveInfo(AnimCurveList, SmartName.DisplayName);
					if (CurrentIndex != INDEX_NONE)
					{
						AnimCurveList.RemoveAt(CurrentIndex);
					}
				}
			}

			// Sort final list
			struct FSortSmartNamesAlphabetically
			{
				bool operator()(const TSharedPtr<FDisplayedAnimCurveInfo>& A, const TSharedPtr<FDisplayedAnimCurveInfo>& B) const
				{
					return (A.Get()->SmartName.DisplayName.Compare(B.Get()->SmartName.DisplayName) < 0);
				}
			};

			AnimCurveList.Sort(FSortSmartNamesAlphabetically());
		}
	}

	AnimCurveListView->RequestListRefresh();
}

void SAnimCurveViewer::CreateAnimCurveTypeList(TSharedRef<SHorizontalBox> HorizontalBox)
{
	// Add toggle button for 'all curves'
	HorizontalBox->AddSlot()
	.AutoWidth()
	.Padding(3, 1)
	[
		SNew(SCheckBox)
		.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
		.ToolTipText(LOCTEXT("ShowAllCurvesTooltip", "Show all curves, or only active curves."))
		.Type(ESlateCheckBoxType::ToggleButton)
		.IsChecked(this, &SAnimCurveViewer::IsShowingAllCurves)
		.OnCheckStateChanged(this, &SAnimCurveViewer::OnToggleShowingAllCurves)
		.Padding(4)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ShowAllCurves", "All Curves"))
		]
	];

	// Add check box for each curve type flag
	TArray<int32> CurveFlagsToList;
	CurveFlagsToList.Add(ACF_DriveMorphTarget);
	CurveFlagsToList.Add(ACF_DriveAttribute);
	CurveFlagsToList.Add(ACF_DriveMaterial);
	// @fixme: when we move curve types to skeleton fix this
//	CurveFlagsToList.Add(ACF_Metadata);

	for (int32 CurveFlagIndex = 0; CurveFlagIndex < CurveFlagsToList.Num(); ++CurveFlagIndex)
	{
		HorizontalBox->AddSlot()
		.AutoWidth()
		.Padding(3, 1)
		[
			SNew(SAnimCurveTypeList)
			.IsEnabled(this, &SAnimCurveViewer::IsCurveFilterEnabled)
			.CurveFlags(CurveFlagsToList[CurveFlagIndex])
			.AnimCurveViewerPtr(SharedThis(this))
		];
	}
}

void SAnimCurveViewer::AddAnimCurveOverride( FName& Name, float Weight)
{
	float& Value = OverrideCurves.FindOrAdd(Name);
	Value = Weight;

	UAnimSingleNodeInstance* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(CachedPreviewInstance);
	if (SingleNodeInstance)
	{
		SingleNodeInstance->SetPreviewCurveOverride(Name, Value, false);
	}
}

void SAnimCurveViewer::RemoveAnimCurveOverride(FName& Name)
{
	OverrideCurves.Remove(Name);

	UAnimSingleNodeInstance* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(CachedPreviewInstance);
	if (SingleNodeInstance)
	{
		SingleNodeInstance->SetPreviewCurveOverride(Name, 0.f, true);
	}
}


void SAnimCurveViewer::OnPostUndo()
{
	RefreshCurveList();
}

void SAnimCurveViewer::OnPreviewAssetChanged(class UAnimationAsset* NewAsset)
{
	OverrideCurves.Empty();
	RefreshCurveList();
}

void SAnimCurveViewer::ApplyCustomCurveOverride(UAnimInstance* AnimInstance) const
{
	for (auto Iter = OverrideCurves.CreateConstIterator(); Iter; ++Iter)
	{ 
		// @todo we might want to save original curve flags? or just change curve to apply flags only
		AnimInstance->AddCurveValue(Iter.Key(), Iter.Value(), CurrentCurveFlag);
	}
}

void SAnimCurveViewer::RefreshCurveList()
{
	CreateAnimCurveList(FilterText.ToString());
}

void SAnimCurveViewer::OnNameCommitted(const FText& InNewName, ETextCommit::Type, TSharedPtr<FDisplayedAnimCurveInfo> Item)
{
	const FSmartNameMapping* Mapping = GetAnimCurveMapping();
	if (Mapping)
	{
		FName NewName(*InNewName.ToString());
		if (!Mapping->Exists(NewName))
		{
			FScopedTransaction Transaction(LOCTEXT("TransactionRename", "Rename Element"));
			CurrentSkeleton->RenameSmartnameAndModify(ContainerName, Item->SmartName.UID, NewName);
			// remove it, so that it can readd it. 
			AnimCurveList.Remove(Item);
		}
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("InvalidName"), FText::FromName(NewName) );
			FNotificationInfo Info(FText::Format(LOCTEXT("AnimCurveRenamed", "The name \"{InvalidName}\" is already used."), Args));

			Info.bUseLargeFont = false;
			Info.ExpireDuration = 5.0f;

			TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if (Notification.IsValid())
			{
				Notification->SetCompletionState(SNotificationItem::CS_Fail);
			}
		}
	}
}

void SAnimCurveViewer::OnDeleteNameClicked()
{
	TArray< TSharedPtr<FDisplayedAnimCurveInfo> > SelectedItems = AnimCurveListView->GetSelectedItems();
	TArray<FSmartNameMapping::UID> SelectedUids;

	for (TSharedPtr<FDisplayedAnimCurveInfo> Item : SelectedItems)
	{
		SelectedUids.Add(Item->SmartName.UID);
	}

	FAssetRegistryModule& AssetModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> AnimationAssets;
	AssetModule.Get().GetAssetsByClass(UAnimSequenceBase::StaticClass()->GetFName(), AnimationAssets, true);
	FAssetData SkeletonData(CurrentSkeleton);
	const FString CurrentSkeletonName = SkeletonData.GetExportTextName();

	GWarn->BeginSlowTask(LOCTEXT("CollectAnimationsTaskDesc", "Collecting assets..."), true);

	for (int32 Idx = AnimationAssets.Num() - 1; Idx >= 0; --Idx)
	{
		FAssetData& Data = AnimationAssets[Idx];
		bool bRemove = true;

		const FString SkeletonDataTag = Data.GetTagValueRef<FString>("Skeleton");
		if (!SkeletonDataTag.IsEmpty() && SkeletonDataTag == CurrentSkeletonName)
		{
			FString CurveData;
			if (!Data.GetTagValue(USkeleton::CurveTag, CurveData))
			{
				// This asset is old; it hasn't been loaded since before smartnames were added for curves.
				// unfortunately the only way to delete curves safely is to load old assets to see if they're
				// using the selected name. We only load what we have to here.
				UObject* Asset = Data.GetAsset();
				check(Asset);
				TArray<UObject::FAssetRegistryTag> Tags;
				Asset->GetAssetRegistryTags(Tags);

				UObject::FAssetRegistryTag* CurveTag = Tags.FindByPredicate([](const UObject::FAssetRegistryTag& InTag)
				{
					return InTag.Name == USkeleton::CurveTag;
				});
				CurveData = CurveTag->Value;
			}

			TArray<FString> ParsedStringUids;
			CurveData.ParseIntoArray(ParsedStringUids, *USkeleton::CurveTagDelimiter, true);

			for (const FString& UidString : ParsedStringUids)
			{
				FSmartNameMapping::UID Uid = (FSmartNameMapping::UID)TCString<TCHAR>::Strtoui64(*UidString, NULL, 10);
				if (SelectedUids.Contains(Uid))
				{
					bRemove = false;
					break;
				}
			}
		}

		if (bRemove)
		{
			AnimationAssets.RemoveAt(Idx);
		}
	}

	GWarn->EndSlowTask();

	// AnimationAssets now only contains assets that are using the selected curve(s)
	if (AnimationAssets.Num() > 0)
	{
		FString ConfirmMessage = LOCTEXT("DeleteCurveMessage", "The following assets use the selected curve(s), deleting will remove the curves from these assets. Continue?\n\n").ToString();

		for (FAssetData& Data : AnimationAssets)
		{
			ConfirmMessage += Data.AssetName.ToString() + " (" + Data.AssetClass.ToString() + ")\n";
		}

		FText Title = LOCTEXT("DeleteCurveDialogTitle", "Confirm Deletion");
		FText Message = FText::FromString(ConfirmMessage);
		if (FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title) == EAppReturnType::No)
		{
			return;
		}
		else
		{
			// Proceed to delete the curves
			GWarn->BeginSlowTask(FText::Format(LOCTEXT("DeleteCurvesTaskDesc", "Deleting curve from skeleton {0}"), FText::FromString(CurrentSkeleton->GetName())), true);
			FScopedTransaction Transaction(LOCTEXT("DeleteCurvesTransactionName", "Delete skeleton curve"));

			// Remove curves from animation assets
			for (FAssetData& Data : AnimationAssets)
			{
				UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Data.GetAsset());
				USkeleton* MySkeleton = Sequence->GetSkeleton();
				Sequence->Modify(true);
				for (FSmartNameMapping::UID Uid : SelectedUids)
				{
					FSmartName CurveToDelete;
					if (MySkeleton->GetSmartNameByUID(ContainerName, Uid, CurveToDelete))
					{
						Sequence->RawCurveData.DeleteCurveData(CurveToDelete);
						Sequence->MarkRawDataAsModified();
					}
				}
			}
			GWarn->EndSlowTask();

			GWarn->BeginSlowTask(LOCTEXT("RebuildingAnimations", "Rebaking/compressing modified animations"), true);

			// Rebake/compress the animations
			for (TObjectIterator<UAnimSequence> It; It; ++It)
			{
				UAnimSequence* Seq = *It;

				GWarn->StatusUpdate(1, 2, FText::Format(LOCTEXT("RebuildingAnimationsStatus", "Rebuilding {0}"), FText::FromString(Seq->GetName())));
				Seq->RequestSyncAnimRecompression();
			}
			GWarn->EndSlowTask();
		}
	}

	if (SelectedUids.Num() > 0)
	{
		// Remove names from skeleton
		CurrentSkeleton->RemoveSmartnamesAndModify(ContainerName, SelectedUids);
		for (int32 ListIndex = SelectedItems.Num() - 1; ListIndex >= 0 ; --ListIndex)
		{
			AnimCurveList.Remove(SelectedItems[ListIndex]);
		}
	}

	RefreshCurveList();

	TSharedPtr<FPersona> Persona = PersonaPtr.Pin();
	if (Persona.IsValid())
	{
		if (Persona->GetCurrentMode() == FPersonaModes::AnimationEditMode)
		{
			// If we're in animation mode then we need to reinitialise the mode to
			// refresh the sequencer which will now have a widget it shouldn't have
			Persona->ReinitMode();
		}
	}
}

bool SAnimCurveViewer::CanDelete()
{
	return AnimCurveListView->GetNumItemsSelected() > 0;
}

#undef LOCTEXT_NAMESPACE

