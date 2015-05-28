// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SettingsPrivatePCH.h"


/* FSettingsContainer structors
 *****************************************************************************/

FSettingsContainer::FSettingsContainer( const FName& InName )
	: Name(InName)
{ }


/* FSettingsContainer interface
 *****************************************************************************/

ISettingsSectionPtr FSettingsContainer::AddSection( const FName& CategoryName, const FName& SectionName, const FText& InDisplayName, const FText& InDescription, const TWeakObjectPtr<UObject>& SettingsObject )
{
	TSharedPtr<FSettingsCategory> Category = Categories.FindRef(CategoryName);

	if (!Category.IsValid())
	{
		DescribeCategory(CategoryName, FText::FromString(FName::NameToDisplayString(CategoryName.ToString(), false)), FText::GetEmpty());
		Category = Categories.FindRef(CategoryName);
	}

	ISettingsSectionRef Section = Category->AddSection(SectionName, InDisplayName, InDescription, SettingsObject);
	CategoryModifiedDelegate.Broadcast(CategoryName);

	return Section;
}


ISettingsSectionPtr FSettingsContainer::AddSection( const FName& CategoryName, const FName& SectionName, const FText& InDisplayName, const FText& InDescription, const TSharedRef<SWidget>& CustomWidget )
{
	TSharedPtr<FSettingsCategory> Category = Categories.FindRef(CategoryName);

	if (!Category.IsValid())
	{
		DescribeCategory(CategoryName, FText::FromString(FName::NameToDisplayString(CategoryName.ToString(), false)), FText::GetEmpty());
		Category = Categories.FindRef(CategoryName);
	}

	ISettingsSectionRef Section = Category->AddSection(SectionName, InDisplayName, InDescription, CustomWidget);
	CategoryModifiedDelegate.Broadcast(CategoryName);

	return Section;
}


void FSettingsContainer::RemoveSection( const FName& CategoryName, const FName& SectionName )
{
	TSharedPtr<FSettingsCategory> Category = Categories.FindRef(CategoryName);

	if (Category.IsValid())
	{
		ISettingsSectionPtr Section = Category->GetSection(SectionName);

		if (Section.IsValid())
		{
			Category->RemoveSection(SectionName);
			SectionRemovedDelegate.Broadcast(Section.ToSharedRef());
			CategoryModifiedDelegate.Broadcast(CategoryName);
		}
	}
}

/* ISettingsContainer interface
 *****************************************************************************/

void FSettingsContainer::Describe( const FText& InDisplayName, const FText& InDescription, const FName& InIconName )
{
	Description = InDescription;
	DisplayName = InDisplayName;
	IconName = InIconName;
}


void FSettingsContainer::DescribeCategory( const FName& CategoryName, const FText& InDisplayName, const FText& InDescription )
{
	TSharedPtr<FSettingsCategory>& Category = Categories.FindOrAdd(CategoryName);

	if (!Category.IsValid())
	{
		Category = MakeShareable(new FSettingsCategory(CategoryName));
	}

	Category->Describe(InDisplayName, InDescription);
	CategoryModifiedDelegate.Broadcast(CategoryName);
}


int32 FSettingsContainer::GetCategories( TArray<ISettingsCategoryPtr>& OutCategories ) const
{
	OutCategories.Empty(Categories.Num());

	for (TMap<FName, TSharedPtr<FSettingsCategory> >::TConstIterator It(Categories); It; ++It)
	{
		OutCategories.Add(It.Value());
	}

	return OutCategories.Num();
}
