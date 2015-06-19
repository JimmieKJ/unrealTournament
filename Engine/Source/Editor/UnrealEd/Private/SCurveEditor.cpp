// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "RichCurveEditorCommands.h"
#include "SCurveEditor.h"
#include "ScopedTransaction.h"
#include "SColorGradientEditor.h"
#include "GenericCommands.h"
#include "SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "SCurveEditor"

const static FVector2D	CONST_KeySize		= FVector2D(11,11);
const static FVector2D	CONST_TangentSize	= FVector2D(7,7);
const static FVector2D	CONST_CurveSize		= FVector2D(12,12);

const static float		CONST_FitMargin		= 0.05f;
const static float		CONST_MinViewRange	= 0.01f;
const static float		CONST_DefaultZoomRange	= 1.0f;
const static float      CONST_KeyTangentOffset = 60.0f;


//////////////////////////////////////////////////////////////////////////
// SCurveEditor

void SCurveEditor::Construct(const FArguments& InArgs)
{
	CurveFactory = NULL;
	Commands = TSharedPtr< FUICommandList >(new FUICommandList);
	CurveOwner = NULL;

	// view input
	ViewMinInput = InArgs._ViewMinInput;
	ViewMaxInput = InArgs._ViewMaxInput;
	// data input - only used when it's set
	DataMinInput = InArgs._DataMinInput;
	DataMaxInput = InArgs._DataMaxInput;

	ViewMinOutput = InArgs._ViewMinOutput;
	ViewMaxOutput = InArgs._ViewMaxOutput;

	InputSnap = InArgs._InputSnap;
	OutputSnap = InArgs._OutputSnap;
	bSnappingEnabled = InArgs._SnappingEnabled;

	bZoomToFitVertical = InArgs._ZoomToFitVertical;
	bZoomToFitHorizontal = InArgs._ZoomToFitHorizontal;
	DesiredSize = InArgs._DesiredSize;

	GridColor = InArgs._GridColor;

	bIsUsingSlider = false;

	// if editor size is set, use it, otherwise, use default value
	if (DesiredSize.Get().IsZero())
	{
		DesiredSize.Set(FVector2D(128, 64));
	}

	TimelineLength = InArgs._TimelineLength;
	SetInputViewRangeHandler = InArgs._OnSetInputViewRange;
	SetOutputViewRangeHandler = InArgs._OnSetOutputViewRange;
	bDrawCurve = InArgs._DrawCurve;
	bHideUI = InArgs._HideUI;
	bAllowZoomOutput = InArgs._AllowZoomOutput;
	bAlwaysDisplayColorCurves = InArgs._AlwaysDisplayColorCurves;
	bShowZoomButtons = InArgs._ShowZoomButtons;
	bShowCurveSelector = InArgs._ShowCurveSelector;
	bDrawInputGridNumbers = InArgs._ShowInputGridNumbers;
	bDrawOutputGridNumbers = InArgs._ShowOutputGridNumbers;
	bShowCurveToolTips = InArgs._ShowCurveToolTips;

	OnCreateAsset = InArgs._OnCreateAsset;

	DragState = EDragState::None;
	DragThreshold = 4;

	//Simple r/g/b for now
	CurveColors.Add(FLinearColor(1.0f, 0.0f, 0.0f));
	CurveColors.Add(FLinearColor(0.0f, 1.0f, 0.0f));
	CurveColors.Add(FLinearColor(0.05f, 0.05f, 1.0f));

	TransactionIndex = -1;

	Commands->MapAction(FGenericCommands::Get().Undo,
		FExecuteAction::CreateSP(this, &SCurveEditor::UndoAction));

	Commands->MapAction(FGenericCommands::Get().Redo,
		FExecuteAction::CreateSP(this, &SCurveEditor::RedoAction));

	Commands->MapAction(FRichCurveEditorCommands::Get().ZoomToFitHorizontal,
		FExecuteAction::CreateSP(this, &SCurveEditor::ZoomToFitHorizontal, false));

	Commands->MapAction(FRichCurveEditorCommands::Get().ZoomToFitVertical,
		FExecuteAction::CreateSP(this, &SCurveEditor::ZoomToFitVertical, false));

	Commands->MapAction(FRichCurveEditorCommands::Get().ZoomToFitAll,
		FExecuteAction::CreateSP(this, &SCurveEditor::ZoomToFitAll, false));

	Commands->MapAction(FRichCurveEditorCommands::Get().ZoomToFitSelected,
		FExecuteAction::CreateSP(this, &SCurveEditor::ZoomToFitAll, true));

	Commands->MapAction(FRichCurveEditorCommands::Get().ToggleSnapping,
		FExecuteAction::CreateSP(this, &SCurveEditor::ToggleSnapping),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SCurveEditor::IsSnappingEnabled));

	Commands->MapAction(FRichCurveEditorCommands::Get().InterpolationConstant,
		FExecuteAction::CreateSP(this, &SCurveEditor::OnSelectInterpolationMode, RCIM_Constant, RCTM_Auto),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SCurveEditor::IsInterpolationModeSelected, RCIM_Constant, RCTM_Auto));

	Commands->MapAction(FRichCurveEditorCommands::Get().InterpolationLinear,
		FExecuteAction::CreateSP(this, &SCurveEditor::OnSelectInterpolationMode, RCIM_Linear, RCTM_Auto),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SCurveEditor::IsInterpolationModeSelected, RCIM_Linear, RCTM_Auto));

	Commands->MapAction(FRichCurveEditorCommands::Get().InterpolationCubicAuto,
		FExecuteAction::CreateSP(this, &SCurveEditor::OnSelectInterpolationMode, RCIM_Cubic, RCTM_Auto),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SCurveEditor::IsInterpolationModeSelected, RCIM_Cubic, RCTM_Auto));

	Commands->MapAction(FRichCurveEditorCommands::Get().InterpolationCubicUser,
		FExecuteAction::CreateSP(this, &SCurveEditor::OnSelectInterpolationMode, RCIM_Cubic, RCTM_User),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SCurveEditor::IsInterpolationModeSelected, RCIM_Cubic, RCTM_User));

	Commands->MapAction(FRichCurveEditorCommands::Get().InterpolationCubicBreak,
		FExecuteAction::CreateSP(this, &SCurveEditor::OnSelectInterpolationMode, RCIM_Cubic, RCTM_Break),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SCurveEditor::IsInterpolationModeSelected, RCIM_Cubic, RCTM_Break));

	SAssignNew(WarningMessageText, SErrorText);

	TSharedRef<SBox> CurveSelector = SNew(SBox)
		.VAlign(VAlign_Top)
		.Visibility(this, &SCurveEditor::GetCurveSelectorVisibility)
		[
			CreateCurveSelectionWidget()
		];

	CurveSelectionWidget = CurveSelector;

	InputAxisName = InArgs._XAxisName.IsSet() ? FText::FromString(InArgs._XAxisName.GetValue()) : LOCTEXT("Time", "Time");
	OutputAxisName = InArgs._YAxisName.IsSet() ? FText::FromString(InArgs._YAxisName.GetValue()) : LOCTEXT("Value", "Value");

	this->ChildSlot
	[
		SNew( SHorizontalBox )

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			.Visibility( this, &SCurveEditor::GetCurveAreaVisibility )

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(30, 12, 0, 0))
			[
				CurveSelector
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Left)
				.BorderImage( FEditorStyle::GetBrush("NoBorder") )
				.DesiredSizeScale(FVector2D(256.0f,32.0f))
				.Padding(FMargin(2, 12, 0, 0))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ToolTipText(LOCTEXT("ZoomToFitHorizontal", "Zoom To Fit Horizontal"))
						.Visibility(this, &SCurveEditor::GetZoomButtonVisibility)
						.OnClicked(this, &SCurveEditor::ZoomToFitHorizontalClicked)
						.ContentPadding(1)
						[
							SNew(SImage) 
							.Image( FEditorStyle::GetBrush("CurveEd.FitHorizontal") ) 
							.ColorAndOpacity( FSlateColor::UseForeground() ) 
						]
					]
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ToolTipText(LOCTEXT("ZoomToFitVertical", "Zoom To Fit Vertical"))
						.Visibility(this, &SCurveEditor::GetZoomButtonVisibility)
						.OnClicked(this, &SCurveEditor::ZoomToFitVerticalClicked)
						.ContentPadding(1)
						[
							SNew(SImage) 
							.Image( FEditorStyle::GetBrush("CurveEd.FitVertical") ) 
							.ColorAndOpacity( FSlateColor::UseForeground() ) 
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("NoBorder"))
						.Visibility(this, &SCurveEditor::GetEditVisibility)
						.VAlign(VAlign_Center)
						[
							SNew(SNumericEntryBox<float>)
							.IsEnabled(this, &SCurveEditor::GetInputEditEnabled)
							.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
							.Value(this, &SCurveEditor::OnGetTime)
							.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
							.OnValueCommitted(this, &SCurveEditor::OnTimeComitted)
							.OnValueChanged(this, &SCurveEditor::OnTimeChanged)
							.OnBeginSliderMovement(this, &SCurveEditor::OnBeginSliderMovement, LOCTEXT("SetValue", "Set New Time"))
							.OnEndSliderMovement(this, &SCurveEditor::OnEndSliderMovement)
							.LabelVAlign(VAlign_Center)
							.AllowSpin(true)
							.MinValue(TOptional<float>())
							.MaxValue(TOptional<float>())
							.MaxSliderValue(TOptional<float>())
							.MinSliderValue(TOptional<float>())
							.Delta(0.001f)
							.MinDesiredValueWidth(60.0f)
							.Label()
							[
								SNew(STextBlock)
								.Text(InputAxisName)
							]
						]
					]
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBorder)
						.BorderImage( FEditorStyle::GetBrush("NoBorder") )
						.Visibility(this, &SCurveEditor::GetEditVisibility)
						.VAlign(VAlign_Center)
						[
							SNew(SNumericEntryBox<float>)
							.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
							.Value(this, &SCurveEditor::OnGetValue)
							.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
							.OnValueCommitted(this, &SCurveEditor::OnValueComitted)
							.OnValueChanged(this, &SCurveEditor::OnValueChanged)
							.OnBeginSliderMovement(this, &SCurveEditor::OnBeginSliderMovement, LOCTEXT("SetValue", "Set New Value"))
							.OnEndSliderMovement(this, &SCurveEditor::OnEndSliderMovement)
							.LabelVAlign(VAlign_Center)
							.AllowSpin(true)
							.MinValue(TOptional<float>())
							.MaxValue(TOptional<float>())
							.MaxSliderValue(TOptional<float>())
							.MinSliderValue(TOptional<float>())
							.Delta(.001f)
							.MinDesiredValueWidth(60.0f)
							.Label()
							[
								SNew(STextBlock)
								.Text(OutputAxisName)
							]
						]
					]
				]
			]
		]


		+ SVerticalBox::Slot()
		.VAlign(VAlign_Bottom)
		.FillHeight(.75f)
		[
			SNew( SBorder )
			.Visibility( this, &SCurveEditor::GetColorGradientVisibility )
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			.BorderBackgroundColor( FLinearColor( .8f, .8f, .8f, .60f ) )
			.Padding(1.0f)
			[
				SAssignNew( GradientViewer, SColorGradientEditor )
				.ViewMinInput( ViewMinInput )
				.ViewMaxInput( ViewMaxInput )
				.IsEditingEnabled( this, &SCurveEditor::IsEditingEnabled )
			]
		]
		]
	];

	if (GEditor != NULL)
	{
		GEditor->RegisterForUndo(this);
	}
}

