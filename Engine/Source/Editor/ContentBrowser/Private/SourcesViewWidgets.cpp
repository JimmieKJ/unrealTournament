// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserPCH.h"
#include "PathViewTypes.h"
#include "CollectionViewTypes.h"
#include "SourcesViewWidgets.h"

#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/AssetPathDragDropOp.h"
#include "DragDropHandler.h"
#include "ContentBrowserUtils.h"
#include "CollectionViewUtils.h"
#include "SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

//////////////////////////
// SAssetTreeItem
//////////////////////////

void SAssetTreeItem::Construct( const FArguments& InArgs )
{
	TreeItem = InArgs._TreeItem;
	OnNameChanged = InArgs._OnNameChanged;
	OnVerifyNameChanged = InArgs._OnVerifyNameChanged;
	OnAssetsDragDropped = InArgs._OnAssetsDragDropped;
	OnPathsDragDropped = InArgs._OnPathsDragDropped;
	OnFilesDragDropped = InArgs._OnFilesDragDropped;
	IsItemExpanded = InArgs._IsItemExpanded;
	bDraggedOver = false;

	FolderOpenBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderOpen");
	FolderClosedBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderClosed");
	FolderOpenCodeBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderOpenCode");
	FolderClosedCodeBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderClosedCode");
	FolderDeveloperBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderDeveloper");
		
	FolderType = EFolderType::Normal;
	if( ContentBrowserUtils::IsDevelopersFolder(InArgs._TreeItem->FolderPath) )
	{
		FolderType = EFolderType::Developer;
	}
	else if( ContentBrowserUtils::IsClassPath/*IsClassRootDir*/(InArgs._TreeItem->FolderPath) )
	{
		FolderType = EFolderType::Code;
	}

	bool bIsRoot = !InArgs._TreeItem->Parent.IsValid();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(this, &SAssetTreeItem::GetBorderImage)
		.Padding( FMargin( 0, bIsRoot ? 3 : 0, 0, 0 ) )	// For root items in the tree, give them a little breathing room on the top
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 2, 0)
			.VAlign(VAlign_Center)
			[
				// Folder Icon
				SNew(SImage) 
				.Image(this, &SAssetTreeItem::GetFolderIcon)
				.ColorAndOpacity(this, &SAssetTreeItem::GetFolderColor)
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineRenameWidget, SInlineEditableTextBlock)
					.Text(this, &SAssetTreeItem::GetNameText)
					.ToolTipText(this, &SAssetTreeItem::GetToolTipText)
					.Font( FEditorStyle::GetFontStyle(bIsRoot ? "ContentBrowser.SourceTreeRootItemFont" : "ContentBrowser.SourceTreeItemFont") )
					.HighlightText( InArgs._HighlightText )
					.OnTextCommitted(this, &SAssetTreeItem::HandleNameCommitted)
					.OnVerifyTextChanged(this, &SAssetTreeItem::VerifyNameChanged)
					.IsSelected( InArgs._IsSelected )
					.IsReadOnly( this, &SAssetTreeItem::IsReadOnly )
			]
		]
	];

	if( InlineRenameWidget.IsValid() )
	{
		EnterEditingModeDelegateHandle = TreeItem.Pin()->OnRenamedRequestEvent.AddSP( InlineRenameWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode );
	}
}

SAssetTreeItem::~SAssetTreeItem()
{
	if( InlineRenameWidget.IsValid() )
	{
		TreeItem.Pin()->OnRenamedRequestEvent.Remove( EnterEditingModeDelegateHandle );
	}
}

bool SAssetTreeItem::ValidateDragDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) const
{
	TSharedPtr<FTreeItem> TreeItemPinned = TreeItem.Pin();
	return TreeItemPinned.IsValid() && DragDropHandler::ValidateDragDropOnAssetFolder(MyGeometry, DragDropEvent, TreeItemPinned->FolderPath);
}

void SAssetTreeItem::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	bDraggedOver = false;

	if (ValidateDragDrop(MyGeometry, DragDropEvent))
	{
		bDraggedOver = true;
	}
}

