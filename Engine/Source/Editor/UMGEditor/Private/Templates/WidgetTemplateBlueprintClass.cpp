// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "Blueprint/UserWidget.h"
#include "WidgetTemplateBlueprintClass.h"
#include "IDocumentation.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "UMGEditor"

FWidgetTemplateBlueprintClass::FWidgetTemplateBlueprintClass(const FAssetData& InWidgetAssetData, TSubclassOf<UUserWidget> InUserWidgetClass)
	: FWidgetTemplateClass(), WidgetAssetData(InWidgetAssetData)
{
	if (InUserWidgetClass)
	{
		WidgetClass = CastChecked<UClass>(InUserWidgetClass);
		Name = WidgetClass->GetDisplayNameText();
	}
	else
	{
		Name = FText::FromString(FName::NameToDisplayString(WidgetAssetData.AssetName.ToString(), false));
	}
}

FWidgetTemplateBlueprintClass::~FWidgetTemplateBlueprintClass()
{
}

FText FWidgetTemplateBlueprintClass::GetCategory() const
{
	if ( WidgetClass.Get() )
	{
		UUserWidget* DefaultUserWidget = WidgetClass->GetDefaultObject<UUserWidget>();
		return DefaultUserWidget->GetPaletteCategory();
	}
	else
	{
		//If the blueprint is unloaded we need to extract it from the asset metadata.
		auto DefaultUserWidget = UUserWidget::StaticClass()->GetDefaultObject<UUserWidget>();
		return DefaultUserWidget->GetPaletteCategory();
	}
}

UWidget* FWidgetTemplateBlueprintClass::Create(UWidgetTree* Tree)
{
	// Load the blueprint asset if needed
	if (!WidgetClass.Get())
	{
		FString AssetPath = WidgetAssetData.ObjectPath.ToString();
		UWidgetBlueprint* LoadedWidget = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
		WidgetClass = CastChecked<UClass>(LoadedWidget->GeneratedClass);
	}

	return FWidgetTemplateClass::Create(Tree);
}

const FSlateBrush* FWidgetTemplateBlueprintClass::GetIcon() const
{
	auto DefaultUserWidget = UUserWidget::StaticClass()->GetDefaultObject<UUserWidget>();
	return DefaultUserWidget->GetEditorIcon();
}

TSharedRef<IToolTip> FWidgetTemplateBlueprintClass::GetToolTip() const
{
	FText Description;
	if ( const FString* DescriptionStr = WidgetAssetData.TagsAndValues.Find( GET_MEMBER_NAME_CHECKED( UBlueprint, BlueprintDescription ) ) )
	{
		if ( !DescriptionStr->IsEmpty() )
		{
			Description = FText::FromString( DescriptionStr->Replace( TEXT( "\\n" ), TEXT( "\n" ) ) );
		}
	}
	else
	{
		Description = Name;
	}

	return IDocumentation::Get()->CreateToolTip( Description, nullptr, FString( TEXT( "Shared/Types/" ) ) + Name.ToString(), TEXT( "Class" ) );
}

FReply FWidgetTemplateBlueprintClass::OnDoubleClicked()
{
	FAssetEditorManager::Get().OpenEditorForAsset( WidgetAssetData.GetAsset() );
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
