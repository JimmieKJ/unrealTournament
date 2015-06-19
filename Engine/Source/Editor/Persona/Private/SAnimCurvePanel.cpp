// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"

#include "SAnimCurvePanel.h"
#include "ScopedTransaction.h"
#include "SAnimCurveEd.h"
#include "Editor/KismetWidgets/Public/SScrubWidget.h"
#include "AssetRegistryModule.h"
#include "Kismet2NameValidators.h"
#include "SExpandableArea.h"
#include "STextEntryPopup.h"

#define LOCTEXT_NAMESPACE "AnimCurvePanel"

//////////////////////////////////////////////////////////////////////////
//  FAnimCurveInterface interface 

/** Interface you implement if you want the CurveEditor to be able to edit curves on you */
class FAnimCurveBaseInterface : public FCurveOwnerInterface
{
public:
	TWeakObjectPtr<UAnimSequenceBase>	BaseSequence;
	USkeleton::AnimCurveUID CurveUID;
	FAnimCurveBase*	CurveData;

public:
	FAnimCurveBaseInterface( UAnimSequenceBase * BaseSeq, USkeleton::AnimCurveUID InCurveUID)
		: BaseSequence(BaseSeq)
		, CurveUID(InCurveUID)
	{
		CurveData = BaseSequence.Get()->RawCurveData.GetCurveData( CurveUID );
		// they should be valid
		check (BaseSequence.IsValid());
		check (CurveData);
	}

	/** Returns set of curves to edit. Must not release the curves while being edited. */
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override
	{
		TArray<FRichCurveEditInfoConst> Curves;
		FFloatCurve * FloatCurveData = (FFloatCurve*)(CurveData);
		Curves.Add(&FloatCurveData->FloatCurve);

		return Curves;
	}

	/** Returns set of curves to query. Must not release the curves while being edited. */
	virtual TArray<FRichCurveEditInfo> GetCurves() override
	{
		TArray<FRichCurveEditInfo> Curves;
		FFloatCurve * FloatCurveData = (FFloatCurve*)(CurveData);
		Curves.Add(&FloatCurveData->FloatCurve);

		return Curves;
	}

	/** Called to modify the owner of the curve */
	virtual void ModifyOwner() override
	{
		if (BaseSequence.IsValid())
		{
			BaseSequence.Get()->Modify(true);
		}
	}

	/** Called to make curve owner transactional */
	virtual void MakeTransactional() override
	{
		if (BaseSequence.IsValid())
		{
			BaseSequence.Get()->SetFlags(BaseSequence.Get()->GetFlags() | RF_Transactional);
		}
	}

	/** Called to get the name of a curve back from the animation skeleton */
	virtual FText GetCurveName(USkeleton::AnimCurveUID Uid) const
	{
		if(BaseSequence.IsValid())
		{
			FSmartNameMapping* NameMapping = BaseSequence.Get()->GetSkeleton()->SmartNames.GetContainer(USkeleton::AnimCurveMappingName);
			if(NameMapping)
			{
				FName CurveName;
				if(NameMapping->GetName(Uid, CurveName))
				{
					return FText::FromName(CurveName);
				}
			}
		}
		return FText::GetEmpty();
	}

	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override
	{
	}

	virtual bool IsValidCurve( FRichCurveEditInfo CurveInfo ) override
	{
		// Get the curve with the ID directly from the sequence and compare it since undo/redo can cause previously
		// used curves to become invalid.
		FAnimCurveBase* CurrentCurveData = BaseSequence.Get()->RawCurveData.GetCurveData( CurveUID );
		return CurrentCurveData != nullptr &&
			CurveInfo.CurveToEdit == &((FFloatCurve*)CurrentCurveData)->FloatCurve;
	}
};
//////////////////////////////////////////////////////////////////////////
//  SCurveEd Track : each track for curve editing 

/** Widget for editing a single track of animation curve - this includes CurveEditor */
class SCurveEdTrack : public SCompoundWidget
{
private:
	/** Pointer to notify panel for drawing*/
	TSharedPtr<class SCurveEditor>			CurveEditor;

	/** Name of curve it's editing - CurveName should be unique within this tracks**/
	FAnimCurveBaseInterface	*				CurveInterface;

	/** Curve Panel Ptr **/
	TWeakPtr<SAnimCurvePanel>				PanelPtr;

	/** is using expanded editor **/
	bool									bUseExpandEditor;

public:
	SLATE_BEGIN_ARGS( SCurveEdTrack )
		: _AnimCurvePanel()
		, _Sequence()
		, _CurveUid()
		, _WidgetWidth()
		, _ViewInputMin()
		, _ViewInputMax()
		, _OnSetInputViewRange()
		, _OnGetScrubValue()
	{}
	SLATE_ARGUMENT( TSharedPtr<SAnimCurvePanel>, AnimCurvePanel)
	// editing related variables
	SLATE_ARGUMENT( class UAnimSequenceBase*, Sequence )
	SLATE_ARGUMENT( USkeleton::AnimCurveUID, CurveUid )
	// widget viewing related variables
	SLATE_ARGUMENT( float, WidgetWidth ) // @todo do I need this?
	SLATE_ATTRIBUTE( float, ViewInputMin )
	SLATE_ATTRIBUTE( float, ViewInputMax )
	SLATE_EVENT( FOnSetInputViewRange, OnSetInputViewRange )
	SLATE_EVENT( FOnGetScrubValue, OnGetScrubValue )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SCurveEdTrack();