void SAssetTreeItem::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (Operation.IsValid())
	{
		Operation->SetCursorOverride(TOptional<EMouseCursor::Type>());

		if (Operation->IsOfType<FAssetDragDropOp>())
		{
			TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);
			DragDropOp->ResetToDefaultToolTip();
		}
	}

	bDraggedOver = false;
}

FReply SAssetTreeItem::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	bDraggedOver = false;

	if (ValidateDragDrop(MyGeometry, DragDropEvent))
	{
		bDraggedOver = true;
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SAssetTreeItem::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	bDraggedOver = false;

	if (ValidateDragDrop(MyGeometry, DragDropEvent))
	{
		TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
		if (!Operation.IsValid())
		{
			return FReply::Unhandled();
		}

		if (Operation->IsOfType<FAssetDragDropOp>())
		{
			TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>( DragDropEvent.GetOperation() );
			OnAssetsDragDropped.ExecuteIfBound(DragDropOp->AssetData, TreeItem.Pin());
			return FReply::Handled();
		}
		else if (Operation->IsOfType<FAssetPathDragDropOp>())
		{
			TSharedPtr<FAssetPathDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetPathDragDropOp>( DragDropEvent.GetOperation() );
			OnPathsDragDropped.ExecuteIfBound(DragDropOp->PathNames, TreeItem.Pin());
			return FReply::Handled();
		}
		else if (Operation->IsOfType<FExternalDragOperation>())
		{
			TSharedPtr<FExternalDragOperation> DragDropOp = StaticCastSharedPtr<FExternalDragOperation>( DragDropEvent.GetOperation() );
			OnFilesDragDropped.ExecuteIfBound(DragDropOp->GetFiles(), TreeItem.Pin());
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SAssetTreeItem::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	LastGeometry = AllottedGeometry;
}

bool SAssetTreeItem::VerifyNameChanged(const FText& InName, FText& OutError) const
{
	if ( TreeItem.IsValid() )
	{
		TSharedPtr<FTreeItem> TreeItemPtr = TreeItem.Pin();
		if(OnVerifyNameChanged.IsBound())
		{
			return OnVerifyNameChanged.Execute(InName, OutError, TreeItemPtr->FolderPath);
		}
	}

	return true;
}

void SAssetTreeItem::HandleNameCommitted( const FText& NewText, ETextCommit::Type /*CommitInfo*/ )
{
	if ( TreeItem.IsValid() )
	{
		TSharedPtr<FTreeItem> TreeItemPtr = TreeItem.Pin();

		if ( TreeItemPtr->bNamingFolder )
		{
			TreeItemPtr->bNamingFolder = false;

			const FString OldPath = TreeItemPtr->FolderPath;
			FString Path;
			TreeItemPtr->FolderPath.Split(TEXT("/"), &Path, NULL, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			TreeItemPtr->DisplayName = NewText;
			TreeItemPtr->FolderName = NewText.ToString();
			TreeItemPtr->FolderPath = Path + TEXT("/") + NewText.ToString();

			FVector2D MessageLoc;
			MessageLoc.X = LastGeometry.AbsolutePosition.X;
			MessageLoc.Y = LastGeometry.AbsolutePosition.Y + LastGeometry.Size.Y * LastGeometry.Scale;

			OnNameChanged.ExecuteIfBound(TreeItemPtr, OldPath, MessageLoc);
		}
	}
}

bool SAssetTreeItem::IsReadOnly() const
{
	if ( TreeItem.IsValid() )
	{
		return !TreeItem.Pin()->bNamingFolder;
	}
	else
	{
		return true;
	}
}

bool SAssetTreeItem::IsValidAssetPath() const
{
	if ( TreeItem.IsValid() )
	{
		// The classes folder is not a real path
		return !ContentBrowserUtils::IsClassPath(TreeItem.Pin()->FolderPath);
	}
	else
	{
		return false;
	}
}

const FSlateBrush* SAssetTreeItem::GetFolderIcon() const
{
	switch( FolderType )
	{
	case EFolderType::Code:
		return ( IsItemExpanded.Get() ) ? FolderOpenCodeBrush : FolderClosedCodeBrush;

	case EFolderType::Developer:
		return FolderDeveloperBrush;

	default:
		return ( IsItemExpanded.Get() ) ? FolderOpenBrush : FolderClosedBrush;
	}
}

FSlateColor SAssetTreeItem::GetFolderColor() const
{
	if ( TreeItem.IsValid() )
	{
		const TSharedPtr<FLinearColor> Color = ContentBrowserUtils::LoadColor( TreeItem.Pin()->FolderPath );
		if ( Color.IsValid() )
		{
			return *Color.Get();
		}
	}
	return ContentBrowserUtils::GetDefaultColor();
}

FText SAssetTreeItem::GetNameText() const
{
	TSharedPtr<FTreeItem> TreeItemPin = TreeItem.Pin();
	if ( TreeItemPin.IsValid() )
	{
		return TreeItemPin->DisplayName;
	}
	else
	{
		return FText();
	}
}

FText SAssetTreeItem::GetToolTipText() const
{
	TSharedPtr<FTreeItem> TreeItemPin = TreeItem.Pin();
	if ( TreeItemPin.IsValid() )
	{
		return FText::FromString(TreeItemPin->FolderPath);
	}
	else
	{
		return FText();
	}
}

const FSlateBrush* SAssetTreeItem::GetBorderImage() const
{
	return bDraggedOver ? FEditorStyle::GetBrush("Menu.Background") : FEditorStyle::GetBrush("NoBorder");
}



//////////////////////////
// SCollectionListItem
//////////////////////////

void SCollectionListItem::Construct( const FArguments& InArgs )
{
	ParentWidget = InArgs._ParentWidget;
	CollectionItem = InArgs._CollectionItem;
	OnBeginNameChange = InArgs._OnBeginNameChange;
	OnNameChangeCommit = InArgs._OnNameChangeCommit;
	OnVerifyRenameCommit = InArgs._OnVerifyRenameCommit;
	OnAssetsDragDropped = InArgs._OnAssetsDragDropped;
	bDraggedOver = false;

	FString CollectionTypeImage;
	FText CollectionTypeTooltip;
	switch (InArgs._CollectionItem->CollectionType)
	{
	case ECollectionShareType::CST_Shared:
		CollectionTypeImage = TEXT("ContentBrowser.Shared");
		CollectionTypeTooltip = LOCTEXT("SharedCollectionTooltip", "Shared. This collection is visible to everyone.");
		break;

	case ECollectionShareType::CST_Private:
		CollectionTypeImage = TEXT("ContentBrowser.Private");
		CollectionTypeTooltip = LOCTEXT("PrivateCollectionTooltip", "Private. This collection is only visible to you.");
		break;

	case ECollectionShareType::CST_Local:
		CollectionTypeImage = TEXT("ContentBrowser.Local");
		CollectionTypeTooltip = LOCTEXT("LocalCollectionTooltip", "Local. This collection is only visible to you and is not in source control.");
		break;

	default:
		CollectionTypeImage = TEXT("");
		CollectionTypeTooltip = FText::GetEmpty();
		break;
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(this, &SCollectionListItem::GetBorderImage)
		.Padding(0)
		[
			SNew(SHorizontalBox)
			.ToolTip( SNew(SToolTip).Text(CollectionTypeTooltip) )

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 4, 0)
			[
				// Type Icon
				SNew(SImage)
				.Image( FEditorStyle::GetBrush(*CollectionTypeImage) )
				.ColorAndOpacity( this, &SCollectionListItem::GetCollectionColor )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineRenameWidget, SInlineEditableTextBlock)
					.Text( this, &SCollectionListItem::GetNameText )
					.Font( FEditorStyle::GetFontStyle("ContentBrowser.SourceListItemFont") )
					.OnBeginTextEdit(this, &SCollectionListItem::HandleBeginNameChange)
					.OnTextCommitted(this, &SCollectionListItem::HandleNameCommitted)
					.OnVerifyTextChanged(this, &SCollectionListItem::HandleVerifyNameChanged)
					.IsSelected( InArgs._IsSelected )
					.IsReadOnly( InArgs._IsReadOnly )
			]
		]
	];

	if(InlineRenameWidget.IsValid())
	{
		// This is broadcast when the context menu / input binding requests a rename
		EnterEditingModeDelegateHandle = CollectionItem.Pin()->OnRenamedRequestEvent.AddSP(InlineRenameWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);
	}
}

SCollectionListItem::~SCollectionListItem()
{
	if(InlineRenameWidget.IsValid())
	{
		CollectionItem.Pin()->OnRenamedRequestEvent.Remove( EnterEditingModeDelegateHandle );
	}
}

void SCollectionListItem::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Cache this widget's geometry so it can pop up warnings over itself
	CachedGeometry = AllottedGeometry;
}

