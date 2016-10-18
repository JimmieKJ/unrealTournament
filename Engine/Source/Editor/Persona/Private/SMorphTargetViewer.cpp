// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "SMorphTargetViewer.h"
#include "ObjectTools.h"
#include "AssetRegistryModule.h"
#include "ScopedTransaction.h"
#include "SSearchBox.h"
#include "Animation/MorphTarget.h"
#include "Animation/AnimInstance.h"

#define LOCTEXT_NAMESPACE "SMorphTargetViewer"

static const FName ColumnId_MorphTargetNameLabel( "MorphTargetName" );
static const FName ColumnID_MorphTargetWeightLabel( "Weight" );
static const FName ColumnID_MorphTargetEditLabel( "Edit" );
static const FName ColumnID_MorphTargetVertCountLabel( "NumberOfVerts" );

const float MaxMorphWeight = 5.f;

//////////////////////////////////////////////////////////////////////////
// SMorphTargetListRow

typedef TSharedPtr< FDisplayedMorphTargetInfo > FDisplayedMorphTargetInfoPtr;

class SMorphTargetListRow
	: public SMultiColumnTableRow< FDisplayedMorphTargetInfoPtr >
{
public:

	SLATE_BEGIN_ARGS( SMorphTargetListRow ) {}

	/** The item for this row **/
	SLATE_ARGUMENT( FDisplayedMorphTargetInfoPtr, Item )

		/* The SMorphTargetViewer that we push the morph target weights into */
		SLATE_ARGUMENT( class SMorphTargetViewer*, MorphTargetViewer )

		/* Widget used to display the list of morph targets */
		SLATE_ARGUMENT( TSharedPtr<SMorphTargetListType>, MorphTargetListView )

		/* Persona used to update the viewport when a weight slider is dragged */
		SLATE_ARGUMENT( TWeakPtr<FPersona>, Persona )

		SLATE_END_ARGS()

		void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView );

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;

private:

	/**
	* Called when the user changes the value of the SSpinBox
	*
	* @param NewWeight - The new number the SSpinBox is set to
	*
	*/
	void OnMorphTargetWeightChanged( float NewWeight );
	/**
	* Called when the user types the value and enters
	*
	* @param NewWeight - The new number the SSpinBox is set to
	*
	*/
	void OnMorphTargetWeightValueCommitted( float NewWeight, ETextCommit::Type CommitType);

	/** Auto fill check call back functions */
	void OnMorphTargetAutoFillChecked(ECheckBoxState InState);
	ECheckBoxState IsMorphTargetAutoFillChangedChecked() const;
	
	/**
	* Returns the weight of this morph target
	*
	* @return SearchText - The new number the SSpinBox is set to
	*
	*/
	float GetWeight() const;

	/* The SMorphTargetViewer that we push the morph target weights into */
	SMorphTargetViewer* MorphTargetViewer;

	/** Widget used to display the list of morph targets */
	TSharedPtr<SMorphTargetListType> MorphTargetListView;

	/** The name and weight of the morph target */
	FDisplayedMorphTargetInfoPtr	Item;

	/** Pointer back to the Persona that owns us */
	TWeakPtr<FPersona> PersonaPtr;
};

void SMorphTargetListRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	Item = InArgs._Item;
	MorphTargetViewer = InArgs._MorphTargetViewer;
	MorphTargetListView = InArgs._MorphTargetListView;
	PersonaPtr = InArgs._Persona;

	check( Item.IsValid() );

	SMultiColumnTableRow< FDisplayedMorphTargetInfoPtr >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
}

TSharedRef< SWidget > SMorphTargetListRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	if ( ColumnName == ColumnId_MorphTargetNameLabel )
	{
		return
			SNew( SVerticalBox )

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 0.0f, 4.0f )
			.VAlign( VAlign_Center )
			[
				SNew( STextBlock )
				.Text( FText::FromName(Item->Name) )
				.HighlightText( MorphTargetViewer->GetFilterText() )
			];
	}
	else if ( ColumnName == ColumnID_MorphTargetWeightLabel )
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
				.Value( this, &SMorphTargetListRow::GetWeight )
				.OnValueChanged( this, &SMorphTargetListRow::OnMorphTargetWeightChanged )
				.OnValueCommitted( this, &SMorphTargetListRow::OnMorphTargetWeightValueCommitted )
			];
	}
	else if ( ColumnName == ColumnID_MorphTargetEditLabel )
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
				.OnCheckStateChanged(this, &SMorphTargetListRow::OnMorphTargetAutoFillChecked)
				.IsChecked(this, &SMorphTargetListRow::IsMorphTargetAutoFillChangedChecked)
			];
	}
	else
	{
		return
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f, 4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					[
						SNew(STextBlock)
						.Text(FText::AsNumber(Item->NumberOfVerts))
						.HighlightText(MorphTargetViewer->GetFilterText())
					]
				];
	}
}