FText SCurveEditor::GetIsCurveVisibleToolTip(TSharedPtr<FCurveViewModel> CurveViewModel) const
{
	return CurveViewModel->bIsVisible ?
		FText::Format(LOCTEXT("HideFormat", "Hide {0} curve"), FText::FromName(CurveViewModel->CurveInfo.CurveName)) :
		FText::Format(LOCTEXT("ShowFormat", "Show {0} curve"), FText::FromName(CurveViewModel->CurveInfo.CurveName));
}

ECheckBoxState SCurveEditor::IsCurveVisible(TSharedPtr<FCurveViewModel> CurveViewModel) const
{
	return CurveViewModel->bIsVisible ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SCurveEditor::OnCurveIsVisibleChanged(ECheckBoxState NewCheckboxState, TSharedPtr<FCurveViewModel> CurveViewModel)
{
	if (NewCheckboxState == ECheckBoxState::Checked)
	{
		CurveViewModel->bIsVisible = true;
	}
	else
	{
		CurveViewModel->bIsVisible = false;
		RemoveCurveKeysFromSelection(CurveViewModel);
	}
}

FText SCurveEditor::GetIsCurveLockedToolTip(TSharedPtr<FCurveViewModel> CurveViewModel) const
{
	return CurveViewModel->bIsLocked ?
		FText::Format(LOCTEXT("UnlockFormat", "Unlock {0} curve for editing"), FText::FromName(CurveViewModel->CurveInfo.CurveName)) :
		FText::Format(LOCTEXT("LockFormat", "Lock {0} curve for editing"), FText::FromName(CurveViewModel->CurveInfo.CurveName));
}

ECheckBoxState SCurveEditor::IsCurveLocked(TSharedPtr<FCurveViewModel> CurveViewModel) const
{
	return CurveViewModel->bIsLocked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SCurveEditor::OnCurveIsLockedChanged(ECheckBoxState NewCheckboxState, TSharedPtr<FCurveViewModel> CurveViewModel)
{
	if (NewCheckboxState == ECheckBoxState::Checked)
	{
		CurveViewModel->bIsLocked = true;
		RemoveCurveKeysFromSelection(CurveViewModel);
	}
	else
	{
		CurveViewModel->bIsLocked = false;
	}
}

void SCurveEditor::RemoveCurveKeysFromSelection(TSharedPtr<FCurveViewModel> CurveViewModel)
{
	TArray<FSelectedCurveKey> SelectedKeysForLockedCurve;
	for (auto SelectedKey : SelectedKeys)
	{
		if (SelectedKey.Curve == CurveViewModel->CurveInfo.CurveToEdit)
		{
			SelectedKeysForLockedCurve.Add(SelectedKey);
		}
	}
	for (auto KeyToDeselect : SelectedKeysForLockedCurve)
	{
		RemoveFromSelection(KeyToDeselect);
	}
}

FText SCurveEditor::GetCurveToolTipNameText() const
{
	return CurveToolTipNameText;
}

FText SCurveEditor::GetCurveToolTipInputText() const
{
	return CurveToolTipInputText;
}

FText SCurveEditor::GetCurveToolTipOutputText() const
{
	return CurveToolTipOutputText;
}

SCurveEditor::~SCurveEditor()
{
	if (GEditor != NULL)
	{
		GEditor->UnregisterForUndo(this);
	}
}

TSharedRef<SWidget> SCurveEditor::CreateCurveSelectionWidget() const
{
	TSharedRef<SVerticalBox> CurveBox = SNew(SVerticalBox);
	if (CurveViewModels.Num() > 1)
	{
		// Only create curve controls if there are more than one.
		for (auto CurveViewModel : CurveViewModels)
		{
			CurveBox->AddSlot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.Padding(0, 0, 5, 0)
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
					.ColorAndOpacity(CurveViewModel->Color)
					.Text(FText::FromName(CurveViewModel->CurveInfo.CurveName))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked(this, &SCurveEditor::IsCurveVisible, CurveViewModel)
					.OnCheckStateChanged(this, &SCurveEditor::OnCurveIsVisibleChanged, CurveViewModel)
					.ToolTipText(this, &SCurveEditor::GetIsCurveVisibleToolTip, CurveViewModel)
					.CheckedImage(FEditorStyle::GetBrush("CurveEd.Visible"))
					.CheckedHoveredImage(FEditorStyle::GetBrush("CurveEd.VisibleHighlight"))
					.CheckedPressedImage(FEditorStyle::GetBrush("CurveEd.Visible"))
					.UncheckedImage(FEditorStyle::GetBrush("CurveEd.Invisible"))
					.UncheckedHoveredImage(FEditorStyle::GetBrush("CurveEd.InvisibleHighlight"))
					.UncheckedPressedImage(FEditorStyle::GetBrush("CurveEd.Invisible"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2, 0, 0, 0)
				[
					SNew(SCheckBox)
					.IsChecked(this, &SCurveEditor::IsCurveLocked, CurveViewModel)
					.OnCheckStateChanged(this, &SCurveEditor::OnCurveIsLockedChanged, CurveViewModel)
					.ToolTipText(this, &SCurveEditor::GetIsCurveLockedToolTip, CurveViewModel)
					.CheckedImage(FEditorStyle::GetBrush("CurveEd.Locked"))
					.CheckedHoveredImage(FEditorStyle::GetBrush("CurveEd.LockedHighlight"))
					.CheckedPressedImage(FEditorStyle::GetBrush("CurveEd.Locked"))
					.UncheckedImage(FEditorStyle::GetBrush("CurveEd.Unlocked"))
					.UncheckedHoveredImage(FEditorStyle::GetBrush("CurveEd.UnlockedHighlight"))
					.UncheckedPressedImage(FEditorStyle::GetBrush("CurveEd.Unlocked"))
					.Visibility(bCanEditTrack ? EVisibility::Visible : EVisibility::Collapsed)
				]
			];
		}
	}

	TSharedRef<SBorder> Border = SNew(SBorder)
		.Padding(FMargin(3, 2, 2, 2))
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f))
		[
			CurveBox
		];

	return Border;
}

void SCurveEditor::PushWarningMenu( FVector2D Position, const FText& Message )
{
	WarningMessageText->SetError(Message);

	FSlateApplication::Get().PushMenu(
		SharedThis( this ),
		WarningMessageText->AsWidget(),
		Position,
		FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu));
}


void SCurveEditor::PushKeyMenu(const FGeometry& InMyGeometry, FVector2D Position)
{
	FMenuBuilder MenuBuilder(true, Commands.ToSharedRef());

	if (SelectedKeys.Num() == 0)
	{
		MenuBuilder.BeginSection("EditCurveEditorActions", LOCTEXT("Actions", "Actions"));
		{
			FUIAction Action = FUIAction(FExecuteAction::CreateSP(this, &SCurveEditor::AddNewKey, InMyGeometry, Position));
			MenuBuilder.AddMenuEntry(
				LOCTEXT("AddKey", "Add Key"),
				LOCTEXT("AddKey_ToolTip", "Create a new point in the curve.  You can also use (Shift + Left Mouse Button)"),
				FSlateIcon(),
				Action);
		}
		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection("CurveEditorInterpolation", LOCTEXT("KeyInterpolationMode", "Key Interpolation"));
	{
		MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().InterpolationCubicAuto);
		MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().InterpolationCubicUser);
		MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().InterpolationCubicBreak);
		MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().InterpolationLinear);
		MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().InterpolationConstant);
	}
	MenuBuilder.EndSection(); //CurveEditorInterpolation

	FSlateApplication::Get().PushMenu(
		SharedThis( this ),
		MenuBuilder.MakeWidget(),
		Position,
		FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu));
}


FVector2D SCurveEditor::ComputeDesiredSize( float ) const
{
	return DesiredSize.Get();
}

