// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "ActorComponentDetails.h"
#include "ObjectEditorUtils.h"
#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "ActorComponentDetails"

TSharedRef<IDetailCustomization> FActorComponentDetails::MakeInstance()
{
	return MakeShareable( new FActorComponentDetails );
}

void FActorComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	AddExperimentalWarningCategory(DetailBuilder);

	TSharedPtr<IPropertyHandle> PrimaryTickProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UActorComponent, PrimaryComponentTick));

	// Defaults only show tick properties
	if (DetailBuilder.GetDetailsView().HasClassDefaultObject())
	{
		IDetailCategoryBuilder& TickCategory = DetailBuilder.EditCategory("ComponentTick");

		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bStartWithTickEnabled)));
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, TickInterval)));
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bTickEvenWhenPaused)), EPropertyLocation::Advanced);
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bAllowTickOnDedicatedServer)), EPropertyLocation::Advanced);
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, TickGroup)), EPropertyLocation::Advanced);
	}

	PrimaryTickProperty->MarkHiddenByCustomization();
}

void FActorComponentDetails::AddExperimentalWarningCategory( IDetailLayoutBuilder& DetailBuilder )
{
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailBuilder.GetDetailsView().GetSelectedObjects();

	bool bIsExperimental = false;
	bool bIsEarlyAccess = false;
	for (const TWeakObjectPtr<UObject>& SelectedObjectPtr : SelectedObjects)
	{
		if (UObject* SelectedObject = SelectedObjectPtr.Get())
		{
			bool bObjectClassIsExperimental, bObjectClassIsEarlyAccess;
			FObjectEditorUtils::GetClassDevelopmentStatus(SelectedObject->GetClass(), bObjectClassIsExperimental, bObjectClassIsEarlyAccess);
			bIsExperimental |= bObjectClassIsExperimental;
			bIsEarlyAccess |= bObjectClassIsEarlyAccess;
		}
	}


	if (bIsExperimental || bIsEarlyAccess)
	{
		const FName CategoryName(TEXT("Warning"));
		const FText CategoryDisplayName = LOCTEXT("WarningCategoryDisplayName", "Warning");
		const FText WarningText = bIsExperimental ? LOCTEXT("ExperimentalClassWarning", "Uses experimental class") : LOCTEXT("EarlyAccessClassWarning", "Uses early access class");
		const FText SearchString = WarningText;
		const FText Tooltip = bIsExperimental ? LOCTEXT("ExperimentalClassTooltip", "Here be dragons!  Uses one or more unsupported 'experimental' classes") : LOCTEXT("EarlyAccessClassTooltip", "Uses one or more 'early access' classes");
		const FString ExcerptName = bIsExperimental ? TEXT("ComponentUsesExperimentalClass") : TEXT("ComponentUsesEarlyAccessClass");
		const FSlateBrush* WarningIcon = FEditorStyle::GetBrush(bIsExperimental ? "PropertyEditor.ExperimentalClass" : "PropertyEditor.EarlyAccessClass");

		IDetailCategoryBuilder& WarningCategory = DetailBuilder.EditCategory(CategoryName, CategoryDisplayName, ECategoryPriority::Transform);

		FDetailWidgetRow& WarningRow = WarningCategory.AddCustomRow(SearchString)
			.WholeRowContent()
			[
				SNew(SHorizontalBox)
				.ToolTip(IDocumentation::Get()->CreateToolTip(Tooltip, nullptr, TEXT("Shared/LevelEditor"), ExcerptName))
				.Visibility(EVisibility::Visible)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SImage)
					.Image(WarningIcon)
				]

				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(WarningText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];
	}
}

#undef LOCTEXT_NAMESPACE