	// input handling for curve name
	void NewCurveNameEntered( const FText& NewText, ETextCommit::Type CommitInfo );

	// Duplicate the current track
	void DuplicateTrack();

	// Delete current track
	void DeleteTrack();

	// Sets the current mode for this curve
	void ToggleCurveMode(ECheckBoxState NewState, EAnimCurveFlags ModeToSet);

	// Returns whether this curve is of the specificed mode type
	ECheckBoxState IsCurveOfMode(EAnimCurveFlags ModeToTest) const;

	/**
	 * Build and display curve track context menu.
	 *
	 */
	FReply OnContextMenu();

	// expand editor mode 
	ECheckBoxState IsEditorExpanded() const;
	void ToggleExpandEditor(ECheckBoxState NewType);
	const FSlateBrush* GetExpandContent() const;
	FVector2D GetDesiredSize() const;

	// Bound to attribute for curve name, uses curve interface to request from skeleton
	FText GetCurveName(USkeleton::AnimCurveUID Uid) const;
};

//////////////////////////////////////////////////////////////////////////
// SCurveEdTrack

void SCurveEdTrack::Construct(const FArguments& InArgs)
{
	TSharedRef<SAnimCurvePanel> PanelRef = InArgs._AnimCurvePanel.ToSharedRef();
	PanelPtr = InArgs._AnimCurvePanel;
	bUseExpandEditor = false;
	// now create CurveInterface, 
	// find which curve this belongs to
	UAnimSequenceBase * Sequence = InArgs._Sequence;
	check (Sequence);

	// get the curve data
	FAnimCurveBase * Curve = Sequence->RawCurveData.GetCurveData(InArgs._CurveUid);
	check (Curve);

	CurveInterface = new FAnimCurveBaseInterface(Sequence, InArgs._CurveUid);
	int32 NumberOfKeys = Sequence->GetNumberOfFrames();
	//////////////////////////////
	
	TSharedPtr<SBorder> CurveBorder = nullptr;
	TSharedPtr<SHorizontalBox> InnerBox = nullptr;

	SAssignNew(CurveBorder, SBorder)
	.Padding(FMargin(2.0f, 2.0f))
	[
		SAssignNew(InnerBox, SHorizontalBox)
	];
	
	bool bIsMetadata = Curve->GetCurveTypeFlag(ACF_Metadata);
	if(!bIsMetadata)
	{
		InnerBox->AddSlot()
		.FillWidth(1)
		[
			// Notification editor panel
			SAssignNew(CurveEditor, SAnimCurveEd)
			.ViewMinInput(InArgs._ViewInputMin)
			.ViewMaxInput(InArgs._ViewInputMax)
			.DataMinInput(0.f)
			.DataMaxInput(Sequence->SequenceLength)
			// @fixme fix this to delegate
			.TimelineLength(Sequence->SequenceLength)
			.NumberOfKeys(NumberOfKeys)
			.DesiredSize(this, &SCurveEdTrack::GetDesiredSize)
			.OnSetInputViewRange(InArgs._OnSetInputViewRange)
			.OnGetScrubValue(InArgs._OnGetScrubValue)
		];

		//Inform track widget about the curve and whether it is editable or not.
		CurveEditor->SetCurveOwner(CurveInterface, true);
	}

	TSharedPtr<SHorizontalBox> NameBox = nullptr;
	SHorizontalBox::FSlot& CurveSlot = InnerBox->AddSlot()
	[
		SNew(SBox)
		.WidthOverride(InArgs._WidgetWidth)
		.VAlign(VAlign_Center)
		[
			SAssignNew(NameBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
			[
				// Name of track
				SNew(SEditableText)
				.MinDesiredWidth(64.0f)
				.IsEnabled(true)
				.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
				.SelectAllTextWhenFocused(true)
				.Text(this, &SCurveEdTrack::GetCurveName, Curve->CurveUid)
				.OnTextCommitted(this, &SCurveEdTrack::NewCurveNameEntered)
			]
		]
	];

	// Need to autowidth non-metadata names to maximise curve editor area and
	// add the expansion checkbox (unnecessary for metadata)
	if(!bIsMetadata)
	{
		CurveSlot.AutoWidth();

		NameBox->AddSlot()
		.AutoWidth()
		[
			// Name of track
			SNew(SCheckBox)
			.IsChecked(this, &SCurveEdTrack::IsEditorExpanded)
			.OnCheckStateChanged(this, &SCurveEdTrack::ToggleExpandEditor)
			.ToolTipText(LOCTEXT("Expand window", "Expand window"))
			.IsEnabled(true)
			[
				SNew(SImage)
				.Image(this, &SCurveEdTrack::GetExpandContent)
			]
		];
	}

	// Add track options combo button
	NameBox->AddSlot()
	.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
	.AutoWidth()
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("DisplayTrackOptionsMenuTooltip", "Display track options menu"))
		.OnClicked(this, &SCurveEdTrack::OnContextMenu)
		.Content()
		[
			SNew(SImage)
			.Image(FEditorStyle::GetBrush("ComboButton.Arrow"))
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];

	this->ChildSlot
	[
		CurveBorder->AsShared()
	];
}