EVisibility SCurveEditor::GetCurveAreaVisibility() const
{
	return AreCurvesVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SCurveEditor::GetCurveSelectorVisibility() const
{
	return  (IsHovered() || (false == bHideUI)) && bShowCurveSelector ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SCurveEditor::GetEditVisibility() const
{
	return (SelectedKeys.Num() > 0) && (IsHovered() || (false == bHideUI)) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SCurveEditor::GetColorGradientVisibility() const
{
	return bIsGradientEditorVisible && IsLinearColorCurve() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SCurveEditor::GetZoomButtonVisibility() const
{
	return (IsHovered() || (false == bHideUI)) && bShowZoomButtons ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SCurveEditor::GetInputEditEnabled() const
{
	return (SelectedKeys.Num() == 1);
}

int32 SCurveEditor::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// Rendering info
	bool bEnabled = ShouldBeEnabled( bParentEnabled );
	ESlateDrawEffect::Type DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FSlateBrush* TimelineAreaBrush = FEditorStyle::GetBrush("CurveEd.TimelineArea");
	const FSlateBrush* WhiteBrush = FEditorStyle::GetBrush("WhiteTexture");

	FGeometry CurveAreaGeometry = AllottedGeometry;

	// Positioning info
	FTrackScaleInfo ScaleInfo(ViewMinInput.Get(),  ViewMaxInput.Get(), ViewMinOutput.Get(), ViewMaxOutput.Get(), CurveAreaGeometry.Size);

	// Draw background to indicate valid timeline area
	float ZeroInputX = ScaleInfo.InputToLocalX(0.f);
	float ZeroOutputY = ScaleInfo.OutputToLocalY(0.f);

	// timeline background
	int32 BackgroundLayerId = LayerId;
	float TimelineMaxX = ScaleInfo.InputToLocalX(TimelineLength.Get());
	FSlateDrawElement::MakeBox
		(
		OutDrawElements,
		BackgroundLayerId,
		CurveAreaGeometry.ToPaintGeometry(FVector2D(ZeroInputX, 0), FVector2D(TimelineMaxX - ZeroInputX, CurveAreaGeometry.Size.Y)),
		TimelineAreaBrush,
		MyClippingRect,
		DrawEffects,
		TimelineAreaBrush->GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
		);

	// grid lines.
	int32 GridLineLayerId = BackgroundLayerId + 1;
	PaintGridLines(CurveAreaGeometry, ScaleInfo, OutDrawElements, GridLineLayerId, MyClippingRect, DrawEffects);

	// time=0 line
	int32 ZeroLineLayerId = GridLineLayerId + 1;
	TArray<FVector2D> ZeroLinePoints;
	ZeroLinePoints.Add( FVector2D( ZeroInputX, 0 ) );
	ZeroLinePoints.Add( FVector2D( ZeroInputX, CurveAreaGeometry.Size.Y ) );
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		ZeroLineLayerId,
		AllottedGeometry.ToPaintGeometry(),
		ZeroLinePoints,
		MyClippingRect,
		DrawEffects,
		FLinearColor::White,
		false );

	// value=0 line
	if( AreCurvesVisible() )
	{
		FSlateDrawElement::MakeBox
		(
			OutDrawElements,
			ZeroLineLayerId,
			CurveAreaGeometry.ToPaintGeometry( FVector2D(0, ZeroOutputY), FVector2D(CurveAreaGeometry.Size.X, 1) ),
			WhiteBrush,
			MyClippingRect,
			DrawEffects,
			WhiteBrush->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint()
		);
	}

	int32 LockedCurveLayerID = ZeroLineLayerId + 1;
	int32 CurveLayerId = LockedCurveLayerID + 1;

	int32 KeyLayerId = CurveLayerId + 1;
	int32 SelectedKeyLayerId = KeyLayerId + 1;

	if( AreCurvesVisible() )
	{
		//Paint the curves, unlocked curves will be on top
		for ( auto CurveViewModel : CurveViewModels )
		{
			if (CurveViewModel->bIsVisible)
				{
				PaintCurve(CurveViewModel, CurveAreaGeometry, ScaleInfo, OutDrawElements, CurveViewModel->bIsLocked ? LockedCurveLayerID : CurveLayerId, MyClippingRect, DrawEffects, InWidgetStyle);
				}
			}

		//Paint the keys on top of the curve
		for (auto CurveViewModel : CurveViewModels)
		{
			if (CurveViewModel->bIsVisible)
		{
				PaintKeys(CurveViewModel, ScaleInfo, OutDrawElements, KeyLayerId, SelectedKeyLayerId, CurveAreaGeometry, MyClippingRect, DrawEffects, InWidgetStyle);
			}
		}
	}

	// Paint children
	int32 ChildrenLayerId = SelectedKeyLayerId + 1;
	int32 MarqueeLayerId = SCompoundWidget::OnPaint(Args, CurveAreaGeometry, MyClippingRect, OutDrawElements, ChildrenLayerId, InWidgetStyle, bParentEnabled);

	// Paint marquee
	if (DragState == EDragState::MarqueeSelect)
	{
		PaintMarquee(AllottedGeometry, MyClippingRect, OutDrawElements, MarqueeLayerId);
	}

	return MarqueeLayerId + 1;
}

void SCurveEditor::PaintCurve(TSharedPtr<FCurveViewModel> CurveViewModel, const FGeometry &AllottedGeometry, FTrackScaleInfo &ScaleInfo, FSlateWindowElementList &OutDrawElements,
	int32 LayerId, const FSlateRect& MyClippingRect, ESlateDrawEffect::Type DrawEffects, const FWidgetStyle &InWidgetStyle )const
{
	if (CurveViewModel.IsValid())
	{
		if (bDrawCurve)
		{
			FLinearColor Color = InWidgetStyle.GetColorAndOpacityTint() * CurveViewModel->Color;

			// Fade out curves which are locked.
			if(CurveViewModel->bIsLocked)
			{
				Color *= FLinearColor(1.0f,1.0f,1.0f,0.35f); 
			}

			TArray<FVector2D> LinePoints;
			int32 CurveDrawInterval = 1;

			FRichCurve* Curve = CurveViewModel->CurveInfo.CurveToEdit;
			if (Curve->GetNumKeys() < 2) 
			{
				//Not enough point, just draw flat line
				float Value = Curve->Eval(0.0f);
				float Y = ScaleInfo.OutputToLocalY(Value);
				LinePoints.Add(FVector2D(0.0f, Y));
				LinePoints.Add(FVector2D(AllottedGeometry.Size.X, Y));

				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), LinePoints, MyClippingRect, DrawEffects, Color);
				LinePoints.Empty();
			}
			else
			{
				//Add arrive and exit lines
				{
					const FRichCurveKey& FirstKey = Curve->GetFirstKey();
					const FRichCurveKey& LastKey = Curve->GetLastKey();

					float ArriveX = ScaleInfo.InputToLocalX(FirstKey.Time);
					float ArriveY = ScaleInfo.OutputToLocalY(FirstKey.Value);
					float LeaveY  = ScaleInfo.OutputToLocalY(LastKey.Value);
					float LeaveX  = ScaleInfo.InputToLocalX(LastKey.Time);

					//Arrival line
					LinePoints.Add(FVector2D(0.0f, ArriveY));
					LinePoints.Add(FVector2D(ArriveX, ArriveY));
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), LinePoints, MyClippingRect, DrawEffects, Color);
					LinePoints.Empty();

					//Leave line
					LinePoints.Add(FVector2D(AllottedGeometry.Size.X, LeaveY));
					LinePoints.Add(FVector2D(LeaveX, LeaveY));
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), LinePoints, MyClippingRect, DrawEffects, Color);
					LinePoints.Empty();
				}


				//Add enclosed segments
				TArray<FRichCurveKey> Keys = Curve->GetCopyOfKeys();
				for(int32 i = 0;i<Keys.Num()-1;++i)
				{
					CreateLinesForSegment(Curve, Keys[i], Keys[i+1],LinePoints, ScaleInfo);
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), LinePoints, MyClippingRect, DrawEffects, Color);
					LinePoints.Empty();
				}
			}
		}
	}
}


void SCurveEditor::CreateLinesForSegment( FRichCurve* Curve, const FRichCurveKey& Key1, const FRichCurveKey& Key2, TArray<FVector2D>& Points, FTrackScaleInfo &ScaleInfo ) const
{
	switch(Key1.InterpMode)
	{
	case RCIM_Constant:
		{
			//@todo: should really only need 3 points here but something about the line rendering isn't quite behaving as I'd expect, so need extras
			Points.Add(FVector2D(Key1.Time, Key1.Value));
			Points.Add(FVector2D(Key2.Time, Key1.Value));
			Points.Add(FVector2D(Key2.Time, Key1.Value));
			Points.Add(FVector2D(Key2.Time, Key2.Value));
			Points.Add(FVector2D(Key2.Time, Key1.Value));
		}break;
	case RCIM_Linear:
		{
			Points.Add(FVector2D(Key1.Time, Key1.Value));
			Points.Add(FVector2D(Key2.Time, Key2.Value));
		}break;
	case RCIM_Cubic:
		{
			const float StepSize = 1.0f;
			//clamp to screen to avoid massive slowdown when zoomed in
			float StartX	= FMath::Max(ScaleInfo.InputToLocalX(Key1.Time), 0.0f) ;
			float EndX		= FMath::Min(ScaleInfo.InputToLocalX(Key2.Time),ScaleInfo.WidgetSize.X);
			for(;StartX<EndX; StartX += StepSize)
			{
				float CurveIn = ScaleInfo.LocalXToInput(FMath::Min(StartX,EndX));
				float CurveOut = Curve->Eval(CurveIn);
				Points.Add(FVector2D(CurveIn,CurveOut));
			}
			Points.Add(FVector2D(Key2.Time,Key2.Value));

		}break;
	}

	//Transform to screen
	for(auto It = Points.CreateIterator();It;++It)
	{
		FVector2D Vec2D = *It;
		Vec2D.X = ScaleInfo.InputToLocalX(Vec2D.X);
		Vec2D.Y = ScaleInfo.OutputToLocalY(Vec2D.Y);
		*It = Vec2D;
	}
}

void SCurveEditor::PaintKeys(TSharedPtr<FCurveViewModel> CurveViewModel, FTrackScaleInfo &ScaleInfo, FSlateWindowElementList &OutDrawElements, int32 LayerId, int32 SelectedLayerId, const FGeometry &AllottedGeometry, const FSlateRect& MyClippingRect, ESlateDrawEffect::Type DrawEffects, const FWidgetStyle &InWidgetStyle ) const
{
	FLinearColor KeyColor = CurveViewModel->bIsLocked ? FLinearColor(0.1f,0.1f,0.1f,1.f) : InWidgetStyle.GetColorAndOpacityTint();

	// Iterate over each key
	ERichCurveInterpMode LastInterpMode = RCIM_Linear;
	FRichCurve* Curve = CurveViewModel->CurveInfo.CurveToEdit;
	for (auto It(Curve->GetKeyHandleIterator()); It; ++It)
	{
		FKeyHandle KeyHandle = It.Key();

		// Work out where it is
		FVector2D KeyLocation(
			ScaleInfo.InputToLocalX(Curve->GetKeyTime(KeyHandle)),
			ScaleInfo.OutputToLocalY(Curve->GetKeyValue(KeyHandle)));
		FVector2D KeyIconLocation = KeyLocation - (CONST_KeySize / 2);

		// Get brush
		bool IsSelected = IsKeySelected(FSelectedCurveKey(Curve,KeyHandle));
		const FSlateBrush* KeyBrush = IsSelected ? FEditorStyle::GetBrush("CurveEd.CurveKeySelected") : FEditorStyle::GetBrush("CurveEd.CurveKey");
		int32 LayerToUse = IsSelected ? SelectedLayerId: LayerId;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerToUse,
			AllottedGeometry.ToPaintGeometry( KeyIconLocation, CONST_KeySize ),
			KeyBrush,
			MyClippingRect,
			DrawEffects,
			KeyBrush->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint() * KeyColor
			);

		//Handle drawing the tangent controls for curve
		if(IsSelected && (Curve->GetKeyInterpMode(KeyHandle) == RCIM_Cubic || LastInterpMode == RCIM_Cubic))
		{
			PaintTangent(ScaleInfo, Curve, KeyHandle, KeyLocation, OutDrawElements, LayerId, AllottedGeometry, MyClippingRect, DrawEffects, LayerToUse, InWidgetStyle);
		}

		LastInterpMode = Curve->GetKeyInterpMode(KeyHandle);
	}
}


void SCurveEditor::PaintTangent( FTrackScaleInfo &ScaleInfo, FRichCurve* Curve, FKeyHandle KeyHandle, FVector2D KeyLocation, FSlateWindowElementList &OutDrawElements, int32 LayerId, const FGeometry &AllottedGeometry, const FSlateRect& MyClippingRect, ESlateDrawEffect::Type DrawEffects, int32 LayerToUse, const FWidgetStyle &InWidgetStyle ) const
{
	FVector2D ArriveTangentLocation, LeaveTangentLocation;
	GetTangentPoints(ScaleInfo, FSelectedCurveKey(Curve,KeyHandle), ArriveTangentLocation, LeaveTangentLocation);

	FVector2D ArriveTangentIconLocation = ArriveTangentLocation - (CONST_TangentSize / 2);
	FVector2D LeaveTangentIconLocation = LeaveTangentLocation - (CONST_TangentSize / 2);

	const FSlateBrush* TangentBrush = FEditorStyle::GetBrush("CurveEd.Tangent");
	const FLinearColor TangentColor = FEditorStyle::GetColor("CurveEd.TangentColor");

	//Add lines from tangent control point to 'key'
	TArray<FVector2D> LinePoints;
	LinePoints.Add(KeyLocation);
	LinePoints.Add(ArriveTangentLocation);
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		LinePoints,
		MyClippingRect,
		DrawEffects,
		TangentColor
		);

	LinePoints.Empty();
	LinePoints.Add(KeyLocation);
	LinePoints.Add(LeaveTangentLocation);
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		LinePoints,
		MyClippingRect,
		DrawEffects,
		TangentColor
		);

	//Arrive tangent control
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerToUse,
		AllottedGeometry.ToPaintGeometry(ArriveTangentIconLocation, CONST_TangentSize ),
		TangentBrush,
		MyClippingRect,
		DrawEffects,
		TangentBrush->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint()
		);
	//Leave tangent control
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerToUse,
		AllottedGeometry.ToPaintGeometry(LeaveTangentIconLocation, CONST_TangentSize ),
		TangentBrush,
		MyClippingRect,
		DrawEffects,
		TangentBrush->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint()
		);
}


float SCurveEditor::CalcGridLineStepDistancePow2(double RawValue)
{
	return float(double(FMath::RoundUpToPowerOfTwo(uint32(RawValue*1024.0))>>1)/1024.0);
}

