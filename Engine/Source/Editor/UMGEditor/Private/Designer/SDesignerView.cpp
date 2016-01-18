// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"

#include "ISettingsModule.h"

#include "Designer/DesignTimeUtils.h"

#include "Extensions/CanvasSlotExtension.h"
#include "Extensions/GridSlotExtension.h"
#include "Extensions/HorizontalSlotExtension.h"
#include "Extensions/UniformGridSlotExtension.h"
#include "Extensions/VerticalSlotExtension.h"
#include "SDesignerView.h"

#include "BlueprintEditor.h"
#include "SKismetInspector.h"
#include "BlueprintEditorUtils.h"

#include "WidgetTemplateDragDropOp.h"
#include "SZoomPan.h"
#include "SRuler.h"
#include "SDisappearingBar.h"
#include "SDesignerToolBar.h"
#include "DesignerCommands.h"
#include "STransformHandle.h"
#include "Runtime/Engine/Classes/Engine/UserInterfaceSettings.h"
#include "Runtime/Engine/Classes/Engine/RendererSettings.h"
#include "SDPIScaler.h"
#include "SNumericEntryBox.h"

#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintCompiler.h"
#include "WidgetBlueprintEditor.h"
#include "WidgetBlueprintEditorUtils.h"

#include "ObjectEditorUtils.h"
#include "Blueprint/WidgetTree.h"
#include "ScopedTransaction.h"
#include "Settings/WidgetDesignerSettings.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/NamedSlot.h"

#define LOCTEXT_NAMESPACE "UMG"

const float HoveredAnimationTime = 0.150f;


struct FWidgetHitResult
{
public:
	FWidgetReference Widget;
	FArrangedWidget WidgetArranged;

	UNamedSlot* NamedSlot;
	FArrangedWidget NamedSlotArranged;

public:
	FWidgetHitResult()
		: WidgetArranged(SNullWidget::NullWidget, FGeometry())
		, NamedSlotArranged(SNullWidget::NullWidget, FGeometry())
	{
	}
};


class FSelectedWidgetDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FSelectedWidgetDragDropOp, FDecoratedDragDropOp)

	struct FDraggingWidgetReference
	{
		FWidgetReference Widget;

		FVector2D DraggedOffset;
	};

	struct FItem
	{
		/** The slot properties for the old slot the widget was in, is used to attempt to reapply the same layout information */
		TMap<FName, FString> ExportedSlotProperties;

		/** The widget being dragged */
		UWidget* Template;

		/** The preview widget being dragged */
		UWidget* Preview;

		/** Can the drag drop change the widget's parent? */
		bool bStayingInParent;

		/** The original parent of the widget. */
		FWidgetReference ParentWidget;

		FVector2D DraggedOffset;
	};

	TArray<FItem> DraggedWidgets;

	static TSharedRef<FSelectedWidgetDragDropOp> New(TSharedPtr<FWidgetBlueprintEditor> Editor, const TArray<FDraggingWidgetReference>& InWidgets);
};

TSharedRef<FSelectedWidgetDragDropOp> FSelectedWidgetDragDropOp::New(TSharedPtr<FWidgetBlueprintEditor> Editor, const TArray<FDraggingWidgetReference>& InWidgets)
{
	TSharedRef<FSelectedWidgetDragDropOp> Operation = MakeShareable(new FSelectedWidgetDragDropOp());

	for (const auto& InDraggedWidget : InWidgets)
	{
		FItem DraggedWidget;
		DraggedWidget.bStayingInParent = false;

		if (UPanelWidget* PanelTemplate = InDraggedWidget.Widget.GetTemplate()->GetParent())
		{
			DraggedWidget.ParentWidget = Editor->GetReferenceFromTemplate(PanelTemplate);
			DraggedWidget.bStayingInParent = PanelTemplate->LockToPanelOnDrag() || GetDefault<UWidgetDesignerSettings>()->bLockToPanelOnDragByDefault;
		}

		// Cache the preview and template, it's not safe to query the preview/template while dragging the widget as it no longer
		// exists in the tree.
		DraggedWidget.Preview = InDraggedWidget.Widget.GetPreview();
		DraggedWidget.Template = InDraggedWidget.Widget.GetTemplate();

		DraggedWidget.DraggedOffset = InDraggedWidget.DraggedOffset;

		FWidgetBlueprintEditorUtils::ExportPropertiesToText(InDraggedWidget.Widget.GetTemplate()->Slot, DraggedWidget.ExportedSlotProperties);

		Operation->DraggedWidgets.Add(DraggedWidget);
	}

	// Set the display text based on whether we're dragging a single or multiple widgets
	if (InWidgets.Num() == 1)
	{
		Operation->DefaultHoverText = InWidgets[0].Widget.GetTemplate()->GetLabelText();
		Operation->CurrentHoverText = InWidgets[0].Widget.GetTemplate()->GetLabelText();
	}
	else
	{
		Operation->CurrentHoverText = Operation->DefaultHoverText = LOCTEXT("DragMultipleWidgets", "Multiple Widgets");
	}

	Operation->Construct();
	return Operation;
}

//////////////////////////////////////////////////////////////////////////

static bool LocateWidgetsUnderCursor_Helper(FArrangedWidget& Candidate, FVector2D InAbsoluteCursorLocation, FArrangedChildren& OutWidgetsUnderCursor, bool bIgnoreEnabledStatus)
{
	const bool bCandidateUnderCursor =
		// Candidate is physically under the cursor
		Candidate.Geometry.IsUnderLocation(InAbsoluteCursorLocation);

	bool bHitAnyWidget = false;
	if ( bCandidateUnderCursor )
	{
		// The candidate widget is under the mouse
		OutWidgetsUnderCursor.AddWidget(Candidate);

		// Check to see if we were asked to still allow children to be hit test visible
		bool bHitChildWidget = false;
		
		if ( Candidate.Widget->GetVisibility().AreChildrenHitTestVisible() )//!= 0 || OutWidgetsUnderCursor. )
		{
			FArrangedChildren ArrangedChildren(OutWidgetsUnderCursor.GetFilter());
			Candidate.Widget->ArrangeChildren(Candidate.Geometry, ArrangedChildren);

			// A widget's children are implicitly Z-ordered from first to last
			for ( int32 ChildIndex = ArrangedChildren.Num() - 1; !bHitChildWidget && ChildIndex >= 0; --ChildIndex )
			{
				FArrangedWidget& SomeChild = ArrangedChildren[ChildIndex];
				bHitChildWidget = ( SomeChild.Widget->IsEnabled() || bIgnoreEnabledStatus ) && LocateWidgetsUnderCursor_Helper(SomeChild, InAbsoluteCursorLocation, OutWidgetsUnderCursor, bIgnoreEnabledStatus);
			}
		}

		// If we hit a child widget or we hit our candidate widget then we'll append our widgets
		const bool bHitCandidateWidget = OutWidgetsUnderCursor.Accepts(Candidate.Widget->GetVisibility()) &&
			Candidate.Widget->GetVisibility().AreChildrenHitTestVisible();
		
		bHitAnyWidget = bHitChildWidget || bHitCandidateWidget;
		if ( !bHitAnyWidget )
		{
			// No child widgets were hit, and even though the cursor was over our candidate widget, the candidate
			// widget was not hit-testable, so we won't report it
			check(OutWidgetsUnderCursor.Last() == Candidate);
			OutWidgetsUnderCursor.Remove(OutWidgetsUnderCursor.Num() - 1);
		}
	}

	return bHitAnyWidget;
}

/////////////////////////////////////////////////////
// SDesignerView

const FString SDesignerView::ConfigSectionName = "UMGEditor.Designer";
const uint32 SDesignerView::DefaultResolutionWidth = 1280;
const uint32 SDesignerView::DefaultResolutionHeight = 720;
const FString SDesignerView::DefaultAspectRatio = "16:9";

void SDesignerView::Construct(const FArguments& InArgs, TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
{
	ScopedTransaction = nullptr;

	PreviewWidget = nullptr;
	BlueprintEditor = InBlueprintEditor;

	DesignerMessage = EDesignerMessage::None;
	TransformMode = ETransformMode::Layout;

	SetStartupResolution();

	CachedPreviewDesiredSize = FVector2D(0, 0);

	ResolutionTextFade = FCurveSequence(0.0f, 1.0f);
	ResolutionTextFade.Play(this->AsShared());

	HoveredWidgetOutlineFade = FCurveSequence(0.0f, 0.15f);

	SelectedWidgetContextMenuLocation = FVector2D(0, 0);

	bMovingExistingWidget = false;

	// TODO UMG - Register these with the module through some public interface to allow for new extensions to be registered.
	Register(MakeShareable(new FVerticalSlotExtension()));
	Register(MakeShareable(new FHorizontalSlotExtension()));
	Register(MakeShareable(new FCanvasSlotExtension()));
	Register(MakeShareable(new FUniformGridSlotExtension()));
	Register(MakeShareable(new FGridSlotExtension()));

	GEditor->OnBlueprintReinstanced().AddRaw(this, &SDesignerView::OnBlueprintReinstanced);

	BindCommands();

	SDesignSurface::Construct(SDesignSurface::FArguments()
		.AllowContinousZoomInterpolation(false)
		.Content()
		[
			SNew(SOverlay)

			// The bottom layer of the overlay where the actual preview widget appears.
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(PreviewHitTestRoot, SZoomPan)
				.Visibility(EVisibility::HitTestInvisible)
				.ZoomAmount(this, &SDesignerView::GetZoomAmount)
				.ViewOffset(this, &SDesignerView::GetViewOffset)
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					[
						SNew(SBorder)
						[
							SNew(SSpacer)
							.Size(FVector2D(1, 1))
						]
					]

					+ SOverlay::Slot()
					[
						SNew(SBorder)
						.Padding(FMargin(0))
						.BorderImage(this, &SDesignerView::GetPreviewBackground)
						[
							SAssignNew(PreviewAreaConstraint, SBox)
							.WidthOverride(this, &SDesignerView::GetPreviewAreaWidth)
							.HeightOverride(this, &SDesignerView::GetPreviewAreaHeight)
							[
								SAssignNew(PreviewSurface, SDPIScaler)
								.DPIScale(this, &SDesignerView::GetPreviewDPIScale)
							]
						]
					]
				]
			]

			// A layer in the overlay where we put all the user intractable widgets, like the reorder widgets.
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(EffectsLayer, SPaintSurface)
				.OnPaintHandler(this, &SDesignerView::HandleEffectsPainting)
			]

			// A layer in the overlay where we put all the user intractable widgets, like the reorder widgets.
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(ExtensionWidgetCanvas, SCanvas)
				.Visibility(EVisibility::SelfHitTestInvisible)
			]

			+ SOverlay::Slot()
			[
				SNew(SGridPanel)
				.FillColumn(1, 1.0f)
				.FillRow(1, 1.0f)

				// Corner
				+ SGridPanel::Slot(0, 0)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(FLinearColor(FColor(48, 48, 48)))
				]

				// Top Ruler
				+ SGridPanel::Slot(1, 0)
				[
					SAssignNew(TopRuler, SRuler)
					.Orientation(Orient_Horizontal)
					.Visibility(this, &SDesignerView::GetRulerVisibility)
				]

				// Side Ruler
				+ SGridPanel::Slot(0, 1)
				[
					SAssignNew(SideRuler, SRuler)
					.Orientation(Orient_Vertical)
					.Visibility(this, &SDesignerView::GetRulerVisibility)
				]

				// Designer overlay UI, toolbar, status messages, zoom level...etc
				+ SGridPanel::Slot(1, 1)
				[
					CreateOverlayUI()
				]
			]
		]
	);

	auto PinnedBlueprintEditor = BlueprintEditor.Pin();
	PinnedBlueprintEditor->OnSelectedWidgetsChanged.AddRaw(this, &SDesignerView::OnEditorSelectionChanged);
	PinnedBlueprintEditor->OnHoveredWidgetSet.AddRaw(this, &SDesignerView::OnHoveredWidgetSet);
	PinnedBlueprintEditor->OnHoveredWidgetCleared.AddRaw(this, &SDesignerView::OnHoveredWidgetCleared);

	ZoomToFit(/*bInstantZoom*/ true);
}

