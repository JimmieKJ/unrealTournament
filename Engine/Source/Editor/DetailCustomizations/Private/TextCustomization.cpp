// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "TextCustomization.h"
#include "IPropertyUtilities.h"

namespace
{
	class STextPropertyWidget : public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(STextPropertyWidget)
			: _Font( FEditorStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont") ) ) 
			{}
			SLATE_ATTRIBUTE( FSlateFontInfo, Font )
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& Arguments, const TSharedRef<IPropertyHandle>& InPropertyHandle, const TSharedPtr<IPropertyUtilities>& InPropertyUtilities);
		void GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth );
		bool SupportsKeyboardFocus() const;
		FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent );
		void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
		bool CanEdit() const;
		bool IsReadOnly() const;

	private:
		TSharedPtr<IPropertyHandle> PropertyHandle;

		TSharedPtr<IPropertyUtilities> PropertyUtilities;

		TSharedPtr< class SWidget > PrimaryWidget;

		TSharedPtr<SMultiLineEditableTextBox> MultiLineWidget;

		TSharedPtr<SEditableTextBox> SingleLineWidget;

		TOptional< float > PreviousHeight;

		bool bIsMultiLine;

		static FText MultipleValuesText;
	};

	FText STextPropertyWidget::MultipleValuesText(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"));

	void STextPropertyWidget::Construct(const FArguments& InArgs, const TSharedRef<IPropertyHandle>& InPropertyHandle, const TSharedPtr<IPropertyUtilities>& InPropertyUtilities)
	{
		PropertyHandle = InPropertyHandle;
		PropertyUtilities = InPropertyUtilities;

		const auto& GetTextValue = [this]() -> FText
		{
			FText TextValue;

			TArray<const void*> RawData;
			PropertyHandle->AccessRawData(RawData);
			if (RawData.Num() == 1)
			{
				const FText* RawDatum = reinterpret_cast<const FText*>(RawData.Top());
				if (RawDatum)
				{
					TextValue = *RawDatum;
				}
			}
			else if(RawData.Num() > 1)
			{
				TextValue = MultipleValuesText;
			}
			else
			{
				TextValue = FText::GetEmpty();
			}

			return TextValue;
		};

		const auto& OnTextCommitted = [this](const FText& NewText, ETextCommit::Type CommitInfo)
		{
			TArray<void*> RawData;
			PropertyHandle->AccessRawData(RawData);

			// Don't commit the Multiple Values text if there are multiple properties being set
			if (RawData.Num() > 0 && (RawData.Num() == 1 || NewText.ToString() != MultipleValuesText.ToString()))
			{
				PropertyHandle->NotifyPreChange();

				for (void* const RawDatum : RawData)
				{
					FText& PropertyValue = *(reinterpret_cast<FText* const>(RawDatum));

					// FText::FromString on the result of FText::ToString is intentional. For now, we want to nuke any namespace/key info and let it get regenerated from scratch,
					// rather than risk adopting whatever came through some chain of calls. This will be replaced when preserving of identity is implemented.
					PropertyValue = FText::FromString(NewText.ToString());
				}

				PropertyHandle->NotifyPostChange();
				PropertyHandle->NotifyFinishedChangingProperties();
			}
		};

		TSharedPtr<SHorizontalBox> HorizontalBox;

		bool bIsPassword = PropertyHandle->GetBoolMetaData("PasswordField");
		bIsMultiLine = PropertyHandle->GetBoolMetaData("MultiLine");
		if(bIsMultiLine)
		{
			ChildSlot
				[
					SAssignNew(HorizontalBox, SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(MultiLineWidget, SMultiLineEditableTextBox)
						.Text_Lambda(GetTextValue)
						.Font(InArgs._Font)
						.SelectAllTextWhenFocused(false)
						.ClearKeyboardFocusOnCommit(false)
						.OnTextCommitted_Lambda(OnTextCommitted)
						.SelectAllTextOnCommit(false)
						.IsReadOnly(this, &STextPropertyWidget::IsReadOnly)
						.AutoWrapText(true)
						.ModiferKeyForNewLine(EModifierKey::Shift)
						.IsPassword(bIsPassword)
					]
				];

			PrimaryWidget = MultiLineWidget;
		}
		else
		{
			ChildSlot
				[
					SAssignNew(HorizontalBox, SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew( SingleLineWidget, SEditableTextBox )
						.Text_Lambda(GetTextValue)
						.Font( InArgs._Font )
						.SelectAllTextWhenFocused( true )
						.ClearKeyboardFocusOnCommit(false)
						.OnTextCommitted_Lambda(OnTextCommitted)
						.SelectAllTextOnCommit( true )
						.IsReadOnly(this, &STextPropertyWidget::IsReadOnly)
						.IsPassword(bIsPassword)

					]
				];

			PrimaryWidget = SingleLineWidget;
		}

		SetEnabled( TAttribute<bool>( this, &STextPropertyWidget::CanEdit ) );
	}

	void STextPropertyWidget::GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth )
	{
		if(bIsMultiLine)
		{
			OutMinDesiredWidth = 250.0f;
		}
		else
		{
			OutMinDesiredWidth = 125.0f;
		}

		OutMaxDesiredWidth = 600.0f;
	}

	bool STextPropertyWidget::SupportsKeyboardFocus() const
	{
		return PrimaryWidget.IsValid() && PrimaryWidget->SupportsKeyboardFocus();
	}

	FReply STextPropertyWidget::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
	{
		// Forward keyboard focus to our editable text widget
		return FReply::Handled().SetUserFocus(PrimaryWidget.ToSharedRef(), InFocusEvent.GetCause());
	}

	void STextPropertyWidget::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
	{
		const float CurrentHeight = AllottedGeometry.GetLocalSize().Y;
		if (bIsMultiLine && PreviousHeight.IsSet() && PreviousHeight.GetValue() != CurrentHeight && PropertyUtilities.IsValid())
		{
			PropertyUtilities->RequestRefresh();
		}
		PreviousHeight = CurrentHeight;
	}

	bool STextPropertyWidget::CanEdit() const
	{
		return PropertyHandle.IsValid() ? !PropertyHandle->IsEditConst() : true;
	}

	bool STextPropertyWidget::IsReadOnly() const
	{
		return !CanEdit();
	}
}

void FTextCustomization::CustomizeHeader( TSharedRef<class IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils )
{
	const bool bIsMultiLine = InPropertyHandle->GetProperty()->GetBoolMetaData("MultiLine");

	HeaderRow.FilterString(InPropertyHandle->GetPropertyDisplayName())
		.NameContent()
		[
			InPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(bIsMultiLine ? 250.f : 125.f)
		.MaxDesiredWidth(600.f)
		[
			SNew(STextPropertyWidget, InPropertyHandle, PropertyTypeCustomizationUtils.GetPropertyUtilities())
		];
}

void FTextCustomization::CustomizeChildren( TSharedRef<class IPropertyHandle> InPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils )
{

}