float SCurveEditor::GetTimeStep(FTrackScaleInfo &ScaleInfo) const
{
	const float MaxGridPixelSpacing = 150.0f;

	const float GridPixelSpacing = FMath::Min(ScaleInfo.WidgetSize.GetMin()/1.5f, MaxGridPixelSpacing);

	double MaxTimeStep = ScaleInfo.LocalXToInput(ViewMinInput.Get() + GridPixelSpacing) - ScaleInfo.LocalXToInput(ViewMinInput.Get());

	return CalcGridLineStepDistancePow2(MaxTimeStep);
}

void SCurveEditor::PaintGridLines(const FGeometry &AllottedGeometry, FTrackScaleInfo &ScaleInfo, FSlateWindowElementList &OutDrawElements, 
	int32 LayerId, const FSlateRect& MyClippingRect, ESlateDrawEffect::Type DrawEffects )const
{
	const float MaxGridPixelSpacing = 150.0f;

	const float GridPixelSpacing = FMath::Min(ScaleInfo.WidgetSize.GetMin()/1.5f, MaxGridPixelSpacing);

	const FLinearColor GridTextColor = FLinearColor(1.0f,1.0f,1.0f, 0.75f) ;

	//Vertical grid(time)
	{
		float TimeStep = GetTimeStep(ScaleInfo);
		float ScreenStepTime = ScaleInfo.InputToLocalX(TimeStep) -  ScaleInfo.InputToLocalX(0.0f);

		if(ScreenStepTime >= 1.0f)
		{
			float StartTime = ScaleInfo.LocalXToInput(0.0f);
			TArray<FVector2D> LinePoints;
			float ScaleX = (TimeStep)/(AllottedGeometry.Size.X);

			//draw vertical grid lines
			float StartOffset = -FMath::Fractional(StartTime / TimeStep)*ScreenStepTime;
			float Time =  ScaleInfo.LocalXToInput(StartOffset);
			for(float X = StartOffset;X< AllottedGeometry.Size.X;X+= ScreenStepTime, Time += TimeStep)
			{
				if(SMALL_NUMBER < FMath::Abs(X)) //don't show at 0 to avoid overlapping with center axis 
				{
					LinePoints.Add(FVector2D(X, 0.0));
					LinePoints.Add(FVector2D(X, AllottedGeometry.Size.Y));
					FSlateDrawElement::MakeLines(
						OutDrawElements,
						LayerId,
						AllottedGeometry.ToPaintGeometry(),
						LinePoints,
						MyClippingRect,
						DrawEffects,
						GridColor,
						false);

					//Show grid time
					if (bDrawInputGridNumbers)
					{
						FString TimeStr = FString::Printf(TEXT("%.2f"), Time);
						FSlateDrawElement::MakeText(OutDrawElements,LayerId,AllottedGeometry.MakeChild(FVector2D(X, 0.0), FVector2D(1.0f, ScaleX )).ToPaintGeometry(),TimeStr,
							FEditorStyle::GetFontStyle("CurveEd.InfoFont"), MyClippingRect, DrawEffects, GridTextColor );
					}

					LinePoints.Empty();
				}
			}
		}
	}

	//Horizontal grid(values)
	// This is only useful if the curves are visible
	if( AreCurvesVisible() )
	{
		double MaxValueStep = ScaleInfo.LocalYToOutput(0) - ScaleInfo.LocalYToOutput(GridPixelSpacing) ;
		float ValueStep = CalcGridLineStepDistancePow2(MaxValueStep);
		float ScreenStepValue = ScaleInfo.OutputToLocalY(0.0f) - ScaleInfo.OutputToLocalY(ValueStep);  
		if(ScreenStepValue >= 1.0f)
		{
			float StartValue = ScaleInfo.LocalYToOutput(0.0f);
			TArray<FVector2D> LinePoints;

			float StartOffset = FMath::Fractional(StartValue / ValueStep)*ScreenStepValue;
			float Value = ScaleInfo.LocalYToOutput(StartOffset);
			float ScaleY = (ValueStep)/(AllottedGeometry.Size.Y);

			for(float Y = StartOffset;Y< AllottedGeometry.Size.Y;Y+= ScreenStepValue, Value-=ValueStep)
			{
				if(SMALL_NUMBER < FMath::Abs(Y)) //don't show at 0 to avoid overlapping with center axis 
				{
					LinePoints.Add(FVector2D(0.0f, Y));
					LinePoints.Add(FVector2D(AllottedGeometry.Size.X,Y));
					FSlateDrawElement::MakeLines(
						OutDrawElements,
						LayerId,
						AllottedGeometry.ToPaintGeometry(),
						LinePoints,
						MyClippingRect,
						DrawEffects,
						GridColor,
						false);

					//Show grid value
					if (bDrawOutputGridNumbers)
					{
						FString ValueStr = FString::Printf(TEXT("%.2f"), Value);
						FSlateFontInfo Font = FEditorStyle::GetFontStyle("CurveEd.InfoFont");

						const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
						FVector2D DrawSize = FontMeasureService->Measure(ValueStr, Font);

						// draw at the start
						FSlateDrawElement::MakeText(OutDrawElements,LayerId,AllottedGeometry.MakeChild(FVector2D(0.0f, Y), FVector2D(ScaleY, 1.0f )).ToPaintGeometry(),ValueStr,
												 Font, MyClippingRect, DrawEffects, GridTextColor );

						// draw at the last since sometimes start can be hidden
						FSlateDrawElement::MakeText(OutDrawElements,LayerId,AllottedGeometry.MakeChild(FVector2D(AllottedGeometry.Size.X-DrawSize.X, Y), FVector2D(ScaleY, 1.0f )).ToPaintGeometry(),ValueStr,
												 Font, MyClippingRect, DrawEffects, GridTextColor );
					}
					
					LinePoints.Empty();
				}
			}
		}
	}
}

void SCurveEditor::PaintMarquee(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	FVector2D MarqueTopLeft(
		FMath::Min(MouseDownLocation.X, MouseMoveLocation.X),
		FMath::Min(MouseDownLocation.Y, MouseMoveLocation.Y)
		);

	FVector2D MarqueBottomRight(
		FMath::Max(MouseDownLocation.X, MouseMoveLocation.X),
		FMath::Max(MouseDownLocation.Y, MouseMoveLocation.Y)
		);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(MarqueTopLeft, MarqueBottomRight - MarqueTopLeft),
		FEditorStyle::GetBrush(TEXT("MarqueeSelection")),
		MyClippingRect
		);
}

void SCurveEditor::SetCurveOwner(FCurveOwnerInterface* InCurveOwner, bool bCanEdit)
{
	if(InCurveOwner != CurveOwner)
	{
		EmptySelection();
	}

	GradientViewer->SetCurveOwner(InCurveOwner);

	CurveOwner = InCurveOwner;
	bCanEditTrack = bCanEdit;


	bAreCurvesVisible = !IsLinearColorCurve();
	bIsGradientEditorVisible = IsLinearColorCurve();

	CurveViewModels.Empty();
	if(CurveOwner != NULL)
	{
		int curveIndex = 0;
		for (auto CurveInfo : CurveOwner->GetCurves())
		{
			CurveViewModels.Add(TSharedPtr<FCurveViewModel>(new FCurveViewModel(CurveInfo, CurveColors[curveIndex % CurveColors.Num()], !bCanEdit)));
			curveIndex++;
		}
		CurveOwner->MakeTransactional();
	}

	SelectedKeys.Empty();

	if( bZoomToFitVertical )
	{
		ZoomToFitVertical(false);
	}

	if ( bZoomToFitHorizontal )
	{
		ZoomToFitHorizontal(false);
	}

	CurveSelectionWidget.Pin()->SetContent(CreateCurveSelectionWidget());
}

void SCurveEditor::SetZoomToFit(bool bNewZoomToFitVertical, bool bNewZoomToFitHorizontal)
{
	bZoomToFitVertical = bNewZoomToFitVertical;
	bZoomToFitHorizontal = bNewZoomToFitHorizontal;
}

FCurveOwnerInterface* SCurveEditor::GetCurveOwner() const
{
	return CurveOwner;
}

FRichCurve* SCurveEditor::GetCurve(int32 CurveIndex) const
{
	if(CurveIndex < CurveViewModels.Num())
	{
		return CurveViewModels[CurveIndex]->CurveInfo.CurveToEdit;
	}
	return NULL;
}

void SCurveEditor::DeleteSelectedKeys()
{
	const FScopedTransaction Transaction(LOCTEXT("CurveEditor_RemoveKeys", "Delete Key(s)"));
	CurveOwner->ModifyOwner();
	TSet<FRichCurve*> ChangedCurves;

	// While there are still keys
	while(SelectedKeys.Num() > 0)
	{
		// Pull one out of the selected set
		FSelectedCurveKey Key = SelectedKeys.Pop();
		if(IsValidCurve(Key.Curve))
		{
			// Remove from the curve
			Key.Curve->DeleteKey(Key.KeyHandle);
			ChangedCurves.Add(Key.Curve);
		}
	}

	TArray<FRichCurveEditInfo> ChangedCurveEditInfos;
	for (auto CurveViewModel : CurveViewModels)
	{
		if (ChangedCurves.Contains(CurveViewModel->CurveInfo.CurveToEdit))
		{
			ChangedCurveEditInfos.Add(CurveViewModel->CurveInfo);
		}
	}

	CurveOwner->OnCurveChanged(ChangedCurveEditInfos);
}

