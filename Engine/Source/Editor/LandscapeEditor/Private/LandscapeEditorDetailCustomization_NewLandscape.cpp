// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorPrivatePCH.h"
#include "LandscapeEdMode.h"
#include "LandscapeEditorCommands.h"
#include "LandscapeEditorObject.h"
#include "LandscapeEditorDetails.h"
#include "LandscapeEditorDetailCustomizations.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"

#include "SLandscapeEditor.h"
#include "DlgPickAssetPath.h"
#include "SVectorInputBox.h"
#include "SRotatorInputBox.h"
#include "PackageTools.h"
//#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "DesktopPlatformModule.h"
#include "MainFrame.h"
#include "AssetRegistryModule.h"
#include "ImageWrapper.h"

#include "Landscape.h"
#include "LandscapeLayerInfoObject.h"
#include "TutorialMetaData.h"
#include "SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.NewLandscape"

const int32 FLandscapeEditorDetailCustomization_NewLandscape::SectionSizes[] = {7, 15, 31, 63, 127, 255};
const int32 FLandscapeEditorDetailCustomization_NewLandscape::NumSections[] = {1, 2};

TSharedRef<IDetailCustomization> FLandscapeEditorDetailCustomization_NewLandscape::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorDetailCustomization_NewLandscape);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorDetailCustomization_NewLandscape::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if (!IsToolActive("NewLandscape"))
	{
		return;
	}

	IDetailCategoryBuilder& NewLandscapeCategory = DetailBuilder.EditCategory("New Landscape");

	NewLandscapeCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SUniformGridPanel)
		.SlotPadding(FMargin(10, 2))
		+ SUniformGridPanel::Slot(0, 0)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "RadioButton")
			.IsChecked(this, &FLandscapeEditorDetailCustomization_NewLandscape::NewLandscapeModeIsChecked, ENewLandscapePreviewMode::NewLandscape)
			.OnCheckStateChanged(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnNewLandscapeModeChanged, ENewLandscapePreviewMode::NewLandscape)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NewLandscape", "Create New"))
			]
		]
		+ SUniformGridPanel::Slot(1, 0)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "RadioButton")
			.IsChecked(this, &FLandscapeEditorDetailCustomization_NewLandscape::NewLandscapeModeIsChecked, ENewLandscapePreviewMode::ImportLandscape)
			.OnCheckStateChanged(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnNewLandscapeModeChanged, ENewLandscapePreviewMode::ImportLandscape)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ImportLandscape", "Import from File"))
			]
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_HeightmapFilename = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_HeightmapFilename));
	TSharedRef<IPropertyHandle> PropertyHandle_HeightmapError = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_HeightmapError));
	DetailBuilder.HideProperty(PropertyHandle_HeightmapError);
	PropertyHandle_HeightmapFilename->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnImportHeightmapFilenameChanged));
	NewLandscapeCategory.AddProperty(PropertyHandle_HeightmapFilename)
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)))
	.CustomWidget()
	.NameContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SErrorText)
			.Visibility_Static(&GetHeightmapErrorVisibility, PropertyHandle_HeightmapError)
			.ErrorText(NSLOCTEXT("UnrealEd", "Error", "!"))
			.ToolTip(
				SNew(SToolTip)
				.Text_Static(&GetHeightmapErrorText, PropertyHandle_HeightmapError)
			)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			PropertyHandle_HeightmapFilename->CreatePropertyNameWidget()
		]
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Font(DetailBuilder.GetDetailFont())
			.Text_Static(&GetImportHeightmapFilenameString, PropertyHandle_HeightmapFilename)
			.OnTextCommitted_Static(&SetImportHeightmapFilenameString, PropertyHandle_HeightmapFilename)
			.HintText(LOCTEXT("Import_HeightmapNotSet", "(Please specify a heightmap)"))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(1,0,0,0)
		[
			SNew(SButton)
			//.Font(DetailBuilder.GetDetailFont())
			.ContentPadding(FMargin(4, 0))
			.Text(NSLOCTEXT("UnrealEd", "GenericOpenDialog", "..."))
			.OnClicked_Static(&OnImportHeightmapFilenameButtonClicked, PropertyHandle_HeightmapFilename)
		]
	];

	NewLandscapeCategory.AddCustomRow(LOCTEXT("HeightmapResolution", "Heightmap Resolution"))
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)))
	.NameContent()
	[
		SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(LOCTEXT("HeightmapResolution", "Heightmap Resolution"))
		]
	]
	.ValueContent()
	[
		SNew(SBox)
		.Padding(FMargin(0,0,12,0)) // Line up with the other properties due to having no reset to default button
		[
			SNew(SComboButton)
			.OnGetMenuContent(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetImportLandscapeResolutionMenu)
			.ContentPadding(2)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Font(DetailBuilder.GetDetailFont())
				.Text(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetImportLandscapeResolution)
			]
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_Material = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Material));
	NewLandscapeCategory.AddProperty(PropertyHandle_Material);

	NewLandscapeCategory.AddCustomRow(LOCTEXT("LayersLabel", "Layers"))
	.Visibility(TAttribute<EVisibility>(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMaterialTipVisibility))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(15, 12, 0, 12)
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(LOCTEXT("Material_Tip","Hint: Assign a material to see landscape layers"))
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_Layers = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_Layers));
	NewLandscapeCategory.AddProperty(PropertyHandle_Layers);

	TSharedRef<IPropertyHandle> PropertyHandle_Location = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Location));
	TSharedRef<IPropertyHandle> PropertyHandle_Location_X = PropertyHandle_Location->GetChildHandle("X").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Location_Y = PropertyHandle_Location->GetChildHandle("Y").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Location_Z = PropertyHandle_Location->GetChildHandle("Z").ToSharedRef();
	NewLandscapeCategory.AddProperty(PropertyHandle_Location)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_Location->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(125.0f * 3.0f) // copied from FComponentTransformDetails
	.MaxDesiredWidth(125.0f * 3.0f)
	[
		SNew(SVectorInputBox)
		.bColorAxisLabels(true)
		.Font(DetailBuilder.GetDetailFont())
		.X_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Location_X)
		.Y_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Location_Y)
		.Z_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Location_Z)
		.OnXCommitted_Static(&SetPropertyValue<float>, PropertyHandle_Location_X)
		.OnYCommitted_Static(&SetPropertyValue<float>, PropertyHandle_Location_Y)
		.OnZCommitted_Static(&SetPropertyValue<float>, PropertyHandle_Location_Z)
	];

	TSharedRef<IPropertyHandle> PropertyHandle_Rotation = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Rotation));
	TSharedRef<IPropertyHandle> PropertyHandle_Rotation_Roll  = PropertyHandle_Rotation->GetChildHandle("Roll").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Rotation_Pitch = PropertyHandle_Rotation->GetChildHandle("Pitch").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Rotation_Yaw   = PropertyHandle_Rotation->GetChildHandle("Yaw").ToSharedRef();
	NewLandscapeCategory.AddProperty(PropertyHandle_Rotation)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_Rotation->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(125.0f * 3.0f) // copied from FComponentTransformDetails
	.MaxDesiredWidth(125.0f * 3.0f)
	[
		SNew(SRotatorInputBox)
		.bColorAxisLabels(true)
		.Font(DetailBuilder.GetDetailFont())
		.Roll_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Rotation_Roll)
		.Pitch_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Rotation_Pitch)
		.Yaw_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Rotation_Yaw)
		.OnYawCommitted_Static(&SetPropertyValue<float>, PropertyHandle_Rotation_Yaw) // not allowed to roll or pitch landscape
		.OnYawChanged_Lambda([=](float NewValue){ ensure(PropertyHandle_Rotation_Yaw->SetValue(NewValue, EPropertyValueSetFlags::InteractiveChange) == FPropertyAccess::Success); })
	];

	TSharedRef<IPropertyHandle> PropertyHandle_Scale = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Scale));
	TSharedRef<IPropertyHandle> PropertyHandle_Scale_X = PropertyHandle_Scale->GetChildHandle("X").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Scale_Y = PropertyHandle_Scale->GetChildHandle("Y").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Scale_Z = PropertyHandle_Scale->GetChildHandle("Z").ToSharedRef();
	NewLandscapeCategory.AddProperty(PropertyHandle_Scale)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_Scale->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(125.0f * 3.0f) // copied from FComponentTransformDetails
	.MaxDesiredWidth(125.0f * 3.0f)
	[
		SNew(SVectorInputBox)
		.bColorAxisLabels(true)
		.Font(DetailBuilder.GetDetailFont())
		.X_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Scale_X)
		.Y_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Scale_Y)
		.Z_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Scale_Z)
		.OnXCommitted_Static(&SetScale, PropertyHandle_Scale_X)
		.OnYCommitted_Static(&SetScale, PropertyHandle_Scale_Y)
		.OnZCommitted_Static(&SetScale, PropertyHandle_Scale_Z)
	];

	TSharedRef<IPropertyHandle> PropertyHandle_QuadsPerSection = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_QuadsPerSection));
	NewLandscapeCategory.AddProperty(PropertyHandle_QuadsPerSection)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_QuadsPerSection->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SComboButton)
		.OnGetMenuContent_Static(&GetSectionSizeMenu, PropertyHandle_QuadsPerSection)
		.ContentPadding(2)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text_Static(&GetSectionSize, PropertyHandle_QuadsPerSection)
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_SectionsPerComponent = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_SectionsPerComponent));
	NewLandscapeCategory.AddProperty(PropertyHandle_SectionsPerComponent)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_SectionsPerComponent->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SComboButton)
		.OnGetMenuContent_Static(&GetSectionsPerComponentMenu, PropertyHandle_SectionsPerComponent)
		.ContentPadding(2)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text_Static(&GetSectionsPerComponent, PropertyHandle_SectionsPerComponent)
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_ComponentCount = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_ComponentCount));
	TSharedRef<IPropertyHandle> PropertyHandle_ComponentCount_X = PropertyHandle_ComponentCount->GetChildHandle("X").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_ComponentCount_Y = PropertyHandle_ComponentCount->GetChildHandle("Y").ToSharedRef();
	NewLandscapeCategory.AddProperty(PropertyHandle_ComponentCount)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_ComponentCount->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SNumericEntryBox<int32>)
			.LabelVAlign(VAlign_Center)
			.Font(DetailBuilder.GetDetailFont())
			.MinValue(1)
			.MaxValue(32)
			.MinSliderValue(1)
			.MaxSliderValue(32)
			.AllowSpin(true)
			.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
			.Value_Static(&FLandscapeEditorDetailCustomization_Base::OnGetValue, PropertyHandle_ComponentCount_X)
			.OnValueChanged_Static(&FLandscapeEditorDetailCustomization_Base::OnValueChanged, PropertyHandle_ComponentCount_X)
			.OnValueCommitted_Static(&FLandscapeEditorDetailCustomization_Base::OnValueCommitted, PropertyHandle_ComponentCount_X)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(FText::FromString(FString().AppendChar(0xD7))) // Multiply sign
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SNumericEntryBox<int32>)
			.LabelVAlign(VAlign_Center)
			.Font(DetailBuilder.GetDetailFont())
			.MinValue(1)
			.MaxValue(32)
			.MinSliderValue(1)
			.MaxSliderValue(32)
			.AllowSpin(true)
			.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
			.Value_Static(&FLandscapeEditorDetailCustomization_Base::OnGetValue, PropertyHandle_ComponentCount_Y)
			.OnValueChanged_Static(&FLandscapeEditorDetailCustomization_Base::OnValueChanged, PropertyHandle_ComponentCount_Y)
			.OnValueCommitted_Static(&FLandscapeEditorDetailCustomization_Base::OnValueCommitted, PropertyHandle_ComponentCount_Y)
		]
	];

	NewLandscapeCategory.AddCustomRow(LOCTEXT("Resolution", "Overall Resolution"))
	.RowTag("LandscapeEditor.OverallResolution")
	.NameContent()
	[
		SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(LOCTEXT("Resolution", "Overall Resolution"))
			.ToolTipText(TAttribute<FText>(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetOverallResolutionTooltip))
		]
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SNumericEntryBox<int32>)
			.Font(DetailBuilder.GetDetailFont())
			.MinValue(1)
			.MaxValue(8192)
			.MinSliderValue(1)
			.MaxSliderValue(8192)
			.AllowSpin(true)
			//.MinSliderValue(TAttribute<TOptional<int32> >(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMinLandscapeResolution))
			//.MaxSliderValue(TAttribute<TOptional<int32> >(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMaxLandscapeResolution))
			.Value(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetLandscapeResolutionX)
			.OnValueChanged(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnChangeLandscapeResolutionX)
			.OnValueCommitted(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnCommitLandscapeResolutionX)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(FText::FromString(FString().AppendChar(0xD7))) // Multiply sign
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		.Padding(0,0,12,0) // Line up with the other properties due to having no reset to default button
		[
			SNew(SNumericEntryBox<int32>)
			.Font(DetailBuilder.GetDetailFont())
			.MinValue(1)
			.MaxValue(8192)
			.MinSliderValue(1)
			.MaxSliderValue(8192)
			.AllowSpin(true)
			//.MinSliderValue(TAttribute<TOptional<int32> >(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMinLandscapeResolution))
			//.MaxSliderValue(TAttribute<TOptional<int32> >(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMaxLandscapeResolution))
			.Value(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetLandscapeResolutionY)
			.OnValueChanged(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnChangeLandscapeResolutionY)
			.OnValueCommitted(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnCommitLandscapeResolutionY)
		]
	];

	NewLandscapeCategory.AddCustomRow(LOCTEXT("TotalComponents", "Total Components"))
	.RowTag("LandscapeEditor.TotalComponents")
	.NameContent()
	[
		SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(LOCTEXT("TotalComponents", "Total Components"))
			.ToolTipText(LOCTEXT("NewLandscape_TotalComponents", "The total number of components that will be created for this landscape."))
		]
	]
	.ValueContent()
	[
		SNew(SBox)
		.Padding(FMargin(0,0,12,0)) // Line up with the other properties due to having no reset to default button
		[
			SNew(SEditableTextBox)
			.IsReadOnly(true)
			.Font(DetailBuilder.GetDetailFont())
			.Text(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetTotalComponentCount)
		]
	];

	NewLandscapeCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility_Static(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::NewLandscape)
			.Text(LOCTEXT("FillWorld", "Fill World"))
			.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("FillWorldButton"), TEXT("LevelEditorToolBox")))
			.OnClicked(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnFillWorldButtonClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility_Static(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)
			.Text(LOCTEXT("FitToData", "Fit To Data"))
			.AddMetaData<FTagMetaData>(TEXT("ImportButton"))
			.OnClicked(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnFitImportDataButtonClicked)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		//[
		//]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility_Static(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::NewLandscape)
			.Text(LOCTEXT("Create", "Create"))
			.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("CreateButton"), TEXT("LevelEditorToolBox")))
			.OnClicked(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnCreateButtonClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility_Static(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)
			.Text(LOCTEXT("Import", "Import"))
			.OnClicked(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnCreateButtonClicked)
			.IsEnabled(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetImportButtonIsEnabled)
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText FLandscapeEditorDetailCustomization_NewLandscape::GetOverallResolutionTooltip() const
{
	return (GetEditorMode() && GetEditorMode()->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
	? LOCTEXT("ImportLandscape_OverallResolution", "Overall final resolution of the imported landscape in vertices")
	: LOCTEXT("NewLandscape_OverallResolution", "Overall final resolution of the new landscape in vertices");
}

void FLandscapeEditorDetailCustomization_NewLandscape::SetScale(float NewValue, ETextCommit::Type, TSharedRef<IPropertyHandle> PropertyHandle)
{
	float OldValue = 0;
	PropertyHandle->GetValue(OldValue);

	if (NewValue == 0)
	{
		if (OldValue < 0)
		{
			NewValue = -1;
		}
		else
		{
			NewValue = 1;
		}
	}

	ensure(PropertyHandle->SetValue(NewValue) == FPropertyAccess::Success);

	// Make X and Y scale match
	FName PropertyName = PropertyHandle->GetProperty()->GetFName();
	if (PropertyName == "X")
	{
		TSharedRef<IPropertyHandle> PropertyHandle_Y = PropertyHandle->GetParentHandle()->GetChildHandle("Y").ToSharedRef();
		ensure(PropertyHandle_Y->SetValue(NewValue) == FPropertyAccess::Success);
	}
	else if (PropertyName == "Y")
	{
		TSharedRef<IPropertyHandle> PropertyHandle_X = PropertyHandle->GetParentHandle()->GetChildHandle("X").ToSharedRef();
		ensure(PropertyHandle_X->SetValue(NewValue) == FPropertyAccess::Success);
	}
}

TSharedRef<SWidget> FLandscapeEditorDetailCustomization_NewLandscape::GetSectionSizeMenu(TSharedRef<IPropertyHandle> PropertyHandle)
{
	FMenuBuilder MenuBuilder(true, NULL);

	for (int32 i = 0; i < ARRAY_COUNT(SectionSizes); i++)
	{
		MenuBuilder.AddMenuEntry(FText::Format(LOCTEXT("NxNQuads", "{0}\u00D7{0} Quads"), FText::AsNumber(SectionSizes[i])), FText::GetEmpty(), FSlateIcon(), FExecuteAction::CreateStatic(&OnChangeSectionSize, PropertyHandle, SectionSizes[i]));
	}

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeSectionSize(TSharedRef<IPropertyHandle> PropertyHandle, int32 NewSize)
{
	ensure(PropertyHandle->SetValue(NewSize) == FPropertyAccess::Success);
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetSectionSize(TSharedRef<IPropertyHandle> PropertyHandle)
{
	int32 QuadsPerSection = 0;
	FPropertyAccess::Result Result = PropertyHandle->GetValue(QuadsPerSection);
	check(Result == FPropertyAccess::Success);

	if (Result == FPropertyAccess::MultipleValues)
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	return FText::Format(LOCTEXT("NxNQuads", "{0}\u00D7{0} Quads"), FText::AsNumber(QuadsPerSection));
}

TSharedRef<SWidget> FLandscapeEditorDetailCustomization_NewLandscape::GetSectionsPerComponentMenu(TSharedRef<IPropertyHandle> PropertyHandle)
{
	FMenuBuilder MenuBuilder(true, NULL);

	for (int32 i = 0; i < ARRAY_COUNT(NumSections); i++)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Width"), NumSections[i]);
		Args.Add(TEXT("Height"), NumSections[i]);
		MenuBuilder.AddMenuEntry(FText::Format(NumSections[i] == 1 ? LOCTEXT("1x1Section", "{Width}\u00D7{Height} Section") : LOCTEXT("NxNSections", "{Width}\u00D7{Height} Sections"), Args), FText::GetEmpty(), FSlateIcon(), FExecuteAction::CreateStatic(&OnChangeSectionsPerComponent, PropertyHandle, NumSections[i]));
	}

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeSectionsPerComponent(TSharedRef<IPropertyHandle> PropertyHandle, int32 NewSize)
{
	ensure(PropertyHandle->SetValue(NewSize) == FPropertyAccess::Success);
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetSectionsPerComponent(TSharedRef<IPropertyHandle> PropertyHandle)
{
	int32 SectionsPerComponent = 0;
	FPropertyAccess::Result Result = PropertyHandle->GetValue(SectionsPerComponent);
	check(Result == FPropertyAccess::Success);

	if (Result == FPropertyAccess::MultipleValues)
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Width"), SectionsPerComponent);
	Args.Add(TEXT("Height"), SectionsPerComponent);
	return FText::Format(SectionsPerComponent == 1 ? LOCTEXT("1x1Section", "{Width}\u00D7{Height} Section") : LOCTEXT("NxNSections", "{Width}\u00D7{Height} Sections"), Args);
}

TOptional<int32> FLandscapeEditorDetailCustomization_NewLandscape::GetLandscapeResolutionX() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		return (LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X * LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection + 1);
	}

	return 0;
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeLandscapeResolutionX(int32 NewValue)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		int32 NewComponentCountX = LandscapeEdMode->UISettings->CalcComponentsCount(NewValue);
		if (NewComponentCountX != LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X)
		{
			if (!GEditor->IsTransactionActive())
			{
				GEditor->BeginTransaction(LOCTEXT("ChangeResolutionX_Transaction", "Change Landscape Resolution X"));
			}

			LandscapeEdMode->UISettings->Modify();
			LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = NewComponentCountX;
		}
	}
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnCommitLandscapeResolutionX(int32 NewValue, ETextCommit::Type CommitInfo)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		if (!GEditor->IsTransactionActive())
		{
			GEditor->BeginTransaction(LOCTEXT("ChangeResolutionX_Transaction", "Change Landscape Resolution X"));
		}
		LandscapeEdMode->UISettings->Modify();
		LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = LandscapeEdMode->UISettings->CalcComponentsCount(NewValue);
		GEditor->EndTransaction();
	}
}

TOptional<int32> FLandscapeEditorDetailCustomization_NewLandscape::GetLandscapeResolutionY() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		return (LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y * LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection + 1);
	}

	return 0;
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeLandscapeResolutionY(int32 NewValue)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		int32 NewComponentCountY = LandscapeEdMode->UISettings->CalcComponentsCount(NewValue);
		if (NewComponentCountY != LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y)
		{
			if (!GEditor->IsTransactionActive())
			{
				GEditor->BeginTransaction(LOCTEXT("ChangeResolutionY_Transaction", "Change Landscape Resolution Y"));
			}
			
			LandscapeEdMode->UISettings->Modify();
			LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = NewComponentCountY;
		}
	}
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnCommitLandscapeResolutionY(int32 NewValue, ETextCommit::Type CommitInfo)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		if (!GEditor->IsTransactionActive())
		{
			GEditor->BeginTransaction(LOCTEXT("ChangeResolutionY_Transaction", "Change Landscape Resolution Y"));
		}
		LandscapeEdMode->UISettings->Modify();
		LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = LandscapeEdMode->UISettings->CalcComponentsCount(NewValue);
		GEditor->EndTransaction();
	}
}