TSharedRef<SWidget> SDesignerView::CreateOverlayUI()
{
	return SNew(SOverlay)

	// Top-right corner text indicating PIE is active
	+ SOverlay::Slot()
	.Padding(0)
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SImage)
		.Visibility(this, &SDesignerView::PIENotification)
		.Image(FEditorStyle::GetBrush(TEXT("Graph.PlayInEditor")))
	]

	// Top-right corner text indicating PIE is active
	+ SOverlay::Slot()
	.Padding(20)
	.VAlign(VAlign_Top)
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Visibility(this, &SDesignerView::PIENotification)
		.TextStyle( FEditorStyle::Get(), "Graph.SimulatingText" )
		.Text( LOCTEXT("SIMULATING", "SIMULATING") )
	]

	// Top bar with buttons for changing the designer
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Top)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(6, 2, 0, 0)
		[
			SNew(STextBlock)
			.TextStyle(FEditorStyle::Get(), "Graph.ZoomText")
			.Text(this, &SDesignerView::GetZoomText)
			.ColorAndOpacity(this, &SDesignerView::GetZoomTextColorAndOpacity)
			.Visibility(EVisibility::SelfHitTestInvisible)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SSpacer)
			.Size(FVector2D(1, 1))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SDesignerToolBar)
			.CommandList(CommandList)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "ViewportMenu.Button")
			.ToolTipText(LOCTEXT("ZoomToFit_ToolTip", "Zoom To Fit"))
			.OnClicked(this, &SDesignerView::HandleZoomToFitClicked)
			.ContentPadding(1.0f)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("UMGEditor.ZoomToFit"))
			]
		]
		
		// Preview Screen Size
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.ButtonStyle(FEditorStyle::Get(), "ViewportMenu.Button")
			.ForegroundColor(FLinearColor::Black)
			.OnGetMenuContent(this, &SDesignerView::GetResolutionsMenu)
			.ContentPadding(1.0f)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ScreenSize", "Screen Size"))
				.TextStyle(FEditorStyle::Get(), "ViewportMenu.Label")
			]
		]

		// Screen Fill Size Rule
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.ButtonStyle(FEditorStyle::Get(), "ViewportMenu.Button")
			.ForegroundColor(FLinearColor::Black)
			.OnGetMenuContent(this, &SDesignerView::GetScreenSizingFillMenu)
			.ContentPadding(1.0f)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &SDesignerView::GetScreenSizingFillText)
				.TextStyle(FEditorStyle::Get(), "ViewportMenu.Label")
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(2, 0))
		[
			SNew(SNumericEntryBox<int32>)
			.AllowSpin(true)
			.Delta(1)
			.MinSliderValue(1)
			.MinValue(1)
			.MaxSliderValue(TOptional<int32>(1000))
			.Value(this, &SDesignerView::GetCustomResolutionWidth)
			.OnValueChanged(this, &SDesignerView::OnCustomResolutionWidthChanged)
			.Visibility(this, &SDesignerView::GetCustomResolutionEntryVisibility)
			.MinDesiredValueWidth(50)
			.LabelPadding(0)
			.Label()
			[
				SNumericEntryBox<int32>::BuildLabel(LOCTEXT("Width", "Width"), FLinearColor::White, SNumericEntryBox<int32>::RedLabelBackgroundColor)
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(2, 0))
		[
			SNew(SNumericEntryBox<int32>)
			.AllowSpin(true)
			.Delta(1)
			.MinSliderValue(1)
			.MaxSliderValue(TOptional<int32>(1000))
			.MinValue(1)
			.Value(this, &SDesignerView::GetCustomResolutionHeight)
			.OnValueChanged(this, &SDesignerView::OnCustomResolutionHeightChanged)
			.Visibility(this, &SDesignerView::GetCustomResolutionEntryVisibility)
			.MinDesiredValueWidth(50)
			.LabelPadding(0)
			.Label()
			[
				SNumericEntryBox<int32>::BuildLabel(LOCTEXT("Height", "Height"), FLinearColor::White, SNumericEntryBox<int32>::GreenLabelBackgroundColor)
			]
		]
	]

	// Info Bar, displays heads up information about some actions.
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Bottom)
	[
		SNew(SDisappearingBar)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.10, 0.10, 0.10, 0.75))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(0, 5))
			.Visibility(this, &SDesignerView::GetInfoBarVisibility)
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "Graph.ZoomText")
				.Text(this, &SDesignerView::GetInfoBarText)
			]
		]
	]

	// Bottom bar to show current resolution & AR
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Bottom)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(6, 0, 0, 2)
		[
			SNew(STextBlock)
			.Visibility(this, &SDesignerView::GetResolutionTextVisibility)
			.TextStyle(FEditorStyle::Get(), "Graph.ZoomText")
			.Text(this, &SDesignerView::GetCurrentResolutionText)
			.ColorAndOpacity(this, &SDesignerView::GetResolutionTextColorAndOpacity)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Right)
		.Padding(0, 0, 6, 2)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "Graph.ZoomText")
				.Text(this, &SDesignerView::GetCurrentDPIScaleText)
				.ColorAndOpacity(FLinearColor(1, 1, 1, 0.25f))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6, 0, 0, 0)
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.ContentPadding(FMargin(3, 1))
				.OnClicked(this, &SDesignerView::HandleDPISettingsClicked)
				.ToolTipText(LOCTEXT("DPISettingsTooltip", "Configure the UI Scale Curve to control how the UI is scaled on different resolutions."))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("UMGEditor.DPISettings"))
				]
			]
		]
	];
}

SDesignerView::~SDesignerView()
{
	UWidgetBlueprint* Blueprint = GetBlueprint();
	if ( Blueprint )
	{
		Blueprint->OnChanged().RemoveAll(this);
		Blueprint->OnCompiled().RemoveAll(this);
	}

	if ( BlueprintEditor.IsValid() )
	{
		auto PinnedEditor = BlueprintEditor.Pin();
		PinnedEditor->OnSelectedWidgetsChanged.RemoveAll(this);
		PinnedEditor->OnHoveredWidgetSet.RemoveAll(this);
		PinnedEditor->OnHoveredWidgetCleared.RemoveAll(this);
	}

	if ( GEditor )
	{
		GEditor->OnBlueprintReinstanced().RemoveAll(this);
	}
}

void SDesignerView::BindCommands()
{
	CommandList = MakeShareable(new FUICommandList);

	const FDesignerCommands& Commands = FDesignerCommands::Get();

	CommandList->MapAction(
		Commands.LayoutTransform,
		FExecuteAction::CreateSP(this, &SDesignerView::SetTransformMode, ETransformMode::Layout),
		FCanExecuteAction::CreateSP(this, &SDesignerView::CanSetTransformMode, ETransformMode::Layout),
		FIsActionChecked::CreateSP(this, &SDesignerView::IsTransformModeActive, ETransformMode::Layout)
		);

	CommandList->MapAction(
		Commands.RenderTransform,
		FExecuteAction::CreateSP(this, &SDesignerView::SetTransformMode, ETransformMode::Render),
		FCanExecuteAction::CreateSP(this, &SDesignerView::CanSetTransformMode, ETransformMode::Render),
		FIsActionChecked::CreateSP(this, &SDesignerView::IsTransformModeActive, ETransformMode::Render)
		);
}

void SDesignerView::AddReferencedObjects(FReferenceCollector& Collector)
{
	if ( PreviewWidget )
	{
		Collector.AddReferencedObject(PreviewWidget);
	}

	for (auto& DropPreview : DropPreviews)
	{
		if (DropPreview.Widget)
		{
			Collector.AddReferencedObject(DropPreview.Widget);
		}
		if (DropPreview.Parent)
		{
			Collector.AddReferencedObject(DropPreview.Parent);
		}
	}
}

void SDesignerView::SetTransformMode(ETransformMode::Type InTransformMode)
{
	if ( !InTransaction() )
	{
		TransformMode = InTransformMode;
	}
}

bool SDesignerView::CanSetTransformMode(ETransformMode::Type InTransformMode) const
{
	return true;
}

bool SDesignerView::IsTransformModeActive(ETransformMode::Type InTransformMode) const
{
	return TransformMode == InTransformMode;
}

void SDesignerView::SetStartupResolution()
{
	// Use previously set resolution (or create new entries using default values)
	// Width
	if (!GConfig->GetInt(*ConfigSectionName, TEXT("PreviewWidth"), PreviewWidth, GEditorPerProjectIni))
	{
		GConfig->SetInt(*ConfigSectionName, TEXT("PreviewWidth"), DefaultResolutionWidth, GEditorPerProjectIni);
		PreviewWidth = DefaultResolutionWidth;
	}
	// Height
	if (!GConfig->GetInt(*ConfigSectionName, TEXT("PreviewHeight"), PreviewHeight, GEditorPerProjectIni))
	{
		GConfig->SetInt(*ConfigSectionName, TEXT("PreviewHeight"), DefaultResolutionHeight, GEditorPerProjectIni);
		PreviewHeight = DefaultResolutionHeight;
	}
	// Aspect Ratio
	if (!GConfig->GetString(*ConfigSectionName, TEXT("PreviewAspectRatio"), PreviewAspectRatio, GEditorPerProjectIni))
	{
		GConfig->SetString(*ConfigSectionName, TEXT("PreviewAspectRatio"), *DefaultAspectRatio, GEditorPerProjectIni);
		PreviewAspectRatio = DefaultAspectRatio;
	}
}

float SDesignerView::GetPreviewScale() const
{
	return GetZoomAmount() * GetPreviewDPIScale();
}

const TSet<FWidgetReference>& SDesignerView::GetSelectedWidgets() const
{
	return BlueprintEditor.Pin()->GetSelectedWidgets();
}

FWidgetReference SDesignerView::GetSelectedWidget() const
{
	const TSet<FWidgetReference>& SelectedWidgets = BlueprintEditor.Pin()->GetSelectedWidgets();

	// Only return a selected widget when we have only a single item selected.
	if ( SelectedWidgets.Num() == 1 )
	{
		for ( TSet<FWidgetReference>::TConstIterator SetIt(SelectedWidgets); SetIt; ++SetIt )
		{
			return *SetIt;
		}
	}

	return FWidgetReference();
}

ETransformMode::Type SDesignerView::GetTransformMode() const
{
	return TransformMode;
}

FOptionalSize SDesignerView::GetPreviewAreaWidth() const
{
	FVector2D Area, Size;
	GetPreviewAreaAndSize(Area, Size);

	return Area.X;
}

FOptionalSize SDesignerView::GetPreviewAreaHeight() const
{
	FVector2D Area, Size;
	GetPreviewAreaAndSize(Area, Size);

	return Area.Y;
}

FOptionalSize SDesignerView::GetPreviewSizeWidth() const
{
	FVector2D Area, Size;
	GetPreviewAreaAndSize(Area, Size);

	return Size.X;
}

FOptionalSize SDesignerView::GetPreviewSizeHeight() const
{
	FVector2D Area, Size;
	GetPreviewAreaAndSize(Area, Size);

	return Size.Y;
}

const FSlateBrush* SDesignerView::GetPreviewBackground() const
{
	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		if ( DefaultWidget->PreviewBackground )
		{
			BackgroundImage.SetResourceObject(DefaultWidget->PreviewBackground);
			return &BackgroundImage;
		}
	}

	return nullptr;
}

void SDesignerView::GetPreviewAreaAndSize(FVector2D& Area, FVector2D& Size) const
{
	Area = FVector2D(PreviewWidth, PreviewHeight);
	Size = FVector2D(PreviewWidth, PreviewHeight);

	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		switch ( DefaultWidget->DesignSizeMode )
		{
		case EDesignPreviewSizeMode::Custom:
			Area = DefaultWidget->DesignTimeSize;
			Size = DefaultWidget->DesignTimeSize;
			return;
		case EDesignPreviewSizeMode::CustomOnScreen:
			Size = DefaultWidget->DesignTimeSize;
			return;
		case EDesignPreviewSizeMode::Desired:
			Area = CachedPreviewDesiredSize;
			Size = CachedPreviewDesiredSize;
			return;
		case EDesignPreviewSizeMode::DesiredOnScreen:
			Size = CachedPreviewDesiredSize;
			return;
		case EDesignPreviewSizeMode::FillScreen:
			break;
		}
	}
}

float SDesignerView::GetPreviewDPIScale() const
{
	// If the user is using a custom size then we disable the DPI scaling logic.
	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		if ( DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::Custom || 
			 DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::Desired )
		{
			return 1.0f;
		}
	}

	return GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass())->GetDPIScaleBasedOnSize(FIntPoint(PreviewWidth, PreviewHeight));
}

FSlateRect SDesignerView::ComputeAreaBounds() const
{
	return FSlateRect(0, 0, GetPreviewAreaWidth().Get(), GetPreviewAreaHeight().Get());
}

int32 SDesignerView::GetSnapGridSize() const
{
	const UWidgetDesignerSettings* DesignerSettings = GetDefault<UWidgetDesignerSettings>();
	return DesignerSettings->GridSnapSize;
}

int32 SDesignerView::GetGraphRulePeriod() const
{
	return 10;
}

float SDesignerView::GetGridScaleAmount() const
{
	return GetPreviewDPIScale();
}

EVisibility SDesignerView::GetInfoBarVisibility() const
{
	if ( DesignerMessage != EDesignerMessage::None )
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

FText SDesignerView::GetInfoBarText() const
{
	switch ( DesignerMessage )
	{
	case EDesignerMessage::MoveFromParent:
		return LOCTEXT("PressShiftToMove", "Press Alt to move the widget out of the current parent");
	}

	return FText::GetEmpty();
}

void SDesignerView::OnEditorSelectionChanged()
{
	TSet<FWidgetReference> PendingSelectedWidgets = BlueprintEditor.Pin()->GetSelectedWidgets();

	// Notify all widgets that are no longer selected.
	for ( FWidgetReference& WidgetRef : SelectedWidgetsCache )
	{
		if ( WidgetRef.IsValid() && !PendingSelectedWidgets.Contains(WidgetRef) )
		{
			WidgetRef.GetPreview()->Deselect();
		}
	}

	// Notify all widgets that are now selected.
	for ( FWidgetReference& WidgetRef : PendingSelectedWidgets )
	{
		if ( WidgetRef.IsValid() && !SelectedWidgetsCache.Contains(WidgetRef) )
		{
			WidgetRef.GetPreview()->Select();
		}
	}

	SelectedWidgetsCache = PendingSelectedWidgets;

	CreateExtensionWidgetsForSelection();
}

void SDesignerView::OnHoveredWidgetSet(const FWidgetReference& InHoveredWidget)
{
	HoveredWidgetOutlineFade.Play(this->AsShared());
}

void SDesignerView::OnHoveredWidgetCleared()
{
	HoveredWidgetOutlineFade.JumpToEnd();
}

FGeometry SDesignerView::GetDesignerGeometry() const
{
	return CachedGeometry;
}

void SDesignerView::MarkDesignModifed(bool bRequiresRecompile)
{
	if ( bRequiresRecompile )
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
	else
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
	}
}

bool SDesignerView::GetWidgetParentGeometry(const FWidgetReference& Widget, FGeometry& Geometry) const
{
	if ( UWidget* WidgetPreview = Widget.GetPreview() )
	{
		if ( UPanelWidget* Parent = WidgetPreview->GetParent() )
		{
			return GetWidgetGeometry(Parent, Geometry);
		}
	}

	Geometry = GetDesignerGeometry();
	return true;
}

bool SDesignerView::GetWidgetGeometry(const FWidgetReference& Widget, FGeometry& Geometry) const
{
	if ( const UWidget* WidgetPreview = Widget.GetPreview() )
	{
		return GetWidgetGeometry(WidgetPreview, Geometry);
	}

	return false;
}

bool SDesignerView::GetWidgetGeometry(const UWidget* InPreviewWidget, FGeometry& Geometry) const
{
	TSharedPtr<SWidget> CachedPreviewWidget = InPreviewWidget->GetCachedWidget();
	if ( CachedPreviewWidget.IsValid() )
	{
		const FArrangedWidget* ArrangedWidget = CachedWidgetGeometry.Find(CachedPreviewWidget.ToSharedRef());
		if ( ArrangedWidget )
		{
			Geometry = ArrangedWidget->Geometry;
			return true;
		}
	}

	return false;
}

FGeometry SDesignerView::MakeGeometryWindowLocal(const FGeometry& WidgetGeometry) const
{
	FGeometry NewGeometry = WidgetGeometry;

	TSharedPtr<SWindow> WidgetWindow = FSlateApplication::Get().FindWidgetWindow(SharedThis(this));
	if ( WidgetWindow.IsValid() )
	{
		TSharedRef<SWindow> CurrentWindowRef = WidgetWindow.ToSharedRef();

		NewGeometry.AppendTransform(FSlateLayoutTransform(Inverse(CurrentWindowRef->GetPositionInScreen())));
	}

	return NewGeometry;
}

void SDesignerView::ClearExtensionWidgets()
{
	ExtensionWidgetCanvas->ClearChildren();
}

void SDesignerView::CreateExtensionWidgetsForSelection()
{
	// Remove all the current extension widgets
	ClearExtensionWidgets();

	// Get the selected widgets as an array
	TArray<FWidgetReference> Selected = GetSelectedWidgets().Array();
	
	TArray< TSharedRef<FDesignerSurfaceElement> > ExtensionElements;

	if ( Selected.Num() > 0 )
	{
		// Add transform handles
		ExtensionElements.Add(MakeShareable(new FDesignerSurfaceElement(SNew(STransformHandle, this, ETransformDirection::TopLeft), EExtensionLayoutLocation::TopLeft, FVector2D(-10, -10))));
		ExtensionElements.Add(MakeShareable(new FDesignerSurfaceElement(SNew(STransformHandle, this, ETransformDirection::TopCenter), EExtensionLayoutLocation::TopCenter, FVector2D(-5, -10))));
		ExtensionElements.Add(MakeShareable(new FDesignerSurfaceElement(SNew(STransformHandle, this, ETransformDirection::TopRight), EExtensionLayoutLocation::TopRight, FVector2D(0, -10))));

		ExtensionElements.Add(MakeShareable(new FDesignerSurfaceElement(SNew(STransformHandle, this, ETransformDirection::CenterLeft), EExtensionLayoutLocation::CenterLeft, FVector2D(-10, -5))));
		ExtensionElements.Add(MakeShareable(new FDesignerSurfaceElement(SNew(STransformHandle, this, ETransformDirection::CenterRight), EExtensionLayoutLocation::CenterRight, FVector2D(0, -5))));

		ExtensionElements.Add(MakeShareable(new FDesignerSurfaceElement(SNew(STransformHandle, this, ETransformDirection::BottomLeft), EExtensionLayoutLocation::BottomLeft, FVector2D(-10, 0))));
		ExtensionElements.Add(MakeShareable(new FDesignerSurfaceElement(SNew(STransformHandle, this, ETransformDirection::BottomCenter), EExtensionLayoutLocation::BottomCenter, FVector2D(-5, 0))));
		ExtensionElements.Add(MakeShareable(new FDesignerSurfaceElement(SNew(STransformHandle, this, ETransformDirection::BottomRight), EExtensionLayoutLocation::BottomRight, FVector2D(0, 0))));

		// Build extension widgets for new selection
		for ( TSharedRef<FDesignerExtension>& Ext : DesignerExtensions )
		{
			if ( Ext->CanExtendSelection(Selected) )
			{
				Ext->ExtendSelection(Selected, ExtensionElements);
			}
		}

		// Add Widgets to designer surface
		for ( TSharedRef<FDesignerSurfaceElement>& ExtElement : ExtensionElements )
		{
			ExtensionWidgetCanvas->AddSlot()
				.Position(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateSP(this, &SDesignerView::GetExtensionPosition, ExtElement)))
				.Size(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateSP(this, &SDesignerView::GetExtensionSize, ExtElement)))
				[
					ExtElement->GetWidget()
				];
		}
	}
}

