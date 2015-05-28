// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "GuidStructCustomization.h"


#define LOCTEXT_NAMESPACE "FGuidStructCustomization"


/* IPropertyTypeCustomization interface
 *****************************************************************************/

void FGuidStructCustomization::CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	PropertyHandle = StructPropertyHandle;
	InputValid = true;

	TSharedPtr<SWidget> QuickSetSlotContent;

	// create quick-set menu if needed
	if (PropertyHandle->IsEditConst())
	{
		QuickSetSlotContent = SNullWidget::NullWidget;
	}
	else
	{
		FMenuBuilder QuickSetMenuBuilder(true, nullptr);
		{
			FUIAction GenerateAction(FExecuteAction::CreateSP(this, &FGuidStructCustomization::HandleGuidActionClicked, EPropertyEditorGuidActions::Generate));
			QuickSetMenuBuilder.AddMenuEntry(LOCTEXT("GenerateAction", "Generate"), LOCTEXT("GenerateActionHint", "Generate a new random globally unique identifier (GUID)."), FSlateIcon(), GenerateAction);

			FUIAction InvalidateAction(FExecuteAction::CreateSP(this, &FGuidStructCustomization::HandleGuidActionClicked, EPropertyEditorGuidActions::Invalidate));
			QuickSetMenuBuilder.AddMenuEntry(LOCTEXT("InvalidateAction", "Invalidate"), LOCTEXT("InvalidateActionHint", "Set an invalid globally unique identifier (GUID)."), FSlateIcon(), InvalidateAction);
		}

		QuickSetSlotContent = SNew(SComboButton)
			.ContentPadding(FMargin(6.0, 2.0))
			.MenuContent()
			[
				QuickSetMenuBuilder.MakeWidget()
			];
	}

	// create struct header
	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(325.0f)
		.MaxDesiredWidth(325.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					// text box
					SAssignNew(TextBox, SEditableTextBox)
						.ClearKeyboardFocusOnCommit(false)
						.IsEnabled(!PropertyHandle->IsEditConst())
						.ForegroundColor(this, &FGuidStructCustomization::HandleTextBoxForegroundColor)
						.OnTextChanged(this, &FGuidStructCustomization::HandleTextBoxTextChanged)
						.OnTextCommitted(this, &FGuidStructCustomization::HandleTextBoxTextCommited)
						.SelectAllTextOnCommit(true)
						.Text(this, &FGuidStructCustomization::HandleTextBoxText)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					// quick set menu
					QuickSetSlotContent.ToSharedRef()
				]
		];
}


void FGuidStructCustomization::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	// do nothing
}


/* FGuidStructCustomization implementation
 *****************************************************************************/

void FGuidStructCustomization::SetGuidValue( const FGuid& Guid )
{
	for (uint32 ChildIndex = 0; ChildIndex < 4; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		ChildHandle->SetValue((int32)Guid[ChildIndex]);
	}
}


/* FGuidStructCustomization callbacks
 *****************************************************************************/

void FGuidStructCustomization::HandleGuidActionClicked( EPropertyEditorGuidActions::Type Action )
{
	if (Action == EPropertyEditorGuidActions::Generate)
	{
		SetGuidValue(FGuid::NewGuid());
	}
	else if (Action == EPropertyEditorGuidActions::Invalidate)
	{
		SetGuidValue(FGuid());
	}
}


FSlateColor FGuidStructCustomization::HandleTextBoxForegroundColor( ) const
{
	if (InputValid)
	{
		static const FName InvertedForegroundName("InvertedForeground");
		return FEditorStyle::GetSlateColor(InvertedForegroundName);
	}

	return FLinearColor::Red;
}


FText FGuidStructCustomization::HandleTextBoxText( ) const
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);
	
	if (RawData.Num() != 1)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	if (RawData[0] == nullptr)
	{
		return FText::GetEmpty();
	}

	return FText::FromString(((FGuid*)RawData[0])->ToString(EGuidFormats::DigitsWithHyphensInBraces));
}


void FGuidStructCustomization::HandleTextBoxTextChanged( const FText& NewText )
{
	FGuid Guid;
	InputValid = FGuid::Parse(NewText.ToString(), Guid);
}


void FGuidStructCustomization::HandleTextBoxTextCommited( const FText& NewText, ETextCommit::Type CommitInfo )
{
	FGuid ParsedGuid;
								
	if (FGuid::Parse(NewText.ToString(), ParsedGuid))
	{
		SetGuidValue(ParsedGuid);
	}
}


#undef LOCTEXT_NAMESPACE