TOptional<int32> FLandscapeEditorDetailCustomization_NewLandscape::GetMinLandscapeResolution() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		// Min size is one component
		return (LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection + 1);
	}

	return 0;
}

TOptional<int32> FLandscapeEditorDetailCustomization_NewLandscape::GetMaxLandscapeResolution() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		// Max size is either whole components below 8192 verts, or 32 components
		const int32 QuadsPerComponent = (LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection);
		//return (float)(FMath::Min(32, FMath::FloorToInt(8191 / QuadsPerComponent)) * QuadsPerComponent);
		return (8191 / QuadsPerComponent) * QuadsPerComponent + 1;
	}

	return 0;
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetTotalComponentCount() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		return FText::AsNumber(LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X * LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y);
	}

	return FText::FromString(TEXT("---"));
}


EVisibility FLandscapeEditorDetailCustomization_NewLandscape::GetVisibilityOnlyInNewLandscapeMode(ENewLandscapePreviewMode::Type value)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		if (LandscapeEdMode->NewLandscapePreviewMode == value)
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

ECheckBoxState FLandscapeEditorDetailCustomization_NewLandscape::NewLandscapeModeIsChecked(ENewLandscapePreviewMode::Type value) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		if (LandscapeEdMode->NewLandscapePreviewMode == value)
		{
			return ECheckBoxState::Checked;
		}
	}
	return ECheckBoxState::Unchecked;
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnNewLandscapeModeChanged(ECheckBoxState NewCheckedState, ENewLandscapePreviewMode::Type value)
{
	if (NewCheckedState == ECheckBoxState::Checked)
	{
		FEdModeLandscape* LandscapeEdMode = GetEditorMode();
		if (LandscapeEdMode != NULL)
		{
			LandscapeEdMode->NewLandscapePreviewMode = value;

			if (value == ENewLandscapePreviewMode::ImportLandscape)
			{
				LandscapeEdMode->NewLandscapePreviewMode = ENewLandscapePreviewMode::ImportLandscape;
			}
		}
	}
}

