// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TaskBrowserPrivatePCH.h"
#include "STaskColumn.h"

#define LOCTEXT_NAMESPACE "STaskColumn"

//////////////////////////////////////////////////////////////////////////
// STaskColumn

/**
 * Construct the widget
 *
 * @param InArgs		A declaration from which to construct the widget
 */
void STaskColumn::Construct(const FArguments& InArgs)
{
	TaskBrowser = InArgs._TaskBrowser;
	Field = InArgs._Field;

	this->ChildSlot
	[
		SNew(SButton)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
		.OnClicked( this, &STaskColumn::OnTaskColumnClicked )
		[
			SNew(STextBlock) .Text( GetFieldNameLoc( Field ) ) .TextStyle( FEditorStyle::Get(), "NormalText" )
		]
	];
}

FString STaskColumn::GetFieldNameLoc( const EField InField )
{
	// Get a localized column header button text
	check( InField > EField::Invalid );
	check( InField < EField::NumColumnIDs );
	switch( InField )
	{
	case EField::Number:
		return LOCTEXT("Number", "Number").ToString();
	case EField::Priority:
		return LOCTEXT("Priority", "Priority").ToString();
	case EField::Summary:
		return LOCTEXT("Summary", "Summary").ToString();
	case EField::Status:
		return LOCTEXT("Status", "Status").ToString();
	case EField::CreatedBy:
		return LOCTEXT("CreatedBy", "Created By").ToString();
	case EField::AssignedTo:
		return LOCTEXT("AssignedTo", "Assigned To").ToString();
	}
	return LOCTEXT("BadField", "Bad Field").ToString();
}

FReply STaskColumn::OnTaskColumnClicked()
{
	// Forward the call to the task browser
	TaskBrowser.Pin()->OnTaskColumnClicked( Field );

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE