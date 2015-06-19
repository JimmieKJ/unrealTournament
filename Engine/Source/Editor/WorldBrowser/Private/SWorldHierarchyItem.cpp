// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "WorldBrowserPrivatePCH.h"

#include "SWorldHierarchyItem.h"
#include "SColorPicker.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/LevelStreamingKismet.h"


#define LOCTEXT_NAMESPACE "WorldBrowser"


//////////////////////////
// SWorldHierarchyItem
//////////////////////////

void SWorldHierarchyItem::Construct(const FArguments& InArgs, TSharedRef<STableViewBase> OwnerTableView)
{
	WorldModel = InArgs._InWorldModel;
	LevelModel = InArgs._InItemModel;
	IsItemExpanded = InArgs._IsItemExpanded;
	HighlightText = InArgs._HighlightText;

	StreamingLevelAlwaysLoadedBrush = FEditorStyle::GetBrush("WorldBrowser.LevelStreamingAlwaysLoaded");
	StreamingLevelKismetBrush = FEditorStyle::GetBrush("WorldBrowser.LevelStreamingBlueprint");

	SMultiColumnTableRow<TSharedPtr<FLevelModel>>::Construct(
		FSuperRowType::FArguments()
			.OnDragDetected(this, &SWorldHierarchyItem::OnItemDragDetected), 
		OwnerTableView
	);

}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef< SWidget > SWorldHierarchyItem::GenerateWidgetForColumn( const FName& ColumnID )
{
	TSharedPtr< SWidget > TableRowContent = SNullWidget::NullWidget;

	if (ColumnID == HierarchyColumns::ColumnID_LevelLabel)
	{
		TableRowContent =
			SNew(SHorizontalBox)
			
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
			]
			
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(7) // in case brush for this item is empty
				[
					SNew(SImage)
					.Image(this, &SWorldHierarchyItem::GetLevelIconBrush)
				]
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(this, &SWorldHierarchyItem::GetLevelDisplayNameFont)
				.Text(this, &SWorldHierarchyItem::GetLevelDisplayNameText)
				.ColorAndOpacity(this, &SWorldHierarchyItem::GetLevelDisplayNameColorAndOpacity)
				.HighlightText(HighlightText)
				.ToolTipText(LOCTEXT("DoubleClickToolTip", "Double-Click to make this the current Level"))
			]
		;
	}
	else if (ColumnID == HierarchyColumns::ColumnID_Lock)
	{
		TableRowContent = 
			SAssignNew(LockButton, SButton)
			.ContentPadding(0 )
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.IsEnabled(this, &SWorldHierarchyItem::IsLockEnabled)
			.OnClicked(this, &SWorldHierarchyItem::OnToggleLock)
			.ToolTipText(this, &SWorldHierarchyItem::GetLevelLockToolTip)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Content()
			[
				SNew(SImage)
				.Image(this, &SWorldHierarchyItem::GetLevelLockBrush)
			]
		;
	}
	else if (ColumnID == HierarchyColumns::ColumnID_Visibility)
	{
		TableRowContent = 
			SAssignNew(VisibilityButton, SButton)
			.ContentPadding(0)
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.IsEnabled(this, &SWorldHierarchyItem::IsVisibilityEnabled)
			.OnClicked(this, &SWorldHierarchyItem::OnToggleVisibility)
			.ToolTipText(LOCTEXT("VisibilityButtonToolTip", "Toggle Level Visibility"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Content()
			[
				SNew( SImage )
				.Image(this, &SWorldHierarchyItem::GetLevelVisibilityBrush)
			]
		;
	}
	else if (ColumnID == HierarchyColumns::ColumnID_Color)
	{
		if (LevelModel->SupportsLevelColor())
		{
			TableRowContent =
				SAssignNew(ColorButton, SButton)
				.ContentPadding(0)
				.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
				.IsEnabled(true)
				.OnClicked(this, &SWorldHierarchyItem::OnChangeColor)
				.ToolTipText(LOCTEXT("LevelColorButtonToolTip", "Change Level Color"))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Content()
				[
					SNew(SImage)
					.ColorAndOpacity(this, &SWorldHierarchyItem::GetDrawColor)
					.Image(this, &SWorldHierarchyItem::GetLevelColorBrush)
				]
			;
		}
		else
		{
			TableRowContent =
				SNew(SHorizontalBox);
		}
	}
	else if (ColumnID == HierarchyColumns::ColumnID_Kismet)
	{
		TableRowContent =
			SAssignNew(KismetButton, SButton)
			.ContentPadding(0)
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.IsEnabled(this, &SWorldHierarchyItem::IsKismetEnabled)
			.OnClicked(this, &SWorldHierarchyItem::OnOpenKismet)
			.ToolTipText(LOCTEXT("KismetButtonToolTip", "Open Level Blueprint"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Content()
			[
				SNew(SImage)
				.Image(this, &SWorldHierarchyItem::GetLevelKismetBrush)
			]
		;
	}
	else if (ColumnID == HierarchyColumns::ColumnID_SCCStatus)
	{
		TableRowContent = 
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew( SImage )
						.Image(this, &SWorldHierarchyItem::GetSCCStateImage)
						.ToolTipText(this, &SWorldHierarchyItem::GetSCCStateTooltip)
				]
			]
		;
	}
	else if (ColumnID == HierarchyColumns::ColumnID_Save)
	{
		TableRowContent = 
			SAssignNew(SaveButton, SButton)
			.ContentPadding(0)
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.IsEnabled(this, &SWorldHierarchyItem::IsSaveEnabled)
			.OnClicked(this, &SWorldHierarchyItem::OnSave)
			.ToolTipText(LOCTEXT("SaveButtonToolTip", "Save Level"))
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.Content()
			[
				SNew(SImage)
				.Image(this, &SWorldHierarchyItem::GetLevelSaveBrush)
			]
		;
	}

	return TableRowContent.ToSharedRef();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText SWorldHierarchyItem::GetLevelDisplayNameText() const
{
	return FText::FromString(LevelModel->GetDisplayName());
}

bool SWorldHierarchyItem::IsSaveEnabled() const
{
	return LevelModel->IsLoaded();
}

bool SWorldHierarchyItem::IsLockEnabled() const
{
	return LevelModel->IsLoaded();
}

bool SWorldHierarchyItem::IsVisibilityEnabled() const
{
	return LevelModel->IsLoaded();
}

bool SWorldHierarchyItem::IsKismetEnabled() const
{
	return LevelModel->HasKismet();
}

FSlateColor SWorldHierarchyItem::GetDrawColor() const
{
	return FSlateColor(LevelModel->GetLevelColor());
}

FReply SWorldHierarchyItem::OnToggleVisibility()
{
	FLevelModelList LevelList; 
	LevelList.Add(LevelModel);

	if (LevelModel->IsVisible())
	{
		WorldModel->HideLevels(LevelList);
	}
	else
	{
		WorldModel->ShowLevels(LevelList);
	}

	return FReply::Handled();
}

FReply SWorldHierarchyItem::OnToggleLock()
{
	FLevelModelList LevelList; 
	LevelList.Add(LevelModel);
	
	if (LevelModel->IsLocked())
	{
		WorldModel->UnlockLevels(LevelList);
	}
	else
	{
		WorldModel->LockLevels(LevelList);
	}
	
	return FReply::Handled();
}

FReply SWorldHierarchyItem::OnSave()
{
	FLevelModelList LevelList; 
	LevelList.Add(LevelModel);


	WorldModel->SaveLevels(LevelList);
	return FReply::Handled();
}

FReply SWorldHierarchyItem::OnOpenKismet()
{
	LevelModel->OpenKismet();
	return FReply::Handled();
}

void SWorldHierarchyItem::OnSetColorFromColorPicker(FLinearColor NewColor)
{
	LevelModel->SetLevelColor(NewColor);
}

void SWorldHierarchyItem::OnColorPickerCancelled(FLinearColor OriginalColor)
{
	LevelModel->SetLevelColor(OriginalColor);
}

void SWorldHierarchyItem::OnColorPickerInteractiveBegin()
{
	GEditor->BeginTransaction(NSLOCTEXT("FColorStructCustomization", "SetColorProperty", "Edit Level Draw Color"));
}

void SWorldHierarchyItem::OnColorPickerInteractiveEnd()
{
	GEditor->EndTransaction();
}

FReply SWorldHierarchyItem::OnChangeColor()
{
	if (LevelModel.IsValid())
	{
		FColorPickerArgs PickerArgs;
		PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
		PickerArgs.InitialColorOverride = LevelModel->GetLevelColor();
		PickerArgs.bOnlyRefreshOnMouseUp = false;
		PickerArgs.bOnlyRefreshOnOk = false;
		PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SWorldHierarchyItem::OnSetColorFromColorPicker);
		PickerArgs.OnColorPickerCancelled = FOnColorPickerCancelled::CreateSP(this, &SWorldHierarchyItem::OnColorPickerCancelled);
		PickerArgs.OnInteractivePickBegin = FSimpleDelegate::CreateSP(this, &SWorldHierarchyItem::OnColorPickerInteractiveBegin);
		PickerArgs.OnInteractivePickEnd = FSimpleDelegate::CreateSP(this, &SWorldHierarchyItem::OnColorPickerInteractiveEnd);
		PickerArgs.ParentWidget = AsShared();

		OpenColorPicker(PickerArgs);
	}

	return FReply::Handled();
}

FReply SWorldHierarchyItem::OnItemDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		TSharedPtr<FLevelDragDropOp> Op = WorldModel->CreateDragDropOp();
		if (Op.IsValid())
		{
			// Start a drag-drop
			return FReply::Handled().BeginDragDrop(Op.ToSharedRef());
		}
	}

	return FReply::Unhandled();
}

