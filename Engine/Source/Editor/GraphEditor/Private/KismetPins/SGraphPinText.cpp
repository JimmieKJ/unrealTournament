// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphPinText.h"


void SGraphPinText::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinText::GetDefaultValueWidget()
{
	return SNew(SEditableTextBox)
				.Style( FEditorStyle::Get(), "Graph.EditableTextBox" )
				.Text( this, &SGraphPinText::GetTypeInValue )
				.SelectAllTextWhenFocused(true)
				.MinDesiredWidth( 18.0f )
				.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
				.IsReadOnly(this, &SGraphPinText::GetDefaultValueIsReadOnly )
				.OnTextCommitted( this, &SGraphPinText::SetTypeInValue )
				.ForegroundColor( FSlateColor::UseForeground() );
}

FText SGraphPinText::GetTypeInValue() const
{
	return GraphPinObj->DefaultTextValue;
}

void SGraphPinText::SetTypeInValue(const FText& NewTypeInValue, ETextCommit::Type /*CommitInfo*/)
{
	// Update the localization string.
	GraphPinObj->GetSchema()->TrySetDefaultText(*GraphPinObj, NewTypeInValue);
}

bool SGraphPinText::GetDefaultValueIsReadOnly() const
{
	return GraphPinObj->bDefaultValueIsReadOnly;
}