FVector2D SDesignerView::GetExtensionPosition(TSharedRef<FDesignerSurfaceElement> ExtensionElement) const
{
	FWidgetReference SelectedWidget = GetSelectedWidget();

	if ( SelectedWidget.IsValid() )
	{
		FGeometry SelectedWidgetGeometry_RelativeToDesigner;
		FGeometry SelectedWidgetParentGeometry_RelativeToDesigner;

		if ( GetWidgetGeometry(SelectedWidget, SelectedWidgetGeometry_RelativeToDesigner) && GetWidgetParentGeometry(SelectedWidget, SelectedWidgetParentGeometry_RelativeToDesigner) )
		{
			SelectedWidgetGeometry_RelativeToDesigner.AppendTransform(FSlateLayoutTransform(Inverse(CachedGeometry.AbsolutePosition)));
			SelectedWidgetParentGeometry_RelativeToDesigner.AppendTransform(FSlateLayoutTransform(Inverse(CachedGeometry.AbsolutePosition)));

			const FVector2D WidgetPostion_DesignerSpace = SelectedWidgetGeometry_RelativeToDesigner.AbsolutePosition;
			const FVector2D WidgetSize = SelectedWidgetGeometry_RelativeToDesigner.Size * GetPreviewScale();

			const FVector2D ParentPostion_DesignerSpace = SelectedWidgetParentGeometry_RelativeToDesigner.AbsolutePosition;
			const FVector2D ParentSize = SelectedWidgetParentGeometry_RelativeToDesigner.Size * GetPreviewScale();

			FVector2D FinalPosition(0, 0);

			// Get the initial offset based on the location around the selected object.
			switch ( ExtensionElement->GetLocation() )
			{
			case EExtensionLayoutLocation::Absolute:
			{
				FinalPosition = ParentPostion_DesignerSpace;
				break;
			}
			case EExtensionLayoutLocation::TopLeft:
				FinalPosition = WidgetPostion_DesignerSpace;
				break;
			case EExtensionLayoutLocation::TopCenter:
				FinalPosition = WidgetPostion_DesignerSpace + FVector2D(WidgetSize.X * 0.5f, 0);
				break;
			case EExtensionLayoutLocation::TopRight:
				FinalPosition = WidgetPostion_DesignerSpace + FVector2D(WidgetSize.X, 0);
				break;

			case EExtensionLayoutLocation::CenterLeft:
				FinalPosition = WidgetPostion_DesignerSpace + FVector2D(0, WidgetSize.Y * 0.5f);
				break;
			case EExtensionLayoutLocation::CenterCenter:
				FinalPosition = WidgetPostion_DesignerSpace + FVector2D(WidgetSize.X * 0.5f, WidgetSize.Y * 0.5f);
				break;
			case EExtensionLayoutLocation::CenterRight:
				FinalPosition = WidgetPostion_DesignerSpace + FVector2D(WidgetSize.X, WidgetSize.Y * 0.5f);
				break;

			case EExtensionLayoutLocation::BottomLeft:
				FinalPosition = WidgetPostion_DesignerSpace + FVector2D(0, WidgetSize.Y);
				break;
			case EExtensionLayoutLocation::BottomCenter:
				FinalPosition = WidgetPostion_DesignerSpace + FVector2D(WidgetSize.X * 0.5f, WidgetSize.Y);
				break;
			case EExtensionLayoutLocation::BottomRight:
				FinalPosition = WidgetPostion_DesignerSpace + WidgetSize;
				break;
			}

			// Add the alignment offset
			FinalPosition += ParentSize * ExtensionElement->GetAlignment();

			return FinalPosition + ExtensionElement->GetOffset();
		}
	}

	return FVector2D(0, 0);
}

FVector2D SDesignerView::GetExtensionSize(TSharedRef<FDesignerSurfaceElement> ExtensionElement) const
{
	return ExtensionElement->GetWidget()->GetDesiredSize();
}

void SDesignerView::ClearDropPreviews()
{
	for (const auto& DropPreview : DropPreviews)
	{
		if (DropPreview.Parent)
		{
			DropPreview.Parent->RemoveChild(DropPreview.Widget);
		}

		UWidgetBlueprint* BP = GetBlueprint();
		BP->WidgetTree->RemoveWidget(DropPreview.Widget);
	}
	DropPreviews.Empty();
}

UWidgetBlueprint* SDesignerView::GetBlueprint() const
{
	if ( BlueprintEditor.IsValid() )
	{
		UBlueprint* BP = BlueprintEditor.Pin()->GetBlueprintObj();
		return Cast<UWidgetBlueprint>(BP);
	}

	return nullptr;
}

void SDesignerView::Register(TSharedRef<FDesignerExtension> Extension)
{
	Extension->Initialize(this, GetBlueprint());
	DesignerExtensions.Add(Extension);
}

void SDesignerView::OnBlueprintReinstanced()
{
	// Because widget blueprints can contain other widget blueprints, the safe thing to do is to have all
	// designers jettison their previews on the compilation of any widget blueprint.  We do this to prevent
	// having slate widgets that still may reference data in their owner UWidget that has been garbage collected.
	CachedWidgetGeometry.Reset();

	PreviewWidget = nullptr;
	PreviewSurface->SetContent(SNullWidget::NullWidget);
}

SDesignerView::FWidgetHitResult::FWidgetHitResult()
	: Widget()
	, WidgetArranged(SNullWidget::NullWidget, FGeometry())
	, NamedSlot(NAME_None)
{
}

bool SDesignerView::FindWidgetUnderCursor(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TSubclassOf<UWidget> FindType, FWidgetHitResult& HitResult)
{
	//@TODO UMG Make it so you can request dropable widgets only, to find the first parentable.

	FArrangedChildren Children(EVisibility::All);

	PreviewHitTestRoot->SetVisibility(EVisibility::Visible);
	FArrangedWidget WindowWidgetGeometry(PreviewHitTestRoot.ToSharedRef(), MyGeometry);
	LocateWidgetsUnderCursor_Helper(WindowWidgetGeometry, MouseEvent.GetScreenSpacePosition(), Children, true);

	PreviewHitTestRoot->SetVisibility(EVisibility::HitTestInvisible);

	HitResult.Widget = FWidgetReference();
	HitResult.NamedSlot = NAME_None;

	UUserWidget* PreviewUserWidget = BlueprintEditor.Pin()->GetPreview();
	if ( PreviewUserWidget )
	{
		UWidget* WidgetUnderCursor = nullptr;

		// We loop through each hit slate widget until we arrive at one that we can access from the root widget.
		for ( int32 ChildIndex = Children.Num() - 1; ChildIndex >= 0; ChildIndex-- )
		{
			FArrangedWidget& Child = Children.GetInternalArray()[ChildIndex];
			WidgetUnderCursor = PreviewUserWidget->GetWidgetHandle(Child.Widget);
			
			if (WidgetUnderCursor == nullptr)
			{
				continue;
			}

			// Ignore the drop preview widgets when doing widget picking
			if (DropPreviews.ContainsByPredicate([WidgetUnderCursor](const FDropPreview& Preview){ return Preview.Widget == WidgetUnderCursor; }))
			{
				WidgetUnderCursor = nullptr;
				continue;
			}

			// Ignore widgets that don't pass our find widget filter.
			if ( WidgetUnderCursor->GetClass()->IsChildOf(FindType) == false )
			{
				WidgetUnderCursor = nullptr;
				continue;
			}
			
			// We successfully found a widget that's accessible from the root.
			if ( WidgetUnderCursor )
			{
				HitResult.Widget = BlueprintEditor.Pin()->GetReferenceFromPreview(WidgetUnderCursor);
				HitResult.WidgetArranged = Child;

				if ( UUserWidget* UserWidgetUnderCursor = Cast<UUserWidget>(WidgetUnderCursor) )
				{
					// Find the named slot we're over, if any
					for ( int32 SubChildIndex = Children.Num() - 1; SubChildIndex > ChildIndex; SubChildIndex-- )
					{
						FArrangedWidget& SubChild = Children.GetInternalArray()[SubChildIndex];
						UNamedSlot* NamedSlot = Cast<UNamedSlot>(UserWidgetUnderCursor->GetWidgetHandle(SubChild.Widget));
						if ( NamedSlot )
						{
							HitResult.NamedSlot = NamedSlot->GetFName();
							break;
						}
					}
				}

				return true;
			}
		}
	}

	return false;
}

void SDesignerView::ResolvePendingSelectedWidgets()
{
	if ( PendingSelectedWidget.IsValid() )
	{
		TSet<FWidgetReference> SelectedTemplates;
		SelectedTemplates.Add(PendingSelectedWidget);
		BlueprintEditor.Pin()->SelectWidgets(SelectedTemplates, FSlateApplication::Get().GetModifierKeys().IsControlDown());

		PendingSelectedWidget = FWidgetReference();
	}
}

FReply SDesignerView::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SDesignSurface::OnMouseButtonDown(MyGeometry, MouseEvent);

	//TODO UMG Undoable Selection
	FWidgetHitResult HitResult;
	if ( FindWidgetUnderCursor(MyGeometry, MouseEvent, UWidget::StaticClass(), HitResult) )
	{
		SelectedWidgetContextMenuLocation = HitResult.WidgetArranged.Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

		PendingSelectedWidget = HitResult.Widget;

		if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			const TSet<FWidgetReference>& SelectedWidgets = GetSelectedWidgets();

			bool bResolvePendingSelectionImmediately = true;

			if (SelectedWidgets.Num() > 0)
			{
				for (const auto& SelectedWidget : SelectedWidgets)
				{
					if (PendingSelectedWidget == SelectedWidget || PendingSelectedWidget.GetTemplate()->IsChildOf(SelectedWidget.GetTemplate()))
					{
						bResolvePendingSelectionImmediately = false;
						break;
					}
				}
			}

			// If the newly clicked item is a child of the active selection, add it to the pending set of selected 
			// widgets, if they begin dragging we can just move the parent, but if it's not part of the parent set, 
			// we want to immediately begin dragging it.  Also if the currently selected widget is the root widget, 
			// we won't be moving him so just resolve immediately.
			if ( bResolvePendingSelectionImmediately )
			{
				ResolvePendingSelectedWidgets();
			}

			DraggingStartPositionScreenSpace = MouseEvent.GetScreenSpacePosition();
		}
	}
	else
	{
		// Clear the selection immediately if we didn't click anything.
		if(MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			TSet<FWidgetReference> SelectedTemplates;
			BlueprintEditor.Pin()->SelectWidgets(SelectedTemplates, false);
		}
	}

	// Capture mouse for the drag handle and general mouse actions
	return FReply::Handled().PreventThrottling().SetUserFocus(AsShared(), EFocusCause::Mouse).CaptureMouse(AsShared());
}

FReply SDesignerView::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( HasMouseCapture() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		ResolvePendingSelectedWidgets();

		bMovingExistingWidget = false;
		DesignerMessage = EDesignerMessage::None;

		EndTransaction(false);
	}
	else if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{
		if ( !bIsPanning )
		{
			ResolvePendingSelectedWidgets();

			ShowContextMenu(MyGeometry, MouseEvent);
		}
	}

	SDesignSurface::OnMouseButtonUp(MyGeometry, MouseEvent);

	return FReply::Handled().ReleaseMouseCapture();
}