FReply FLandscapeEditorDetailCustomization_NewLandscape::OnCreateButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr && 
		LandscapeEdMode->GetWorld() != nullptr && 
		LandscapeEdMode->GetWorld()->GetCurrentLevel()->bIsVisible)
	{
		FScopedTransaction Transaction(LOCTEXT("Undo", "Creating New Landscape"));

		// Initialize heightmap data
		TArray<uint16> Data;
		const int32 ComponentCountX = LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X;
		const int32 ComponentCountY = LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y;
		const int32 QuadsPerComponent = LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection;
		const int32 SizeX = LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X * QuadsPerComponent + 1;
		const int32 SizeY = LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y * QuadsPerComponent + 1;
		Data.AddUninitialized(SizeX * SizeY);
		uint16* WordData = Data.GetData();

		// Initialize blank heightmap data
		for (int32 i = 0; i < SizeX * SizeY; i++)
		{
			WordData[i] = 32768;
		}

		TArray<FLandscapeImportLayerInfo> LayerInfos;

		if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
		{
			const int32 ImportSizeX = LandscapeEdMode->UISettings->ImportLandscape_Width;
			const int32 ImportSizeY = LandscapeEdMode->UISettings->ImportLandscape_Height;

			const TArray<FLandscapeImportLayer>& ImportLandscapeLayersList = LandscapeEdMode->UISettings->ImportLandscape_Layers;
			LayerInfos.Reserve(ImportLandscapeLayersList.Num());

			// Fill in LayerInfos array and allocate data
			for (int32 LayerIdx = 0; LayerIdx < ImportLandscapeLayersList.Num(); LayerIdx++)
			{
				LayerInfos.Add(ImportLandscapeLayersList[LayerIdx]); //slicing is fine here
				FLandscapeImportLayerInfo& ImportLayer = LayerInfos.Last();

				if (ImportLayer.LayerInfo != NULL && ImportLayer.SourceFilePath != "")
				{
					FFileHelper::LoadFileToArray(ImportLayer.LayerData, *ImportLayer.SourceFilePath, FILEREAD_Silent);

					if (ImportLayer.SourceFilePath.EndsWith(".png"))
					{
						IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
						IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

						const TArray<uint8>* RawData = NULL;
						if (ImageWrapper->SetCompressed(ImportLayer.LayerData.GetData(), ImportLayer.LayerData.Num()) &&
							ImageWrapper->GetRaw(ERGBFormat::Gray, 8, RawData))
						{
							ImportLayer.LayerData = *RawData; // agh I want to use MoveTemp() here
						}
						else
						{
							ImportLayer.LayerData.Empty();
						}
					}

					// Remove invalid raw weightmap data
					if (ImportLayer.LayerData.Num() != (ImportSizeX * ImportSizeY))
					{
						ImportLayer.LayerData.Empty();
					}
				}
			}

			const TArray<uint16>& ImportData = LandscapeEdMode->UISettings->GetImportLandscapeData();
			if (ImportData.Num() != 0)
			{
				const int32 OffsetX = (SizeX - ImportSizeX) / 2;
				const int32 OffsetY = (SizeY - ImportSizeY) / 2;

				// Heightmap
				Data = LandscapeEditorUtils::ExpandData(ImportData,
					0, 0, ImportSizeX - 1, ImportSizeY - 1,
					-OffsetX, -OffsetY, SizeX - OffsetX - 1, SizeY - OffsetY - 1);

				// Layers
				for (int32 LayerIdx = 0; LayerIdx < LayerInfos.Num(); LayerIdx++)
				{
					TArray<uint8>& ImportLayerData = LayerInfos[LayerIdx].LayerData;
					if (ImportLayerData.Num())
					{
						ImportLayerData = LandscapeEditorUtils::ExpandData(ImportLayerData,
							0, 0, ImportSizeX - 1, ImportSizeY - 1,
							-OffsetX, -OffsetY, SizeX - OffsetX - 1, SizeY - OffsetY - 1);
					}
				}
			}

			LandscapeEdMode->UISettings->ClearImportLandscapeData();
		}

		const float ComponentSize = QuadsPerComponent;
		const FVector Offset = FTransform(LandscapeEdMode->UISettings->NewLandscape_Rotation, FVector::ZeroVector, LandscapeEdMode->UISettings->NewLandscape_Scale).TransformVector(FVector(-ComponentCountX * ComponentSize / 2, -ComponentCountY * ComponentSize / 2, 0));
		ALandscape* Landscape = LandscapeEdMode->GetWorld()->SpawnActor<ALandscape>(LandscapeEdMode->UISettings->NewLandscape_Location + Offset, LandscapeEdMode->UISettings->NewLandscape_Rotation);

		//if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
		{
			Landscape->LandscapeMaterial = LandscapeEdMode->UISettings->NewLandscape_Material.Get();
		}

		Landscape->SetActorRelativeScale3D(LandscapeEdMode->UISettings->NewLandscape_Scale);
		Landscape->Import(FGuid::NewGuid(), SizeX, SizeY, QuadsPerComponent, LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent, LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection, Data.GetData(), NULL, LayerInfos);

		// automatically calculate a lighting LOD that won't crash lightmass (hopefully)
		// < 2048x2048 -> LOD0
		// >=2048x2048 -> LOD1
		// >= 4096x4096 -> LOD2
		// >= 8192x8192 -> LOD3
		Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((SizeX * SizeY) / (2048 * 2048) + 1), (uint32)2);

		if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
		{
			Landscape->ReimportHeightmapFilePath = LandscapeEdMode->UISettings->ImportLandscape_HeightmapFilename;
		}

		ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo(true);
		LandscapeInfo->UpdateLayerInfoMap(Landscape);

		const TArray<FLandscapeImportLayer>& ImportLandscapeLayersList = LandscapeEdMode->UISettings->ImportLandscape_Layers;
		for (int32 i = 0; i < ImportLandscapeLayersList.Num(); i++)
		{
			if (ImportLandscapeLayersList[i].LayerInfo != NULL)
			{
				if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
				{
					Landscape->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(ImportLandscapeLayersList[i].LayerInfo, ImportLandscapeLayersList[i].SourceFilePath));
				}
				else
				{
					Landscape->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(ImportLandscapeLayersList[i].LayerInfo));
				}

				int32 LayerInfoIndex = LandscapeInfo->GetLayerInfoIndex(ImportLandscapeLayersList[i].LayerName);
				if (ensure(LayerInfoIndex != INDEX_NONE))
				{
					FLandscapeInfoLayerSettings& LayerSettings = LandscapeInfo->Layers[LayerInfoIndex];
					LayerSettings.LayerInfoObj = ImportLandscapeLayersList[i].LayerInfo;
				}
			}
		}

		LandscapeEdMode->UpdateLandscapeList();
		LandscapeEdMode->CurrentToolTarget.LandscapeInfo = Landscape->GetLandscapeInfo();
		LandscapeEdMode->CurrentToolTarget.TargetType = ELandscapeToolTargetType::Heightmap;
		LandscapeEdMode->CurrentToolTarget.LayerInfo = NULL;
		LandscapeEdMode->CurrentToolTarget.LayerName = NAME_None;
		LandscapeEdMode->UpdateTargetList();

		LandscapeEdMode->SetCurrentTool("Select"); // change tool so switching back to the manage mode doesn't give "New Landscape" again
		LandscapeEdMode->SetCurrentTool("Sculpt"); // change to sculpting mode and tool
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_NewLandscape::OnFillWorldButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		FVector& NewLandscapeLocation = LandscapeEdMode->UISettings->NewLandscape_Location;
		NewLandscapeLocation.X = 0;
		NewLandscapeLocation.Y = 0;

		const int32 QuadsPerComponent = LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection;
		LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = FMath::CeilToInt(WORLD_MAX / QuadsPerComponent / LandscapeEdMode->UISettings->NewLandscape_Scale.X);
		LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = FMath::CeilToInt(WORLD_MAX / QuadsPerComponent / LandscapeEdMode->UISettings->NewLandscape_Scale.Y);
		LandscapeEdMode->UISettings->NewLandscape_ClampSize();
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_NewLandscape::OnFitImportDataButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		ChooseBestComponentSizeForImport(LandscapeEdMode);
	}

	return FReply::Handled();
}

