// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "PropertyLocalizationDataGathering.h"
#include "TextPackageNamespaceUtil.h"
#include "UTextProperty.h"

FPropertyLocalizationDataGatherer::FPropertyLocalizationDataGatherer(TArray<FGatherableTextData>& InOutGatherableTextDataArray, const UPackage* const InPackage, EPropertyLocalizationGathererResultFlags& OutResultFlags)
	: GatherableTextDataArray(InOutGatherableTextDataArray)
	, Package(InPackage)
	, ResultFlags(OutResultFlags)
	, AllObjectsInPackage()
{
	// Build up the list of objects that are within our package - we won't follow object references to things outside of our package
	{
		TArray<UObject*> AllObjectsInPackageArray;
		GetObjectsWithOuter(Package, AllObjectsInPackageArray, true, RF_Transient, EInternalObjectFlags::PendingKill);

		AllObjectsInPackage.Reserve(AllObjectsInPackageArray.Num());
		for (UObject* Object : AllObjectsInPackageArray)
		{
			AllObjectsInPackage.Add(Object);
		}
	}

	TArray<UObject*> RootObjectsInPackage;
	GetObjectsWithOuter(Package, RootObjectsInPackage, false, RF_Transient, EInternalObjectFlags::PendingKill);

	// Iterate over each root object in the package
	for (const UObject* Object : RootObjectsInPackage)
	{
		GatherLocalizationDataFromObjectWithCallbacks(Object, EPropertyLocalizationGathererTextFlags::None);
	}
}