void SCollectionListItem::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FAssetDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if ( DragDropOp.IsValid() )
	{
		bool bCanDrop = DragDropOp->AssetData.Num() > 0;

		if (bCanDrop)
		{
			bDraggedOver = true;
		}
	}
}

void SCollectionListItem::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	bDraggedOver = false;
}

FReply SCollectionListItem::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FAssetDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (DragDropOp.IsValid())
	{
		bool bCanDrop = DragDropOp->AssetData.Num() > 0;

		if (bCanDrop)
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SCollectionListItem::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	bDraggedOver = false;

	TSharedPtr<FAssetDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (DragDropOp.IsValid() && ensure(CollectionItem.IsValid()))
	{
		FText Message;
		OnAssetsDragDropped.ExecuteIfBound(DragDropOp->AssetData, CollectionItem.Pin(), Message);

		// Added items to the collection or failed. Either way, display the message.
		ContentBrowserUtils::DisplayMessage(Message, CachedGeometry.GetClippingRect(), SharedThis(this));

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SCollectionListItem::HandleBeginNameChange( const FText& OldText )
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if ( CollectionItemPtr.IsValid() )
	{
		// If we get here via a context menu or input binding, bRenaming will already be set on the item.
		// If we got here by double clicking the editable text field, we need to set it now.
		CollectionItemPtr->bRenaming = true;

		OnBeginNameChange.ExecuteIfBound( CollectionItemPtr );
	}
}

void SCollectionListItem::HandleNameCommitted( const FText& NewText, ETextCommit::Type CommitInfo )
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if ( CollectionItemPtr.IsValid() )
	{
		if ( CollectionItemPtr->bRenaming )
		{
			CollectionItemPtr->bRenaming = false;

			if ( OnNameChangeCommit.IsBound() )
			{
				FText WarningMessage;
				bool bIsCommitted = (CommitInfo != ETextCommit::OnCleared);
				if ( !OnNameChangeCommit.Execute(CollectionItemPtr, NewText.ToString(), bIsCommitted, WarningMessage) && ParentWidget.IsValid() && bIsCommitted )
				{
					// Failed to rename/create a collection, display a warning.
					ContentBrowserUtils::DisplayMessage(WarningMessage, CachedGeometry.GetClippingRect(), ParentWidget.ToSharedRef());
				}
			}				
		}
	}
}