FReply SDesignerView::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.GetCursorDelta().IsZero() )
	{
		return FReply::Unhandled();
	}

	FReply SurfaceHandled = SDesignSurface::OnMouseMove(MyGeometry, MouseEvent);
	if ( SurfaceHandled.IsEventHandled() )
	{
		return SurfaceHandled;
	}

	if ( MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && HasMouseCapture() )
	{
		const TSet<FWidgetReference>& SelectedWidgets = GetSelectedWidgets();

		if (SelectedWidgets.Num() > 0 && !bMovingExistingWidget)
		{
			if ( TransformMode == ETransformMode::Layout )
			{
				bool bIsRootWidgetSelected = false;
				for (const auto& SelectedWidget : SelectedWidgets)
				{
					if (SelectedWidget.GetTemplate()->GetParent() == nullptr)
					{
						bIsRootWidgetSelected = true;
						break;
					}
				}

				if (!bIsRootWidgetSelected)
				{
					bMovingExistingWidget = true;
					//Drag selected widgets
					return FReply::Handled().DetectDrag(AsShared(), EKeys::LeftMouseButton);
				}
			}
			else
			{
				checkSlow(TransformMode == ETransformMode::Render);
				checkSlow(bMovingExistingWidget == false);

				if (SelectedWidgets.Num() == 1)
				{
					BeginTransaction(LOCTEXT("MoveWidgetRT", "Move Widget (Render Transform)"));
				}
				else
				{
					check(SelectedWidgets.Num() > 1);
					BeginTransaction(LOCTEXT("MoveWidgetsRT", "Move Widgets (Render Transform)"));
				}

				for (const auto& SelectedWidget : SelectedWidgets)
				{
					if (UWidget* WidgetPreview = SelectedWidget.GetPreview())
					{
						FGeometry ParentGeometry;
						if (GetWidgetParentGeometry(SelectedWidget, ParentGeometry))
						{
							const FSlateRenderTransform& AbsoluteToLocalTransform = Inverse(ParentGeometry.GetAccumulatedRenderTransform());

							FWidgetTransform RenderTransform = WidgetPreview->RenderTransform;
							RenderTransform.Translation += AbsoluteToLocalTransform.TransformVector(MouseEvent.GetCursorDelta());

							static const FName RenderTransformName(TEXT("RenderTransform"));

							FObjectEditorUtils::SetPropertyValue<UWidget, FWidgetTransform>(WidgetPreview, RenderTransformName, RenderTransform);
							FObjectEditorUtils::SetPropertyValue<UWidget, FWidgetTransform>(SelectedWidget.GetTemplate(), RenderTransformName, RenderTransform);
						}
					}
				}
			}
		}
	}
	

	// Update the hovered widget under the mouse
	auto PinnedBlueprintEditor = BlueprintEditor.Pin();
	FWidgetHitResult HitResult;
	if ( FindWidgetUnderCursor(MyGeometry, MouseEvent, UWidget::StaticClass(), HitResult) )
	{
		PinnedBlueprintEditor->SetHoveredWidget(HitResult.Widget);
	}
	else if (PinnedBlueprintEditor->GetHoveredWidget().IsValid())
	{
		PinnedBlueprintEditor->ClearHoveredWidget();
	}

	return FReply::Unhandled();
}

void SDesignerView::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SDesignSurface::OnMouseEnter(MyGeometry, MouseEvent);

	BlueprintEditor.Pin()->ClearHoveredWidget();
}

void SDesignerView::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SDesignSurface::OnMouseLeave(MouseEvent);

	BlueprintEditor.Pin()->ClearHoveredWidget();
}

FReply SDesignerView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	BlueprintEditor.Pin()->PasteDropLocation = SelectedWidgetContextMenuLocation;

	if ( BlueprintEditor.Pin()->DesignerCommandList->ProcessCommandBindings(InKeyEvent) )
	{
		return FReply::Handled();
	}

	if ( CommandList->ProcessCommandBindings(InKeyEvent) )
	{
		return FReply::Handled();
	}

	const UWidgetDesignerSettings* DesignerSettings = GetDefault<UWidgetDesignerSettings>();

	if ( InKeyEvent.GetKey() == EKeys::Up )
	{
		return NudgeSelectedWidget(FVector2D(0, -DesignerSettings->GridSnapSize));
	}
	else if ( InKeyEvent.GetKey() == EKeys::Down )
	{
		return NudgeSelectedWidget(FVector2D(0, DesignerSettings->GridSnapSize));
	}
	else if ( InKeyEvent.GetKey() == EKeys::Left )
	{
		return NudgeSelectedWidget(FVector2D(-DesignerSettings->GridSnapSize, 0));
	}
	else if ( InKeyEvent.GetKey() == EKeys::Right )
	{
		return NudgeSelectedWidget(FVector2D(DesignerSettings->GridSnapSize, 0));
	}

	return FReply::Unhandled();
}

FReply SDesignerView::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return FReply::Unhandled();
}

FReply SDesignerView::NudgeSelectedWidget(FVector2D Nudge)
{
	for ( const FWidgetReference& WidgetRef : GetSelectedWidgets() )
	{
		if ( WidgetRef.IsValid() )
		{
			UWidget* CurrentTemplateWidget = WidgetRef.GetTemplate();
			UWidget* CurrentPreviewWidget = WidgetRef.GetPreview();

			if (CurrentTemplateWidget && CurrentPreviewWidget)
			{
				UCanvasPanelSlot* TemplateSlot = Cast<UCanvasPanelSlot>(CurrentTemplateWidget->Slot);
				UCanvasPanelSlot* PreviewSlot = Cast<UCanvasPanelSlot>(CurrentPreviewWidget->Slot);

				if ( TemplateSlot && PreviewSlot )
				{
					const UWidgetDesignerSettings* DesignerSettings = GetDefault<UWidgetDesignerSettings>();

					const FVector2D OldPosition = TemplateSlot->GetPosition();
					FVector2D NewPosition = OldPosition + Nudge;

					// Determine the new position aligned to the grid.
					if ( DesignerSettings->GridSnapEnabled )
					{
						if ( Nudge.X != 0 )
						{
							NewPosition.X = ( (int32)NewPosition.X ) - ( ( (int32)NewPosition.X ) % DesignerSettings->GridSnapSize );
						}
						if ( Nudge.Y != 0 )
						{
							NewPosition.Y = ( (int32)NewPosition.Y ) - ( ( (int32)NewPosition.Y ) % DesignerSettings->GridSnapSize );
						}
					}

					// Offset the size by the same amount moved if we're anchoring along that axis.
					FVector2D NewSize = TemplateSlot->GetSize();
					if ( TemplateSlot->GetAnchors().IsStretchedHorizontal() )
					{
						NewSize.X -= NewPosition.X - OldPosition.X;
					}
					if ( TemplateSlot->GetAnchors().IsStretchedVertical() )
					{
						NewSize.Y -= NewPosition.Y - OldPosition.Y;
					}

					FScopedTransaction Transaction(LOCTEXT("Designer_NudgeWidget", "Nudge Widget"));

					TemplateSlot->Modify();

					TemplateSlot->SetPosition(NewPosition);
					TemplateSlot->SetSize(NewSize);
					PreviewSlot->SetPosition(NewPosition);
					PreviewSlot->SetSize(NewSize);
				}
			}
		}
	}

	return FReply::Handled();
}

void SDesignerView::ShowContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	FWidgetBlueprintEditorUtils::CreateWidgetContextMenu(MenuBuilder, BlueprintEditor.Pin().ToSharedRef(), SelectedWidgetContextMenuLocation);

	TSharedPtr<SWidget> MenuContent = MenuBuilder.MakeWidget();

	if ( MenuContent.IsValid() )
	{
		FVector2D SummonLocation = MouseEvent.GetScreenSpacePosition();
		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuContent.ToSharedRef(), SummonLocation, FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	}
}

void SDesignerView::PopulateWidgetGeometryCache(FArrangedWidget& Root)
{
	FArrangedChildren ArrangedChildren(EVisibility::All);
	Root.Widget->ArrangeChildren(Root.Geometry, ArrangedChildren);

	CachedWidgetGeometry.Add(Root.Widget, Root);

	// A widget's children are implicitly Z-ordered from first to last
	for ( int32 ChildIndex = ArrangedChildren.Num() - 1; ChildIndex >= 0; --ChildIndex )
	{
		FArrangedWidget& SomeChild = ArrangedChildren[ChildIndex];
		PopulateWidgetGeometryCache(SomeChild);
	}
}

int32 SDesignerView::HandleEffectsPainting(const FOnPaintHandlerParams& PaintArgs)
{
	const TSet<FWidgetReference>& SelectedWidgets = GetSelectedWidgets();

	// Allow the extensions to paint anything they want.
	for ( const TSharedRef<FDesignerExtension>& Ext : DesignerExtensions )
	{
		Ext->Paint(SelectedWidgets, PaintArgs.Geometry, PaintArgs.ClippingRect, PaintArgs.OutDrawElements, PaintArgs.Layer);
	}

	static const FName SelectionOutlineName("UMGEditor.SelectionOutline");

	static const FLinearColor SelectedTint(0, 1, 0);
	
	const FSlateBrush* SelectionOutlineBrush = FEditorStyle::Get().GetBrush(SelectionOutlineName);
	FVector2D SelectionBrushInflationAmount = FVector2D(16, 16) * FVector2D(SelectionOutlineBrush->Margin.Left, SelectionOutlineBrush->Margin.Top) * ( 1.0f / GetPreviewScale() );

	for ( const FWidgetReference& SelectedWidget : SelectedWidgets )
	{
		TSharedPtr<SWidget> SelectedSlateWidget = SelectedWidget.GetPreviewSlate();

		if ( SelectedSlateWidget.IsValid() )
		{
			TSharedRef<SWidget> Widget = SelectedSlateWidget.ToSharedRef();

			FArrangedWidget ArrangedWidget(SNullWidget::NullWidget, FGeometry());
			FDesignTimeUtils::GetArrangedWidgetRelativeToWindow(Widget, ArrangedWidget);

			// Draw selection effect
			FPaintGeometry SelectionGeometry = ArrangedWidget.Geometry.ToInflatedPaintGeometry(SelectionBrushInflationAmount);

			FSlateDrawElement::MakeBox(
				PaintArgs.OutDrawElements,
				PaintArgs.Layer,
				SelectionGeometry,
				SelectionOutlineBrush,
				PaintArgs.ClippingRect,
				ESlateDrawEffect::None,
				SelectedTint
				);
		}
	}

	const FWidgetReference& HoveredWidget = BlueprintEditor.Pin()->GetHoveredWidget();
	TSharedPtr<SWidget> HoveredSlateWidget = HoveredWidget.GetPreviewSlate();

	// Don't draw the hovered effect if it's also the selected widget
	if ( HoveredSlateWidget.IsValid() && !SelectedWidgets.Contains(HoveredWidget) )
	{
		const FLinearColor HoveredTint(0, 0.5, 1, HoveredWidgetOutlineFade.GetLerp()); // Azure = 0x007FFF

		TSharedRef<SWidget> Widget = HoveredSlateWidget.ToSharedRef();

		FArrangedWidget ArrangedWidget(SNullWidget::NullWidget, FGeometry());
		FDesignTimeUtils::GetArrangedWidgetRelativeToWindow(Widget, ArrangedWidget);

		// Draw hovered effect
		FPaintGeometry HoveredGeometry = ArrangedWidget.Geometry.ToInflatedPaintGeometry(SelectionBrushInflationAmount);

		FSlateDrawElement::MakeBox(
			PaintArgs.OutDrawElements,
			PaintArgs.Layer,
			HoveredGeometry,
			SelectionOutlineBrush,
			PaintArgs.ClippingRect,
			ESlateDrawEffect::None,
			HoveredTint
			);
	}

	return PaintArgs.Layer + 1;
}

