// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "Serialization/PropertyLocalizationDataGathering.h"
#include "UTextProperty.h"

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromPropertiesOfDataStructure(UStruct* const Structure, void* const Data, const bool ForceIsEditorOnly)
{
	FString Path;
	{
		UClass* const Class = Cast<UClass>(Structure);
		if (Class)
		{
			UObject* const Object = reinterpret_cast<UObject* const>(Data);
			Path = Object->GetPathName();
		}
	}

	// Iterate over all fields of the object's class.
	for (TFieldIterator<UProperty> PropIt(Structure, EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); PropIt; ++PropIt)
	{
		GatherLocalizationDataFromChildTextProperies(Path, *PropIt, PropIt->ContainerPtrToValuePtr<void>(Data));
	}
}

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromChildTextProperies(const FString& PathToParent, UProperty* const Property, void* const ValueAddress, const bool ForceIsEditorOnly)
{
	UTextProperty* const TextProperty = Cast<UTextProperty>(Property);
	UArrayProperty* const ArrayProperty = Cast<UArrayProperty>(Property);
	UStructProperty* const StructProperty = Cast<UStructProperty>(Property);

	// Property is a text property.
	if (TextProperty)
	{
		GatherLocalizationDataFromTextProperty(PathToParent, TextProperty, ValueAddress, ForceIsEditorOnly);
	}
	else
	{
		// Handles both native, fixed-size arrays and plain old non-array properties.
		const bool IsFixedSizeArray = Property->ArrayDim > 1;
		for(int32 i = 0; i < Property->ArrayDim; ++i)
		{
			const FString PathToElement = FString(PathToParent.IsEmpty() ? TEXT("") : PathToParent + TEXT(".")) + (IsFixedSizeArray ? Property->GetName() + FString::Printf(TEXT("[%d]"), i) : Property->GetName());
			void* const ElementValueAddress = reinterpret_cast<uint8*>(ValueAddress) + Property->ElementSize * i;

			// Property is a DYNAMIC array property.
			if (ArrayProperty)
			{
				// Iterate over all elements of the array.
				FScriptArrayHelper ScriptArrayHelper(ArrayProperty, ElementValueAddress);
				const int32 ElementCount = ScriptArrayHelper.Num();
				for(int32 j = 0; j < ElementCount; ++j)
				{
					GatherLocalizationDataFromChildTextProperies(PathToElement + FString::Printf(TEXT("(%d)"), j), ArrayProperty->Inner, ScriptArrayHelper.GetRawPtr(j), ForceIsEditorOnly || Property->HasAnyPropertyFlags(CPF_EditorOnly));
				}
			}
			// Property is a struct property.
			else if (StructProperty)
			{
				// Iterate over all properties of the struct's class.
				for (TFieldIterator<UProperty>PropIt(StructProperty->Struct, EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); PropIt; ++PropIt)
				{
					const FString Path = PathToElement;

					GatherLocalizationDataFromChildTextProperies(PathToElement, *PropIt, PropIt->ContainerPtrToValuePtr<void>(ElementValueAddress), ForceIsEditorOnly || Property->HasAnyPropertyFlags(CPF_EditorOnly));
				}
			}
		}
	}
}

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromTextProperty(const FString& PathToParent, UTextProperty* const TextProperty, void* const ValueAddress, bool ForceIsEditorOnly)
{
	const bool IsFixedSizeArray = TextProperty->ArrayDim > 1;
	for(int32 i = 0; i < TextProperty->ArrayDim; ++i)
	{
		const FString PathToElement = FString(PathToParent.IsEmpty() ? TEXT("") : PathToParent + TEXT(".")) + (IsFixedSizeArray ? TextProperty->GetName() + FString::Printf(TEXT("[%d]"), i) : TextProperty->GetName());
		FText* const ElementValueAddress = reinterpret_cast<FText*>(reinterpret_cast<uint8*>(ValueAddress) + TextProperty->ElementSize * i);

		UPackage* const Package = TextProperty->GetOutermost();
		if( FTextInspector::GetFlags(*ElementValueAddress) & ETextFlag::ConvertedProperty )
		{
			Package->MarkPackageDirty();
		}

		FString Namespace;
		FString Key;
		const FTextDisplayStringRef DisplayString = FTextInspector::GetSharedDisplayString(*ElementValueAddress);
		const bool FoundNamespaceAndKey = FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(DisplayString, Namespace, Key);

		if (!ElementValueAddress->ShouldGatherForLocalization())
		{
			return;
		}

		FTextSourceData SourceData;
		{
			const FString* SourceString = FTextInspector::GetSourceString(*ElementValueAddress);
			SourceData.SourceString = SourceString ? *SourceString : TEXT("");
		}

		FGatherableTextData* GatherableTextData = GatherableTextDataArray.FindByPredicate([&](const FGatherableTextData& Candidate)
		{
			return	Candidate.NamespaceName == Namespace && 
				Candidate.SourceData.SourceString == SourceData.SourceString &&
				Candidate.SourceData.SourceStringMetaData == SourceData.SourceStringMetaData;
		});
		if(!GatherableTextData)
		{
			GatherableTextData = new(GatherableTextDataArray) FGatherableTextData;
			GatherableTextData->NamespaceName = Namespace;
			GatherableTextData->SourceData = SourceData;
		}

		// If this text has no key, fix it by generating a new GUID for the key.
		if (!FoundNamespaceAndKey || Key.IsEmpty())
		{
			const FString NewKey = FGuid::NewGuid().ToString();
			bool HasSuccessfullySetKey = false;
			if (FoundNamespaceAndKey)
			{
				HasSuccessfullySetKey = FTextLocalizationManager::Get().UpdateDisplayString(DisplayString, *DisplayString, Namespace, NewKey);
			}
			else
			{
				HasSuccessfullySetKey = FTextLocalizationManager::Get().AddDisplayString(DisplayString, TEXT(""), NewKey);
			}

			if (HasSuccessfullySetKey)
			{
				Key = NewKey;
			}
		}

		// Add to map.
		{
			FTextSourceSiteContext& SourceSiteContext = *(new(GatherableTextData->SourceSiteContexts) FTextSourceSiteContext);
			SourceSiteContext.KeyName = Key;
			SourceSiteContext.SiteDescription = PathToElement;
			SourceSiteContext.IsEditorOnly = ForceIsEditorOnly || TextProperty->HasAnyPropertyFlags(CPF_EditorOnly);
			SourceSiteContext.IsOptional = false;
		}
	}
}