void SMorphTargetListRow::OnMorphTargetAutoFillChecked(ECheckBoxState InState)
{
	Item->bAutoFillData = InState == ECheckBoxState::Checked;

	if (Item->bAutoFillData)
	{
		// clear value so that it can be filled up
		MorphTargetViewer->AddMorphTargetOverride(Item->Name, 0.f, true);
	}
}

ECheckBoxState SMorphTargetListRow::IsMorphTargetAutoFillChangedChecked() const
{
	return (Item->bAutoFillData)? ECheckBoxState::Checked: ECheckBoxState::Unchecked;
}

void SMorphTargetListRow::OnMorphTargetWeightChanged( float NewWeight )
{
	// First change this item...
	// the delta feature is a bit confusing when debugging morphtargets, and you're not sure why it's changing, so I'm disabling it for now. 
	// I think in practice, you want each morph target to move independentaly. It is very unlikely you'd like to move multiple things together. 
#if 0 
	float Delta = NewWeight - GetWeight();
#endif
	Item->Weight = NewWeight;
	Item->bAutoFillData = false;

	MorphTargetViewer->AddMorphTargetOverride( Item->Name, Item->Weight, false );

	if (PersonaPtr.IsValid())
	{
		PersonaPtr.Pin()->RefreshViewport();
	}

#if 0 
	TArray< TSharedPtr< FDisplayedMorphTargetInfo > > SelectedRows = MorphTargetListView->GetSelectedItems();

	// ...then any selected rows need changing by the same delta
	for ( auto ItemIt = SelectedRows.CreateIterator(); ItemIt; ++ItemIt )
	{
		TSharedPtr< FDisplayedMorphTargetInfo > RowItem = ( *ItemIt );

		if ( RowItem != Item ) // Don't do "this" row again if it's selected
		{
			RowItem->Weight = FMath::Clamp(RowItem->Weight + Delta, -MaxMorphWeight, MaxMorphWeight);
			RowItem->bAutoFillData = false;
			MorphTargetViewer->AddMorphTargetOverride( RowItem->Name, RowItem->Weight, false );
		}
	}
#endif
}

void SMorphTargetListRow::OnMorphTargetWeightValueCommitted( float NewWeight, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
	{
		float NewValidWeight = FMath::Clamp(NewWeight, -MaxMorphWeight, MaxMorphWeight);
		Item->Weight = NewValidWeight;
		Item->bAutoFillData = false;

		MorphTargetViewer->AddMorphTargetOverride(Item->Name, Item->Weight, false);

		TArray< TSharedPtr< FDisplayedMorphTargetInfo > > SelectedRows = MorphTargetListView->GetSelectedItems();

		// ...then any selected rows need changing by the same delta
		for(auto ItemIt = SelectedRows.CreateIterator(); ItemIt; ++ItemIt)
		{
			TSharedPtr< FDisplayedMorphTargetInfo > RowItem = (*ItemIt);

			if(RowItem != Item) // Don't do "this" row again if it's selected
			{
				RowItem->Weight = NewValidWeight;
				RowItem->bAutoFillData = false;
				MorphTargetViewer->AddMorphTargetOverride(RowItem->Name, RowItem->Weight, false);
			}
		}

		if(PersonaPtr.IsValid())
		{
			PersonaPtr.Pin()->RefreshViewport();
		}
	}
}

float SMorphTargetListRow::GetWeight() const 
{ 
	if (PersonaPtr.IsValid() && Item->bAutoFillData)
	{
		USkeletalMeshComponent* SkelComp = PersonaPtr.Pin()->GetPreviewMeshComponent();
		UAnimInstance* AnimInstance = (SkelComp) ? SkelComp->GetAnimInstance() : nullptr;
		if (AnimInstance)
		{
			// make sure if they have value that's not same as saved value
			TMap<FName, float> MorphCurves;
			AnimInstance->GetAnimationCurveList(ACF_DriveMorphTarget, MorphCurves);
			const float* CurrentValue = MorphCurves.Find(Item->Name);
			if (CurrentValue && *CurrentValue != 0.f)
			{
				return *CurrentValue;
			}
		}
	}

	return Item->Weight; 
}
//////////////////////////////////////////////////////////////////////////
// SMorphTargetViewer

