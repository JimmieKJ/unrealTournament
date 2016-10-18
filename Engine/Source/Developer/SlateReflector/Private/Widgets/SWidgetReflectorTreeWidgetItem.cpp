// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateReflectorPrivatePCH.h"
#include "SWidgetReflectorTreeWidgetItem.h"
#include "SHyperlink.h"


/* SMultiColumnTableRow overrides
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SWidget> SReflectorTreeWidgetItem::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == TEXT("WidgetName"))
	{
		return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SExpanderArrow, SharedThis(this))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &SReflectorTreeWidgetItem::GetWidgetType)
			.ColorAndOpacity(this, &SReflectorTreeWidgetItem::GetTint)
		];
	}
	else if (ColumnName == TEXT("WidgetInfo"))
	{
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 0.0f))
			[
				SNew(SHyperlink)
				.Text(this, &SReflectorTreeWidgetItem::GetReadableLocationAsText)
				.OnNavigate(this, &SReflectorTreeWidgetItem::HandleHyperlinkNavigate)
			];
	}
	else if (ColumnName == "Visibility")
	{
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 0.0f))
			[
				SNew(STextBlock)
					.Text(this, &SReflectorTreeWidgetItem::GetVisibilityAsString)
			];
	}
	else if (ColumnName == "ForegroundColor")
	{
		const FSlateColor Foreground = WidgetInfo->GetWidgetForegroundColor();

		return SNew(SBorder)
			// Show unset color as an empty space.
			.Visibility(Foreground.IsColorSpecified() ? EVisibility::Visible : EVisibility::Hidden)
			// Show a checkerboard background so we can see alpha values well
			.BorderImage(FCoreStyle::Get().GetBrush("Checkerboard"))
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 0.0f))
			[
				// Show a color block
				SNew(SColorBlock)
					.Color(Foreground.GetSpecifiedColor())
					.Size(FVector2D(16.0f, 16.0f))
			];
	}
	else if (ColumnName == "Address")
	{
		const FText Address = FText::FromString(WidgetInfo->GetWidgetAddress());
		
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 0.0f))
			[
				SNew(SHyperlink)
				.ToolTipText(NSLOCTEXT("SWidgetReflector", "ClickToCopy", "Click to copy address."))
				.Text(Address)
				.OnNavigate_Lambda([Address](){ FPlatformMisc::ClipboardCopy(*Address.ToString()); })
			];
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SReflectorTreeWidgetItem::HandleHyperlinkNavigate()
{
	FAssetData AssetData = WidgetInfo->GetWidgetAssetData();
	if ( AssetData.IsValid() )
	{
		if ( OnAccessAsset.IsBound() )
		{
			AssetData.GetPackage();
			OnAccessAsset.Execute(AssetData.GetAsset());
			return;
		}
	}

	if ( OnAccessSourceCode.IsBound() )
	{
		OnAccessSourceCode.Execute(GetWidgetFile(), GetWidgetLineNumber(), 0);
	}
}