bool FLandscapeEditorDetailCustomization_NewLandscape::GetImportButtonIsEnabled() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		if (LandscapeEdMode->UISettings->ImportLandscape_HeightmapError != ELandscapeImportHeightmapError::None &&
			LandscapeEdMode->UISettings->ImportLandscape_HeightmapError != ELandscapeImportHeightmapError::ColorPng &&
			LandscapeEdMode->UISettings->ImportLandscape_HeightmapError != ELandscapeImportHeightmapError::LowBitDepth)
		{
			return false;
		}

		for (int32 i = 0; i < LandscapeEdMode->UISettings->ImportLandscape_Layers.Num(); ++i)
		{
			if (LandscapeEdMode->UISettings->ImportLandscape_Layers[i].ImportError != ELandscapeImportLayerError::None &&
				LandscapeEdMode->UISettings->ImportLandscape_Layers[i].ImportError != ELandscapeImportLayerError::ColorPng)
			{
				return false;
			}
		}

		return true;
	}
	return false;
}


EVisibility FLandscapeEditorDetailCustomization_NewLandscape::GetHeightmapErrorVisibility(TSharedRef<IPropertyHandle> PropertyHandle_HeightmapError)
{
	uint8 HeightmapErrorAsByte = 0;
	FPropertyAccess::Result Result = PropertyHandle_HeightmapError->GetValue(HeightmapErrorAsByte);
	check(Result == FPropertyAccess::Success);

	if (Result == FPropertyAccess::MultipleValues)
	{
		return EVisibility::Visible;
	}

	if (HeightmapErrorAsByte != ELandscapeImportHeightmapError::None)
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetHeightmapErrorText(TSharedRef<IPropertyHandle> PropertyHandle_HeightmapError)
{
	uint8 HeightmapErrorAsByte = 0;
	FPropertyAccess::Result Result = PropertyHandle_HeightmapError->GetValue(HeightmapErrorAsByte);
	check(Result == FPropertyAccess::Success);

	if (Result == FPropertyAccess::MultipleValues)
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	switch (HeightmapErrorAsByte)
	{
	case ELandscapeImportHeightmapError::None:
		return FText::GetEmpty();
	case ELandscapeImportHeightmapError::FileNotFound:
		return LOCTEXT("Import_HeightmapFileDoesNotExist", "The Heightmap file does not exist");
	case ELandscapeImportHeightmapError::InvalidSize:
		return LOCTEXT("Import_HeightmapFileInvalidSize", "The Heightmap file has an invalid size (possibly not 16-bit?)");
	case ELandscapeImportHeightmapError::CorruptFile:
		return LOCTEXT("Import_HeightmapFileCorruptPng", "The Heightmap file cannot be read (corrupt png?)");
	case ELandscapeImportHeightmapError::ColorPng:
		return LOCTEXT("Import_HeightmapFileColorPng", "The Heightmap file appears to be a color png, grayscale is expected. The import *can* continue, but the result may not be what you expect...");
	case ELandscapeImportHeightmapError::LowBitDepth:
		return LOCTEXT("Import_HeightmapFileLowBitDepth", "The Heightmap file appears to be an 8-bit png, 16-bit is preferred. The import *can* continue, but the result may be lower quality than desired.");
	default:
		check(0);
	}

	return FText::GetEmpty();
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetImportHeightmapFilenameString(TSharedRef<IPropertyHandle> PropertyHandle_HeightmapFilename)
{
	FString HeightmapFilename;
	FPropertyAccess::Result Result = PropertyHandle_HeightmapFilename->GetValue(HeightmapFilename);
	check(Result == FPropertyAccess::Success);

	if (Result == FPropertyAccess::MultipleValues)
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	return FText::FromString(HeightmapFilename);
}

void FLandscapeEditorDetailCustomization_NewLandscape::SetImportHeightmapFilenameString(const FText& NewValue, ETextCommit::Type CommitInfo, TSharedRef<IPropertyHandle> PropertyHandle_HeightmapFilename)
{
	FString HeightmapFilename = NewValue.ToString();
	ensure(PropertyHandle_HeightmapFilename->SetValue(HeightmapFilename) == FPropertyAccess::Success);
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnImportHeightmapFilenameChanged()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		ImportResolutions.Reset(1);
		LandscapeEdMode->UISettings->ImportLandscape_Width = 0;
		LandscapeEdMode->UISettings->ImportLandscape_Height = 0;
		LandscapeEdMode->UISettings->ClearImportLandscapeData();
		LandscapeEdMode->UISettings->ImportLandscape_HeightmapError = ELandscapeImportHeightmapError::None;

		if (!LandscapeEdMode->UISettings->ImportLandscape_HeightmapFilename.IsEmpty())
		{
			if (LandscapeEdMode->UISettings->ImportLandscape_HeightmapFilename.EndsWith(".png"))
			{
				TArray<uint8> ImportData;
				if (!FFileHelper::LoadFileToArray(ImportData, *LandscapeEdMode->UISettings->ImportLandscape_HeightmapFilename, FILEREAD_Silent))
				{
					LandscapeEdMode->UISettings->ImportLandscape_HeightmapError = ELandscapeImportHeightmapError::FileNotFound;
				}
				else
				{
					IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
					IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

					if (!ImageWrapper->SetCompressed(ImportData.GetData(), ImportData.Num()))
					{
						LandscapeEdMode->UISettings->ImportLandscape_HeightmapError = ELandscapeImportHeightmapError::CorruptFile;
					}
					else if (ImageWrapper->GetWidth() <= 0 || ImageWrapper->GetHeight() <= 0)
					{
						LandscapeEdMode->UISettings->ImportLandscape_HeightmapError = ELandscapeImportHeightmapError::InvalidSize;
					}
					else
					{
						if (ImageWrapper->GetFormat() != ERGBFormat::Gray)
						{
							LandscapeEdMode->UISettings->ImportLandscape_HeightmapError = ELandscapeImportHeightmapError::ColorPng;
							FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Import_HeightmapFileColorPng", "The Heightmap file appears to be a color png, grayscale is expected. The import *can* continue, but the result may not be what you expect..."));
						}
						else if (ImageWrapper->GetBitDepth() != 16)
						{
							LandscapeEdMode->UISettings->ImportLandscape_HeightmapError = ELandscapeImportHeightmapError::LowBitDepth;
							FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Import_HeightmapFileLowBitDepth", "The Heightmap file appears to be an 8-bit png, 16-bit is preferred. The import *can* continue, but the result may be lower quality than desired."));
						}
						FLandscapeImportResolution ImportResolution;
						ImportResolution.Width = ImageWrapper->GetWidth();
						ImportResolution.Height = ImageWrapper->GetHeight();
						ImportResolutions.Add(ImportResolution);
					}
				}
			}
			else
			{
				int64 ImportFileSize = IFileManager::Get().FileSize(*LandscapeEdMode->UISettings->ImportLandscape_HeightmapFilename);

				if (ImportFileSize < 0)
				{
					LandscapeEdMode->UISettings->ImportLandscape_HeightmapError = ELandscapeImportHeightmapError::FileNotFound;
				}
				else if (ImportFileSize == 0 || ImportFileSize % 2 != 0)
				{
					LandscapeEdMode->UISettings->ImportLandscape_HeightmapError = ELandscapeImportHeightmapError::InvalidSize;
				}
				else
				{
					ImportFileSize /= 2;

					// Find all possible heightmap sizes, up to 8192 width/height
					int32 InsertIndex = 0;
					const int32 MinWidth = FMath::DivideAndRoundUp(ImportFileSize, (int64)8192);
					const int32 MaxWidth = FMath::TruncToInt(FMath::Sqrt(ImportFileSize));
					for (int32 Width = MinWidth; Width <= MaxWidth; Width++)
					{
						if (ImportFileSize % Width == 0)
						{
							const int32 Height = ImportFileSize / Width;

							FLandscapeImportResolution ImportResolution;
							ImportResolution.Width = Width;
							ImportResolution.Height = Height;
							ImportResolutions.Insert(ImportResolution, InsertIndex++);

							if (Width != Height)
							{
								ImportResolution.Width = Height;
								ImportResolution.Height = Width;
								ImportResolutions.Insert(ImportResolution, InsertIndex);
							}
						}
					}

					if (ImportResolutions.Num() == 0)
					{
						LandscapeEdMode->UISettings->ImportLandscape_HeightmapError = ELandscapeImportHeightmapError::InvalidSize;
					}
				}
			}
		}

		if (ImportResolutions.Num() > 0)
		{
			int32 i = ImportResolutions.Num() / 2;
			LandscapeEdMode->UISettings->ImportLandscape_Width = ImportResolutions[i].Width;
			LandscapeEdMode->UISettings->ImportLandscape_Height = ImportResolutions[i].Height;
			LandscapeEdMode->UISettings->ClearImportLandscapeData();
			ChooseBestComponentSizeForImport(LandscapeEdMode);
		}
		else
		{
			LandscapeEdMode->UISettings->ImportLandscape_Width = 0;
			LandscapeEdMode->UISettings->ImportLandscape_Height = 0;
			LandscapeEdMode->UISettings->ClearImportLandscapeData();
		}
	}
}

FReply FLandscapeEditorDetailCustomization_NewLandscape::OnImportHeightmapFilenameButtonClicked(TSharedRef<IPropertyHandle> PropertyHandle_HeightmapFilename)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	check(LandscapeEdMode != NULL);

	// Prompt the user for the Filenames
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform != NULL)
	{
		void* ParentWindowWindowHandle = NULL;

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if (MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		TArray<FString> OpenFilenames;
		bool bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowWindowHandle,
			NSLOCTEXT("UnrealEd", "Import", "Import").ToString(),
			LandscapeEdMode->UISettings->LastImportPath,
			TEXT(""),
			TEXT("All Heightmap files|*.png;*.raw;*.r16|Heightmap png files|*.png|Heightmap .raw/.r16 files|*.raw;*.r16|All files (*.*)|*.*"),
			EFileDialogFlags::None,
			OpenFilenames);

		if (bOpened)
		{
			ensure(PropertyHandle_HeightmapFilename->SetValue(OpenFilenames[0]) == FPropertyAccess::Success);
			LandscapeEdMode->UISettings->LastImportPath = FPaths::GetPath(OpenFilenames[0]);
		}
	}

	return FReply::Handled();

}