void SDesignerView::UpdatePreviewWidget(bool bForceUpdate)
{
	UUserWidget* LatestPreviewWidget = BlueprintEditor.Pin()->GetPreview();

	if ( LatestPreviewWidget != PreviewWidget || bForceUpdate )
	{
		PreviewWidget = LatestPreviewWidget;
		if ( PreviewWidget )
		{
			TSharedRef<SWidget> NewPreviewSlateWidget = PreviewWidget->TakeWidget();
			NewPreviewSlateWidget->SlatePrepass();

			PreviewSlateWidget = NewPreviewSlateWidget;

			// The constraint box for the widget size needs to inside the DPI scaler in order to make sure it too
			// is sized accurately for the size screen it's on.
			TSharedRef<SBox> NewPreviewSizeConstraintBox = SAssignNew(PreviewSizeConstraint, SBox)
				.WidthOverride(this, &SDesignerView::GetPreviewSizeWidth)
				.HeightOverride(this, &SDesignerView::GetPreviewSizeHeight)
				[
					NewPreviewSlateWidget
				];

			PreviewSurface->SetContent(NewPreviewSizeConstraintBox);

			// Notify all selected widgets that they are selected, because there are new preview objects
			// state may have been lost so this will recreate it if the widget does something special when
			// selected.
			for ( const FWidgetReference& WidgetRef : GetSelectedWidgets() )
			{
				if ( WidgetRef.IsValid() )
				{
					WidgetRef.GetPreview()->Select();
				}
			}
		}
		else
		{
			ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoWidgetPreview", "No Widget Preview"))
				]
			];
		}
	}
}

void SDesignerView::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	UUserWidget* DefaultWidget = GetDefaultWidget();
	if ( DefaultWidget && ( DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::CustomOnScreen || DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::DesiredOnScreen ) )
	{
		PreviewAreaConstraint->SetHAlign(HAlign_Left);
		PreviewAreaConstraint->SetVAlign(VAlign_Top);
	}
	else
	{
		PreviewAreaConstraint->SetHAlign(HAlign_Fill);
		PreviewAreaConstraint->SetVAlign(VAlign_Fill);
	}

	// Tick the parent first to update CachedGeometry
	SDesignSurface::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const bool bForceUpdate = false;
	UpdatePreviewWidget(bForceUpdate);

	// Perform an arrange children pass to cache the geometry of all widgets so that we can query it later.
	{
		CachedWidgetGeometry.Reset();
		FArrangedWidget WindowWidgetGeometry(PreviewHitTestRoot.ToSharedRef(), AllottedGeometry);
		PopulateWidgetGeometryCache(WindowWidgetGeometry);
	}

	TArray< TFunction<void()> >& QueuedActions = BlueprintEditor.Pin()->GetQueuedDesignerActions();
	for ( TFunction<void()>& Action : QueuedActions )
	{
		Action();
	}

	if ( QueuedActions.Num() > 0 )
	{
		QueuedActions.Reset();

		CachedWidgetGeometry.Reset();
		FArrangedWidget WindowWidgetGeometry(PreviewHitTestRoot.ToSharedRef(), AllottedGeometry);
		PopulateWidgetGeometryCache(WindowWidgetGeometry);
	}

	// Tick all designer extensions in case they need to update widgets
	for ( const TSharedRef<FDesignerExtension>& Ext : DesignerExtensions )
	{
		Ext->Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	}

	// Compute the origin in absolute space.
	FGeometry RootGeometry = CachedWidgetGeometry.FindChecked(PreviewSurface.ToSharedRef()).Geometry;
	FVector2D AbsoluteOrigin = MakeGeometryWindowLocal(RootGeometry).LocalToAbsolute(FVector2D::ZeroVector);

	TopRuler->SetRuling(AbsoluteOrigin, 1.0f / GetPreviewScale());
	SideRuler->SetRuling(AbsoluteOrigin, 1.0f / GetPreviewScale());

	if ( IsHovered() )
	{
		// Get cursor in absolute window space.
		FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
		CursorPos = MakeGeometryWindowLocal(RootGeometry).LocalToAbsolute(RootGeometry.AbsoluteToLocal(CursorPos));

		TopRuler->SetCursor(CursorPos);
		SideRuler->SetCursor(CursorPos);
	}
	else
	{
		TopRuler->SetCursor(TOptional<FVector2D>());
		SideRuler->SetCursor(TOptional<FVector2D>());
	}

	DefaultWidget = GetDefaultWidget();
	if (DefaultWidget)
	{
		const bool bNeedDesiredSize = 
			DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::Desired || 
			DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::DesiredOnScreen;

		if ( bNeedDesiredSize && PreviewWidget )
		{
			TSharedPtr<SWidget> CachedWidget = PreviewWidget->GetCachedWidget();
			if ( CachedWidget.IsValid() )
			{
				CachedPreviewDesiredSize = CachedWidget->GetDesiredSize();
			}
		}
	}
}

FReply SDesignerView::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	typedef FSelectedWidgetDragDropOp::FDraggingWidgetReference FDragWidget;

	SDesignSurface::OnDragDetected(MyGeometry, MouseEvent);

	const TSet<FWidgetReference>& SelectedWidgets = GetSelectedWidgets();

	if (SelectedWidgets.Num() > 0)
	{
		TArray<FDragWidget> DraggingWidgetCandidates;

		// Clear any pending selected widgets, the user has already decided what widget they want.
		PendingSelectedWidget = FWidgetReference();

		for (const auto& SelectedWidget : SelectedWidgets)
		{
			// Determine The offset to keep the widget from the mouse while dragging
			FArrangedWidget ArrangedWidget(SNullWidget::NullWidget, FGeometry());
			FDesignTimeUtils::GetArrangedWidget(SelectedWidget.GetPreview()->GetCachedWidget().ToSharedRef(), ArrangedWidget);
			SelectedWidgetContextMenuLocation = ArrangedWidget.Geometry.AbsoluteToLocal(DraggingStartPositionScreenSpace);

			FDragWidget DraggingWidget;
			DraggingWidget.Widget = SelectedWidget;
			DraggingWidget.DraggedOffset = SelectedWidgetContextMenuLocation;

			DraggingWidgetCandidates.Add(DraggingWidget);
		}

		TArray<FDragWidget> DraggingWidgets;

		for (const auto& Candidate : DraggingWidgetCandidates)
		{
			// check the parent chain of each dragged widget and ignore those that are children of other dragged widgets
			bool bIsChild = false;
			for (auto CursorPtr = Candidate.Widget.GetTemplate()->GetParent(); CursorPtr != nullptr; CursorPtr = CursorPtr->GetParent())
			{
				if (DraggingWidgetCandidates.ContainsByPredicate([CursorPtr](const FDragWidget& W){ return W.Widget.GetTemplate() == CursorPtr; }))
				{
					bIsChild = true;
					break;
				}
			}

			if (!bIsChild)
			{
				DraggingWidgets.Add(Candidate);
			}
		}

		ClearExtensionWidgets();

		return FReply::Handled().BeginDragDrop(FSelectedWidgetDragDropOp::New(BlueprintEditor.Pin(), DraggingWidgets));
	}

	return FReply::Unhandled();
}

void SDesignerView::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	SDesignSurface::OnDragEnter(MyGeometry, DragDropEvent);

	BlueprintEditor.Pin()->ClearHoveredWidget();

	//@TODO UMG Drop Feedback
}

void SDesignerView::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	SDesignSurface::OnDragLeave(DragDropEvent);

	BlueprintEditor.Pin()->ClearHoveredWidget();

	TSharedPtr<FDecoratedDragDropOp> DecoratedDragDropOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
	if ( DecoratedDragDropOp.IsValid() )
	{
		DecoratedDragDropOp->SetCursorOverride(TOptional<EMouseCursor::Type>());
		DecoratedDragDropOp->ResetToDefaultToolTip();
	}
	
	ClearDropPreviews();
}