FReply SCurveEditor::OnMouseButtonDown( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bLeftMouseButton = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	const bool bRightMouseButton = InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton;

	DragState = EDragState::PreDrag;
	if (bLeftMouseButton || bRightMouseButton)
	{
		MouseDownLocation = InMyGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

		// Set keyboard focus to this so that selected text box doesn't try to apply to newly selected keys
		if(!HasKeyboardFocus())
		{
			FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
		}

		// Always capture mouse if we left or right click on the widget
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}

void SCurveEditor::AddNewKey(FGeometry InMyGeometry, FVector2D ScreenPosition)
{
	const FScopedTransaction Transaction(LOCTEXT("CurveEditor_AddKey", "Add Key(s)"));
	CurveOwner->ModifyOwner();
	TArray<FRichCurveEditInfo> ChangedCurveEditInfos;
	for (auto CurveViewModel : CurveViewModels)
	{
		if (!CurveViewModel->bIsLocked)
		{
			FRichCurve* SelectedCurve = CurveViewModel->CurveInfo.CurveToEdit;
			if (IsValidCurve(SelectedCurve))
			{
				FTrackScaleInfo ScaleInfo(ViewMinInput.Get(), ViewMaxInput.Get(), ViewMinOutput.Get(), ViewMaxOutput.Get(), InMyGeometry.Size);

				FVector2D LocalClickPos = InMyGeometry.AbsoluteToLocal(ScreenPosition);

				FVector2D NewKeyLocation = SnapLocation(FVector2D(
					ScaleInfo.LocalXToInput(LocalClickPos.X),
					ScaleInfo.LocalYToOutput(LocalClickPos.Y)));
				FKeyHandle NewKeyHandle = SelectedCurve->AddKey(NewKeyLocation.X, NewKeyLocation.Y);

				EmptySelection();
				AddToSelection(FSelectedCurveKey(SelectedCurve, NewKeyHandle));
				ChangedCurveEditInfos.Add(CurveViewModel->CurveInfo);
			}
		}
	}

	if (ChangedCurveEditInfos.Num() > 0)
	{
		CurveOwner->OnCurveChanged(ChangedCurveEditInfos);
	}
}

void SCurveEditor::OnMouseCaptureLost()
{
	// if we began a drag transaction we need to finish it to make sure undo doesn't get out of sync
	if (DragState == EDragState::DragKey || DragState == EDragState::DragTangent)
	{
		EndDragTransaction();
	}
	DragState = EDragState::None;
}

FReply SCurveEditor::OnMouseButtonUp( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	if (this->HasMouseCapture())
	{
		if (DragState == EDragState::PreDrag)
		{
			// If the user didn't start dragging, handle the mouse operation as a click.
			ProcessClick(InMyGeometry, InMouseEvent);
		}
		else
		{
			EndDrag(InMyGeometry, InMouseEvent);
		}
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

void ClampViewRangeToDataIfBound( float& NewViewMin, float& NewViewMax, const TAttribute< TOptional<float> > & DataMin, const TAttribute< TOptional<float> > & DataMax, const float ViewRange)
{
	// if we have data bound
	const TOptional<float> & Min = DataMin.Get();
	const TOptional<float> & Max = DataMax.Get();
	if ( Min.IsSet() && NewViewMin < Min.GetValue())
	{
		// if we have min data set
		NewViewMin = Min.GetValue();
		NewViewMax = ViewRange;
	}
	else if ( Max.IsSet() && NewViewMax > Max.GetValue() )
	{
		// if we have min data set
		NewViewMin = Max.GetValue() - ViewRange;
		NewViewMax = Max.GetValue();
	}
}

FReply SCurveEditor::OnMouseMove( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	UpdateCurveToolTip(InMyGeometry, InMouseEvent);

	FRichCurve* Curve = GetCurve(0);
	if( Curve != NULL && this->HasMouseCapture())
	{
		if (DragState == EDragState::PreDrag)
		{
			TryStartDrag(InMyGeometry, InMouseEvent);
		}
		if (DragState != EDragState::None)
		{
			ProcessDrag(InMyGeometry, InMouseEvent);
		}
		MouseMoveLocation = InMyGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SCurveEditor::UpdateCurveToolTip(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (bShowCurveToolTips.Get())
	{
		TSharedPtr<FCurveViewModel> HoveredCurve = HitTestCurves(InMyGeometry, InMouseEvent);
		if (HoveredCurve.IsValid())
		{
			FTrackScaleInfo ScaleInfo(ViewMinInput.Get(), ViewMaxInput.Get(), ViewMinOutput.Get(), ViewMaxOutput.Get(), InMyGeometry.Size);
			const FVector2D HitPosition = InMyGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
			float Time = ScaleInfo.LocalXToInput(HitPosition.X);
			float Value = HoveredCurve->CurveInfo.CurveToEdit->Eval(Time);

			FNumberFormattingOptions FormattingOptions;
			FormattingOptions.MaximumFractionalDigits = 2;
			CurveToolTipNameText = FText::FromName(HoveredCurve->CurveInfo.CurveName);
			CurveToolTipInputText = FText::Format(LOCTEXT("CurveToolTipTimeFormat", "{0}: {1}"), InputAxisName, FText::AsNumber(Time, &FormattingOptions));
			CurveToolTipOutputText = FText::Format(LOCTEXT("CurveToolTipValueFormat", "{0}: {1}"), OutputAxisName, FText::AsNumber(Value, &FormattingOptions));

			if (CurveToolTip.IsValid() == false)
			{
				SetToolTip(
					SAssignNew(CurveToolTip, SToolTip)
					.BorderImage( FCoreStyle::Get().GetBrush( "ToolTip.BrightBackground" ) )
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						[
							SNew(STextBlock)
							.Text(this, &SCurveEditor::GetCurveToolTipNameText)
							.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
							.ColorAndOpacity( FLinearColor::Black)
						]
						+ SVerticalBox::Slot()
						[
							SNew(STextBlock)
							.Text(this, &SCurveEditor::GetCurveToolTipInputText)
							.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
							.ColorAndOpacity(FLinearColor::Black)
						]
						+ SVerticalBox::Slot()
						[
							SNew(STextBlock)
							.Text(this, &SCurveEditor::GetCurveToolTipOutputText)
							.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
							.ColorAndOpacity(FLinearColor::Black)
						]
					]);
			}
		}
		else
		{
			CurveToolTip.Reset();
			SetToolTip(CurveToolTip);
		}
	}
}

FReply SCurveEditor::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const float ZoomDelta = -0.1f * MouseEvent.GetWheelDelta();

	if (bAllowZoomOutput)
	{
		const float OutputViewSize = ViewMaxOutput.Get() - ViewMinOutput.Get();
		const float OutputChange = OutputViewSize * ZoomDelta;

		const float NewMinOutput = (ViewMinOutput.Get() - (OutputChange * 0.5f));
		const float NewMaxOutput = (ViewMaxOutput.Get() + (OutputChange * 0.5f));

		SetOutputMinMax(NewMinOutput, NewMaxOutput);
	}

	{
		const float InputViewSize = ViewMaxInput.Get() - ViewMinInput.Get();
		const float InputChange = InputViewSize * ZoomDelta;

		const float NewMinInput = ViewMinInput.Get() - (InputChange * 0.5f);
		const float NewMaxInput = ViewMaxInput.Get() + (InputChange * 0.5f);

		SetInputMinMax(NewMinInput, NewMaxInput);
	}

	return FReply::Handled();
}

FReply SCurveEditor::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if (InKeyEvent.GetKey() == EKeys::Platform_Delete && SelectedKeys.Num() != 0)
	{
		DeleteSelectedKeys();
		return FReply::Handled();
	}
	else
	{
		if( Commands->ProcessCommandBindings( InKeyEvent ) )
		{
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}
}

void SCurveEditor::TryStartDrag(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bLeftMouseButton = InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);
	const bool bRightMouseButton = InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton);
	const bool bControlDown = InMouseEvent.IsControlDown();

	FVector2D DragVector = InMyGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()) - MouseDownLocation;
	if (DragVector.Size() >= DragThreshold)
	{
		if (bLeftMouseButton)
		{
			// Check if we should start dragging keys.
			FSelectedCurveKey HitKey = HitTestKeys(InMyGeometry, InMyGeometry.LocalToAbsolute(MouseDownLocation));
			if (HitKey.IsValid())
			{
				if (IsKeySelected(HitKey) == false)
				{
					if (!bControlDown)
					{
						EmptySelection();
					}
					AddToSelection(HitKey);
				}

				BeginDragTransaction();
				DragState = EDragState::DragKey;
				DraggedKeyHandle = HitKey.KeyHandle;
				PreDragKeyLocations.Empty();
				for (auto selectedKey : SelectedKeys)
				{
					PreDragKeyLocations.Add(selectedKey.KeyHandle, FVector2D
					(
						selectedKey.Curve->GetKeyTime(selectedKey.KeyHandle),
						selectedKey.Curve->GetKeyValue(selectedKey.KeyHandle)
					));
				}
			}
			else
			{
				// Check if we should start dragging a tangent.
				FSelectedTangent Tangent = HitTestCubicTangents(InMyGeometry, InMyGeometry.LocalToAbsolute(MouseDownLocation));
				if (Tangent.IsValid())
				{
					BeginDragTransaction();
					SelectedTangent = Tangent;
					DragState = EDragState::DragTangent;
				}
				else
				{
					// Otherwise if the user left clicked on nothing and start a marquee select.
					DragState = EDragState::MarqueeSelect;
				}
			}
		}
		else if (bRightMouseButton)
		{
			DragState = EDragState::Pan;
		}
		else
		{
			DragState = EDragState::None;
		}
	}
}

void SCurveEditor::ProcessDrag(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	FTrackScaleInfo ScaleInfo(ViewMinInput.Get(), ViewMaxInput.Get(), ViewMinOutput.Get(), ViewMaxOutput.Get(), InMyGeometry.Size);
	FVector2D ScreenDelta = InMouseEvent.GetCursorDelta();

	FVector2D InputDelta;
	InputDelta.X = ScreenDelta.X / ScaleInfo.PixelsPerInput;
	InputDelta.Y = -ScreenDelta.Y / ScaleInfo.PixelsPerOutput;

	if (DragState == EDragState::DragKey)
	{
		FVector2D MousePosition = InMyGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		MoveSelectedKeys(FVector2D(ScaleInfo.LocalXToInput(MousePosition.X), ScaleInfo.LocalYToOutput(MousePosition.Y)));
	}
	else if (DragState == EDragState::DragTangent)
	{
		FVector2D MousePositionScreen = InMouseEvent.GetScreenSpacePosition();
		MousePositionScreen -= InMyGeometry.AbsolutePosition;
		FVector2D MousePositionCurve(ScaleInfo.LocalXToInput(MousePositionScreen.X), ScaleInfo.LocalYToOutput(MousePositionScreen.Y));
		OnMoveTangent(MousePositionCurve);
	}
	else if (DragState == EDragState::Pan)
	{
		// Output is not clamped.
		const float NewMinOutput = (ViewMinOutput.Get() - InputDelta.Y);
		const float NewMaxOutput = (ViewMaxOutput.Get() - InputDelta.Y);

		SetOutputMinMax(NewMinOutput, NewMaxOutput);

		// Input maybe clamped if DataMinInput or DataMaxOutput was set.
		float NewMinInput = ViewMinInput.Get() - InputDelta.X;
		float NewMaxInput = ViewMaxInput.Get() - InputDelta.X;
		ClampViewRangeToDataIfBound(NewMinInput, NewMaxInput, DataMinInput, DataMaxInput, ScaleInfo.ViewInputRange);

		SetInputMinMax(NewMinInput, NewMaxInput);
	}
}

void SCurveEditor::EndDrag(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bControlDown = InMouseEvent.IsControlDown();

	if (DragState == EDragState::DragKey || DragState == EDragState::DragTangent)
	{
			EndDragTransaction();
		}
	else if (DragState == EDragState::MarqueeSelect)
	{
		FVector2D MarqueTopLeft
		(
			FMath::Min(MouseDownLocation.X, MouseMoveLocation.X),
			FMath::Min(MouseDownLocation.Y, MouseMoveLocation.Y)
			);

		FVector2D MarqueBottomRight
		(
			FMath::Max(MouseDownLocation.X, MouseMoveLocation.X),
			FMath::Max(MouseDownLocation.Y, MouseMoveLocation.Y)
			);

		TArray<FSelectedCurveKey> SelectedCurveKeys = GetEditableKeysWithinMarquee(InMyGeometry, MarqueTopLeft, MarqueBottomRight);

		if (!bControlDown)
		{
			EmptySelection();
		}

		for (auto SelectedCurveKey : SelectedCurveKeys)
		{
			
			if (IsKeySelected(SelectedCurveKey))
			{
				RemoveFromSelection(SelectedCurveKey);
			}
			else
			{
				AddToSelection(SelectedCurveKey);
			}
		}
	}
	DragState = EDragState::None;
}