TSharedRef<SWidget> FLandscapeEditorDetailCustomization_NewLandscape::GetImportLandscapeResolutionMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);

	for (int32 i = 0; i < ImportResolutions.Num(); i++)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Width"), ImportResolutions[i].Width);
		Args.Add(TEXT("Height"), ImportResolutions[i].Height);
		MenuBuilder.AddMenuEntry(FText::Format(LOCTEXT("ImportResolution_Format", "{Width}\u00D7{Height}"), Args), FText(), FSlateIcon(), FExecuteAction::CreateSP(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnChangeImportLandscapeResolution, i));
	}

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeImportLandscapeResolution(int32 Index)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		LandscapeEdMode->UISettings->ImportLandscape_Width = ImportResolutions[Index].Width;
		LandscapeEdMode->UISettings->ImportLandscape_Height = ImportResolutions[Index].Height;
		LandscapeEdMode->UISettings->ClearImportLandscapeData();
		ChooseBestComponentSizeForImport(LandscapeEdMode);
	}
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetImportLandscapeResolution() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		const int32 Width = LandscapeEdMode->UISettings->ImportLandscape_Width;
		const int32 Height = LandscapeEdMode->UISettings->ImportLandscape_Height;
		if (Width != 0 && Height != 0)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Width"), Width);
			Args.Add(TEXT("Height"), Height);
			return FText::Format(LOCTEXT("ImportResolution_Format", "{Width}\u00D7{Height}"), Args);
		}
		else
		{
			return LOCTEXT("ImportResolution_Invalid", "(invalid)");
		}
	}

	return FText::GetEmpty();
}

