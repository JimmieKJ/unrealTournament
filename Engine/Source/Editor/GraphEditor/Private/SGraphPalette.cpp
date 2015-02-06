// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphPalette.h"

#include "EditorWidgets.h"

//#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"

#include "IDocumentation.h"
#include "SInlineEditableTextBlock.h"

void SGraphPaletteItem::Construct(const FArguments& InArgs, FCreateWidgetForActionData* const InCreateData)
{
	FSlateFontInfo NameFont = FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10);

	check(InCreateData->Action.IsValid());

	TSharedPtr<FEdGraphSchemaAction> GraphAction = InCreateData->Action;
	ActionPtr = InCreateData->Action;

	// Find icons
	const FSlateBrush* IconBrush = FEditorStyle::GetBrush(TEXT("NoBrush"));
	FSlateColor IconColor = FSlateColor::UseForeground();
	FString ToolTip = GraphAction->TooltipDescription;
	FText IconToolTip = FText::FromString(ToolTip);
	bool bIsReadOnly = false;

	TSharedRef<SWidget> IconWidget = CreateIconWidget( IconToolTip, IconBrush, IconColor );
	TSharedRef<SWidget> NameSlotWidget = CreateTextSlotWidget( NameFont, InCreateData, bIsReadOnly );

	// Create the actual widget
	this->ChildSlot
	[
		SNew(SHorizontalBox)
		// Icon slot
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			IconWidget
		]
		// Name slot
		+SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		.Padding(3,0)
		[
			NameSlotWidget
		]
	];
}

FReply SGraphPaletteItem::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FGraphEditorDragDropAction> GraphDropOp = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();
	if (GraphDropOp.IsValid())
	{
		GraphDropOp->DroppedOnAction(ActionPtr.Pin().ToSharedRef());
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SGraphPaletteItem::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FGraphEditorDragDropAction> GraphDropOp = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();
	if (GraphDropOp.IsValid())
	{
		GraphDropOp->SetHoveredAction( ActionPtr.Pin() );
	}
}

void SGraphPaletteItem::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FGraphEditorDragDropAction> GraphDropOp = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();
	if (GraphDropOp.IsValid())
	{
		GraphDropOp->SetHoveredAction( NULL );
	}
}

FReply SGraphPaletteItem::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( MouseButtonDownDelegate.IsBound() )
	{
		if( MouseButtonDownDelegate.Execute( ActionPtr ) == true )
		{
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

TSharedRef<SWidget> SGraphPaletteItem::CreateIconWidget(const FText& IconToolTip, const FSlateBrush* IconBrush, const FSlateColor& IconColor)
{
	return SNew(SImage)
		.ToolTipText(IconToolTip)
		.Image(IconBrush)
		.ColorAndOpacity(IconColor);
}

TSharedRef<SWidget> SGraphPaletteItem::CreateIconWidget(const FText& IconToolTip, const FSlateBrush* IconBrush, const FSlateColor& IconColor, const FString& DocLink, const FString& DocExcerpt)
{
	TSharedPtr<SToolTip> ToolTipWidget = IDocumentation::Get()->CreateToolTip(IconToolTip, NULL, DocLink, DocExcerpt);

	return SNew(SImage)
		.ToolTip(ToolTipWidget)
		.Image( IconBrush )
		.ColorAndOpacity(IconColor);
}

TSharedRef<SWidget> SGraphPaletteItem::CreateTextSlotWidget( const FSlateFontInfo& NameFont, FCreateWidgetForActionData* const InCreateData, bool bIsReadOnly )
{
	TSharedPtr< SWidget > DisplayWidget;

	// Copy the mouse delegate binding if we want it
	if( InCreateData->bHandleMouseButtonDown )
	{
		MouseButtonDownDelegate = InCreateData->MouseButtonDownDelegate;
	}

	// If the creation data says read only, then it must be read only
	if(InCreateData->bIsReadOnly)
	{
		bIsReadOnly = true;
	}

	InlineRenameWidget =
		SAssignNew(DisplayWidget, SInlineEditableTextBlock)
		.Text(this, &SGraphPaletteItem::GetDisplayText)
		.Font(NameFont)
		.HighlightText(InCreateData->HighlightText)
		.ToolTipText(this, &SGraphPaletteItem::GetItemTooltip)
		.OnTextCommitted(this, &SGraphPaletteItem::OnNameTextCommitted)
		.OnVerifyTextChanged(this, &SGraphPaletteItem::OnNameTextVerifyChanged)
		.IsSelected( InCreateData->IsRowSelectedDelegate )
		.IsReadOnly( bIsReadOnly );

	if(!bIsReadOnly)
	{
		InCreateData->OnRenameRequest->BindSP( InlineRenameWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode );
	}

	return DisplayWidget.ToSharedRef();
}

bool SGraphPaletteItem::OnNameTextVerifyChanged(const FText& InNewText, FText& OutErrorMessage)
{
	return true;
}

void SGraphPaletteItem::OnNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
}

FText SGraphPaletteItem::GetDisplayText() const
{
	return ActionPtr.Pin()->MenuDescription;
}

FText SGraphPaletteItem::GetItemTooltip() const
{
	return ActionPtr.Pin()->MenuDescription;
}


//////////////////////////////////////////////////////////////////////////


void SGraphPalette::Construct(const FArguments& InArgs)
{
	// Create the asset discovery indicator
	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::LoadModuleChecked<FEditorWidgetsModule>("EditorWidgets");
	TSharedRef<SWidget> AssetDiscoveryIndicator = EditorWidgetsModule.CreateAssetDiscoveryIndicator(EAssetDiscoveryIndicatorScaleMode::Scale_Vertical);

	this->ChildSlot
		[
			SNew(SBorder)
			.Padding(2.0f)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				// Content list
				+SVerticalBox::Slot()
					[
						SNew(SOverlay)

						+SOverlay::Slot()
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Fill)
						[
							SAssignNew(GraphActionMenu, SGraphActionMenu)
							.OnActionDragged(this, &SGraphPalette::OnActionDragged)
							.OnCreateWidgetForAction(this, &SGraphPalette::OnCreateWidgetForAction)
							.OnCollectAllActions(this, &SGraphPalette::CollectAllActions)
							.AutoExpandActionMenu(InArgs._AutoExpandActionMenu)
						]

						+SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Bottom)
							.Padding(FMargin(24, 0, 24, 0))
							[
								// Asset discovery indicator
								AssetDiscoveryIndicator
							]
					]

			]
		];

	// Register with the Asset Registry to be informed when it is done loading up files.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnFilesLoaded().AddSP(this, &SGraphPalette::RefreshActionsList, true);
}

TSharedRef<SWidget> SGraphPalette::OnCreateWidgetForAction(FCreateWidgetForActionData* const InCreateData)
{
	return	SNew(SGraphPaletteItem, InCreateData);
}

FReply SGraphPalette::OnActionDragged(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions, const FPointerEvent& MouseEvent)
{
	if( InActions.Num() > 0 && InActions[0].IsValid() )
	{
		TSharedPtr<FEdGraphSchemaAction> InAction = InActions[0];

		return FReply::Handled().BeginDragDrop(FGraphSchemaActionDragDropAction::New(InAction));
	}

	return FReply::Unhandled();
}

void SGraphPalette::RefreshActionsList(bool bPreserveExpansion)
{
	GraphActionMenu->RefreshAllActions(bPreserveExpansion);
}
