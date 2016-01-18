// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "NavAgentSelectorCustomization.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavigationSystem.h"

#define LOCTEXT_NAMESPACE "FNavAgentSelectorCustomization"

TSharedRef<IPropertyTypeCustomization> FNavAgentSelectorCustomization::MakeInstance()
{
	return MakeShareable(new FNavAgentSelectorCustomization);
}

void FNavAgentSelectorCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructHandle = StructPropertyHandle;
	OnAgentStateChanged();

	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(400.0f)
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(this, &FNavAgentSelectorCustomization::GetSupportedDesc)
		.Font(StructCustomizationUtils.GetRegularFont())
	];
}

void FNavAgentSelectorCustomization::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	FString AgentPrefix("bSupportsAgent");
	const UNavigationSystem* DefNavSys = (UNavigationSystem*)(UNavigationSystem::StaticClass()->GetDefaultObject());
	const int32 NumAgents = FMath::Min(DefNavSys->GetSupportedAgents().Num(), 16);

	for (uint32 Idx = 0; Idx < NumChildren; Idx++)
	{
		TSharedPtr<IPropertyHandle> PropHandle = StructPropertyHandle->GetChildHandle(Idx);
		if (PropHandle->GetProperty() && PropHandle->GetProperty()->GetName().StartsWith(AgentPrefix))
		{
			PropHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FNavAgentSelectorCustomization::OnAgentStateChanged));

			int32 AgentIdx = -1;
			TTypeFromString<int32>::FromString(AgentIdx, *(PropHandle->GetProperty()->GetName().Mid(AgentPrefix.Len()) ));

			if (AgentIdx >= 0 && AgentIdx < NumAgents)
			{
				FText PropName = FText::FromName(DefNavSys->GetSupportedAgents()[AgentIdx].Name);
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

void FNavAgentSelectorCustomization::OnAgentStateChanged()
{
	const UNavigationSystem* DefNavSys = (UNavigationSystem*)(UNavigationSystem::StaticClass()->GetDefaultObject());
	const int32 NumAgents = FMath::Min(DefNavSys->GetSupportedAgents().Num(), 16);

	uint32 NumChildren = 0;
	StructHandle->GetNumChildren(NumChildren);

	int32 NumSupported = 0;
	int32 FirstSupportedIdx = -1;

	FString AgentPrefix("bSupportsAgent");
	for (uint32 Idx = 0; Idx < NumChildren; Idx++)
	{
		TSharedPtr<IPropertyHandle> PropHandle = StructHandle->GetChildHandle(Idx);
		if (PropHandle->GetProperty() && PropHandle->GetProperty()->GetName().StartsWith(AgentPrefix))
		{
			bool bSupportsAgent = false;
			FPropertyAccess::Result Result = PropHandle->GetValue(bSupportsAgent);
			if (Result == FPropertyAccess::Success && bSupportsAgent)
			{
				int32 AgentIdx = -1;
				TTypeFromString<int32>::FromString(AgentIdx, *(PropHandle->GetProperty()->GetName().Mid(AgentPrefix.Len())));

				if (AgentIdx >= 0 && AgentIdx < NumAgents)
				{
					NumSupported++;
					if (FirstSupportedIdx < 0)
					{
						FirstSupportedIdx = AgentIdx;
					}
				}
			}
		}
	}

	if (NumSupported == NumAgents)
	{
		SupportedDesc = LOCTEXT("AllAgents", "all");
	}
	else if (NumSupported == 0)
	{
		SupportedDesc = LOCTEXT("NoAgents", "none");
	}
	else if (NumSupported == 1)
	{
		SupportedDesc = FText::FromName(DefNavSys->GetSupportedAgents()[FirstSupportedIdx].Name);
	}
	else
	{
		SupportedDesc = FText::Format(FText::FromString("{0}, ..."), FText::FromName(DefNavSys->GetSupportedAgents()[FirstSupportedIdx].Name));
	}
}

FText FNavAgentSelectorCustomization::GetSupportedDesc() const
{
	return SupportedDesc;
}

#undef LOCTEXT_NAMESPACE