void SMorphTargetViewer::Construct(const FArguments& InArgs)
{
	PersonaPtr = InArgs._Persona;
	SkeletalMesh = NULL;

	if ( PersonaPtr.IsValid() )
	{
		SkeletalMesh = PersonaPtr.Pin()->GetMesh();
		PersonaPtr.Pin()->RegisterOnPreviewMeshChanged( FPersona::FOnPreviewMeshChanged::CreateSP( this, &SMorphTargetViewer::OnPreviewMeshChanged ) );
		PersonaPtr.Pin()->RegisterOnPostUndo(FPersona::FOnPostUndo::CreateSP(this, &SMorphTargetViewer::OnPostUndo));
	}

	const FText SkeletalMeshName = SkeletalMesh ? FText::FromString( SkeletalMesh->GetName() ) : LOCTEXT( "MorphTargetMeshNameLabel", "No Skeletal Mesh Present" );

	ChildSlot
	[
		SNew( SVerticalBox )
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( STextBlock )
			.Text( SkeletalMeshName )
		]

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
				.OnTextChanged( this, &SMorphTargetViewer::OnFilterTextChanged )
				.OnTextCommitted( this, &SMorphTargetViewer::OnFilterTextCommitted )
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight( 1.0f )		// This is required to make the scrollbar work, as content overflows Slate containers by default
		[
			SAssignNew( MorphTargetListView, SMorphTargetListType )
			.ListItemsSource( &MorphTargetList )
			.OnGenerateRow( this, &SMorphTargetViewer::GenerateMorphTargetRow )
			.OnContextMenuOpening( this, &SMorphTargetViewer::OnGetContextMenuContent )
			.ItemHeight( 22.0f )
			.HeaderRow
			(
				SNew( SHeaderRow )
				+ SHeaderRow::Column( ColumnId_MorphTargetNameLabel )
				.DefaultLabel( LOCTEXT( "MorphTargetNameLabel", "Morph Target Name" ) )

				+ SHeaderRow::Column( ColumnID_MorphTargetWeightLabel )
				.DefaultLabel( LOCTEXT( "MorphTargetWeightLabel", "Weight" ) )

				+ SHeaderRow::Column(ColumnID_MorphTargetEditLabel)
				.DefaultLabel(LOCTEXT("MorphTargetEditLabel", "Auto"))

				+ SHeaderRow::Column( ColumnID_MorphTargetVertCountLabel )
				.DefaultLabel( LOCTEXT("MorphTargetVertCountLabel", "Vert Count") )
			)
		]
	];

	CreateMorphTargetList();
}

void SMorphTargetViewer::OnPreviewMeshChanged(class USkeletalMesh* NewPreviewMesh)
{
	SkeletalMesh = NewPreviewMesh;
	CreateMorphTargetList( NameFilterBox->GetText().ToString() );
}

void SMorphTargetViewer::OnFilterTextChanged( const FText& SearchText )
{
	FilterText = SearchText;

	CreateMorphTargetList( SearchText.ToString() );
}

void SMorphTargetViewer::OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo )
{
	// Just do the same as if the user typed in the box
	OnFilterTextChanged( SearchText );
}

TSharedRef<ITableRow> SMorphTargetViewer::GenerateMorphTargetRow(TSharedPtr<FDisplayedMorphTargetInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	check( InInfo.IsValid() );

	return
		SNew( SMorphTargetListRow, OwnerTable )
		.Persona( PersonaPtr )
		.Item( InInfo )
		.MorphTargetViewer( this )
		.MorphTargetListView( MorphTargetListView );
}