bool FPropertyLocalizationDataGatherer::ShouldProcessObject(const UObject* Object, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	if (Object->HasAnyFlags(RF_Transient))
	{
		// Transient objects aren't saved, so skip them as part of the gather
		return false;
	}

	// Skip objects that we've already processed to avoid repeated work and cyclic chains
	bool bAlreadyProcessed = false;
	ProcessedObjects.Add(FObjectAndGatherFlags(Object, GatherTextFlags), &bAlreadyProcessed);
	return !bAlreadyProcessed;
}

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromObjectWithCallbacks(const UObject* Object, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	// See if we have a custom handler for this type
	FLocalizationDataGatheringCallback* CustomCallback = nullptr;
	for (const UClass* Class = Object->GetClass(); Class != nullptr; Class = Class->GetSuperClass())
	{
		CustomCallback = GetTypeSpecificLocalizationDataGatheringCallbacks().Find(Class);
		if (CustomCallback)
		{
			break;
		}
	}

	if (CustomCallback)
	{
		checkf(IsObjectValidForGather(Object), TEXT("Cannot gather for objects outside of the current package! Package: '%s'. Object: '%s'."), *Package->GetFullName(), *Object->GetFullName());

		if (ShouldProcessObject(Object, GatherTextFlags))
		{
			(*CustomCallback)(Object, *this, GatherTextFlags);
		}
	}
	else
	{
		GatherLocalizationDataFromObject(Object, GatherTextFlags);
	}
}

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromObject(const UObject* Object, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	checkf(IsObjectValidForGather(Object), TEXT("Cannot gather for objects outside of the current package! Package: '%s'. Object: '%s'."), *Package->GetFullName(), *Object->GetFullName());

	if (!ShouldProcessObject(Object, GatherTextFlags))
	{
		return;
	}

	const FString Path = Object->GetPathName();

	// Gather text from our fields.
	GatherLocalizationDataFromObjectFields(Path, Object, GatherTextFlags);

	// Also gather from the script data on UStruct types.
	{
		if (!!(GatherTextFlags & EPropertyLocalizationGathererTextFlags::ForceHasScript))
		{
			ResultFlags |= EPropertyLocalizationGathererResultFlags::HasScript;
		}

		const UStruct* Struct = Cast<UStruct>(Object);
		if (Struct)
		{
			GatherScriptBytecode(Path, Struct->Script, !!(GatherTextFlags & EPropertyLocalizationGathererTextFlags::ForceEditorOnlyScriptData));
		}
	}

	// Gather from anything that has us as their outer, as not all objects are reachable via a property pointer.
	{
		TArray<UObject*> ChildObjects;
		GetObjectsWithOuter(Object, ChildObjects, false, RF_Transient, EInternalObjectFlags::PendingKill);

		for (const UObject* ChildObject : ChildObjects)
		{
			GatherLocalizationDataFromObjectWithCallbacks(ChildObject, GatherTextFlags);
		}
	}
}

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromObjectFields(const FString& PathToParent, const UObject* Object, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	for (TFieldIterator<const UField> FieldIt(Object->GetClass(), EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); FieldIt; ++FieldIt)
	{
		// Gather text from the property data
		{
			const UProperty* PropertyField = Cast<UProperty>(*FieldIt);
			if (PropertyField)
			{
				GatherLocalizationDataFromChildTextProperties(PathToParent, PropertyField, PropertyField->ContainerPtrToValuePtr<void>(Object), GatherTextFlags | (PropertyField->HasAnyPropertyFlags(CPF_EditorOnly) ? EPropertyLocalizationGathererTextFlags::ForceEditorOnly : EPropertyLocalizationGathererTextFlags::None));
			}
		}

		// Gather text from the script bytecode
		{
			const UStruct* StructField = Cast<UStruct>(*FieldIt);
			if (StructField && IsObjectValidForGather(StructField))
			{
				GatherLocalizationDataFromObject(StructField, GatherTextFlags);
			}
		}
	}
}

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromStructFields(const FString& PathToParent, const UStruct* Struct, const void* StructData, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	for (TFieldIterator<const UField> FieldIt(Struct, EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); FieldIt; ++FieldIt)
	{
		// Gather text from the property data
		{
			const UProperty* PropertyField = Cast<UProperty>(*FieldIt);
			if (PropertyField)
			{
				GatherLocalizationDataFromChildTextProperties(PathToParent, PropertyField, PropertyField->ContainerPtrToValuePtr<void>(StructData), GatherTextFlags | (PropertyField->HasAnyPropertyFlags(CPF_EditorOnly) ? EPropertyLocalizationGathererTextFlags::ForceEditorOnly : EPropertyLocalizationGathererTextFlags::None));
			}
		}

		// Gather text from the script bytecode
		{
			const UStruct* StructField = Cast<UStruct>(*FieldIt);
			if (StructField && IsObjectValidForGather(StructField))
			{
				GatherLocalizationDataFromObject(StructField, GatherTextFlags);
			}
		}
	}
}

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromChildTextProperties(const FString& PathToParent, const UProperty* const Property, const void* const ValueAddress, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	if (Property->HasAnyPropertyFlags(CPF_Transient))
	{
		// Transient properties aren't saved, so skip them as part of the gather
		return;
	}

	const UTextProperty* const TextProperty = Cast<const UTextProperty>(Property);
	const UArrayProperty* const ArrayProperty = Cast<const UArrayProperty>(Property);
	const UMapProperty* const MapProperty = Cast<const UMapProperty>(Property);
	const USetProperty* const SetProperty = Cast<const USetProperty>(Property);
	const UStructProperty* const StructProperty = Cast<const UStructProperty>(Property);
	const UObjectPropertyBase* const ObjectProperty = Cast<const UObjectPropertyBase>(Property);

	// Property is a text property.
	if (TextProperty)
	{
		GatherLocalizationDataFromTextProperty(PathToParent, TextProperty, ValueAddress, GatherTextFlags);
	}
	else
	{
		const EPropertyLocalizationGathererTextFlags ChildPropertyGatherTextFlags = GatherTextFlags | (Property->HasAnyPropertyFlags(CPF_EditorOnly) ? EPropertyLocalizationGathererTextFlags::ForceEditorOnly : EPropertyLocalizationGathererTextFlags::None);

		// Handles both native, fixed-size arrays and plain old non-array properties.
		const bool IsFixedSizeArray = Property->ArrayDim > 1;
		for(int32 i = 0; i < Property->ArrayDim; ++i)
		{
			const FString PathToElement = FString(PathToParent.IsEmpty() ? TEXT("") : PathToParent + TEXT(".")) + (IsFixedSizeArray ? Property->GetName() + FString::Printf(TEXT("[%d]"), i) : Property->GetName());
			const void* const ElementValueAddress = reinterpret_cast<const uint8*>(ValueAddress) + Property->ElementSize * i;

			// Property is a DYNAMIC array property.
			if (ArrayProperty)
			{
				// Iterate over all elements of the array.
				FScriptArrayHelper ScriptArrayHelper(ArrayProperty, ElementValueAddress);
				const int32 ElementCount = ScriptArrayHelper.Num();
				for(int32 j = 0; j < ElementCount; ++j)
				{
					GatherLocalizationDataFromChildTextProperties(PathToElement + FString::Printf(TEXT("(%d)"), j), ArrayProperty->Inner, ScriptArrayHelper.GetRawPtr(j), ChildPropertyGatherTextFlags);
				}
			}
			// Property is a map property.
			else if (MapProperty)
			{
				// Iterate over all elements of the map.
				FScriptMapHelper ScriptMapHelper(MapProperty, ElementValueAddress);
				const int32 ElementCount = ScriptMapHelper.Num();
				for(int32 j = 0; j < ElementCount; ++j)
				{
					if (!ScriptMapHelper.IsValidIndex(j))
					{
						continue;
					}

					const uint8* MapPairPtr = ScriptMapHelper.GetPairPtr(j);
					GatherLocalizationDataFromChildTextProperties(PathToElement + FString::Printf(TEXT("(%d - Key)"), j), MapProperty->KeyProp, MapPairPtr + MapProperty->MapLayout.KeyOffset, ChildPropertyGatherTextFlags);
					GatherLocalizationDataFromChildTextProperties(PathToElement + FString::Printf(TEXT("(%d - Value)"), j), MapProperty->ValueProp, MapPairPtr + MapProperty->MapLayout.ValueOffset, ChildPropertyGatherTextFlags);
				}
			}
			// Property is a set property.
			else if (SetProperty)
			{
				// Iterate over all elements of the Set.
				FScriptSetHelper ScriptSetHelper(SetProperty, ElementValueAddress);
				const int32 ElementCount = ScriptSetHelper.Num();
				for(int32 j = 0; j < ElementCount; ++j)
				{
					if (!ScriptSetHelper.IsValidIndex(j))
					{
						continue;
					}

					const uint8* ElementPtr = ScriptSetHelper.GetElementPtr(j);
					GatherLocalizationDataFromChildTextProperties(PathToElement + FString::Printf(TEXT("(%d)"), j), SetProperty->ElementProp, ElementPtr + SetProperty->SetLayout.ElementOffset, ChildPropertyGatherTextFlags);
				}
			}
			// Property is a struct property.
			else if (StructProperty)
			{
				GatherLocalizationDataFromStructFields(PathToElement, StructProperty->Struct, ElementValueAddress, ChildPropertyGatherTextFlags);
			}
			// Property is an object property.
			else if (ObjectProperty)
			{
				const UObject* InnerObject = ObjectProperty->GetObjectPropertyValue(ElementValueAddress);
				if (InnerObject && IsObjectValidForGather(InnerObject))
				{
					GatherLocalizationDataFromObjectWithCallbacks(InnerObject, ChildPropertyGatherTextFlags);
				}
			}
		}
	}
}

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromTextProperty(const FString& PathToParent, const UTextProperty* const TextProperty, const void* const ValueAddress, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	const bool IsFixedSizeArray = TextProperty->ArrayDim > 1;
	for(int32 i = 0; i < TextProperty->ArrayDim; ++i)
	{
		const FString PathToElement = FString(PathToParent.IsEmpty() ? TEXT("") : PathToParent + TEXT(".")) + (IsFixedSizeArray ? TextProperty->GetName() + FString::Printf(TEXT("[%d]"), i) : TextProperty->GetName());
		const FText* const ElementValueAddress = reinterpret_cast<const FText*>(reinterpret_cast<const uint8*>(ValueAddress) + TextProperty->ElementSize * i);

		{
			UPackage* const PropertyPackage = TextProperty->GetOutermost();
			if( FTextInspector::GetFlags(*ElementValueAddress) & ETextFlag::ConvertedProperty )
			{
				PropertyPackage->MarkPackageDirty();
			}
		}

		GatherTextInstance(*ElementValueAddress, PathToElement, !!(GatherTextFlags & EPropertyLocalizationGathererTextFlags::ForceEditorOnlyProperties) || TextProperty->HasAnyPropertyFlags(CPF_EditorOnly));
	}
}