/** return a widget */
const FSlateBrush* SCurveEdTrack::GetExpandContent() const
{
	if (bUseExpandEditor)
	{
		return FEditorStyle::GetBrush("Kismet.VariableList.HideForInstance");
	}
	else
	{
		return FEditorStyle::GetBrush("Kismet.VariableList.ExposeForInstance");
	}

}
void SCurveEdTrack::NewCurveNameEntered( const FText& NewText, ETextCommit::Type CommitInfo )
{
	if(CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		if(USkeleton* Skeleton = CurveInterface->BaseSequence->GetSkeleton())
		{
			// Check that the name doesn't already exist
			FName RequestedName = FName(*NewText.ToString());
			FSmartNameMapping* NameMapping = Skeleton->SmartNames.GetContainer(USkeleton::AnimCurveMappingName);
			if(!NameMapping->Exists(RequestedName))
			{
				FScopedTransaction Transaction(LOCTEXT("CurveEditor_RenameCurve", "Rename Curve"));
				Skeleton->RenameSmartnameAndModify(USkeleton::AnimCurveMappingName, CurveInterface->CurveData->CurveUid, FName(*NewText.ToString()));
			}
		}
	}
}

SCurveEdTrack::~SCurveEdTrack()
{
	// @fixme - check -is this okay way of doing it?
	delete CurveInterface;
}

void SCurveEdTrack::DuplicateTrack()
{
	TSharedPtr<SAnimCurvePanel> SharedPanel = PanelPtr.Pin();
	if(SharedPanel.IsValid())
	{
		SharedPanel->DuplicateTrack(CurveInterface->CurveData->CurveUid);
	}
}

void SCurveEdTrack::DeleteTrack()
{
	TSharedPtr<SAnimCurvePanel> SharedPanel = PanelPtr.Pin();
	if(SharedPanel.IsValid())
	{
		SharedPanel->DeleteTrack(CurveInterface->CurveData->CurveUid);
	}
}

void SCurveEdTrack::ToggleCurveMode(ECheckBoxState NewState,EAnimCurveFlags ModeToSet)
{
	const int32 AllModes = (ACF_DrivesMorphTarget|ACF_DrivesMaterial);
	check((ModeToSet&AllModes) != 0); //unexpected value for ModeToSet

	FFloatCurve* CurveData = (FFloatCurve*)(CurveInterface->CurveData);

	FText UndoLabel;
	bool bIsSwitchingFlagOn = !CurveData->GetCurveTypeFlag(ModeToSet);
	check(bIsSwitchingFlagOn == (NewState==ECheckBoxState::Checked));
	
	if(bIsSwitchingFlagOn)
	{
		if(ModeToSet == ACF_DrivesMorphTarget)
		{
			UndoLabel = LOCTEXT("AnimCurve_TurnOnMorphMode", "Enable driving of morph targets");
		}
		else if(ModeToSet == ACF_DrivesMaterial)
		{
			UndoLabel = LOCTEXT("AnimCurve_TurnOnMaterialMode", "Enable driving of materials");
		}
	}
	else
	{
		if(ModeToSet == ACF_DrivesMorphTarget)
		{
			UndoLabel = LOCTEXT("AnimCurve_TurnOffMorphMode", "Disable driving of morph targets");
		}
		else if(ModeToSet == ACF_DrivesMaterial)
		{
			UndoLabel = LOCTEXT("AnimCurve_TurnOffMaterialMode", "Disable driving of materials");
		}
	}

	const FScopedTransaction Transaction( UndoLabel );
	CurveInterface->MakeTransactional();
	CurveInterface->ModifyOwner();

	CurveData->SetCurveTypeFlag(ModeToSet, bIsSwitchingFlagOn);
}

