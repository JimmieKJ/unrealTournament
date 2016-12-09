// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "ISkeletonTreeItem.h"
#include "SkeletonTreeItem.h"
#include "IEditableSkeleton.h"
#include "Widgets/Input/SEditableText.h"
#include "Preferences/PersonaOptions.h"
#include "Engine/SkeletalMeshSocket.h"

class FSkeletonTreeSocketItem : public FSkeletonTreeItem
{
public:
	SKELETON_TREE_ITEM_TYPE(FSkeletonTreeSocketItem, FSkeletonTreeItem)

	FSkeletonTreeSocketItem(USkeletalMeshSocket* InSocket, ESocketParentType InParentType, bool bInIsCustomized, const TSharedRef<class ISkeletonTree>& InSkeletonTree)
		: FSkeletonTreeItem(InSkeletonTree)
		, Socket(InSocket)
		, ParentType(InParentType)
		, bIsCustomized(bInIsCustomized)
		, bInlineEditorExpanded(false)
	{}
	virtual ~FSkeletonTreeSocketItem()
	{}

	/** ISkeletonTreeItem interface */
	virtual TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, const TAttribute<FText>& InFilterText) override;
	virtual void GenerateWidgetForNameColumn(TSharedPtr< SHorizontalBox > Box, const TAttribute<FText>& FilterText, FIsSelected InIsSelected) override;
	virtual TSharedRef< SWidget > GenerateWidgetForDataColumn(const FName& DataColumnName) override;
	virtual TSharedRef< SWidget > GenerateInlineEditWidget(const TAttribute<FText>& FilterText, FIsSelected InIsSelected) override;
	virtual bool HasInlineEditor() const override { return GetDefault<UPersonaOptions>()->bUseInlineSocketEditor; }
	virtual void ToggleInlineEditorExpansion() override { bInlineEditorExpanded = !bInlineEditorExpanded; }
	virtual bool IsInlineEditorExpanded() const override { return bInlineEditorExpanded; }
	virtual FName GetRowItemName() const override { return Socket->SocketName; }
	virtual void RequestRename() override;
	virtual void OnItemDoubleClicked() override;

	/** Get the parent type */
	ESocketParentType GetParentType() const { return ParentType; }

	/** Is this socket customized */
	bool IsSocketCustomized() const { return bIsCustomized; }

	/** Can we customize this socket? */
	bool CanCustomizeSocket() const;

	/** Get the socket we wrap */
	USkeletalMeshSocket* GetSocket() const { return Socket; }

private:
	/** Called when user is renaming a socket to verify the name **/
	bool OnVerifySocketNameChanged(const FText& InText, FText& OutErrorMessage);

	/** Called when user renames a socket **/
	void OnCommitSocketName(const FText& InText, ETextCommit::Type CommitInfo);

	/** Function that returns the current tooltip for this socket */
	FText GetSocketToolTip();

	/** Get the color to display for the socket name */
	FSlateColor GetTextColor() const;

	/** Handle dragging a socket */
	FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Return socket name as FText for display in skeleton tree */
	FText GetSocketNameAsText() const { return FText::FromName(Socket->SocketName); }

private:
	/** The socket we represent */
	USkeletalMeshSocket* Socket;

	/** Whether we are on the skeleton or the mesh */
	ESocketParentType ParentType;

	/** Whether we are customized */
	bool bIsCustomized;

	/** Track expansion state of the in-line editor */
	bool bInlineEditorExpanded;

	/** Box for the user to type in the name - stored here so that SSkeletonTree can set the keyboard focus in Slate */
	TSharedPtr<SEditableText> NameEntryBox;

	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE(FOnRenameRequested);
	FOnRenameRequested OnRenameRequested;
};
