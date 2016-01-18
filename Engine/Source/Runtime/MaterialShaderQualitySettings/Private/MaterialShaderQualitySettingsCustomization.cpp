// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "MaterialShaderQualitySettingsPrivatePCH.h"
#include "MaterialShaderQualitySettingsCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyEditing.h"
#include "ShaderPlatformQualitySettings.h"
#include "SListView.h"
#include "SCompoundWidget.h"
#include "SceneTypes.h"
#include "ShaderQualityOverridesListItem.h"

#define LOCTEXT_NAMESPACE "MaterialShaderQualitySettings"

//////////////////////////////////////////////////////////////////////////
// FMaterialShaderQualitySettingsCustomization
namespace FMaterialShaderQualitySettingsCustomizationConstants
{
	const FText DisabledTip = LOCTEXT("GitHubSourceRequiredToolTip", "This requires GitHub source.");
}

TSharedRef<IDetailCustomization> FMaterialShaderQualitySettingsCustomization::MakeInstance(const FOnUpdateMaterialShaderQuality InUpdateMaterials)
{
	return MakeShareable(new FMaterialShaderQualitySettingsCustomization(InUpdateMaterials));
}

FMaterialShaderQualitySettingsCustomization::FMaterialShaderQualitySettingsCustomization(const FOnUpdateMaterialShaderQuality InUpdateMaterials)
{
	UpdateMaterials = InUpdateMaterials;
}

class SQualityListItem
	: public SMultiColumnTableRow< TSharedPtr<struct FShaderQualityOverridesListItem> >
{
public:

	SLATE_BEGIN_ARGS(SQualityListItem) { }
	SLATE_ARGUMENT(TSharedPtr<FShaderQualityOverridesListItem>, Item)
	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		Item = InArgs._Item;
		check(Item.IsValid());
		const auto args = FSuperRowType::FArguments();
		SMultiColumnTableRow< TSharedPtr<FShaderQualityOverridesListItem> >::Construct(args, InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == TEXT("Quality Option"))
		{
			return	SNew(SBox)
				.HeightOverride(20)
				.Padding(FMargin(3, 0))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->RangeName))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				];
		}
		else
		{
			EMaterialQualityLevel::Type MaterialQualityLevel = EMaterialQualityLevel::Num;

			if (ColumnName == TEXT("Low"))
			{
				MaterialQualityLevel = EMaterialQualityLevel::Low;
			}
			else if (ColumnName == TEXT("Medium"))
			{
				MaterialQualityLevel = EMaterialQualityLevel::Medium;
			}
			else if (ColumnName == TEXT("High"))
			{
				MaterialQualityLevel = EMaterialQualityLevel::High;
			}

			if (MaterialQualityLevel < EMaterialQualityLevel::Num)
			{
				return	SNew(SBox)
					.HeightOverride(20)
					.Padding(FMargin(3, 0))
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.OnCheckStateChanged(this, &SQualityListItem::OnQualityRangeChanged, MaterialQualityLevel)
						.IsChecked(this, &SQualityListItem::IsQualityRangeSet, MaterialQualityLevel)
						.IsEnabled(this, &SQualityListItem::IsEnabled, MaterialQualityLevel)
					];
			}
		}
	
		return SNullWidget::NullWidget;
	}

private:

	ECheckBoxState IsQualityRangeSet(EMaterialQualityLevel::Type QualityLevel) const
	{
		bool* State = Item->QualityProperty->ContainerPtrToValuePtr<bool>(&Item->SettingContainer->GetQualityOverrides(QualityLevel));
		return *State ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void OnQualityRangeChanged(ECheckBoxState NewCheckedState, EMaterialQualityLevel::Type QualityLevel)
	{
		bool* State = Item->QualityProperty->ContainerPtrToValuePtr<bool>(&Item->SettingContainer->GetQualityOverrides(QualityLevel));
		*State = NewCheckedState == ECheckBoxState::Checked;

		// save changes.
		// TODO: use property handles so FSettingSection::Save() does the config saving.
		Item->SettingContainer->UpdateDefaultConfigFile();
	}

	bool IsEnabled(EMaterialQualityLevel::Type QualityLevel) const
	{
		bool* bEnableOverride = &Item->SettingContainer->GetQualityOverrides(QualityLevel).bEnableOverride;
		bool* State = Item->QualityProperty->ContainerPtrToValuePtr<bool>(&Item->SettingContainer->GetQualityOverrides(QualityLevel));

		// Enable all if EnableOverride is set,
		// but also ensure all but the high quality override flag remains enabled.
		const bool bIsOverrideProperty = bEnableOverride == State;
		const bool bIsHighQualityOverrideProperty = (bIsOverrideProperty && QualityLevel == EMaterialQualityLevel::High);
		return (!bIsOverrideProperty && *bEnableOverride == true) || (bIsOverrideProperty && !bIsHighQualityOverrideProperty);
	}

	TSharedPtr< FShaderQualityOverridesListItem > Item;
};