FReply SDesignerView::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	SDesignSurface::OnDragOver(MyGeometry, DragDropEvent);

	ClearDropPreviews();
	
	const bool bIsPreview = true;
	ProcessDropAndAddWidget(MyGeometry, DragDropEvent, bIsPreview);

	if ( DropPreviews.Num() > 0 )
	{
		//@TODO UMG Drop Feedback
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SDesignerView::ProcessDropAndAddWidget(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, const bool bIsPreview)
{
	// In order to prevent the GetWidgetAtCursor code from picking the widgets we're about to move, we need to mark them
	// as the drop preview widgets before any other code can run.
	TSharedPtr<FSelectedWidgetDragDropOp> SelectedDragDropOp = DragDropEvent.GetOperationAs<FSelectedWidgetDragDropOp>();
	if ( SelectedDragDropOp.IsValid() )
	{
		DropPreviews.Empty();
		
		for (const auto& DraggedWidget : SelectedDragDropOp->DraggedWidgets)
		{
			FDropPreview DropPreview;
			DropPreview.Parent = nullptr;
			DropPreview.Widget = DraggedWidget.Preview;
			DropPreviews.Add(DropPreview);
		}
	}

	ClearDropPreviews();

	UWidget* Target = nullptr;

	FWidgetHitResult HitResult;
	if ( FindWidgetUnderCursor(MyGeometry, DragDropEvent, UPanelWidget::StaticClass(), HitResult) )
	{
		Target = bIsPreview ? HitResult.Widget.GetPreview() : HitResult.Widget.GetTemplate();
	}

	FGeometry WidgetUnderCursorGeometry = HitResult.WidgetArranged.Geometry;
	UWidgetBlueprint* BP = GetBlueprint();

	TSharedPtr<FWidgetTemplateDragDropOp> TemplateDragDropOp = DragDropEvent.GetOperationAs<FWidgetTemplateDragDropOp>();
	if ( TemplateDragDropOp.IsValid() )
	{
		BlueprintEditor.Pin()->SetHoveredWidget(HitResult.Widget);

		TemplateDragDropOp->SetCursorOverride(TOptional<EMouseCursor::Type>());

		// If there's no root widget go ahead and add the widget into the root slot.
		if ( BP->WidgetTree->RootWidget == nullptr )
		{
			FScopedTransaction Transaction(LOCTEXT("Designer_AddWidget", "Add Widget"));

			if ( !bIsPreview )
			{
				BP->WidgetTree->SetFlags(RF_Transactional);
				BP->WidgetTree->Modify();
			}

			// TODO UMG This method isn't great, maybe the user widget should just be a canvas.

			// Add it to the root if there are no other widgets to add it to.
			UWidget* Widget = TemplateDragDropOp->Template->Create(BP->WidgetTree);
			Widget->SetDesignerFlags(BlueprintEditor.Pin()->GetCurrentDesignerFlags());

			BP->WidgetTree->RootWidget = Widget;

			FDropPreview DropPreview;
			DropPreview.Widget = Widget;
			DropPreview.Parent = nullptr;
			DropPreviews.Add(DropPreview);

			if ( bIsPreview )
			{
				Transaction.Cancel();
			}

			return;
		}
		// If there's already a root widget we need to try and place our widget into a parent widget that we've picked against
		else if ( Target && Target->IsA(UPanelWidget::StaticClass()) )
		{
			UPanelWidget* Parent = Cast<UPanelWidget>(Target);

			FScopedTransaction Transaction(LOCTEXT("Designer_AddWidget", "Add Widget"));

			// If this isn't a preview operation we need to modify a few things to properly undo the operation.
			if ( !bIsPreview )
			{
				Parent->SetFlags(RF_Transactional);
				Parent->Modify();

				BP->WidgetTree->SetFlags(RF_Transactional);
				BP->WidgetTree->Modify();
			}

			// Construct the widget and mark it for design time rendering.
			UWidget* Widget = TemplateDragDropOp->Template->Create(BP->WidgetTree);
			Widget->SetDesignerFlags(BlueprintEditor.Pin()->GetCurrentDesignerFlags());

			// Determine local position inside the parent widget and add the widget to the slot.
			FVector2D LocalPosition = WidgetUnderCursorGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
			if ( UPanelSlot* Slot = Parent->AddChild(Widget) )
			{
				// Special logic for canvas panel slots.
				if ( UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot) )
				{
					// HACK UMG - This seems like a bad idea to call TakeWidget
					TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
					SlateWidget->SlatePrepass();
					const FVector2D& WidgetDesiredSize = SlateWidget->GetDesiredSize();

					static const FVector2D MinimumDefaultSize(100, 40);
					FVector2D LocalSize = FVector2D(FMath::Max(WidgetDesiredSize.X, MinimumDefaultSize.X), FMath::Max(WidgetDesiredSize.Y, MinimumDefaultSize.Y));

					const UWidgetDesignerSettings* DesignerSettings = GetDefault<UWidgetDesignerSettings>();
					if ( DesignerSettings->GridSnapEnabled )
					{
						LocalPosition.X = ( (int32)LocalPosition.X ) - ( ( (int32)LocalPosition.X ) % DesignerSettings->GridSnapSize );
						LocalPosition.Y = ( (int32)LocalPosition.Y ) - ( ( (int32)LocalPosition.Y ) % DesignerSettings->GridSnapSize );
					}

					CanvasSlot->SetPosition(LocalPosition);
					CanvasSlot->SetSize(LocalSize);
				}

				FDropPreview DropPreview;
				DropPreview.Widget = Widget;
				DropPreview.Parent = Parent;
				DropPreviews.Add(DropPreview);

				if ( bIsPreview )
				{
					Transaction.Cancel();
				}

				return;
			}
			else
			{
				TemplateDragDropOp->SetCursorOverride(EMouseCursor::SlashedCircle);

				// TODO UMG ERROR Slot can not be created because maybe the max children has been reached.
				//          Maybe we can traverse the hierarchy and add it to the first parent that will accept it?
			}

			if ( bIsPreview )
			{
				Transaction.Cancel();
			}
		}
		else
		{
			TemplateDragDropOp->SetCursorOverride(EMouseCursor::SlashedCircle);
		}
	}

	// Attempt to deal with moving widgets from a drag operation.
	if (SelectedDragDropOp.IsValid() && SelectedDragDropOp->DraggedWidgets.Num() > 0)
	{
		SelectedDragDropOp->SetCursorOverride(TOptional<EMouseCursor::Type>());

		FText TransactionText;
		if (SelectedDragDropOp->DraggedWidgets.Num() == 1)
		{
			TransactionText = LOCTEXT("Designer_MoveWidget", "Move Widget");
		}
		else
		{
			TransactionText = LOCTEXT("Designer_MoveWidgets", "Move Widgets");
		}

		FScopedTransaction Transaction(TransactionText);
		bool bWidgetMoved = false;

		for (auto& DraggedWidget : SelectedDragDropOp->DraggedWidgets)
		{
			// If they've pressed alt, and we were staying in the parent, disable that
			// and adjust the designer message to no longer warn.
			if (DragDropEvent.IsAltDown() && DraggedWidget.bStayingInParent)
			{
				DraggedWidget.bStayingInParent = false;
				DesignerMessage = EDesignerMessage::None;
			}

			// If we're staying in the parent we started in, replace the parent found under the cursor with
			// the original one, also update the arranged widget data so that our layout calculations are accurate.
			if (DraggedWidget.bStayingInParent)
			{
				DesignerMessage = EDesignerMessage::MoveFromParent;

				WidgetUnderCursorGeometry = GetDesignerGeometry();
				if (GetWidgetGeometry(DraggedWidget.ParentWidget, WidgetUnderCursorGeometry))
				{
					Target = bIsPreview ? DraggedWidget.ParentWidget.GetPreview() : DraggedWidget.ParentWidget.GetTemplate();
				}
			}

			FWidgetReference TargetReference = bIsPreview ? BlueprintEditor.Pin()->GetReferenceFromPreview(Target) : BlueprintEditor.Pin()->GetReferenceFromTemplate(Target);
			BlueprintEditor.Pin()->SetHoveredWidget(TargetReference);

			// If the widget being hovered over is a panel, attempt to place it into that panel.
			if (Target && Target->IsA(UPanelWidget::StaticClass()))
			{
				bWidgetMoved = true;
				UPanelWidget* NewParent = Cast<UPanelWidget>(Target);

				UWidget* Widget = bIsPreview ? DraggedWidget.Preview : DraggedWidget.Template;

				if (ensure(Widget))
				{
					bool bIsChangingParent = Widget->GetParent() != NewParent;

					check(Widget->GetParent() != nullptr || bIsChangingParent);

					// If this isn't a preview operation we need to modify a few things to properly undo the operation.
					if (!bIsPreview)
					{
						if (bIsChangingParent)
						{
							NewParent->SetFlags(RF_Transactional);
							NewParent->Modify();

							BP->WidgetTree->SetFlags(RF_Transactional);
							BP->WidgetTree->Modify();
						}

						Widget->Modify();
					}

					if (Widget->GetParent() && bIsChangingParent)
					{
						if (!bIsPreview)
						{
							Widget->GetParent()->Modify();
						}

						Widget->GetParent()->RemoveChild(Widget);
					}

					FVector2D ScreenSpacePosition = DragDropEvent.GetScreenSpacePosition();

					const UWidgetDesignerSettings* DesignerSettings = GetDefault<UWidgetDesignerSettings>();
					bool bGridSnapX, bGridSnapY;
					bGridSnapX = bGridSnapY = DesignerSettings->GridSnapEnabled;

					// As long as shift is pressed and we're staying in the same parent,
					// allow the user to lock the movement to a specific axis.
					const bool bLockToAxis =
						FSlateApplication::Get().GetModifierKeys().IsShiftDown() &&
						DraggedWidget.bStayingInParent;

					if (bLockToAxis)
					{
						// Choose the largest axis of movement as the primary axis to lock to.
						FVector2D DragDelta = ScreenSpacePosition - DraggingStartPositionScreenSpace;
						if (FMath::Abs(DragDelta.X) > FMath::Abs(DragDelta.Y))
						{
							// Lock to X Axis
							ScreenSpacePosition.Y = DraggingStartPositionScreenSpace.Y;
							bGridSnapY = false;
						}
						else
						{
							// Lock To Y Axis
							ScreenSpacePosition.X = DraggingStartPositionScreenSpace.X;
							bGridSnapX = false;
						}
					}

					FVector2D LocalPosition = WidgetUnderCursorGeometry.AbsoluteToLocal(ScreenSpacePosition);

					UPanelSlot* Slot;
					if (bIsChangingParent)
					{
						Slot = NewParent->AddChild(Widget);
					}
					else
					{
						check(Widget->GetParent()->GetChildIndex(Widget) != INDEX_NONE);

						Slot = Widget->GetParent()->GetSlots()[Widget->GetParent()->GetChildIndex(Widget)];
					}

					if (Slot != nullptr)
					{
						FVector2D NewPosition = LocalPosition - DraggedWidget.DraggedOffset;

						// Perform grid snapping on X and Y if we need to.
						if (bGridSnapX)
						{
							NewPosition.X = ((int32)NewPosition.X) - (((int32)NewPosition.X) % DesignerSettings->GridSnapSize);
						}

						if (bGridSnapY)
						{
							NewPosition.Y = ((int32)NewPosition.Y) - (((int32)NewPosition.Y) % DesignerSettings->GridSnapSize);
						}

						// HACK UMG: In order to correctly drop items into the canvas that have a non-zero anchor,
						// we need to know the layout information after slate has performed a pre-pass.  So we have
						// to rebase the layout and reinterpret the new position based on anchor point layout data.
						// This should be pulled out into an extension of some kind so that this can be fixed for
						// other widgets as well that may need to do work like this.
						if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
						{
							if (bIsPreview)
							{
								FWidgetBlueprintEditorUtils::ImportPropertiesFromText(Slot, DraggedWidget.ExportedSlotProperties);

								CanvasSlot->SaveBaseLayout();
								CanvasSlot->SetDesiredPosition(NewPosition);
								CanvasSlot->RebaseLayout();

								FWidgetBlueprintEditorUtils::ExportPropertiesToText(Slot, DraggedWidget.ExportedSlotProperties);
							}
							else
							{
								FWidgetBlueprintEditorUtils::ImportPropertiesFromText(Slot, DraggedWidget.ExportedSlotProperties);
							}
						}
						else
						{
							FWidgetBlueprintEditorUtils::ImportPropertiesFromText(Slot, DraggedWidget.ExportedSlotProperties);
						}

						FDropPreview DropPreview;
						DropPreview.Widget = Widget;
						DropPreview.Parent = NewParent;
						DropPreviews.Add(DropPreview);
					}
					else
					{
						SelectedDragDropOp->SetCursorOverride(EMouseCursor::SlashedCircle);

						// TODO UMG ERROR Slot can not be created because maybe the max children has been reached.
						//          Maybe we can traverse the hierarchy and add it to the first parent that will accept it?
					}
				}
			}
			else
			{
				SelectedDragDropOp->SetCursorOverride(EMouseCursor::SlashedCircle);
			}
		}

		if (bIsPreview || !bWidgetMoved)
		{
			Transaction.Cancel();
		}
	}
}

FReply SDesignerView::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	SDesignSurface::OnDrop(MyGeometry, DragDropEvent);

	bMovingExistingWidget = false;

	ClearDropPreviews();
	
	const bool bIsPreview = false;
	ProcessDropAndAddWidget(MyGeometry, DragDropEvent, bIsPreview);

	if (DropPreviews.Num() > 0)
	{
		UWidgetBlueprint* BP = GetBlueprint();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

		TSet<FWidgetReference> SelectedTemplates;

		for (const auto& DropPreview : DropPreviews)
		{
			SelectedTemplates.Add(BlueprintEditor.Pin()->GetReferenceFromTemplate(DropPreview.Widget));
		}

		BlueprintEditor.Pin()->SelectWidgets(SelectedTemplates, false);

		// Regenerate extension widgets now that we've finished moving or placing the widget.
		CreateExtensionWidgetsForSelection();

		DesignerMessage = EDesignerMessage::None;

		DropPreviews.Empty();
		return FReply::Handled();
	}

	DesignerMessage = EDesignerMessage::None;
	
	return FReply::Unhandled();
}