FReply SWorldHierarchyItem::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	auto Op = DragDropEvent.GetOperationAs<FLevelDragDropOp>();
	if (Op.IsValid() && LevelModel->IsGoodToDrop(Op))
	{
		LevelModel->OnDrop(Op);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SWorldHierarchyItem::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	SCompoundWidget::OnDragEnter(MyGeometry, DragDropEvent);
	
	auto Op = DragDropEvent.GetOperationAs<FLevelDragDropOp>();
	if (Op.IsValid())
	{
		// to show mouse hover effect
		SWidget::OnMouseEnter(MyGeometry, DragDropEvent);
		// D&D decorator icon
		Op->bGoodToDrop = LevelModel->IsGoodToDrop(Op);
	}
}

void SWorldHierarchyItem::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	SCompoundWidget::OnDragLeave(DragDropEvent);

	auto Op = DragDropEvent.GetOperationAs<FLevelDragDropOp>();
	if (Op.IsValid())
	{
		// to hide mouse hover effect
		SWidget::OnMouseLeave(DragDropEvent);
		// D&D decorator icon
		Op->bGoodToDrop = false;
	}
}

FSlateFontInfo SWorldHierarchyItem::GetLevelDisplayNameFont() const
{
	if (LevelModel->IsCurrent())
	{
		return FEditorStyle::GetFontStyle("WorldBrowser.LabelFontBold");
	}
	else
	{
		return FEditorStyle::GetFontStyle("WorldBrowser.LabelFont");
	}
}