TSharedRef<ITableRow> FMaterialShaderQualitySettingsCustomization::HandleGenerateQualityWidget(TSharedPtr<FShaderQualityOverridesListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SQualityListItem, OwnerTable)
		.Item(InItem)
		;
}

FReply FMaterialShaderQualitySettingsCustomization::UpdatePreviewShaders()
{
	UpdateMaterials.ExecuteIfBound();
	return FReply::Handled();
}

void FMaterialShaderQualitySettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	IDetailCategoryBuilder& ForwardRenderingCategory = DetailLayout.EditCategory(TEXT("Forward Rendering overrides"));

	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	UShaderPlatformQualitySettings* TargetObject = nullptr;

	if (ObjectsBeingCustomized.Num() >= 1)
	{
		TargetObject = Cast<UShaderPlatformQualitySettings>(ObjectsBeingCustomized[0].Get());
	}
	check(TargetObject != nullptr);

	// Build a list of FShaderQualityRange properties.
	QualityOverrideListSource.Empty();
	// iterate through possible override properties
	for (UBoolProperty* Prop : TFieldRange<UBoolProperty>(FMaterialQualityOverrides::StaticStruct()))
	{
		QualityOverrideListSource.Add(MakeShareable(
			new FShaderQualityOverridesListItem(Prop->GetMetaData("DisplayName"), Prop, TargetObject)));
	}

	ForwardRenderingCategory.AddCustomRow(LOCTEXT("ForwardRenderingMaterialOverrides", "Forward Rendering Material Overrides"))
		[
			SAssignNew(MaterialQualityOverridesListView, SMaterialQualityOverridesListView)
			.ItemHeight(20.0f)
			.ListItemsSource(&QualityOverrideListSource)
			.OnGenerateRow(this, &FMaterialShaderQualitySettingsCustomization::HandleGenerateQualityWidget)
			.SelectionMode(ESelectionMode::None)
			.HeaderRow(
			SNew(SHeaderRow)
			// 
			+ SHeaderRow::Column("Quality Option")
			.HAlignCell(HAlign_Left)
			.FillWidth(1)
			.HeaderContentPadding(FMargin(0, 3))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MaterialQualityList_Category", "Quality Option"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			// 
			+ SHeaderRow::Column("Low")
				.HAlignCell(HAlign_Left)
				.FillWidth(1)
				.HeaderContentPadding(FMargin(0, 3))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MaterialQualityList_Low", "Low"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			// 
			+ SHeaderRow::Column("Medium")
				.HAlignCell(HAlign_Left)
				.FillWidth(1)
				.HeaderContentPadding(FMargin(0, 3))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MaterialQualityList_Medium", "Medium"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			// 
			+ SHeaderRow::Column("High")
				.HAlignCell(HAlign_Left)
				.FillWidth(1)
				.HeaderContentPadding(FMargin(0, 3))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MaterialQualityList_High", "High"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			)
		];

	ForwardRenderingCategory.AddCustomRow(LOCTEXT("ForwardRenderingMaterialOverrides", "Forward Rendering Material Overrides"))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("UpdatePreviewShaders", "Update preview shaders"))
			.ToolTipText(LOCTEXT("UpdatePreviewShadersButton_Tooltip", "Updates the editor to reflect changes to quality settings."))
			.OnClicked(this, &FMaterialShaderQualitySettingsCustomization::UpdatePreviewShaders)
		]
	];

}


//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR