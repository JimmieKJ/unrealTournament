// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ObjectEditorUtils.h"

#if WITH_EDITOR
#include "EditorCategoryUtils.h"

namespace FObjectEditorUtils
{

	FText GetCategoryText( const UProperty* InProperty )
	{
		static const FName NAME_Category(TEXT("Category"));
		if (InProperty && InProperty->HasMetaData(NAME_Category))
		{
			return InProperty->GetMetaDataText(NAME_Category, TEXT("UObjectCategory"), InProperty->GetFullGroupName(false));
		}
		else
		{
			return FText::GetEmpty();
		}
	}

	FString GetCategory( const UProperty* InProperty )
	{
		return GetCategoryText(InProperty).ToString();
	}


	FName GetCategoryFName( const UProperty* InProperty )
	{
		FName OutCategoryName( NAME_None );

		static const FName CategoryKey( TEXT("Category") );
		if( InProperty && InProperty->HasMetaData( CategoryKey ) )
		{
			OutCategoryName = FName( *InProperty->GetMetaData( CategoryKey ) );
		}

		return OutCategoryName;
	}

	bool IsFunctionHiddenFromClass( const UFunction* InFunction,const UClass* Class )
	{
		bool bResult = false;
		if( InFunction )
		{
			bResult = Class->IsFunctionHidden( *InFunction->GetName() );

			static const FName FunctionCategory(TEXT("Category")); // FBlueprintMetadata::MD_FunctionCategory
			if( !bResult && InFunction->HasMetaData( FunctionCategory ) )
			{
				FString const& FuncCategory = InFunction->GetMetaData(FunctionCategory);
				bResult = FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, FuncCategory);
			}
		}
		return bResult;
	}

	bool IsVariableCategoryHiddenFromClass( const UProperty* InVariable,const UClass* Class )
	{
		bool bResult = false;
		if( InVariable && Class )
		{
			bResult = FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, GetCategory(InVariable));
		}
		return bResult;
	}

	void GetClassDevelopmentStatus(UClass* Class, bool& bIsExperimental, bool& bIsEarlyAccess)
	{
		static const FName DevelopmentStatusKey(TEXT("DevelopmentStatus"));
		static const FString EarlyAccessValue(TEXT("EarlyAccess"));
		static const FString ExperimentalValue(TEXT("Experimental"));

		bIsExperimental = bIsEarlyAccess = false;

		FString DevelopmentStatus;
		if ( Class->GetStringMetaDataHierarchical(DevelopmentStatusKey, /*out*/ &DevelopmentStatus) )
		{
			bIsExperimental = DevelopmentStatus == ExperimentalValue;
			bIsEarlyAccess = DevelopmentStatus == EarlyAccessValue;
		}
	}

	bool MigratePropertyValue(UObject* SourceObject, UProperty* SourceProperty, UObject* DestinationObject, UProperty* DestinationProperty)
	{
		FString SourceValue;

		if (SourceObject == nullptr || DestinationObject == nullptr)
		{
			return false;
		}

		// Get the property addresses for the source and destination objects.
		uint8* SourceAddr = SourceProperty->ContainerPtrToValuePtr<uint8>(SourceObject);
		uint8* DestionationAddr = DestinationProperty->ContainerPtrToValuePtr<uint8>(DestinationObject);

		if (SourceAddr == nullptr || DestionationAddr == nullptr)
		{
			return false;
		}

		// Get the current value from the source object.
		SourceProperty->ExportText_Direct(SourceValue, SourceAddr, SourceAddr, nullptr, PPF_Localized);

		if ( !DestinationObject->HasAnyFlags(RF_ClassDefaultObject) )
		{
			FEditPropertyChain PropertyChain;
			PropertyChain.AddHead(DestinationProperty);
			DestinationObject->PreEditChange(PropertyChain);
		}

		// Set the value on the destination object.
		DestinationProperty->ImportText(*SourceValue, DestionationAddr, 0, DestinationObject);

		if ( !DestinationObject->HasAnyFlags(RF_ClassDefaultObject) )
		{
			FPropertyChangedEvent PropertyEvent(DestinationProperty);
			DestinationObject->PostEditChangeProperty(PropertyEvent);
		}

		return true;
	}
}

#endif // WITH_EDITOR