FSlateColor SWorldHierarchyItem::GetLevelDisplayNameColorAndOpacity() const
{
	// Force the text to display red if level is missing
	if (!LevelModel->HasValidPackage())
	{
		return FLinearColor(1.0f, 0.0f, 0.0f);
	}
		
	// Highlight text differently if it doesn't match the search filter (e.g., parent levels to child levels that
	// match search criteria.)
	if (LevelModel->GetLevelFilteredOutFlag())
	{
		return FLinearColor(0.30f, 0.30f, 0.30f);
	}

	if (!LevelModel->IsLoaded())
	{
		return FSlateColor::UseSubduedForeground();
	}
		
	if (LevelModel->IsCurrent())
	{
		return LevelModel->GetLevelSelectionFlag() ? FSlateColor::UseForeground() : FLinearColor(0.12f, 0.56f, 1.0f);
	}

	return FSlateColor::UseForeground();
}

const FSlateBrush* SWorldHierarchyItem::GetLevelIconBrush() const
{
	UClass* StreamingClass = LevelModel->GetStreamingClass();
	if (StreamingClass == ULevelStreamingKismet::StaticClass())
	{
		return StreamingLevelKismetBrush;
	}

	if (StreamingClass == ULevelStreamingAlwaysLoaded::StaticClass())
	{
		return StreamingLevelAlwaysLoadedBrush;
	}

	return nullptr;
}

const FSlateBrush* SWorldHierarchyItem::GetLevelVisibilityBrush() const
{
	if (LevelModel->GetLevelObject())
	{
		if (LevelModel->IsVisible())
		{
			return VisibilityButton->IsHovered() ? FEditorStyle::GetBrush( "Level.VisibleHighlightIcon16x" ) :
													FEditorStyle::GetBrush( "Level.VisibleIcon16x" );
		}
		else
		{
			return VisibilityButton->IsHovered() ? FEditorStyle::GetBrush( "Level.NotVisibleHighlightIcon16x" ) :
													FEditorStyle::GetBrush( "Level.NotVisibleIcon16x" );
		}
	}
	else
	{
		return FEditorStyle::GetBrush( "Level.EmptyIcon16x" );
	}
}

