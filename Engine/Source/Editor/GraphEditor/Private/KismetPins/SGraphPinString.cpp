// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphPinString.h"
#include "Editor/UnrealEd/Public/ScopedTransaction.h"

void SGraphPinString::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinString::GetDefaultValueWidget()
{
	return SNew(SBox)
		.MinDesiredWidth(18.0f)
		.MaxDesiredHeight(200)
		[
			SNew(SMultiLineEditableTextBox)
			.Style(FEditorStyle::Get(), "Graph.EditableTextBox")
			.Text(this, &SGraphPinString::GetTypeInValue)
			.SelectAllTextWhenFocused(true)
			.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
			.IsReadOnly(this, &SGraphPinString::GetDefaultValueIsReadOnly)
			.OnTextCommitted(this, &SGraphPinString::SetTypeInValue)
			.ForegroundColor(FSlateColor::UseForeground())
			.WrapTextAt(400)
			.ModiferKeyForNewLine(EModifierKey::Shift)
		];
}

FText SGraphPinString::GetTypeInValue() const
{
	return FText::FromString( GraphPinObj->GetDefaultAsString() );
}

void SGraphPinString::SetTypeInValue(const FText& NewTypeInValue, ETextCommit::Type /*CommitInfo*/)
{
	if(GraphPinObj->GetDefaultAsString() != NewTypeInValue.ToString())
	{
		const FScopedTransaction Transaction( NSLOCTEXT("GraphEditor", "ChangeStringPinValue", "Change String Pin Value" ) );
		GraphPinObj->Modify();

		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, NewTypeInValue.ToString());
	}
}

bool SGraphPinString::GetDefaultValueIsReadOnly() const
{
	return GraphPinObj->bDefaultValueIsReadOnly;
}