void SCurveEditor::ProcessClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bLeftMouseButton = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	const bool bRightMouseButton = InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
	const bool bControlDown = InMouseEvent.IsControlDown();
	const bool bShiftDown = InMouseEvent.IsShiftDown();

	FSelectedCurveKey HitKey = HitTestKeys(InMyGeometry, InMouseEvent.GetScreenSpacePosition());
		if (bLeftMouseButton)
		{
			// If the user left clicked a key, update selection based on modifier key state.
			if (HitKey.IsValid())
			{
				if (!IsKeySelected(HitKey))
				{
					if (!bControlDown)
					{
						EmptySelection();
					}
					AddToSelection(HitKey);
				}
				else if (bControlDown)
				{
					RemoveFromSelection(HitKey);
				}
			}
			else
			{
				// If the user didn't click a key, add a new one if shift is held down, or try to select a curve.
				if (bShiftDown)
				{
					AddNewKey(InMyGeometry, InMouseEvent.GetScreenSpacePosition());
				}
				else
				{
					// clicking on background clears key selection
					EmptySelection();
				}
			}
		}
		else if (bRightMouseButton)
		{
			// If the user right clicked, handle opening context menus.
			if (HitKey.IsValid())
			{
				// Make sure key is selected in readiness for context menu
				if (!IsKeySelected(HitKey))
				{
					EmptySelection();
					AddToSelection(HitKey);
				}
			PushKeyMenu(InMyGeometry, InMouseEvent.GetScreenSpacePosition());
			}
			else
			{
				FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();
				if (!HitTestCubicTangents(InMyGeometry, ScreenPos).IsValid())
				{
					CreateContextMenu(InMyGeometry, ScreenPos);
				}
			else
			{
				EmptySelection();
			}
		}
	}
}

TOptional<float> SCurveEditor::OnGetTime() const
{
	if ( SelectedKeys.Num() == 1 )
	{
		return GetKeyTime(SelectedKeys[0]);
	}

	// Value couldn't be accessed.  Return an unset value
	return TOptional<float>();
}

void SCurveEditor::OnTimeComitted(float NewTime, ETextCommit::Type CommitType)
{
	// Don't digest the number if we just clicked away from the pop-up
	if ( !bIsUsingSlider && ((CommitType == ETextCommit::OnEnter) || ( CommitType == ETextCommit::OnUserMovedFocus )) )
	{
		if ( SelectedKeys.Num() >= 1 )
		{
			auto Key = SelectedKeys[0];
			if ( IsValidCurve(Key.Curve) )
			{
				const FScopedTransaction Transaction(LOCTEXT("CurveEditor_NewTime", "New Time Entered"));
				CurveOwner->ModifyOwner();
				Key.Curve->SetKeyTime(Key.KeyHandle, NewTime);
				TArray<FRichCurveEditInfo> ChangedCurveEditInfos;
				ChangedCurveEditInfos.Add(GetViewModelForCurve(Key.Curve)->CurveInfo);
				CurveOwner->OnCurveChanged(ChangedCurveEditInfos);
			}
		}

		FSlateApplication::Get().DismissAllMenus();
	}
}

void SCurveEditor::OnTimeChanged(float NewTime)
{
	if ( bIsUsingSlider )
	{
		if ( SelectedKeys.Num() >= 1 )
		{
			auto Key = SelectedKeys[0];
			if ( IsValidCurve(Key.Curve) )
			{
				const FScopedTransaction Transaction( LOCTEXT( "CurveEditor_NewTime", "New Time Entered" ) );
				CurveOwner->ModifyOwner();
				Key.Curve->SetKeyTime(Key.KeyHandle, NewTime);
				TArray<FRichCurveEditInfo> ChangedCurveEditInfos;
				ChangedCurveEditInfos.Add(GetViewModelForCurve(Key.Curve)->CurveInfo);
				CurveOwner->OnCurveChanged(ChangedCurveEditInfos);
			}
		}
	}
}

TOptional<float> SCurveEditor::OnGetValue() const
{
	TOptional<float> Value;

	// Return the value string if all selected keys have the same output string, otherwise empty
	if ( SelectedKeys.Num() > 0 )
	{
		Value = GetKeyValue(SelectedKeys[0]);
		for ( int32 i=1; i < SelectedKeys.Num(); i++ )
		{
			TOptional<float> NewValue = GetKeyValue(SelectedKeys[i]);
			bool bAreEqual = ( ( !Value.IsSet() && !NewValue.IsSet() ) || ( Value.IsSet() && NewValue.IsSet() && Value.GetValue() == NewValue.GetValue() ) );
			if ( !bAreEqual )
			{
				return TOptional<float>();
			}
		}
	}

	return Value;
}

void SCurveEditor::OnValueComitted(float NewValue, ETextCommit::Type CommitType)
{
	// Don't digest the number if we just clicked away from the popup
	if ( !bIsUsingSlider && ((CommitType == ETextCommit::OnEnter) || ( CommitType == ETextCommit::OnUserMovedFocus )) )
	{
		const FScopedTransaction Transaction( LOCTEXT( "CurveEditor_NewValue", "New Value Entered" ) );
		CurveOwner->ModifyOwner();
		TSet<FRichCurve*> ChangedCurves;

		// Iterate over selected set
		for ( int32 i=0; i < SelectedKeys.Num(); i++ )
		{
			auto Key = SelectedKeys[i];
			if ( IsValidCurve(Key.Curve) )
			{
				// Fill in each element of this key
				Key.Curve->SetKeyValue(Key.KeyHandle, NewValue);
				ChangedCurves.Add(Key.Curve);
			}
		}

		TArray<FRichCurveEditInfo> ChangedCurveEditInfos;
		for (auto CurveViewModel : CurveViewModels)
		{
			if (ChangedCurves.Contains(CurveViewModel->CurveInfo.CurveToEdit))
			{
				ChangedCurveEditInfos.Add(CurveViewModel->CurveInfo);
			}
		}
		CurveOwner->OnCurveChanged(ChangedCurveEditInfos);

		FSlateApplication::Get().DismissAllMenus();
	}
}

void SCurveEditor::OnValueChanged(float NewValue)
{
	if ( bIsUsingSlider )
	{
		const FScopedTransaction Transaction(LOCTEXT("CurveEditor_NewValue", "New Value Entered"));
		TSet<FRichCurve*> ChangedCurves;

		// Iterate over selected set
		for ( int32 i=0; i < SelectedKeys.Num(); i++ )
		{
			auto Key = SelectedKeys[i];
			if ( IsValidCurve(Key.Curve) )
			{
				CurveOwner->ModifyOwner();

				// Fill in each element of this key
				Key.Curve->SetKeyValue(Key.KeyHandle, NewValue);
				ChangedCurves.Add(Key.Curve);
			}
		}

		TArray<FRichCurveEditInfo> ChangedCurveEditInfos;
		for (auto CurveViewModel : CurveViewModels)
		{
			if (ChangedCurves.Contains(CurveViewModel->CurveInfo.CurveToEdit))
			{
				ChangedCurveEditInfos.Add(CurveViewModel->CurveInfo);
			}
		}
		CurveOwner->OnCurveChanged(ChangedCurveEditInfos);
	}
}

void SCurveEditor::OnBeginSliderMovement(FText TransactionName)
{
	bIsUsingSlider = true;

	GEditor->BeginTransaction(TransactionName);
}

void SCurveEditor::OnEndSliderMovement(float NewValue)
{
	bIsUsingSlider = false;

	GEditor->EndTransaction();
}


SCurveEditor::FSelectedCurveKey SCurveEditor::HitTestKeys(const FGeometry& InMyGeometry, const FVector2D& HitScreenPosition)
{	
	FSelectedCurveKey SelectedKey(NULL,FKeyHandle());

	if( AreCurvesVisible() )
	{
		FTrackScaleInfo ScaleInfo(ViewMinInput.Get(),  ViewMaxInput.Get(), ViewMinOutput.Get(), ViewMaxOutput.Get(), InMyGeometry.Size);

		const FVector2D HitPosition = InMyGeometry.AbsoluteToLocal( HitScreenPosition );


		for(auto CurveViewModel : CurveViewModels)
		{
			if (!CurveViewModel->bIsLocked && CurveViewModel->bIsVisible)
		{
				FRichCurve* Curve = CurveViewModel->CurveInfo.CurveToEdit;
			if(Curve != NULL)
			{
				for (auto It(Curve->GetKeyHandleIterator()); It; ++It)
				{
					float KeyScreenX = ScaleInfo.InputToLocalX(Curve->GetKeyTime(It.Key()));
					float KeyScreenY = ScaleInfo.OutputToLocalY(Curve->GetKeyValue(It.Key()));

					if(	HitPosition.X > (KeyScreenX - (0.5f * CONST_KeySize.X)) && 
						HitPosition.X < (KeyScreenX + (0.5f * CONST_KeySize.X)) &&
						HitPosition.Y > (KeyScreenY - (0.5f * CONST_KeySize.Y)) &&
						HitPosition.Y < (KeyScreenY + (0.5f * CONST_KeySize.Y)) )
					{
							return  FSelectedCurveKey(Curve, It.Key());
						}
					}
				}
			}
		}
	}

	return SelectedKey;
}

void SCurveEditor::MoveSelectedKeys(FVector2D InNewLocation)
{
	const FScopedTransaction Transaction( LOCTEXT("CurveEditor_MoveKeys", "Move Keys") );
	CurveOwner->ModifyOwner();

	// track all unique curves encountered so their tangents can be updated later
	TSet<FRichCurve*> UniqueCurves;

	FVector2D SnappedNewLocation = SnapLocation(InNewLocation);

	// The total move distance for all keys is the difference between the current snapped location
	// and the start location of the key which was actually dragged.
	FVector2D TotalMoveDistance = SnappedNewLocation - PreDragKeyLocations[DraggedKeyHandle];

	for (int32 i = 0; i < SelectedKeys.Num(); i++)
	{
		FSelectedCurveKey OldKey = SelectedKeys[i];

		if (!IsValidCurve(OldKey.Curve))
		{
			continue;
		}

		FKeyHandle OldKeyHandle = OldKey.KeyHandle;
		FRichCurve* Curve = OldKey.Curve;

		FVector2D PreDragLocation = PreDragKeyLocations[OldKeyHandle];
		FVector2D NewLocation = PreDragLocation + TotalMoveDistance;

		// Update the key's value without updating the tangents.
		Curve->SetKeyValue(OldKeyHandle, NewLocation.Y, false);

		// Changing the time of a key returns a new handle, so make sure to update existing references.
		FKeyHandle KeyHandle = Curve->SetKeyTime(OldKeyHandle, NewLocation.X);
		SelectedKeys[i] = FSelectedCurveKey(Curve, KeyHandle);
		PreDragKeyLocations.Remove(OldKeyHandle);
		PreDragKeyLocations.Add(KeyHandle, PreDragLocation);

		UniqueCurves.Add(Curve);
	}

	// update auto tangents for all curves encountered, once each only
	for(TSet<FRichCurve*>::TIterator SetIt(UniqueCurves);SetIt;++SetIt)
	{
		(*SetIt)->AutoSetTangents();
	}
}

TOptional<float> SCurveEditor::GetKeyValue(FSelectedCurveKey Key) const
{
	if(IsValidCurve(Key.Curve))
	{
		return Key.Curve->GetKeyValue(Key.KeyHandle);
	}

	return TOptional<float>();
}

TOptional<float> SCurveEditor::GetKeyTime(FSelectedCurveKey Key) const
{
	if ( IsValidCurve(Key.Curve) )
	{
		return Key.Curve->GetKeyTime(Key.KeyHandle);
	}

	return TOptional<float>();
}

void SCurveEditor::EmptySelection()
{
	SelectedKeys.Empty();
}

void SCurveEditor::AddToSelection(FSelectedCurveKey Key)
{
	SelectedKeys.AddUnique(Key);
}

void SCurveEditor::RemoveFromSelection(FSelectedCurveKey Key)
{
	SelectedKeys.Remove(Key);
}

bool SCurveEditor::IsKeySelected(FSelectedCurveKey Key) const
{
	return SelectedKeys.Contains(Key);
}

bool SCurveEditor::AreKeysSelected() const
{
	return SelectedKeys.Num() > 0;
}


TArray<FRichCurve*> SCurveEditor::GetCurvesToFit() const
{
	TArray<FRichCurve*> FitCurves;

	for(auto CurveViewModel : CurveViewModels)
	{
		if (CurveViewModel->bIsVisible)
		{
			FitCurves.Add(CurveViewModel->CurveInfo.CurveToEdit);
		}
	}

	return FitCurves;
}


void SCurveEditor::ZoomToFitHorizontal(bool OnlySelected)
{
	TArray<FRichCurve*> CurvesToFit = GetCurvesToFit();

	if(CurveViewModels.Num() > 0)
	{
		float InMin = FLT_MAX;
		float InMax = -FLT_MAX;
		int32 TotalKeys = 0;

		if (OnlySelected)
		{
			for (auto SelectedKey : SelectedKeys)
			{
				TotalKeys++;
				float KeyTime = SelectedKey.Curve->GetKeyTime(SelectedKey.KeyHandle);
				InMin = FMath::Min(KeyTime, InMin);
				InMax = FMath::Max(KeyTime, InMax);
			}
		}
		else
		{
			for(auto It = CurvesToFit.CreateConstIterator();It;++It)
			{
				FRichCurve* Curve = *It;
				float MinTime, MaxTime;
				Curve->GetTimeRange(MinTime, MaxTime);
				InMin = FMath::Min(MinTime, InMin);
				InMax = FMath::Max(MaxTime, InMax);
				TotalKeys += Curve->GetNumKeys();
			}
		}

		if (TotalKeys > 0)
		{
			// Clamp the minimum size
			float Size = InMax - InMin;
			if (Size < CONST_MinViewRange)
			{
				InMin -= (0.5f*CONST_MinViewRange);
				InMax += (0.5f*CONST_MinViewRange);
				Size = InMax - InMin;
			}

			// add margin
			InMin -= CONST_FitMargin*Size;
			InMax += CONST_FitMargin*Size;
		}
		else
		{
			InMin = -CONST_FitMargin*2.0f;
			InMax = (CONST_DefaultZoomRange + CONST_FitMargin)*2.0;
		}

		SetInputMinMax(InMin, InMax);
	}
}

FReply SCurveEditor::ZoomToFitHorizontalClicked()
{
	ZoomToFitHorizontal(false);
	return FReply::Handled();
}

/** Set Default output values when range is too small **/
void SCurveEditor::SetDefaultOutput(const float MinZoomRange)
{
	const float NewMinOutput = (ViewMinOutput.Get() - (0.5f*MinZoomRange));
	const float NewMaxOutput = (ViewMaxOutput.Get() + (0.5f*MinZoomRange));

	SetOutputMinMax(NewMinOutput, NewMaxOutput);
}

void SCurveEditor::ZoomToFitVertical(bool OnlySelected)
{
	TArray<FRichCurve*> CurvesToFit = GetCurvesToFit();

	if(CurvesToFit.Num() > 0)
	{
		float InMin = FLT_MAX;
		float InMax = -FLT_MAX;
		int32 TotalKeys = 0;

		if (OnlySelected)
		{
			for (auto SelectedKey : SelectedKeys)
			{
				TotalKeys++;
				float KeyValue = SelectedKey.Curve->GetKeyValue(SelectedKey.KeyHandle);
				InMin = FMath::Min(KeyValue, InMin);
				InMax = FMath::Max(KeyValue, InMax);
			}
		}
		else
		{
			for(auto It = CurvesToFit.CreateConstIterator();It;++It)
			{
				FRichCurve* Curve = *It;
				float MinVal, MaxVal;
				Curve->GetValueRange(MinVal, MaxVal);
				InMin = FMath::Min(MinVal, InMin);
				InMax = FMath::Max(MaxVal, InMax);
				TotalKeys += Curve->GetNumKeys();
			}
		}

		const float MinZoomRange = (TotalKeys > 0 ) ? CONST_MinViewRange: CONST_DefaultZoomRange;

		// Clamp the minimum size
		float Size = InMax - InMin;
		if( Size < MinZoomRange )
		{
			SetDefaultOutput(MinZoomRange);
			InMin = ViewMinOutput.Get();
			InMax = ViewMaxOutput.Get();
			Size = InMax - InMin;
		}

		// add margin
		const float NewMinOutput = (InMin - CONST_FitMargin*Size);
		const float NewMaxOutput = (InMax + CONST_FitMargin*Size);

		SetOutputMinMax(NewMinOutput, NewMaxOutput);
	}
}

FReply SCurveEditor::ZoomToFitVerticalClicked()
{
	ZoomToFitVertical(false);
	return FReply::Handled();
}

void SCurveEditor::ZoomToFitAll(bool OnlySelected)
{
	if (OnlySelected && SelectedKeys.Num() == 0)
	{
		// Don't zoom to selected if there is no selection.
		return;
	}
	ZoomToFitHorizontal(OnlySelected);
	ZoomToFitVertical(OnlySelected);
}

void SCurveEditor::ToggleSnapping()
{
	if (bSnappingEnabled.IsBound() == false)
	{
		bSnappingEnabled = !bSnappingEnabled.Get();
	}
}

bool SCurveEditor::IsSnappingEnabled()
{
	return bSnappingEnabled.Get();
}

void SCurveEditor::CreateContextMenu(const FGeometry& InMyGeometry, const FVector2D& ScreenPosition)
{
	const bool CloseAfterSelection = true;
	FMenuBuilder MenuBuilder( CloseAfterSelection, NULL );

	MenuBuilder.BeginSection("EditCurveEditorActions", LOCTEXT("Actions", "Actions"));
	{
		FUIAction Action = FUIAction(FExecuteAction::CreateSP(this, &SCurveEditor::AddNewKey, InMyGeometry, ScreenPosition));
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("AddKey", "Add Key"),
			LOCTEXT("AddKey_ToolTip", "Create a new point in the curve.  You can also use (Shift + Left Mouse Button)"),
			FSlateIcon(),
			Action
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("CurveEditorActions", LOCTEXT("CurveAction", "Curve Actions") );
	{
		if( OnCreateAsset.IsBound() && IsEditingEnabled() )
		{
			FUIAction Action = FUIAction( FExecuteAction::CreateSP( this, &SCurveEditor::OnCreateExternalCurveClicked ) );
			MenuBuilder.AddMenuEntry
			( 
				LOCTEXT("CreateExternalCurve", "Create External Curve"),
				LOCTEXT("CreateExternalCurve_ToolTip", "Create an external asset using this internal curve"), 
				FSlateIcon(), 
				Action
			);
		}

		if( IsLinearColorCurve() && !bAlwaysDisplayColorCurves )
		{
			FUIAction ShowCurveAction( FExecuteAction::CreateSP( this, &SCurveEditor::OnShowCurveToggled ), FCanExecuteAction(), FIsActionChecked::CreateSP( this, &SCurveEditor::AreCurvesVisible ) );
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("ShowCurves","Show Curves"),
				LOCTEXT("ShowCurves_ToolTip", "Toggles displaying the curves for linear colors"),
				FSlateIcon(),
				ShowCurveAction,
				NAME_None,
				EUserInterfaceActionType::ToggleButton 
			);
		}

		if( IsLinearColorCurve() )
		{
			FUIAction ShowGradientAction( FExecuteAction::CreateSP( this, &SCurveEditor::OnShowGradientToggled ), FCanExecuteAction(), FIsActionChecked::CreateSP( this, &SCurveEditor::IsGradientEditorVisible ) );
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("ShowGradient","Show Gradient"),
				LOCTEXT("ShowGradient_ToolTip", "Toggles displaying the gradient for linear colors"),
				FSlateIcon(),
				ShowGradientAction,
				NAME_None,
				EUserInterfaceActionType::ToggleButton 
			);
		}
	}

	MenuBuilder.EndSection();
	FSlateApplication::Get().PushMenu( SharedThis( this ), MenuBuilder.MakeWidget(), FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu ) );
}

void SCurveEditor::OnCreateExternalCurveClicked()
{
	OnCreateAsset.ExecuteIfBound();
}


UObject* SCurveEditor::CreateCurveObject( TSubclassOf<UCurveBase> CurveType, UObject* PackagePtr, FName& AssetName )
{
	UObject* NewObj = NULL;
	CurveFactory = Cast<UCurveFactory>(NewObject<UFactory>(GetTransientPackage(), UCurveFactory::StaticClass()));
	if(CurveFactory)
	{
		CurveFactory->CurveClass = CurveType;
		NewObj = CurveFactory->FactoryCreateNew( CurveFactory->GetSupportedClass(), PackagePtr, AssetName, RF_Public|RF_Standalone, NULL, GWarn );
	}
	CurveFactory = NULL;
	return NewObj;
}

bool SCurveEditor::IsEditingEnabled() const
{
	return bCanEditTrack;
}

void SCurveEditor::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( CurveFactory );
}

TSharedPtr<FUICommandList> SCurveEditor::GetCommands()
{
	return Commands;
}

bool SCurveEditor::IsValidCurve( FRichCurve* Curve ) const
{
	bool bIsValid = false;
	if(Curve && CurveOwner)
	{
		for(auto CurveViewModel : CurveViewModels)
		{
			if(CurveViewModel->CurveInfo.CurveToEdit == Curve && CurveOwner->IsValidCurve(CurveViewModel->CurveInfo))
			{
				bIsValid = true;
				break;
			}
		}
	}
	return bIsValid;
}

void SCurveEditor::SetInputMinMax(float NewMin, float NewMax)
{
	if (SetInputViewRangeHandler.IsBound())
	{
		SetInputViewRangeHandler.Execute(NewMin, NewMax);
	}
	else
	{
		//if no delegate and view min input isn't using a delegate just set value directly
		if (ViewMinInput.IsBound() == false)
		{
			ViewMinInput.Set(NewMin);
		}

		if (ViewMaxInput.IsBound() == false)
		{
			ViewMaxInput.Set(NewMax);
		}
	}
}

void SCurveEditor::SetOutputMinMax(float NewMin, float NewMax)
{
	if (SetOutputViewRangeHandler.IsBound())
	{
		SetOutputViewRangeHandler.Execute(NewMin, NewMax);
	}
	else
	{
		//if no delegate and view min output isn't using a delegate just set value directly
		if (ViewMinOutput.IsBound() == false)
		{
			ViewMinOutput.Set(NewMin);
		}

		if (ViewMaxOutput.IsBound() == false)
		{
			ViewMaxOutput.Set(NewMax);
		}
	}
}

TSharedPtr<FCurveViewModel> SCurveEditor::HitTestCurves(  const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	if( AreCurvesVisible() )
	{
		FTrackScaleInfo ScaleInfo(ViewMinInput.Get(),  ViewMaxInput.Get(), ViewMinOutput.Get(), ViewMaxOutput.Get(), InMyGeometry.Size);

		const FVector2D HitPosition = InMyGeometry.AbsoluteToLocal( InMouseEvent.GetScreenSpacePosition()  );

		TArray<FRichCurve*> CurvesHit;

		for(auto CurveViewModel : CurveViewModels)
		{

			FRichCurve* Curve = CurveViewModel->CurveInfo.CurveToEdit;
			if(Curve != NULL)
			{
				float Time		 = ScaleInfo.LocalXToInput(HitPosition.X);
				float KeyScreenY = ScaleInfo.OutputToLocalY(Curve->Eval(Time));

				if( HitPosition.Y > (KeyScreenY - (0.5f * CONST_CurveSize.Y)) &&
					HitPosition.Y < (KeyScreenY + (0.5f * CONST_CurveSize.Y)))
				{
					return CurveViewModel;
				}
			}
		}
	}

	return TSharedPtr<FCurveViewModel>();
}