bool SCollectionListItem::HandleVerifyNameChanged( const FText& NewText, FText& OutErrorMessage )
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if (CollectionItemPtr.IsValid())
	{
		return !OnVerifyRenameCommit.IsBound() || OnVerifyRenameCommit.Execute(CollectionItemPtr, NewText.ToString(), CachedGeometry.GetClippingRect(), OutErrorMessage);
	}

	return true;
}

FText SCollectionListItem::GetNameText() const
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if ( CollectionItemPtr.IsValid() )
	{
		return FText::FromString(CollectionItemPtr->CollectionName);
	}
	else
	{
		return FText();
	}
}

FSlateColor SCollectionListItem::GetCollectionColor() const
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if ( CollectionItemPtr.IsValid() )
	{
		const TSharedPtr<FLinearColor> Color = CollectionViewUtils::LoadColor(CollectionItemPtr->CollectionName, CollectionItemPtr->CollectionType);
		if( Color.IsValid() )
		{
			return *Color.Get();
		}
	}
	
	return CollectionViewUtils::GetDefaultColor();
}

const FSlateBrush* SCollectionListItem::GetBorderImage() const
{
	return bDraggedOver ? FEditorStyle::GetBrush("Menu.Background") : FEditorStyle::GetBrush("NoBorder");
}


#undef LOCTEXT_NAMESPACE
