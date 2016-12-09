// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ObjectEditorUtils.h"
#include "UObject/PropertyPortFlags.h"

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

	static void CopySinglePropertyRecursive(const void* const InSourcePtr, UProperty* InSourceProperty, void* const InTargetPtr, UObject* InDestinationObject, UProperty* InDestinationProperty)
	{
		// Properties that are *object* properties are tricky
		// Sometimes the object will be a reference to a PIE-world object, and copying that reference back to an actor CDO asset is not a good idea
		// If the property is referencing an actor or actor component in the PIE world, then we can try and fix that reference up to the equivalent
		// from the editor world; otherwise we have to skip it
		bool bNeedsGenericCopy = true;
		if (UStructProperty* const DestStructProperty = Cast<UStructProperty>(InDestinationProperty))
		{
			UStructProperty* const SrcStructProperty = Cast<UStructProperty>(InSourceProperty);

			// Ensure that the target struct is initialized before copying fields from the source.
			DestStructProperty->InitializeValue_InContainer(InTargetPtr);

			const int32 PropertyArrayDim = DestStructProperty->ArrayDim;
			for (int32 ArrayIndex = 0; ArrayIndex < PropertyArrayDim; ArrayIndex++)
			{
				const void* const SourcePtr = SrcStructProperty->ContainerPtrToValuePtr<void>(InSourcePtr, ArrayIndex);
				void* const TargetPtr = DestStructProperty->ContainerPtrToValuePtr<void>(InTargetPtr, ArrayIndex);

				for (TFieldIterator<UProperty> It(SrcStructProperty->Struct); It; ++It)
				{
					UProperty* const InnerProperty = *It;
					CopySinglePropertyRecursive(SourcePtr, InnerProperty, TargetPtr, InDestinationObject, InnerProperty);
				}
			}

			bNeedsGenericCopy = false;
		}
		else if (UArrayProperty* const DestArrayProperty = Cast<UArrayProperty>(InDestinationProperty))
		{
			UArrayProperty* const SrcArrayProperty = Cast<UArrayProperty>(InSourceProperty);

			check(InDestinationProperty->ArrayDim == 1);
			FScriptArrayHelper SourceArrayHelper(SrcArrayProperty, SrcArrayProperty->ContainerPtrToValuePtr<void>(InSourcePtr));
			FScriptArrayHelper TargetArrayHelper(DestArrayProperty, DestArrayProperty->ContainerPtrToValuePtr<void>(InTargetPtr));

			UProperty* InnerProperty = SrcArrayProperty->Inner;
			int32 Num = SourceArrayHelper.Num();

			TargetArrayHelper.EmptyAndAddValues(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				CopySinglePropertyRecursive(SourceArrayHelper.GetRawPtr(Index), InnerProperty, TargetArrayHelper.GetRawPtr(Index), InDestinationObject, InnerProperty);
			}

			bNeedsGenericCopy = false;
		}

		// Handle copying properties that either aren't an object, or aren't part of the PIE world
		if (bNeedsGenericCopy)
		{
			const uint8* SourceAddr = InSourceProperty->ContainerPtrToValuePtr<uint8>(InSourcePtr);
			uint8* DestinationAddr = InDestinationProperty->ContainerPtrToValuePtr<uint8>(InTargetPtr);

			InSourceProperty->CopyCompleteValue(DestinationAddr, SourceAddr);
		}
	}

	bool MigratePropertyValue(UObject* SourceObject, UProperty* SourceProperty, UObject* DestinationObject, UProperty* DestinationProperty)
	{
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

		if (!DestinationObject->HasAnyFlags(RF_ClassDefaultObject))
		{
			FEditPropertyChain PropertyChain;
			PropertyChain.AddHead(DestinationProperty);
			DestinationObject->PreEditChange(PropertyChain);
		}

		CopySinglePropertyRecursive(SourceObject, SourceProperty, DestinationObject, DestinationObject, DestinationProperty);

		if (!DestinationObject->HasAnyFlags(RF_ClassDefaultObject))
		{
			FPropertyChangedEvent PropertyEvent(DestinationProperty);
			DestinationObject->PostEditChangeProperty(PropertyEvent);
		}

		return true;
	}
}

#endif // WITH_EDITOR
