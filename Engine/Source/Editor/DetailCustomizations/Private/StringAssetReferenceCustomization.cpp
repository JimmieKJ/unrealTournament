// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "StringAssetReferenceCustomization.h"
#include "AssetData.h"

void FStringAssetReferenceCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	StructPropertyHandle = InStructPropertyHandle;

	FString ClassFilterString = StructPropertyHandle->GetMetaData("AllowedClasses");
	if( !ClassFilterString.IsEmpty() )
	{
		TArray<FString> CustomClassFilterNames;
		ClassFilterString.ParseIntoArray(CustomClassFilterNames, TEXT(","), true);

		for(auto It = CustomClassFilterNames.CreateConstIterator(); It; ++It)
		{
			const FString& ClassName = *It;

			UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName);
			if(!Class)
			{
				Class = LoadObject<UClass>(nullptr, *ClassName);
			}
			if(Class)
			{
				CustomClassFilters.Add(Class);
			}
		}
	}

	bExactClass = StructPropertyHandle->GetBoolMetaData("ExactClass");

	FOnShouldFilterAsset AssetFilter;
	UClass* ClassFilter = UObject::StaticClass();
	if(CustomClassFilters.Num() == 1 && !bExactClass)
	{
		// If we only have one class to filter on, set it as the class type filter rather than use a filter callback
		// We can only do this if we don't need an exact match, as the class filter also allows derived types
		// The class filter is much faster than the callback as we're not performing two different sets of type tests
		// (one against UObject, one against the actual type)
		ClassFilter = CustomClassFilters[0];
	}
	else if(CustomClassFilters.Num() > 0)
	{
		// Only bind the filter if we have classes that need filtering
		AssetFilter.BindSP(this, &FStringAssetReferenceCustomization::OnShouldFilterAsset);
	}

	// Can the field be cleared
	const bool bAllowClear = !(StructPropertyHandle->GetMetaDataProperty()->PropertyFlags & CPF_NoClear);

	HeaderRow
	.NameContent()
	[
		InStructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0.0f)
	[
		// Add an object entry box.  Even though this isn't an object entry, we will simulate one
		SNew( SObjectPropertyEntryBox )
		.PropertyHandle( InStructPropertyHandle )
		.ThumbnailPool( StructCustomizationUtils.GetThumbnailPool() )
		.AllowedClass( ClassFilter )
		.OnShouldFilterAsset( AssetFilter )
		.AllowClear( bAllowClear )
	];
}

void FStringAssetReferenceCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

bool FStringAssetReferenceCustomization::OnShouldFilterAsset( const FAssetData& InAssetData ) const
{
	// Only bound if we have classes to filter on, so we don't need to test for an empty array here
	UClass* const AssetClass = InAssetData.GetClass();
	for(auto It = CustomClassFilters.CreateConstIterator(); It; ++It)
	{
		UClass* const FilterClass = *It;
		const bool bMatchesFilter = (bExactClass) ? AssetClass == FilterClass : AssetClass->IsChildOf(FilterClass);
		if(bMatchesFilter)
		{
			return false;
		}
	}

	return true;
}