SCurveEditor::FSelectedTangent SCurveEditor::HitTestCubicTangents( const FGeometry& InMyGeometry, const FVector2D& HitScreenPosition )
{
	FSelectedTangent Tangent;

	if( AreCurvesVisible() )
	{
		FTrackScaleInfo ScaleInfo(ViewMinInput.Get(),  ViewMaxInput.Get(), ViewMinOutput.Get(), ViewMaxOutput.Get(), InMyGeometry.Size);

		const FVector2D HitPosition = InMyGeometry.AbsoluteToLocal( HitScreenPosition);

		for(auto It = SelectedKeys.CreateConstIterator();It;++It)
		{
			FSelectedCurveKey Key = *It;
			if(Key.IsValid())
			{
				float Time		 = ScaleInfo.LocalXToInput(HitPosition.X);
				float KeyScreenY = ScaleInfo.OutputToLocalY(Key.Curve->Eval(Time));

				FVector2D Arrive, Leave;
				GetTangentPoints(ScaleInfo, Key, Arrive, Leave);

				if( HitPosition.Y > (Arrive.Y - (0.5f * CONST_CurveSize.Y)) &&
					HitPosition.Y < (Arrive.Y + (0.5f * CONST_CurveSize.Y)) &&
					HitPosition.X > (Arrive.X - (0.5f * CONST_TangentSize.X)) && 
					HitPosition.X < (Arrive.X + (0.5f * CONST_TangentSize.X)))
				{
					Tangent.Key = Key;
					Tangent.bIsArrival = true;
					break;
				}
				if( HitPosition.Y > (Leave.Y - (0.5f * CONST_CurveSize.Y)) &&
					HitPosition.Y < (Leave.Y  + (0.5f * CONST_CurveSize.Y)) &&
					HitPosition.X > (Leave.X - (0.5f * CONST_TangentSize.X)) && 
					HitPosition.X < (Leave.X + (0.5f * CONST_TangentSize.X)))
				{
					Tangent.Key = Key;
					Tangent.bIsArrival = false;
					break;
				}
			}
		}
	}

	return Tangent;
}

void SCurveEditor::OnSelectInterpolationMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode)
{
	if(SelectedKeys.Num() > 0)
	{
		const FScopedTransaction Transaction(LOCTEXT("CurveEditor_SetInterpolationMode", "Select Interpolation Mode"));
		CurveOwner->ModifyOwner();
		TSet<FRichCurve*> ChangedCurves;

		for(auto It = SelectedKeys.CreateIterator();It;++It)
		{
			FSelectedCurveKey& Key = *It;
			check(IsValidCurve(Key.Curve));
			Key.Curve->SetKeyInterpMode(Key.KeyHandle,InterpMode );
			Key.Curve->SetKeyTangentMode(Key.KeyHandle,TangentMode );
		}

		TArray<FRichCurveEditInfo> ChangedCurveEditInfos;
		for (auto CurveViewModel : CurveViewModels)
		{
			if (ChangedCurves.Contains(CurveViewModel->CurveInfo.CurveToEdit))
			{
				ChangedCurveEditInfos.Add(CurveViewModel->CurveInfo);
			}
		}
		CurveOwner->OnCurveChanged(ChangedCurveEditInfos);
	}
}

bool SCurveEditor::IsInterpolationModeSelected(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode)
{
	if (SelectedKeys.Num() > 0)
	{
		for (auto SelectedKey : SelectedKeys)
		{
			if (SelectedKey.Curve->GetKeyInterpMode(SelectedKey.KeyHandle) != InterpMode || SelectedKey.Curve->GetKeyTangentMode(SelectedKey.KeyHandle) != TangentMode)
			{
				return false;
			}
		}
		return true;
	}
	else
	{
					return false;
				}
			}

/* Given a tangent value for a key, calculates the 2D delta vector from that key in curve space */
static inline FVector2D CalcTangentDir(float Tangent)
{
	const float Angle = FMath::Atan(Tangent);
	return FVector2D( FMath::Cos(Angle), -FMath::Sin(Angle) );
}

/*Given a 2d delta vector in curve space, calculates a tangent value */
static inline float CalcTangent(const FVector2D& HandleDelta)
{
	// Ensure X is positive and non-zero.
	// Tangent is gradient of handle.
	return HandleDelta.Y / FMath::Max<double>(HandleDelta.X, KINDA_SMALL_NUMBER);
}

void SCurveEditor::OnMoveTangent(FVector2D MouseCurvePosition)
{
	auto& RichKey = SelectedTangent.Key.Curve->GetKey(SelectedTangent.Key.KeyHandle);

	const FSelectedCurveKey &Key = SelectedTangent.Key;

	FVector2D KeyPosition( Key.Curve->GetKeyTime(Key.KeyHandle),Key.Curve->GetKeyValue(Key.KeyHandle) );

	FVector2D Movement = MouseCurvePosition - KeyPosition;
	if(SelectedTangent.bIsArrival)
	{
		Movement *= -1.0f;
	}
	float Tangent = CalcTangent(Movement);

	if(RichKey.TangentMode  != RCTM_Break)
	{
		RichKey.ArriveTangent = Tangent;
		RichKey.LeaveTangent  = Tangent;
		
		RichKey.TangentMode = RCTM_User;
	}
	else
	{
		if(SelectedTangent.bIsArrival)
		{
			RichKey.ArriveTangent = Tangent;
		}
		else
		{
			RichKey.LeaveTangent = Tangent;
		}
	}	

}
void SCurveEditor::GetTangentPoints(  FTrackScaleInfo &ScaleInfo, const FSelectedCurveKey &Key, FVector2D& Arrive, FVector2D& Leave ) const
{
	FVector2D ArriveTangentDir = CalcTangentDir( Key.Curve->GetKey(Key.KeyHandle).ArriveTangent);
	FVector2D LeaveTangentDir = CalcTangentDir( Key.Curve->GetKey(Key.KeyHandle).LeaveTangent);

	FVector2D KeyPosition( Key.Curve->GetKeyTime(Key.KeyHandle),Key.Curve->GetKeyValue(Key.KeyHandle) );

	ArriveTangentDir.Y *= -1.0f;
	LeaveTangentDir.Y *= -1.0f;
	FVector2D ArrivePosition = -ArriveTangentDir + KeyPosition;

	FVector2D LeavePosition = LeaveTangentDir + KeyPosition;

	Arrive = FVector2D(ScaleInfo.InputToLocalX(ArrivePosition.X), ScaleInfo.OutputToLocalY(ArrivePosition.Y));
	Leave = FVector2D(ScaleInfo.InputToLocalX(LeavePosition.X), ScaleInfo.OutputToLocalY(LeavePosition.Y));

	FVector2D KeyScreenPosition = FVector2D(ScaleInfo.InputToLocalX(KeyPosition.X), ScaleInfo.OutputToLocalY(KeyPosition.Y));

	FVector2D ToArrive = Arrive - KeyScreenPosition;
	ToArrive.Normalize();

	Arrive = KeyScreenPosition + ToArrive*CONST_KeyTangentOffset;

	FVector2D ToLeave = Leave - KeyScreenPosition;
	ToLeave.Normalize();

	Leave = KeyScreenPosition + ToLeave*CONST_KeyTangentOffset;
}

TArray<SCurveEditor::FSelectedCurveKey> SCurveEditor::GetEditableKeysWithinMarquee(const FGeometry& InMyGeometry, FVector2D MarqueeTopLeft, FVector2D MarqueeBottomRight) const
{
	TArray<FSelectedCurveKey> KeysWithinMarquee;
	if (AreCurvesVisible())
	{
		FTrackScaleInfo ScaleInfo(ViewMinInput.Get(), ViewMaxInput.Get(), ViewMinOutput.Get(), ViewMaxOutput.Get(), InMyGeometry.Size);
		for (auto CurveViewModel : CurveViewModels)
		{
			if (!CurveViewModel->bIsLocked && CurveViewModel->bIsVisible)
			{
				FRichCurve* Curve = CurveViewModel->CurveInfo.CurveToEdit;
				if (Curve != NULL)
				{
					for (auto It(Curve->GetKeyHandleIterator()); It; ++It)
					{
						float KeyScreenX = ScaleInfo.InputToLocalX(Curve->GetKeyTime(It.Key()));
						float KeyScreenY = ScaleInfo.OutputToLocalY(Curve->GetKeyValue(It.Key()));

						if (KeyScreenX >= (MarqueeTopLeft.X - (0.5f * CONST_KeySize.X)) &&
							KeyScreenX <= (MarqueeBottomRight.X + (0.5f * CONST_KeySize.X)) &&
							KeyScreenY >= (MarqueeTopLeft.Y - (0.5f * CONST_KeySize.Y)) &&
							KeyScreenY <= (MarqueeBottomRight.Y + (0.5f * CONST_KeySize.Y)))
						{
							KeysWithinMarquee.Add(FSelectedCurveKey(Curve, It.Key()));
						}
					}
				}
			}
		}
	}

	return KeysWithinMarquee;
}

void SCurveEditor::BeginDragTransaction()
{
	TransactionIndex = GEditor->BeginTransaction( LOCTEXT("CurveEditor_Drag", "Mouse Drag") );
	CurveOwner->ModifyOwner();
}

void SCurveEditor::EndDragTransaction()
{
	if ( TransactionIndex >= 0 )
	{
		TArray<FRichCurveEditInfo> ChangedCurveEditInfos;
		for (auto CurveViewModel : CurveViewModels)
		{
			ChangedCurveEditInfos.Add(CurveViewModel->CurveInfo);
		}
		CurveOwner->OnCurveChanged(ChangedCurveEditInfos);
		GEditor->EndTransaction();
		TransactionIndex = -1;
	}
}

bool SCurveEditor::FSelectedTangent::IsValid() const
{
	return Key.IsValid();
}

void SCurveEditor::UndoAction()
{
	GEditor->UndoTransaction();
}

void SCurveEditor::RedoAction()
{
	GEditor->RedoTransaction();
}

void SCurveEditor::PostUndo(bool bSuccess)
{
	//remove any invalid keys
	for(int32 i = 0;i<SelectedKeys.Num();++i)
	{
		auto Key = SelectedKeys[i];
		if(!IsValidCurve(Key.Curve) || !Key.IsValid())
		{
			SelectedKeys.RemoveAt(i);
			i--;
		}
	}
}

bool SCurveEditor::IsLinearColorCurve() const 
{
	return CurveOwner && CurveOwner->IsLinearColorCurve();
}

FVector2D SCurveEditor::SnapLocation(FVector2D InLocation)
{
	if (bSnappingEnabled.Get())
	{
		InLocation.X = InputSnap != 0 ? FMath::RoundToInt(InLocation.X / InputSnap.Get()) * InputSnap.Get() : InLocation.X;
		InLocation.Y = OutputSnap != 0 ? FMath::RoundToInt(InLocation.Y / OutputSnap.Get()) * OutputSnap.Get() : InLocation.Y;
	}
	return InLocation;
}

TSharedPtr<FCurveViewModel> SCurveEditor::GetViewModelForCurve(FRichCurve* InCurve)
{
	for (auto CurveViewModel : CurveViewModels)
	{
		if (InCurve == CurveViewModel->CurveInfo.CurveToEdit)
		{
			return CurveViewModel;
		}
	}
	return TSharedPtr<FCurveViewModel>();
}

#undef LOCTEXT_NAMESPACE