TSharedPtr<SWidget> SMorphTargetViewer::OnGetContextMenuContent() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, NULL);

	MenuBuilder.BeginSection("MorphTargetAction", LOCTEXT( "MorphsAction", "Selected Item Actions" ) );
	{
		FUIAction Action = FUIAction( FExecuteAction::CreateSP( this, &SMorphTargetViewer::OnDeleteMorphTargets ), 
									  FCanExecuteAction::CreateSP( this, &SMorphTargetViewer::CanPerformDelete ) );
		const FText Label = LOCTEXT("DeleteMorphTargetButtonLabel", "Delete");
		const FText ToolTipText = LOCTEXT("DeleteMorphTargetButtonTooltip", "Deletes the selected morph targets.");
		MenuBuilder.AddMenuEntry( Label, ToolTipText, FSlateIcon(), Action);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SMorphTargetViewer::CreateMorphTargetList( const FString& SearchText )
{
	MorphTargetList.Empty();

	if ( SkeletalMesh )
	{
		UDebugSkelMeshComponent* MeshComponent = PersonaPtr.Pin()->GetPreviewMeshComponent();
		TArray<UMorphTarget*>& MorphTargets = SkeletalMesh->MorphTargets;

		bool bDoFiltering = !SearchText.IsEmpty();

		for ( int32 I = 0; I < MorphTargets.Num(); ++I )
		{
			if ( bDoFiltering && !MorphTargets[I]->GetName().Contains( SearchText ) )
			{
				continue; // Skip items that don't match our filter
			}

			int32 NumberOfVerts = (MorphTargets[I]->MorphLODModels.Num() > 0)? MorphTargets[I]->MorphLODModels[0].Vertices.Num() : 0;

			TSharedRef<FDisplayedMorphTargetInfo> Info = FDisplayedMorphTargetInfo::Make( MorphTargets[I]->GetFName(), NumberOfVerts);
			if(MeshComponent)
			{
				const float *CurveValPtr = MeshComponent->GetMorphTargetCurves().Find( MorphTargets[I]->GetFName() );
				if(CurveValPtr)
				{
					Info.Get().Weight = (*CurveValPtr);
				}
			}

			MorphTargetList.Add( Info );
		}
	}

	MorphTargetListView->RequestListRefresh();
}

void SMorphTargetViewer::AddMorphTargetOverride( FName& Name, float Weight, bool bRemoveZeroWeight )
{
	if ( PersonaPtr.IsValid() )
	{
		UDebugSkelMeshComponent* Mesh = PersonaPtr.Pin()->GetPreviewMeshComponent();

		if ( Mesh )
		{
			Mesh->SetMorphTarget( Name, Weight, bRemoveZeroWeight );
		}
	}
}

bool SMorphTargetViewer::CanPerformDelete() const
{
	TArray< TSharedPtr< FDisplayedMorphTargetInfo > > SelectedRows = MorphTargetListView->GetSelectedItems();
	return SelectedRows.Num() > 0;
}

void SMorphTargetViewer::OnDeleteMorphTargets()
{
	TArray< TSharedPtr< FDisplayedMorphTargetInfo > > SelectedRows = MorphTargetListView->GetSelectedItems();
	
	for (int RowIndex = 0; RowIndex < SelectedRows.Num(); ++RowIndex)
	{
		UMorphTarget* MorphTarget = SkeletalMesh->FindMorphTarget(SelectedRows[RowIndex]->Name);
		if(MorphTarget)
		{
			MorphTarget->RemoveFromRoot();
			MorphTarget->ClearFlags(RF_Standalone);

			FScopedTransaction Transaction(LOCTEXT("DeleteMorphTarget", "Delete Morph Target"));
			SkeletalMesh->Modify();
			MorphTarget->Modify();

			//Clean up override usage
			AddMorphTargetOverride(SelectedRows[RowIndex]->Name, 0.0f, true);

			SkeletalMesh->UnregisterMorphTarget(MorphTarget);
		}
	}

	CreateMorphTargetList( NameFilterBox->GetText().ToString() );
}

SMorphTargetViewer::~SMorphTargetViewer()
{
	if ( PersonaPtr.IsValid() )
	{
		PersonaPtr.Pin()->UnregisterOnPreviewMeshChanged(this);
		PersonaPtr.Pin()->UnregisterOnPostUndo(this);

		UDebugSkelMeshComponent* Mesh = PersonaPtr.Pin()->GetPreviewMeshComponent();

		if ( Mesh )
		{
			Mesh->ClearMorphTargets();
		}
	}
}

void SMorphTargetViewer::OnPostUndo()
{
	CreateMorphTargetList();
}

#undef LOCTEXT_NAMESPACE