void FPropertyLocalizationDataGatherer::GatherTextInstance(const FText& Text, const FString& Description, const bool bIsEditorOnly)
{
	FString Namespace;
	FString Key;
	const FTextDisplayStringRef DisplayString = FTextInspector::GetSharedDisplayString(Text);
	const bool bFoundNamespaceAndKey = FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(DisplayString, Namespace, Key);

	if (!bFoundNamespaceAndKey || !Text.ShouldGatherForLocalization())
	{
		return;
	}

	ResultFlags |= EPropertyLocalizationGathererResultFlags::HasText;

	FTextSourceData SourceData;
	{
		const FString* SourceString = FTextInspector::GetSourceString(Text);
		SourceData.SourceString = SourceString ? *SourceString : TEXT("");
	}

	FGatherableTextData* GatherableTextData = GatherableTextDataArray.FindByPredicate([&](const FGatherableTextData& Candidate)
	{
		return Candidate.NamespaceName.Equals(Namespace, ESearchCase::CaseSensitive)
			&& Candidate.SourceData.SourceString.Equals(SourceData.SourceString, ESearchCase::CaseSensitive) 
			&& Candidate.SourceData.SourceStringMetaData == SourceData.SourceStringMetaData;
	});
	if(!GatherableTextData)
	{
		GatherableTextData = &GatherableTextDataArray[GatherableTextDataArray.AddDefaulted()];
		GatherableTextData->NamespaceName = Namespace;
		GatherableTextData->SourceData = SourceData;
	}

#if USE_STABLE_LOCALIZATION_KEYS
	// Make sure the namespace is what we expect for this package
	// This would usually be done when the text is loaded, but we also gather text when saving packages so we need to make sure things are consistent
	{
		const FString PackageNamespace = TextNamespaceUtil::GetPackageNamespace(Package);
		if (!PackageNamespace.IsEmpty())
		{
			Namespace = TextNamespaceUtil::BuildFullNamespace(Namespace, PackageNamespace);
		}
	}
#endif // USE_STABLE_LOCALIZATION_KEYS

	// We might attempt to add the same text multiple times if we process the same object with slightly different flags - only add this source site once though.
	{
		static const FLocMetadataObject DefaultMetadataObject;
		const bool bFoundSourceSiteContext = GatherableTextData->SourceSiteContexts.ContainsByPredicate([&](const FTextSourceSiteContext& InSourceSiteContext) -> bool
		{
			return InSourceSiteContext.KeyName.Equals(Key, ESearchCase::CaseSensitive)
				&& InSourceSiteContext.SiteDescription.Equals(Description, ESearchCase::CaseSensitive)
				&& InSourceSiteContext.IsEditorOnly == bIsEditorOnly
				&& InSourceSiteContext.IsOptional == false
				&& InSourceSiteContext.InfoMetaData == DefaultMetadataObject
				&& InSourceSiteContext.KeyMetaData == DefaultMetadataObject;
		});

		if (!bFoundSourceSiteContext)
		{
			FTextSourceSiteContext& SourceSiteContext = GatherableTextData->SourceSiteContexts[GatherableTextData->SourceSiteContexts.AddDefaulted()];
			SourceSiteContext.KeyName = Key;
			SourceSiteContext.SiteDescription = Description;
			SourceSiteContext.IsEditorOnly = bIsEditorOnly;
			SourceSiteContext.IsOptional = false;
		}
	}
}