void FLandscapeEditorDetailCustomization_NewLandscape::ChooseBestComponentSizeForImport(FEdModeLandscape* LandscapeEdMode)
{
	int32 Width = LandscapeEdMode->UISettings->ImportLandscape_Width;
	int32 Height = LandscapeEdMode->UISettings->ImportLandscape_Height;

	bool bFoundMatch = false;
	if (Width > 0 && Height > 0)
	{
		// Try to find a section size and number of sections that matches the dimensions of the heightfield
		for (int32 SectionSizesIdx = ARRAY_COUNT(SectionSizes) - 1; SectionSizesIdx >= 0; SectionSizesIdx--)
		{
			for (int32 NumSectionsIdx = ARRAY_COUNT(NumSections) - 1; NumSectionsIdx >= 0; NumSectionsIdx--)
			{
				int32 ss = SectionSizes[SectionSizesIdx];
				int32 ns = NumSections[NumSectionsIdx];

				if (((Width - 1) % (ss * ns)) == 0 && ((Width - 1) / (ss * ns)) <= 32 &&
					((Height - 1) % (ss * ns)) == 0 && ((Height - 1) / (ss * ns)) <= 32)
				{
					bFoundMatch = true;
					LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection = ss;
					LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent = ns;
					LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = (Width - 1) / (ss * ns);
					LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = (Height - 1) / (ss * ns);
					LandscapeEdMode->UISettings->NewLandscape_ClampSize();
					break;
				}
			}
			if (bFoundMatch)
			{
				break;
			}
		}

		if (!bFoundMatch)
		{
			// if there was no exact match, try increasing the section size until we encompass the whole heightmap
			const int32 CurrentSectionSize = LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection;
			const int32 CurrentNumSections = LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent;
			for (int32 SectionSizesIdx = 0; SectionSizesIdx < ARRAY_COUNT(SectionSizes); SectionSizesIdx++)
			{
				if (SectionSizes[SectionSizesIdx] < CurrentSectionSize)
				{
					continue;
				}

				const int32 ComponentsX = FMath::DivideAndRoundUp((Width - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
				const int32 ComponentsY = FMath::DivideAndRoundUp((Height - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
				if (ComponentsX <= 32 && ComponentsY <= 32)
				{
					bFoundMatch = true;
					LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection = SectionSizes[SectionSizesIdx];
					//LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent = ;
					LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = ComponentsX;
					LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = ComponentsY;
					LandscapeEdMode->UISettings->NewLandscape_ClampSize();
					break;
				}
			}

			if (!bFoundMatch)
			{
				// if the heightmap is very large, fall back to using the largest values we support
				const int32 MaxSectionSize = SectionSizes[ARRAY_COUNT(SectionSizes) - 1];
				const int32 ComponentsX = FMath::DivideAndRoundUp((Width - 1), MaxSectionSize * CurrentNumSections);
				const int32 ComponentsY = FMath::DivideAndRoundUp((Height - 1), MaxSectionSize * CurrentNumSections);

				bFoundMatch = true;
				LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection = SectionSizes[MaxSectionSize];
				//LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent = ;
				LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = ComponentsX;
				LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = ComponentsY;
				LandscapeEdMode->UISettings->NewLandscape_ClampSize();
			}
		}

		check(bFoundMatch);
	}
}

EVisibility FLandscapeEditorDetailCustomization_NewLandscape::GetMaterialTipVisibility() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		if (LandscapeEdMode->UISettings->ImportLandscape_Layers.Num() == 0)
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

//////////////////////////////////////////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FLandscapeEditorStructCustomization_FLandscapeImportLayer::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorStructCustomization_FLandscapeImportLayer);
}

void FLandscapeEditorStructCustomization_FLandscapeImportLayer::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FLandscapeEditorStructCustomization_FLandscapeImportLayer::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<IPropertyHandle> PropertyHandle_LayerName = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, LayerName)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, LayerInfo)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_SourceFilePath = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, SourceFilePath)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_ThumbnailMIC = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, ThumbnailMIC)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_LayerError = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, ImportError)).ToSharedRef();

	FName LayerName;
	FText LayerNameText;
	FPropertyAccess::Result Result = PropertyHandle_LayerName->GetValue(LayerName);
	checkSlow(Result == FPropertyAccess::Success);
	LayerNameText = FText::FromName(LayerName);
	if (Result == FPropertyAccess::MultipleValues)
	{
		LayerName = NAME_None;
		LayerNameText = NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	UObject* ThumbnailMIC = NULL;
	Result = PropertyHandle_ThumbnailMIC->GetValue(ThumbnailMIC);
	checkSlow(Result == FPropertyAccess::Success);

	ChildBuilder.AddChildContent(LayerNameText)
	.NameContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(SErrorText)
			.Visibility_Static(&GetErrorVisibility, PropertyHandle_LayerError)
			.ErrorText(NSLOCTEXT("UnrealEd", "Error", "!"))
			.ToolTip(
				SNew(SToolTip)
				.Text_Static(&GetErrorText, PropertyHandle_LayerError)
			)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(STextBlock)
			.Font(StructCustomizationUtils.GetRegularFont())
			.Text(LayerNameText)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(SLandscapeAssetThumbnail, ThumbnailMIC, StructCustomizationUtils.GetThumbnailPool().ToSharedRef())
			.ThumbnailSize(FIntPoint(48, 48))
		]
	]
	.ValueContent()
	.MinDesiredWidth(250.0f) // copied from SPropertyEditorAsset::GetDesiredWidth
	.MaxDesiredWidth(0)
	[
		SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0,0,12,0)) // Line up with the other properties due to having no reset to default button
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(ULandscapeLayerInfoObject::StaticClass())
					.PropertyHandle(PropertyHandle_LayerInfo)
					.OnShouldFilterAsset_Static(&ShouldFilterLayerInfo, LayerName)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SComboButton)
					//.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
					.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
					.HasDownArrow(false)
					//.ContentPadding(0)
					.ContentPadding(4.0f)
					.ForegroundColor(FSlateColor::UseForeground())
					.IsFocusable(false)
					.ToolTipText(LOCTEXT("Target_Create", "Create Layer Info"))
					.Visibility_Static(&GetImportLayerCreateVisibility, PropertyHandle_LayerInfo)
					.OnGetMenuContent_Static(&OnGetImportLayerCreateMenu, PropertyHandle_LayerInfo, LayerName)
					.ButtonContent()
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("LandscapeEditor.Target_Create"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				.Visibility_Static(&FLandscapeEditorDetailCustomization_NewLandscape::GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)
				+ SHorizontalBox::Slot()
				[
					PropertyHandle_SourceFilePath->CreatePropertyValueWidget()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(FMargin(4, 0))
					.Text(NSLOCTEXT("UnrealEd", "GenericOpenDialog", "..."))
					.OnClicked_Static(&FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnLayerFilenameButtonClicked, PropertyHandle_SourceFilePath)
				]
			]
		]
	];
}

