// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "SourceCodeAccessSettingsDetails.h"
#include "ISourceCodeAccessModule.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "SourceCodeAccessSettingsDetails"

TSharedRef<IDetailCustomization> FSourceCodeAccessSettingsDetails::MakeInstance()
{
	return MakeShareable(new FSourceCodeAccessSettingsDetails());
}

void FSourceCodeAccessSettingsDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	TSharedRef<IPropertyHandle> PreferredProviderPropertyHandle = DetailLayout.GetProperty("PreferredAccessor");
	DetailLayout.HideProperty("PreferredAccessor");

	// regenerate accessors list
	Accessors.Empty();

	const int32 FeatureCount = IModularFeatures::Get().GetModularFeatureImplementationCount("SourceCodeAccessor");
	for(int32 FeatureIndex = 0; FeatureIndex < FeatureCount; FeatureIndex++)
	{
		IModularFeature* Feature = IModularFeatures::Get().GetModularFeatureImplementation("SourceCodeAccessor", FeatureIndex);
		check(Feature);

		ISourceCodeAccessor& Accessor = *static_cast<ISourceCodeAccessor*>(Feature);
		if(Accessor.GetFName() != FName("None"))
		{
			Accessors.Add(MakeShareable(new FAccessorItem(Accessor.GetNameText(), Accessor.GetFName())));
		}
	}

	IDetailCategoryBuilder& AccessorCategory = DetailLayout.EditCategory( "Accessor" );
	AccessorCategory.AddCustomRow( LOCTEXT("PreferredAccessorFilterString", "Source Code Editor") )
	.NameContent()
	[
		PreferredProviderPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(113)
	.MaxDesiredWidth(113)
	[
		SNew(SComboBox< TSharedPtr<FAccessorItem>>)
		.ToolTipText(LOCTEXT("PreferredAccessorToolTip", "Choose the way to access source code."))
		.OptionsSource(&Accessors)
		.OnSelectionChanged(this, &FSourceCodeAccessSettingsDetails::OnSelectionChanged, PreferredProviderPropertyHandle)
		.ContentPadding(2)
		.OnGenerateWidget(this, &FSourceCodeAccessSettingsDetails::OnGenerateWidget)
		.Content()
		[
			SNew(STextBlock)
			.Text(this, &FSourceCodeAccessSettingsDetails::GetAccessorText)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
		]
	];
}

TSharedRef<SWidget> FSourceCodeAccessSettingsDetails::OnGenerateWidget( TSharedPtr<FAccessorItem> InItem )
{
	return SNew(STextBlock)
		.Text(InItem->Text);
}

void FSourceCodeAccessSettingsDetails::OnSelectionChanged(TSharedPtr<FAccessorItem> InItem, ESelectInfo::Type InSeletionInfo, TSharedRef<IPropertyHandle> PreferredProviderPropertyHandle)
{
	PreferredProviderPropertyHandle->SetValue(InItem->Name.ToString());
	ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
	SourceCodeAccessModule.SetAccessor(InItem->Name);
}

FText FSourceCodeAccessSettingsDetails::GetAccessorText() const
{
	ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
	return SourceCodeAccessModule.GetAccessor().GetNameText();
}

#undef LOCTEXT_NAMESPACE