const FSlateBrush* SWorldHierarchyItem::GetLevelLockBrush() const
{
	if (!LevelModel->IsLoaded() || LevelModel->IsPersistent())
	{
		//Locking the persistent level is not allowed; stub in a different brush
		return FEditorStyle::GetBrush( "Level.EmptyIcon16x" );
	}
	else 
	{
		////Non-Persistent
		//if ( GEngine && GEngine->bLockReadOnlyLevels )
		//{
		//	if(LevelModel->IsReadOnly())
		//	{
		//		return LockButton->IsHovered() ? FEditorStyle::GetBrush( "Level.ReadOnlyLockedHighlightIcon16x" ) :
		//											FEditorStyle::GetBrush( "Level.ReadOnlyLockedIcon16x" );
		//	}
		//}
			
		if (LevelModel->IsLocked())
		{
			return LockButton->IsHovered() ? FEditorStyle::GetBrush( "Level.LockedHighlightIcon16x" ) :
												FEditorStyle::GetBrush( "Level.LockedIcon16x" );
		}
		else
		{
			return LockButton->IsHovered() ? FEditorStyle::GetBrush( "Level.UnlockedHighlightIcon16x" ) :
												FEditorStyle::GetBrush( "Level.UnlockedIcon16x" );
		}
	}
}

FText SWorldHierarchyItem::GetLevelLockToolTip() const
{
	//Non-Persistent
	if (GEngine && GEngine->bLockReadOnlyLevels)
	{
		if (LevelModel->IsFileReadOnly())
		{
			return LOCTEXT("ReadOnly_LockButtonToolTip", "Read-Only levels are locked!");
		}
	}

	return LOCTEXT("LockButtonToolTip", "Toggle Level Lock");
}

FText SWorldHierarchyItem::GetSCCStateTooltip() const
{
	FSourceControlStatePtr SourceControlState = ISourceControlModule::Get().GetProvider().GetState(LevelModel->GetPackageFileName(), EStateCacheUsage::Use);
	if(SourceControlState.IsValid())
	{
		return SourceControlState->GetDisplayTooltip();
	}

	return FText::GetEmpty();
}

const FSlateBrush* SWorldHierarchyItem::GetSCCStateImage() const
{
	FSourceControlStatePtr SourceControlState = ISourceControlModule::Get().GetProvider().GetState(LevelModel->GetPackageFileName(), EStateCacheUsage::Use);
	if(SourceControlState.IsValid())
	{
		return FEditorStyle::GetBrush(SourceControlState->GetSmallIconName());
	}

	return NULL;
}

const FSlateBrush* SWorldHierarchyItem::GetLevelSaveBrush() const
{
	if (LevelModel->IsLoaded())
	{
		if (LevelModel->IsLocked())
		{
			return FEditorStyle::GetBrush( "Level.SaveDisabledIcon16x" );
		}
		else
		{
			if (LevelModel->IsDirty())
			{
				return SaveButton->IsHovered() ? FEditorStyle::GetBrush( "Level.SaveModifiedHighlightIcon16x" ) :
													FEditorStyle::GetBrush( "Level.SaveModifiedIcon16x" );
			}
			else
			{
				return SaveButton->IsHovered() ? FEditorStyle::GetBrush( "Level.SaveHighlightIcon16x" ) :
													FEditorStyle::GetBrush( "Level.SaveIcon16x" );
			}
		}								
	}
	else
	{
		return FEditorStyle::GetBrush( "Level.EmptyIcon16x" );
	}	
}

const FSlateBrush* SWorldHierarchyItem::GetLevelKismetBrush() const
{
	if (LevelModel->IsLoaded())
	{
		if (LevelModel->HasKismet())
		{
			return KismetButton->IsHovered() ? FEditorStyle::GetBrush( "Level.ScriptHighlightIcon16x" ) :
												FEditorStyle::GetBrush( "Level.ScriptIcon16x" );
		}
		else
		{
			return FEditorStyle::GetBrush( "Level.EmptyIcon16x" );
		}
	}
	else
	{
		return FEditorStyle::GetBrush( "Level.EmptyIcon16x" );
	}	
}

const FSlateBrush* SWorldHierarchyItem::GetLevelColorBrush() const
{
	return FEditorStyle::GetBrush("Level.ColorIcon40x");
}

#undef LOCTEXT_NAMESPACE