ECheckBoxState SCurveEdTrack::IsCurveOfMode(EAnimCurveFlags ModeToTest) const
{
	FFloatCurve* CurveData = (FFloatCurve*)(CurveInterface->CurveData);
	return CurveData->GetCurveTypeFlag(ModeToTest) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FReply SCurveEdTrack::OnContextMenu()
{
	TSharedPtr<SAnimCurvePanel> PanelShared = PanelPtr.Pin();
	if(PanelShared.IsValid())
	{
		FFloatCurve* Curve = (FFloatCurve*)(CurveInterface->CurveData);
		FSlateApplication::Get().PushMenu(SharedThis(this),
										  PanelShared->CreateCurveContextMenu(Curve),
										  FSlateApplication::Get().GetCursorPos(),
										  FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
	}

	return FReply::Handled();
}

ECheckBoxState SCurveEdTrack::IsEditorExpanded() const
{
	return (bUseExpandEditor)? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SCurveEdTrack::ToggleExpandEditor(ECheckBoxState NewType)
{
	bUseExpandEditor = (NewType == ECheckBoxState::Checked);
}

FVector2D SCurveEdTrack::GetDesiredSize() const
{
	if (bUseExpandEditor)
	{
		return FVector2D(128.f, 128.f);
	}
	else
	{
		return FVector2D(128.f, 32.f);
	}
}

FText SCurveEdTrack::GetCurveName(USkeleton::AnimCurveUID Uid) const
{
	return CurveInterface->GetCurveName(Uid);
}

//////////////////////////////////////////////////////////////////////////
// SAnimCurvePanel

/**
 * Name validator for anim curves
 */
class FCurveNameValidator : public FStringSetNameValidator
{
public:
	FCurveNameValidator(FRawCurveTracks& Tracks, FSmartNameMapping* NameMapping, const FString& ExistingName)
		: FStringSetNameValidator(ExistingName)
	{
		FName CurveName;
		for(FFloatCurve& Curve : Tracks.FloatCurves)
		{
			if(NameMapping->GetName(Curve.CurveUid, CurveName))
			{
				Names.Add(CurveName.ToString());
			}
		}
	}
};

void SAnimCurvePanel::Construct(const FArguments& InArgs)
{
	SAnimTrackPanel::Construct( SAnimTrackPanel::FArguments()
		.WidgetWidth(InArgs._WidgetWidth)
		.ViewInputMin(InArgs._ViewInputMin)
		.ViewInputMax(InArgs._ViewInputMax)
		.InputMin(InArgs._InputMin)
		.InputMax(InArgs._InputMax)
		.OnSetInputViewRange(InArgs._OnSetInputViewRange));

	WeakPersona = InArgs._Persona;
	Sequence = InArgs._Sequence;
	WidgetWidth = InArgs._WidgetWidth;
	OnGetScrubValue = InArgs._OnGetScrubValue;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew( SExpandableArea )
			.AreaTitle( LOCTEXT("Curves", "Curves") )
			.BodyContent()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						// Name of track
						SNew(SButton)
						.Text( LOCTEXT("AddFloatTrack", "Add...") )
						.ToolTipText( LOCTEXT("AddTrackTooltip", "Add float track above here") )
						.OnClicked( this, &SAnimCurvePanel::AddButtonClicked )
						.AddMetaData<FTagMetaData>(TEXT("AnimCurve.AddFloat"))
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew( SComboButton )
						.ContentPadding(FMargin(2.0f))
						.OnGetMenuContent(this, &SAnimCurvePanel::GenerateCurveList)
					]

					+SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(5,0)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
						.Text(FText::FromString(FString::Printf(TEXT(" Total Number : %d "), Sequence->RawCurveData.FloatCurves.Num())))
					]

					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.Padding( 2.0f, 0.0f )
					[
						// Name of track
						SNew(SButton)
						.ToolTipText( LOCTEXT("DisplayTrackOptionsMenuForAllTracksTooltip", "Display track options menu for all tracks") )
						.OnClicked( this, &SAnimCurvePanel::OnContextMenu )
						.Visibility( TAttribute<EVisibility>( this, &SAnimCurvePanel::IsSetAllTracksButtonVisible ) )
						.Content()
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("ComboButton.Arrow"))
							.ColorAndOpacity( FSlateColor::UseForeground() )
						]
					]

				]

				+SVerticalBox::Slot()
				.Padding(FMargin(0.0f, 5.0f, 0.0f, 0.0f))
				.AutoHeight()
				[
					SAssignNew(PanelSlot, SSplitter)
					.Orientation(Orient_Vertical)
				]
			]
		]
	];

	UpdatePanel();
}

FReply SAnimCurvePanel::AddButtonClicked()
{
	USkeleton* CurrentSkeleton = Sequence->GetSkeleton();
	check(CurrentSkeleton);

	FMenuBuilder MenuBuilder(true, NULL);
	
	MenuBuilder.BeginSection("ConstantCurves", LOCTEXT("ConstantCurveHeading", "Constant Curve"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("NewMetadataSubMenu", "Add Metadata..."),
			LOCTEXT("NewMetadataSubMenuToolTip", "Add a new metadata entry to the sequence"),
			FNewMenuDelegate::CreateRaw(this, &SAnimCurvePanel::FillMetadataEntryMenu));
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Curves", LOCTEXT("CurveHeading", "Curve"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("NewVariableCurveSubMenu", "Add Variable Curve..."),
			LOCTEXT("NewVariableCurveSubMenuToolTip", "Add a new variable curve to the sequence"),
			FNewMenuDelegate::CreateRaw(this, &SAnimCurvePanel::FillVariableCurveMenu));
	}
	MenuBuilder.EndSection();

	// Show dialog to enter new track name
	FSlateApplication::Get().PushMenu(
		SharedThis( this ),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup)
		);

	return FReply::Handled();	
}

void SAnimCurvePanel::CreateTrack(const FText& ComittedText, ETextCommit::Type CommitInfo)
{
	if ( CommitInfo == ETextCommit::OnEnter )
	{
		USkeleton* Skeleton = Sequence->GetSkeleton();
		if(Skeleton && !ComittedText.IsEmpty())
		{
			const FScopedTransaction Transaction(LOCTEXT("AnimCurve_AddTrack", "Add New Curve"));
			USkeleton::AnimCurveUID CurveUid;

			if(Skeleton->AddSmartnameAndModify(USkeleton::AnimCurveMappingName, FName(*ComittedText.ToString()), CurveUid))
			{
				AddVariableCurve(CurveUid);
			}
		}

		FSlateApplication::Get().DismissAllMenus();
	}
}

