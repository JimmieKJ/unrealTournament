// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphPinString.h"


void SGraphPinString::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinString::GetDefaultValueWidget()
{
	return SNew(SEditableTextBox)
		.Style( FEditorStyle::Get(), "Graph.EditableTextBox" )
		.Text( this, &SGraphPinString::GetTypeInValue )
		.SelectAllTextWhenFocused(true)
		.MinDesiredWidth( 18.0f )
		.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
		.IsReadOnly(this, &SGraphPinString::GetDefaultValueIsReadOnly )
		.OnTextCommitted( this, &SGraphPinString::SetTypeInValue )
		.ForegroundColor( FSlateColor::UseForeground() );
}

FText SGraphPinString::GetTypeInValue() const
{
	return FText::FromString( GraphPinObj->GetDefaultAsString() );
}

void SGraphPinString::SetTypeInValue(const FText& NewTypeInValue, ETextCommit::Type /*CommitInfo*/)
{
	GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, NewTypeInValue.ToString());
}

bool SGraphPinString::GetDefaultValueIsReadOnly() const
{
	return GraphPinObj->bDefaultValueIsReadOnly;
}