FReply FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnLayerFilenameButtonClicked(TSharedRef<IPropertyHandle> PropertyHandle_LayerFilename)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	check(LandscapeEdMode != NULL);

	// Prompt the user for the Filenames
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform != NULL)
	{
		void* ParentWindowWindowHandle = NULL;

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if (MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		TArray<FString> OpenFilenames;
		bool bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowWindowHandle,
			NSLOCTEXT("UnrealEd", "Import", "Import").ToString(),
			LandscapeEdMode->UISettings->LastImportPath,
			TEXT(""),
			TEXT("All Layer files|*.png;*.raw;*.r16|Layer png files|*.png|Layer files (*.raw,*.r16)|*.raw;*.r16|All files (*.*)|*.*"),
			EFileDialogFlags::None,
			OpenFilenames);

		if (bOpened)
		{
			ensure(PropertyHandle_LayerFilename->SetValue(OpenFilenames[0]) == FPropertyAccess::Success);
			LandscapeEdMode->UISettings->LastImportPath = FPaths::GetPath(OpenFilenames[0]);
		}
	}

	return FReply::Handled();
}

bool FLandscapeEditorStructCustomization_FLandscapeImportLayer::ShouldFilterLayerInfo(const FAssetData& AssetData, FName LayerName)
{
	ULandscapeLayerInfoObject* LayerInfo = CastChecked<ULandscapeLayerInfoObject>(AssetData.GetAsset());
	if (LayerInfo->LayerName != LayerName)
	{
		return true;
	}

	return false;
}

