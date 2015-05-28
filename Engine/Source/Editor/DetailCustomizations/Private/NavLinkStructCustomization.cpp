// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "NavLinkStructCustomization.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "AI/Navigation/NavigationSystem.h"

#define LOCTEXT_NAMESPACE "FNavLinkStructCustomization"

TSharedRef<IPropertyTypeCustomization> FNavLinkStructCustomization::MakeInstance( )
{
	return MakeShareable(new FNavLinkStructCustomization);
}

void FNavLinkStructCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	TSharedPtr<IPropertyHandle> CommentHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNavigationLinkBase,Description));
	FString Desc;

	if (CommentHandle.IsValid())
	{
		CommentHandle->GetValue(Desc);
	}

	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(400.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Desc))
		.Font(StructCustomizationUtils.GetRegularFont())
	];
}

void FNavLinkStructCustomization::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	FString AgentPrefix("bSupportsAgent");
	const UNavigationSystem* DefNavSys = (UNavigationSystem*)(UNavigationSystem::StaticClass()->GetDefaultObject());
	const int32 NumAgents = FMath::Min(DefNavSys->GetSupportedAgents().Num(), 16);

	for (uint32 i = 0; i < NumChildren; i++)
	{
		TSharedPtr<IPropertyHandle> PropHandle = StructPropertyHandle->GetChildHandle(i);
		if (PropHandle->GetProperty() && PropHandle->GetProperty()->GetName().StartsWith(AgentPrefix))
		{
			int32 AgentIdx = -1;
			TTypeFromString<int32>::FromString(AgentIdx, *(PropHandle->GetProperty()->GetName().Mid(AgentPrefix.Len()) ));

			if (AgentIdx >= 0 && AgentIdx < NumAgents)
			{
				FText PropName = FText::Format(LOCTEXT("SupportedAgentFmt", "Supports Agent: {0}"), FText::FromName(DefNavSys->GetSupportedAgents()[AgentIdx].Name));
				StructBuilder.AddChildContent(PropName)
					.NameContent()
					[
						SNew(STextBlock)
						.Text(PropName)
						.Font(StructCustomizationUtils.GetRegularFont())
					]
					.ValueContent()
					[
						PropHandle->CreatePropertyValueWidget()
					];
			}

			continue;
		}

		StructBuilder.AddChildProperty(PropHandle.ToSharedRef());
	}
}

#undef LOCTEXT_NAMESPACE