FText SDesignerView::GetResolutionText(int32 Width, int32 Height, const FString& AspectRatio) const
{
	FInternationalization& I18N = FInternationalization::Get();
	FFormatNamedArguments Args;
	Args.Add(TEXT("Width"), FText::AsNumber(Width, nullptr, I18N.GetInvariantCulture()));
	Args.Add(TEXT("Height"), FText::AsNumber(Height, nullptr, I18N.GetInvariantCulture()));
	Args.Add(TEXT("AspectRatio"), FText::FromString(AspectRatio));

	return FText::Format(LOCTEXT("CommonResolutionFormat", "{Width} x {Height} ({AspectRatio})"), Args);
}

FText SDesignerView::GetCurrentResolutionText() const
{
	return GetResolutionText(PreviewWidth, PreviewHeight, PreviewAspectRatio);
}

FText SDesignerView::GetCurrentDPIScaleText() const
{
	FInternationalization& I18N = FInternationalization::Get();

	FNumberFormattingOptions Options;
	Options.MinimumIntegralDigits = 1;
	Options.MaximumFractionalDigits = 2;
	Options.MinimumFractionalDigits = 1;

	FText DPIString = FText::AsNumber(GetPreviewDPIScale(), &Options, I18N.GetInvariantCulture());
	return FText::Format(LOCTEXT("CurrentDPIScaleFormat", "DPI Scale {0}"), DPIString);
}

FSlateColor SDesignerView::GetResolutionTextColorAndOpacity() const
{
	return FLinearColor(1, 1, 1, 1.25f - ResolutionTextFade.GetLerp());
}

EVisibility SDesignerView::GetResolutionTextVisibility() const
{
	// If we're using a custom design time size, don't bother showing the resolution
	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		const bool bScreenlessSizing =
			DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::Custom ||
			DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::Desired;

		if ( bScreenlessSizing )
		{
			return EVisibility::Collapsed;
		}
	}

	return EVisibility::SelfHitTestInvisible;
}

EVisibility SDesignerView::PIENotification() const
{
	if ( GEditor->bIsSimulatingInEditor || GEditor->PlayWorld != nullptr )
	{
		return EVisibility::HitTestInvisible;
	}

	return EVisibility::Hidden;
}

FReply SDesignerView::HandleDPISettingsClicked()
{
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer("Project", "Engine", "UI");

	return FReply::Handled();
}

void SDesignerView::HandleOnCommonResolutionSelected(int32 Width, int32 Height, FString AspectRatio)
{
	PreviewWidth = Width;
	PreviewHeight = Height;
	PreviewAspectRatio = AspectRatio;

	GConfig->SetInt(*ConfigSectionName, TEXT("PreviewWidth"), Width, GEditorPerProjectIni);
	GConfig->SetInt(*ConfigSectionName, TEXT("PreviewHeight"), Height, GEditorPerProjectIni);
	GConfig->SetString(*ConfigSectionName, TEXT("PreviewAspectRatio"), *AspectRatio, GEditorPerProjectIni);

	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		// If we using custom or desired design time sizes and the user picks a screen size, he must
		// want to also change his visualization to be custom on screen or desired on screen, doesn't
		// make sense to change it otherwise as it would have no effect.
		if ( DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::Custom )
		{
			DefaultWidget->DesignSizeMode = EDesignPreviewSizeMode::CustomOnScreen;
		}
		else if ( DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::Desired )
		{
			DefaultWidget->DesignSizeMode = EDesignPreviewSizeMode::DesiredOnScreen;
		}

		MarkDesignModifed(/*bRequiresRecompile*/ false);
	}

	ResolutionTextFade.Play( this->AsShared() );
}

bool SDesignerView::HandleIsCommonResolutionSelected(int32 Width, int32 Height) const
{
	// If we're using a custom design time size, none of the other resolutions should appear selected, even if they match.
	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		if ( DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::Custom || DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::Desired )
		{
			return false;
		}
	}
	
	return ( Width == PreviewWidth ) && ( Height == PreviewHeight );
}

void SDesignerView::AddScreenResolutionSection(FMenuBuilder& MenuBuilder, const TArray<FPlayScreenResolution>& Resolutions, const FText& SectionName)
{
	MenuBuilder.BeginSection(NAME_None, SectionName);
	{
		for ( auto Iter = Resolutions.CreateConstIterator(); Iter; ++Iter )
		{
			// Actions for the resolution menu entry
			FExecuteAction OnResolutionSelected = FExecuteAction::CreateRaw(this, &SDesignerView::HandleOnCommonResolutionSelected, Iter->Width, Iter->Height, Iter->AspectRatio);
			FIsActionChecked OnIsResolutionSelected = FIsActionChecked::CreateRaw(this, &SDesignerView::HandleIsCommonResolutionSelected, Iter->Width, Iter->Height);
			FUIAction Action(OnResolutionSelected, FCanExecuteAction(), OnIsResolutionSelected);

			MenuBuilder.AddMenuEntry(FText::FromString(Iter->Description), GetResolutionText(Iter->Width, Iter->Height, Iter->AspectRatio), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::Check);
		}
	}
	MenuBuilder.EndSection();
}

TOptional<int32> SDesignerView::GetCustomResolutionWidth() const
{
	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		return DefaultWidget->DesignTimeSize.X;
	}

	return 1;
}

TOptional<int32> SDesignerView::GetCustomResolutionHeight() const
{
	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		return DefaultWidget->DesignTimeSize.Y;
	}

	return 1;
}

void SDesignerView::OnCustomResolutionWidthChanged(int32 InValue)
{
	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		DefaultWidget->DesignTimeSize.X = InValue;
		MarkDesignModifed(/*bRequiresRecompile*/ false);
	}
}

void SDesignerView::OnCustomResolutionHeightChanged(int32 InValue)
{
	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		DefaultWidget->DesignTimeSize.Y = InValue;
		MarkDesignModifed(/*bRequiresRecompile*/ false);
	}
}

EVisibility SDesignerView::GetCustomResolutionEntryVisibility() const
{
	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		const bool bCustomSizing =
			DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::Custom ||
			DefaultWidget->DesignSizeMode == EDesignPreviewSizeMode::CustomOnScreen;

		return bCustomSizing ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Collapsed;
}

UUserWidget* SDesignerView::GetDefaultWidget() const
{
	TSharedPtr<FWidgetBlueprintEditor> BPEd = BlueprintEditor.Pin();
	if ( UUserWidget* Default = BPEd->GetWidgetBlueprintObj()->GeneratedClass->GetDefaultObject<UUserWidget>() )
	{
		return Default;
	}

	return nullptr;
}

TSharedRef<SWidget> SDesignerView::GetResolutionsMenu()
{
	const ULevelEditorPlaySettings* PlaySettings = GetDefault<ULevelEditorPlaySettings>();
	FMenuBuilder MenuBuilder(true, nullptr);

	// Add the normal set of resolution options.
	AddScreenResolutionSection(MenuBuilder, PlaySettings->PhoneScreenResolutions, LOCTEXT("CommonPhonesSectionHeader", "Phones"));
	AddScreenResolutionSection(MenuBuilder, PlaySettings->TabletScreenResolutions, LOCTEXT("CommonTabletsSectionHeader", "Tablets"));
	AddScreenResolutionSection(MenuBuilder, PlaySettings->LaptopScreenResolutions, LOCTEXT("CommonLaptopsSectionHeader", "Laptops"));
	AddScreenResolutionSection(MenuBuilder, PlaySettings->MonitorScreenResolutions, LOCTEXT("CommoMonitorsSectionHeader", "Monitors"));
	AddScreenResolutionSection(MenuBuilder, PlaySettings->TelevisionScreenResolutions, LOCTEXT("CommonTelevesionsSectionHeader", "Televisions"));

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SDesignerView::GetScreenSizingFillMenu()
{
	const ULevelEditorPlaySettings* PlaySettings = GetDefault<ULevelEditorPlaySettings>();
	FMenuBuilder MenuBuilder(true, nullptr);

	CreateScreenFillEntry(MenuBuilder, EDesignPreviewSizeMode::FillScreen);
	CreateScreenFillEntry(MenuBuilder, EDesignPreviewSizeMode::Custom);
	CreateScreenFillEntry(MenuBuilder, EDesignPreviewSizeMode::CustomOnScreen);
	CreateScreenFillEntry(MenuBuilder, EDesignPreviewSizeMode::Desired);
	CreateScreenFillEntry(MenuBuilder, EDesignPreviewSizeMode::DesiredOnScreen);

	return MenuBuilder.MakeWidget();
}

void SDesignerView::CreateScreenFillEntry(FMenuBuilder& MenuBuilder, EDesignPreviewSizeMode SizeMode)
{
	const static UEnum* PreviewSizeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EDesignPreviewSizeMode"), true);

	// Add desired size option
	FUIAction DesiredSizeAction(
		FExecuteAction::CreateRaw(this, &SDesignerView::OnScreenFillRuleSelected, SizeMode),
		FCanExecuteAction(),
		FIsActionChecked::CreateRaw(this, &SDesignerView::GetIsScreenFillRuleSelected, SizeMode));

	FText EntryText = PreviewSizeEnum->GetEnumText((int32)SizeMode);
	MenuBuilder.AddMenuEntry(EntryText, FText::GetEmpty(), FSlateIcon(), DesiredSizeAction, NAME_None, EUserInterfaceActionType::Check);
}

FText SDesignerView::GetScreenSizingFillText() const
{
	const static UEnum* PreviewSizeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EDesignPreviewSizeMode"), true);

	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		return PreviewSizeEnum->GetEnumText((int32)DefaultWidget->DesignSizeMode);
	}

	return FText::GetEmpty();
}

bool SDesignerView::GetIsScreenFillRuleSelected(EDesignPreviewSizeMode SizeMode) const
{
	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		return DefaultWidget->DesignSizeMode == SizeMode;
	}

	return false;
}

void SDesignerView::OnScreenFillRuleSelected(EDesignPreviewSizeMode SizeMode)
{
	if ( UUserWidget* DefaultWidget = GetDefaultWidget() )
	{
		DefaultWidget->DesignSizeMode = SizeMode;
		MarkDesignModifed(/*bRequiresRecompile*/ false);
	}
}

void SDesignerView::BeginTransaction(const FText& SessionName)
{
	if ( ScopedTransaction == nullptr )
	{
		ScopedTransaction = new FScopedTransaction(SessionName);

		for ( const FWidgetReference& SelectedWidget : GetSelectedWidgets() )
		{
			if ( SelectedWidget.IsValid() )
			{
				SelectedWidget.GetPreview()->Modify();
				SelectedWidget.GetTemplate()->Modify();
			}
		}
	}
}

bool SDesignerView::InTransaction() const
{
	return ScopedTransaction != nullptr;
}

void SDesignerView::EndTransaction(bool bCancel)
{
	if ( ScopedTransaction != nullptr )
	{
		if ( bCancel )
		{
			ScopedTransaction->Cancel();
		}

		delete ScopedTransaction;
		ScopedTransaction = nullptr;
	}
}

FReply SDesignerView::HandleZoomToFitClicked()
{
	ZoomToFit(/*bInstantZoom*/ false);
	return FReply::Handled();
}

EVisibility SDesignerView::GetRulerVisibility() const
{
	return EVisibility::Visible;
}

#undef LOCTEXT_NAMESPACE