void FPropertyLocalizationDataGatherer::GatherScriptBytecode(const FString& PathToScript, const TArray<uint8>& ScriptData, const bool bIsEditorOnly)
{
	struct FGatherTextFromScriptBytecode
	{
	public:
		FGatherTextFromScriptBytecode(const TCHAR* InSourceDescription, const TArray<uint8>& InScript, FPropertyLocalizationDataGatherer& InPropertyLocalizationDataGatherer, const bool InTreatAsEditorOnlyData)
			: SourceDescription(InSourceDescription)
			, Script(const_cast<TArray<uint8>&>(InScript)) // We won't change the script, but we have to lie so that the code in ScriptSerialization.h will compile :(
			, PropertyLocalizationDataGatherer(InPropertyLocalizationDataGatherer)
			, bTreatAsEditorOnlyData(InTreatAsEditorOnlyData)
			, bIsParsingText(false)
		{
			const int32 ScriptSizeBytes = Script.Num();

			int32 iCode = 0;
			while (iCode < ScriptSizeBytes)
			{
				SerializeExpr(iCode, DummyArchive);
			}
		}

	private:
		FLinker* GetLinker()
		{
			return nullptr;
		}

		EExprToken SerializeExpr(int32& iCode, FArchive& Ar)
		{
		#define XFERSTRING()		SerializeString(iCode, Ar)
		#define XFERUNICODESTRING() SerializeUnicodeString(iCode, Ar)
		#define XFERTEXT()			SerializeText(iCode, Ar)

		#define SERIALIZEEXPR_INC
		#define SERIALIZEEXPR_AUTO_UNDEF_XFER_MACROS
		#include "ScriptSerialization.h"
			return Expr;
		#undef SERIALIZEEXPR_INC
		#undef SERIALIZEEXPR_AUTO_UNDEF_XFER_MACROS
		}

		void SerializeString(int32& iCode, FArchive& Ar)
		{
			if (bIsParsingText)
			{
				LastParsedString.Reset();

				do
				{
					LastParsedString += (char)(Script[iCode]);

					iCode += sizeof(uint8);
				}
				while (Script[iCode-1]);
			}
			else
			{
				do
				{
					iCode += sizeof(uint8);
				}
				while (Script[iCode-1]);
			}
		}

		void SerializeUnicodeString(int32& iCode, FArchive& Ar)
		{
			if (bIsParsingText)
			{
				LastParsedString.Reset();

				do
				{
					uint16 UnicodeChar = 0;
#ifdef REQUIRES_ALIGNED_INT_ACCESS
					FMemory::Memcpy(&UnicodeChar, &Script[iCode], sizeof(uint16));
#else
					UnicodeChar = *((uint16*)(&Script[iCode]));
#endif
					LastParsedString += (TCHAR)UnicodeChar;

					iCode += sizeof(uint16);
				}
				while (Script[iCode-1] || Script[iCode-2]);
			}
			else
			{
				do
				{
					iCode += sizeof(uint16);
				}
				while (Script[iCode-1] || Script[iCode-2]);
			}
		}

		void SerializeText(int32& iCode, FArchive& Ar)
		{
			// What kind of text are we dealing with?
			const EBlueprintTextLiteralType TextLiteralType = (EBlueprintTextLiteralType)Script[iCode++];

			switch (TextLiteralType)
			{
			case EBlueprintTextLiteralType::Empty:
				// Don't need to gather empty text
				break;

			case EBlueprintTextLiteralType::LocalizedText:
				{
					bIsParsingText = true;

					SerializeExpr(iCode, Ar);
					const FString SourceString = MoveTemp(LastParsedString);

					SerializeExpr(iCode, Ar);
					const FString TextKey = MoveTemp(LastParsedString);

					SerializeExpr(iCode, Ar);
					const FString TextNamespace = MoveTemp(LastParsedString);

					bIsParsingText = false;

					const FText TextInstance = FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*SourceString, *TextNamespace, *TextKey);

					PropertyLocalizationDataGatherer.GatherTextInstance(TextInstance, FString::Printf(TEXT("%s [Script Bytecode]"), SourceDescription), bTreatAsEditorOnlyData);
				}
				break;

			case EBlueprintTextLiteralType::InvariantText:
				// Don't need to gather invariant text, but we do need to walk over the string in the buffer
				SerializeExpr(iCode, Ar);
				break;

			case EBlueprintTextLiteralType::LiteralString:
				// Don't need to gather literal strings, but we do need to walk over the string in the buffer
				SerializeExpr(iCode, Ar);
				break;

			default:
				checkf(false, TEXT("Unknown EBlueprintTextLiteralType! Please update FGatherTextFromScriptBytecode::SerializeText to handle this type of text."));
				break;
			}
		}

		const TCHAR* SourceDescription;
		TArray<uint8>& Script;
		FPropertyLocalizationDataGatherer& PropertyLocalizationDataGatherer;
		bool bTreatAsEditorOnlyData;

		FArchive DummyArchive;
		bool bIsParsingText;
		FString LastParsedString;
	};

	if (ScriptData.Num() > 0)
	{
		ResultFlags |= EPropertyLocalizationGathererResultFlags::HasScript;
	}

	FGatherTextFromScriptBytecode(*PathToScript, ScriptData, *this, bIsEditorOnly);
}

FPropertyLocalizationDataGatherer::FLocalizationDataGatheringCallbackMap& FPropertyLocalizationDataGatherer::GetTypeSpecificLocalizationDataGatheringCallbacks()
{
	static FLocalizationDataGatheringCallbackMap TypeSpecificLocalizationDataGatheringCallbacks;
	return TypeSpecificLocalizationDataGatheringCallbacks;
}
