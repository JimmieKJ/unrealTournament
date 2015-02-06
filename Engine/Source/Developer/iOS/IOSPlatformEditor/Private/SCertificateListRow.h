// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Implements a row widget for the certificate list view.
 */
class SCertificateListRow
	: public SMultiColumnTableRow<CertificatePtr>
{
public:

	SLATE_BEGIN_ARGS(SCertificateListRow) { }
		SLATE_ARGUMENT(CertificatePtr, Certificate)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The arguments.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		Certificate = InArgs._Certificate;
		
		SMultiColumnTableRow<CertificatePtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

public:

	/**
	 * Generates the widget for the specified column.
	 *
	 * @param ColumnName The name of the column to generate the widget for.
	 * @return The widget.
	 */
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		if (ColumnName == TEXT("Name"))
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SCertificateListRow::HandleSelectedColorAndOpacity)
						.Text(this, &SCertificateListRow::HandleNameText)
				];
		}
		else if (ColumnName == TEXT("Status"))
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SCertificateListRow::HandleStatusTextColorAndOpacity)
						.Text(this, &SCertificateListRow::HandleStatusTextBlockText)
				];
		}
		else if (ColumnName == TEXT("Expires"))
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.ColorAndOpacity(this, &SCertificateListRow::HandleSelectedColorAndOpacity)
					.Text(this, &SCertificateListRow::HandleExpiresText)
				];
		}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:

	// Callback for getting the text in the 'Name' column.
	FText HandleNameText( ) const
	{
		return FText::FromString(Certificate->Name);
	}

	// Callback for getting the status text.
	FText HandleStatusTextBlockText( ) const
	{
		if (Certificate->Status == TEXT("EXPIRED"))
		{
			return FText::FromString(TEXT("Expired"));
		}
		return FText::FromString(TEXT("Valid"));
	}

	// Callback for getting the status text color.
	FSlateColor HandleStatusTextColorAndOpacity( ) const
	{
		if (Certificate->Status == TEXT("EXPIRED"))
		{
			return FSlateColor(FLinearColor(1.0f, 0.0f, 0.0f));
		}
		else if (Certificate->bSelected)
		{
			return FSlateColor(FLinearColor(0.0f, 1.0f, 0.0f));
		}
		return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f));
	}

	// Callback for getting the status text color.
	FSlateColor HandleSelectedColorAndOpacity( ) const
	{
		if (Certificate->bSelected)
		{
			return FSlateColor(FLinearColor(0.0f, 1.0f, 0.0f));
		}
		return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f));
	}

	// Callback for getting the text in the 'Expires' column.
	FText HandleExpiresText( ) const
	{
		return FText::FromString(Certificate->Expires);
	}

private:

	// Holds the target device service used to populate this row.
	CertificatePtr Certificate;
};