FReply SAnimCurvePanel::DuplicateTrack(USkeleton::AnimCurveUID Uid)
{
	const FScopedTransaction Transaction( LOCTEXT("AnimCurve_DuplicateTrack", "Duplicate Curve") );
	
	FSmartNameMapping* NameMapping = Sequence->GetSkeleton()->SmartNames.GetContainer(USkeleton::AnimCurveMappingName);
	FName CurveNameToCopy;
	USkeleton::AnimCurveUID NewUid;

	// Must have a curve that exists to duplicate
	if(NameMapping->Exists(Uid))
	{
		NameMapping->GetName(Uid, CurveNameToCopy);
		TSharedPtr<INameValidatorInterface> Validator = MakeShareable(new FCurveNameValidator(Sequence->RawCurveData, NameMapping, FString(TEXT(""))));

		// Use the validator to pick a reasonable name for the duplicated curve.
		FString NewCurveName = CurveNameToCopy.ToString();
		Validator->FindValidString(NewCurveName);
		if(NameMapping->AddOrFindName(*NewCurveName, NewUid))
		{
			if(Sequence->RawCurveData.DuplicateCurveData(Uid, NewUid))
			{
				Sequence->Modify();
				UpdatePanel();

				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();
}

void SAnimCurvePanel::DeleteTrack(USkeleton::AnimCurveUID Uid)
{
	const FScopedTransaction Transaction( LOCTEXT("AnimCurve_DeleteTrack", "Delete Curve") );
	
	if(Sequence->RawCurveData.GetCurveData(Uid))
	{
		Sequence->Modify(true);
		Sequence->RawCurveData.DeleteCurveData(Uid);
		UpdatePanel();
	}
}

FReply SAnimCurvePanel::OnContextMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);

	MenuBuilder.BeginSection("AnimCurvePanelCurveTypes", LOCTEXT("AllCurveTypesHeading", "All Curve Types"));
	{
		MenuBuilder.AddWidget(
			SNew(SCheckBox)
			.IsChecked( this, &SAnimCurvePanel::AreAllCurvesOfMode, ACF_DrivesMorphTarget )
			.OnCheckStateChanged( this, &SAnimCurvePanel::ToggleAllCurveModes, ACF_DrivesMorphTarget )
			.ToolTipText(LOCTEXT("MorphCurveModeTooltip", "This curve drives a morph target"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MorphCurveMode", "Morph Curve"))
			],
			FText()
		);

		MenuBuilder.AddWidget(
			SNew(SCheckBox)
			.IsChecked( this, &SAnimCurvePanel::AreAllCurvesOfMode, ACF_DrivesMaterial )
			.OnCheckStateChanged( this, &SAnimCurvePanel::ToggleAllCurveModes, ACF_DrivesMaterial )
			.ToolTipText(LOCTEXT("MaterialCurveModeTooltip", "This curve drives a material"))
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MaterialCurveMode", "Material Curve"))
			],
			FText()
		);

		FSlateApplication::Get().PushMenu(	SharedThis(this),
			MenuBuilder.MakeWidget(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup) );
	}
	MenuBuilder.EndSection();

	return FReply::Handled();
}

EVisibility SAnimCurvePanel::IsSetAllTracksButtonVisible() const
{
	return (Tracks.Num() > 1) ? EVisibility::Visible : EVisibility::Hidden;
}

void SAnimCurvePanel::ToggleAllCurveModes(ECheckBoxState NewState, EAnimCurveFlags ModeToSet)
{
	const ECheckBoxState CurrentAllState = AreAllCurvesOfMode(ModeToSet);
	for(TWeakPtr<SCurveEdTrack> TrackWeak : Tracks)
	{
		TSharedPtr<SCurveEdTrack> TrackWidget = TrackWeak.Pin();
		if( TrackWidget.IsValid() )
		{
			const ECheckBoxState CurrentTrackState = TrackWidget->IsCurveOfMode(ModeToSet);
			if( (CurrentAllState == CurrentTrackState) || ((CurrentAllState == ECheckBoxState::Undetermined) && (CurrentTrackState == ECheckBoxState::Unchecked)) )
			{
				TrackWidget->ToggleCurveMode( NewState, ModeToSet );
			}
		}
	}
}

ECheckBoxState SAnimCurvePanel::AreAllCurvesOfMode(EAnimCurveFlags ModeToSet) const
{
	int32 NumChecked = 0;
	for(const TWeakPtr<SCurveEdTrack> TrackWeak : Tracks)
	{
		const TSharedPtr<SCurveEdTrack> TrackWidget = TrackWeak.Pin();
		if( TrackWidget.IsValid() )
		{
			if ( TrackWidget->IsCurveOfMode(ModeToSet) == ECheckBoxState::Checked )
			{
				NumChecked++;
			}
		}
	}
	if( NumChecked == Tracks.Num() )
	{
		return ECheckBoxState::Checked;
	}
	else if( NumChecked == 0 )
	{
		return ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Undetermined;
}

void SAnimCurvePanel::UpdatePanel()
{
	if(Sequence != NULL)
	{
		USkeleton* CurrentSkeleton = Sequence->GetSkeleton();
		FSmartNameMapping* MetadataNameMap = CurrentSkeleton->SmartNames.GetContainer(USkeleton::AnimCurveMappingName);
		// Sort the raw curves before setting up display
		Sequence->RawCurveData.FloatCurves.Sort([MetadataNameMap](const FFloatCurve& A, const FFloatCurve& B)
		{
			bool bAMeta = A.GetCurveTypeFlag(ACF_Metadata);
			bool bBMeta = B.GetCurveTypeFlag(ACF_Metadata);
			
			if(bAMeta != bBMeta)
			{
				return !bAMeta;
			}

			FName AName;
			FName BName;
			MetadataNameMap->GetName(A.CurveUid, AName);
			MetadataNameMap->GetName(B.CurveUid, BName);

			return AName < BName;
		});

		// see if we need to clear or not
		FChildren * VariableChildren = PanelSlot->GetChildren();
		for (int32 Id=VariableChildren->Num()-1; Id>=0; --Id)
		{
			PanelSlot->RemoveAt(Id);
		}

		// Clear all tracks as we're re-adding them all anyway.
		Tracks.Empty();

		// Updating new tracks
		FSmartNameMapping* NameMapping = CurrentSkeleton->SmartNames.GetContainer(USkeleton::AnimCurveMappingName);

		int32 TotalCurve = Sequence->RawCurveData.FloatCurves.Num();
		for(int32 CurrentIt = 0 ; CurrentIt < TotalCurve ; ++CurrentIt)
		{
			FFloatCurve&  Curve = Sequence->RawCurveData.FloatCurves[CurrentIt];

			const bool bEditable = Curve.GetCurveTypeFlag(ACF_Editable);
			const bool bConstant = Curve.GetCurveTypeFlag(ACF_Metadata);
			FName CurveName;

			// if editable, add to the list
			if(bEditable && NameMapping->GetName(Curve.CurveUid, CurveName))
			{
				TSharedPtr<SCurveEdTrack> CurrentTrack;
				PanelSlot->AddSlot()
				.SizeRule(SSplitter::SizeToContent)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					[
						SAssignNew(CurrentTrack, SCurveEdTrack)
						.Sequence(Sequence)
						.CurveUid(Curve.CurveUid)
						.AnimCurvePanel(SharedThis(this))
						.WidgetWidth(WidgetWidth)
						.ViewInputMin(ViewInputMin)
						.ViewInputMax(ViewInputMax)
						.OnGetScrubValue(OnGetScrubValue)
						.OnSetInputViewRange(OnSetInputViewRange)
					]
				];
				Tracks.Add(CurrentTrack);
			}
		}

		TSharedPtr<FPersona> SharedPersona = WeakPersona.Pin();
		if(SharedPersona.IsValid())
		{
			SharedPersona->OnCurvesChanged.Broadcast();
		}
	}
}

void SAnimCurvePanel::SetSequence(class UAnimSequenceBase *	InSequence)
{
	if (InSequence != Sequence)
	{
		Sequence = InSequence;
		UpdatePanel();
	}
}

TSharedRef<SWidget> SAnimCurvePanel::GenerateCurveList()
{
	TSharedPtr<SVerticalBox> MainBox, ListBox;
	TSharedRef<SWidget> NewWidget = SAssignNew(MainBox, SVerticalBox);

	if ( Sequence && Sequence->RawCurveData.FloatCurves.Num() > 0)
	{
		MainBox->AddSlot()
			.AutoHeight()
			.MaxHeight(300)
			[
				SNew( SScrollBox )
				+SScrollBox::Slot() 
				[
					SAssignNew(ListBox, SVerticalBox)
				]
			];

		// Mapping to retrieve curve names
		FSmartNameMapping* NameMapping = Sequence->GetSkeleton()->SmartNames.GetContainer(USkeleton::AnimCurveMappingName);
		check(NameMapping);

		for (auto Iter=Sequence->RawCurveData.FloatCurves.CreateConstIterator(); Iter; ++Iter)
		{
			const FFloatCurve& Curve= *Iter;

			FName CurveName;
			NameMapping->GetName(Curve.CurveUid, CurveName);

			ListBox->AddSlot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				.Padding( 2.0f, 2.0f )
				[
					SNew( SCheckBox )
					.IsChecked(this, &SAnimCurvePanel::IsCurveEditable, Curve.CurveUid)
					.OnCheckStateChanged(this, &SAnimCurvePanel::ToggleEditability, Curve.CurveUid)
					.ToolTipText( LOCTEXT("Show Curves", "Show or Hide Curves") )
					.IsEnabled( true )
					[
						SNew( STextBlock )
						.Text(FText::FromName(CurveName))
					]
				];
		}

		MainBox->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding( 2.0f, 2.0f )
			[
				SNew( SButton )
				.VAlign( VAlign_Center )
				.HAlign( HAlign_Center )
				.OnClicked( this, &SAnimCurvePanel::RefreshPanel )
				[
					SNew( STextBlock )
					.Text( LOCTEXT("RefreshCurve", "Refresh") )
				]
			];

		MainBox->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding( 2.0f, 2.0f )
			[
				SNew( SButton )
				.VAlign( VAlign_Center )
				.HAlign( HAlign_Center )
				.OnClicked( this, &SAnimCurvePanel::ShowAll, true )
				[
					SNew( STextBlock )
					.Text( LOCTEXT("ShowAll", "Show All") )
				]
			];

		MainBox->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding( 2.0f, 2.0f )
			[
				SNew( SButton )
				.VAlign( VAlign_Center )
				.HAlign( HAlign_Center )
				.OnClicked( this, &SAnimCurvePanel::ShowAll, false )
				[
					SNew( STextBlock )
					.Text( LOCTEXT("HideAll", "Hide All") )
				]
			];
	}
	else
	{
		MainBox->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding( 2.0f, 2.0f )
			[
				SNew( STextBlock )
				.Text(LOCTEXT("Not Available", "No curve exists"))
			];
	}

	return NewWidget;
}

ECheckBoxState SAnimCurvePanel::IsCurveEditable(USkeleton::AnimCurveUID Uid) const
{
	if ( Sequence )
	{
		const FFloatCurve* Curve = static_cast<const FFloatCurve *>(Sequence->RawCurveData.GetCurveData(Uid, FRawCurveTracks::FloatType));
		if ( Curve )
		{
			return Curve->GetCurveTypeFlag(ACF_Editable)? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
	}

	return ECheckBoxState::Undetermined;
}

void SAnimCurvePanel::ToggleEditability(ECheckBoxState NewType, USkeleton::AnimCurveUID Uid)
{
	bool bEdit = (NewType == ECheckBoxState::Checked);

	if ( Sequence )
	{
		FFloatCurve * Curve = static_cast<FFloatCurve *>(Sequence->RawCurveData.GetCurveData(Uid, FRawCurveTracks::FloatType));
		if ( Curve )
		{
			Curve->SetCurveTypeFlag(ACF_Editable, bEdit);
		}
	}
}

FReply		SAnimCurvePanel::RefreshPanel()
{
	UpdatePanel();
	return FReply::Handled();
}

FReply		SAnimCurvePanel::ShowAll(bool bShow)
{
	if ( Sequence )
	{
		for (auto Iter = Sequence->RawCurveData.FloatCurves.CreateIterator(); Iter; ++Iter)
		{
			FFloatCurve & Curve = *Iter;
			Curve.SetCurveTypeFlag(ACF_Editable, bShow);
		}

		UpdatePanel();
	}

	return FReply::Handled();
}

void SAnimCurvePanel::FillMetadataEntryMenu(FMenuBuilder& Builder)
{
	USkeleton* CurrentSkeleton = Sequence->GetSkeleton();
	check(CurrentSkeleton);

	FSmartNameMapping* Mapping = CurrentSkeleton->SmartNames.GetContainer(USkeleton::AnimCurveMappingName);
	TArray<USkeleton::AnimCurveUID> CurveUids;
	Mapping->FillUidArray(CurveUids);

	Builder.BeginSection(NAME_None, LOCTEXT("MetadataMenu_ListHeading", "Available Names"));
	for(USkeleton::AnimCurveUID Id : CurveUids)
	{
		if(!Sequence->RawCurveData.GetCurveData(Id))
		{
			FName CurveName;
			if(Mapping->GetName(Id, CurveName))
			{
				const FText Description = LOCTEXT("NewMetadataSubMenu_ToolTip", "Add an existing metadata curve");
				const FText Label = FText::FromName(CurveName);

				FUIAction UIAction;
				UIAction.ExecuteAction.BindRaw(
					this, &SAnimCurvePanel::AddMetadataEntry,
					Id);

				Builder.AddMenuEntry(Label, Description, FSlateIcon(), UIAction);
			}
		}
	}
	Builder.EndSection();

	Builder.AddMenuSeparator();

	const FText Description = LOCTEXT("NewMetadataCreateNew_ToolTip", "Create a new metadata entry");
	const FText Label = LOCTEXT("NewMetadataCreateNew_Label","Create New");
	FUIAction UIAction;
	UIAction.ExecuteAction.BindRaw(this, &SAnimCurvePanel::CreateNewMetadataEntryClicked);

	Builder.AddMenuEntry(Label, Description, FSlateIcon(), UIAction);
}

void SAnimCurvePanel::FillVariableCurveMenu(FMenuBuilder& Builder)
{
	USkeleton* CurrentSkeleton = Sequence->GetSkeleton();
	check(CurrentSkeleton);

	FSmartNameMapping* Mapping = CurrentSkeleton->SmartNames.GetContainer(USkeleton::AnimCurveMappingName);
	TArray<USkeleton::AnimCurveUID> CurveUids;
	Mapping->FillUidArray(CurveUids);

	Builder.BeginSection(NAME_None, LOCTEXT("VariableMenu_ListHeading", "Available Names"));
	for(USkeleton::AnimCurveUID Id : CurveUids)
	{
		if(!Sequence->RawCurveData.GetCurveData(Id))
		{
			FName CurveName;
			if(Mapping->GetName(Id, CurveName))
			{
				const FText Description = LOCTEXT("NewVariableSubMenu_ToolTip", "Add an existing variable curve");
				const FText Label = FText::FromName(CurveName);

				FUIAction UIAction;
				UIAction.ExecuteAction.BindRaw(
					this, &SAnimCurvePanel::AddVariableCurve,
					Id);

				Builder.AddMenuEntry(Label, Description, FSlateIcon(), UIAction);
			}
		}
	}
	Builder.EndSection();

	Builder.AddMenuSeparator();

	const FText Description = LOCTEXT("NewVariableCurveCreateNew_ToolTip", "Create a new variable curve");
	const FText Label = LOCTEXT("NewVariableCurveCreateNew_Label", "Create Curve");
	FUIAction UIAction;
	UIAction.ExecuteAction.BindRaw(this, &SAnimCurvePanel::CreateNewCurveClicked);

	Builder.AddMenuEntry(Label, Description, FSlateIcon(), UIAction);
}

void SAnimCurvePanel::AddMetadataEntry(USkeleton::AnimCurveUID Uid)
{
	if(Sequence->RawCurveData.AddCurveData(Uid))
	{
		Sequence->Modify(true);
		FFloatCurve* Curve = static_cast<FFloatCurve *>(Sequence->RawCurveData.GetCurveData(Uid, FRawCurveTracks::FloatType));
		Curve->FloatCurve.AddKey(0.0f, 1.0f);
		Curve->SetCurveTypeFlag(ACF_Metadata, true);
		RefreshPanel();
	}
}

void SAnimCurvePanel::CreateNewMetadataEntryClicked()
{
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewMetadataCurveEntryLabal", "Metadata Name"))
		.OnTextCommitted(this, &SAnimCurvePanel::CreateNewMetadataEntry);

	FSlateApplication& SlateApp = FSlateApplication::Get();
	SlateApp.PushMenu(
		AsShared(),
		TextEntry,
		SlateApp.GetCursorPos(),
		FPopupTransitionEffect::TypeInPopup
		);
}

void SAnimCurvePanel::CreateNewMetadataEntry(const FText& CommittedText, ETextCommit::Type CommitType)
{
	FSlateApplication::Get().DismissAllMenus();
	if(CommitType == ETextCommit::OnEnter)
	{
		// Add the name to the skeleton and then add the new curve to the sequence
		USkeleton* Skeleton = Sequence->GetSkeleton();
		if(Skeleton && !CommittedText.IsEmpty())
		{
			USkeleton::AnimCurveUID CurveUid;

			if(Skeleton->AddSmartnameAndModify(USkeleton::AnimCurveMappingName, FName(*CommittedText.ToString()), CurveUid))
			{
				AddMetadataEntry(CurveUid);
			}
		}
	}
}

void SAnimCurvePanel::CreateNewCurveClicked()
{
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewCurveEntryLabal", "Curve Name"))
		.OnTextCommitted(this, &SAnimCurvePanel::CreateTrack);

	FSlateApplication& SlateApp = FSlateApplication::Get();
	SlateApp.PushMenu(
		AsShared(),
		TextEntry,
		SlateApp.GetCursorPos(),
		FPopupTransitionEffect::TypeInPopup
		);
}

TSharedRef<SWidget> SAnimCurvePanel::CreateCurveContextMenu(FFloatCurve* Curve) const
{
	FMenuBuilder MenuBuilder(true, NULL);

	MenuBuilder.BeginSection("AnimCurvePanelCurveTypes", LOCTEXT("CurveTypesHeading", "Curve Types"));
	{
		MenuBuilder.AddWidget(
			SNew(SCheckBox)
			.IsChecked(this, &SAnimCurvePanel::GetCurveFlagAsCheckboxState, Curve, ACF_DrivesMorphTarget)
			.OnCheckStateChanged(this, &SAnimCurvePanel::SetCurveFlagFromCheckboxState, Curve, ACF_DrivesMorphTarget)
			.ToolTipText(LOCTEXT("MorphCurveModeTooltip", "This curve drives a morph target"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MorphCurveMode", "Morph Curve"))
			],
			FText()
			);

		MenuBuilder.AddWidget(
			SNew(SCheckBox)
			.IsChecked(this, &SAnimCurvePanel::GetCurveFlagAsCheckboxState, Curve, ACF_DrivesMaterial)
			.OnCheckStateChanged(this, &SAnimCurvePanel::SetCurveFlagFromCheckboxState, Curve, ACF_DrivesMaterial)
			.ToolTipText(LOCTEXT("MaterialCurveModeTooltip", "This curve drives a material"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MaterialCurveMode", "Material Curve"))
			],
			FText()
			);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimCurvePanelTrackOptions", LOCTEXT("TrackOptionsHeading", "Track Options"));
	{
		FText TypeToggleLabel = LOCTEXT("TypeToggleToMetadataLabel", "Convert to Metadata");
		FText TypeToggleToolTip = LOCTEXT("TypeToggleToMetadataToolTip", "Turns this curve into a Metadata entry. This is a destructive operation and will remove the keys in this curve");
		bool bIsConstantCurve = Curve->GetCurveTypeFlag(ACF_Metadata);

		FUIAction NewAction;

		if(bIsConstantCurve)
		{
			TypeToggleLabel = LOCTEXT("TypeToggleToVariableLabel", "Convert to Variable Curve");
			TypeToggleToolTip = LOCTEXT("TypeToggleToVariableToolTip", "Turns this curve into a variable curve.");
		}

		NewAction.ExecuteAction.BindSP(this, &SAnimCurvePanel::ToggleCurveTypeMenuCallback, Curve);
		MenuBuilder.AddMenuEntry(
			TypeToggleLabel,
			TypeToggleToolTip,
			FSlateIcon(),
			NewAction);

		NewAction.ExecuteAction.BindSP(this, &SAnimCurvePanel::DeleteTrack, Curve->CurveUid);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("RemoveTrack", "Remove Track"),
			LOCTEXT("RemoveTrackTooltip", "Remove this track"),
			FSlateIcon(),
			NewAction);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

ECheckBoxState SAnimCurvePanel::GetCurveFlagAsCheckboxState(FFloatCurve* Curve, EAnimCurveFlags InFlag) const
{
	return Curve->GetCurveTypeFlag(InFlag) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAnimCurvePanel::SetCurveFlagFromCheckboxState(ECheckBoxState CheckState, FFloatCurve* Curve, EAnimCurveFlags InFlag)
{
	bool Enabled = CheckState == ECheckBoxState::Checked;
	Curve->SetCurveTypeFlag(InFlag, Enabled);
}

void SAnimCurvePanel::ToggleCurveTypeMenuCallback(FFloatCurve* Curve)
{
	check(Curve);

	FScopedTransaction Transaction(LOCTEXT("CurvePanel_ToggleCurveType", "Toggle curve type"));
	Sequence->Modify(true);
	bool bIsSet = Curve->GetCurveTypeFlag(ACF_Metadata);
	Curve->SetCurveTypeFlag(ACF_Metadata, !bIsSet);

	if(!bIsSet)
	{
		// We're moving to a metadata curve, we need to clear out the keys.
		Curve->FloatCurve.Reset();
		Curve->FloatCurve.AddKey(0.0f, 1.0f);
	}

	UpdatePanel();
}

void SAnimCurvePanel::AddVariableCurve(USkeleton::AnimCurveUID CurveUid)
{
	Sequence->Modify(true);
	Sequence->RawCurveData.AddCurveData(CurveUid);
	UpdatePanel();
}

#undef LOCTEXT_NAMESPACE