EVisibility FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetImportLayerCreateVisibility(TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo)
{
	UObject* LayerInfoAsUObject = NULL;
	if (PropertyHandle_LayerInfo->GetValue(LayerInfoAsUObject) != FPropertyAccess::Fail &&
		LayerInfoAsUObject == NULL)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

TSharedRef<SWidget> FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnGetImportLayerCreateMenu(TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo, FName LayerName)
{
	FMenuBuilder MenuBuilder(true, NULL);

	MenuBuilder.AddMenuEntry(LOCTEXT("Target_Create_Blended", "Weight-Blended Layer (normal)"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&OnImportLayerCreateClicked, PropertyHandle_LayerInfo, LayerName, false)));

	MenuBuilder.AddMenuEntry(LOCTEXT("Target_Create_NoWeightBlend", "Non Weight-Blended Layer"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&OnImportLayerCreateClicked, PropertyHandle_LayerInfo, LayerName, true)));

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnImportLayerCreateClicked(TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo, FName LayerName, bool bNoWeightBlend)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		// Hack as we don't have a direct world pointer in the EdMode...
		ULevel* Level = LandscapeEdMode->CurrentGizmoActor->GetWorld()->GetCurrentLevel();

		// Build default layer object name and package name
		FName LayerObjectName = FName(*FString::Printf(TEXT("%s_LayerInfo"), *LayerName.ToString()));
		FString Path = Level->GetOutermost()->GetName() + TEXT("_sharedassets/");
		if (Path.StartsWith("/Temp/"))
		{
			Path = FString("/Game/") + Path.RightChop(FString("/Temp/").Len());
		}
		FString PackageName = Path + LayerObjectName.ToString();

		TSharedRef<SDlgPickAssetPath> NewLayerDlg =
			SNew(SDlgPickAssetPath)
			.Title(LOCTEXT("CreateNewLayerInfo", "Create New Landscape Layer Info Object"))
			.DefaultAssetPath(FText::FromString(PackageName));

		if (NewLayerDlg->ShowModal() != EAppReturnType::Cancel)
		{
			PackageName = NewLayerDlg->GetFullAssetPath().ToString();
			LayerObjectName = FName(*NewLayerDlg->GetAssetName().ToString());

			UPackage* Package = CreatePackage(NULL, *PackageName);
			ULandscapeLayerInfoObject* LayerInfo = NewObject<ULandscapeLayerInfoObject>(Package, LayerObjectName, RF_Public | RF_Standalone | RF_Transactional);
			LayerInfo->LayerName = LayerName;
			LayerInfo->bNoWeightBlend = bNoWeightBlend;

			const UObject* LayerInfoAsUObject = LayerInfo; // HACK: If SetValue took a reference to a const ptr (T* const &) or a non-reference (T*) then this cast wouldn't be necessary
			ensure(PropertyHandle_LayerInfo->SetValue(LayerInfoAsUObject) == FPropertyAccess::Success);

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(LayerInfo);

			// Mark the package dirty...
			Package->MarkPackageDirty();

			// Show in the content browser
			TArray<UObject*> Objects;
			Objects.Add(LayerInfo);
			GEditor->SyncBrowserToObjects(Objects);
		}
	}
}

EVisibility FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetErrorVisibility(TSharedRef<IPropertyHandle> PropertyHandle_LayerError)
{
	auto LayerError = ELandscapeImportLayerError::None;
	FPropertyAccess::Result Result = PropertyHandle_LayerError->GetValue((uint8&)LayerError);
	if (Result == FPropertyAccess::Fail ||
		Result == FPropertyAccess::MultipleValues)
	{
		return EVisibility::Visible;
	}

	// None of the errors are currently relevant outside of "Import" mode
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		if (LandscapeEdMode->NewLandscapePreviewMode != ENewLandscapePreviewMode::ImportLandscape)
		{
			return EVisibility::Collapsed;
		}
	}

	if (LayerError != ELandscapeImportLayerError::None)
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

FText FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetErrorText(TSharedRef<IPropertyHandle> PropertyHandle_LayerError)
{
	auto LayerError = ELandscapeImportLayerError::None;
	FPropertyAccess::Result Result = PropertyHandle_LayerError->GetValue((uint8&)LayerError);
	if (Result == FPropertyAccess::Fail)
	{
		return LOCTEXT("Import_LayerUnknownError", "Unknown Error");
	}
	else if (Result == FPropertyAccess::MultipleValues)
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	switch (LayerError)
	{
	case ELandscapeImportLayerError::None:
		return FText::GetEmpty();
	case ELandscapeImportLayerError::MissingLayerInfo:
		return LOCTEXT("Import_LayerInfoNotSet", "Can't import a layer file without a layer info");
	case ELandscapeImportLayerError::FileNotFound:
		return LOCTEXT("Import_LayerFileDoesNotExist", "The Layer file does not exist");
	case ELandscapeImportLayerError::FileSizeMismatch:
		return LOCTEXT("Import_LayerSizeMismatch", "Size of the layer file does not match size of heightmap file");
	case ELandscapeImportLayerError::CorruptFile:
		return LOCTEXT("Import_LayerCorruptPng", "The Layer file cannot be read (corrupt png?)");
	case ELandscapeImportLayerError::ColorPng:
		return LOCTEXT("Import_LayerColorPng", "The Layer file appears to be a color png, grayscale is expected. The import *can* continue, but the result may not be what you expect...");
	default:
		checkSlow(0);
		return LOCTEXT("Import_LayerUnknownError", "Unknown Error");
	}

	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
