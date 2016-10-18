// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimBlueprintNodeOptionalPinManager.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimationGraphSchema.h"

FAnimBlueprintNodeOptionalPinManager::FAnimBlueprintNodeOptionalPinManager(class UAnimGraphNode_Base* Node, TArray<UEdGraphPin*>* InOldPins)
	: BaseNode(Node)
	, OldPins(InOldPins)
{
	if (OldPins != NULL)
	{
		for (auto PinIt = OldPins->CreateIterator(); PinIt; ++PinIt)
		{
			UEdGraphPin* Pin = *PinIt;
			OldPinMap.Add(Pin->PinName, Pin);
		}
	}
}

void FAnimBlueprintNodeOptionalPinManager::GetRecordDefaults(UProperty* TestProperty, FOptionalPinFromProperty& Record) const
{
	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();

	// Determine if this is a pose or array of poses
	UArrayProperty* ArrayProp = Cast<UArrayProperty>(TestProperty);
	UStructProperty* StructProp = Cast<UStructProperty>((ArrayProp != NULL) ? ArrayProp->Inner : TestProperty);
	const bool bIsPoseInput = (StructProp != NULL) && (StructProp->Struct->IsChildOf(FPoseLinkBase::StaticStruct()));

	//@TODO: Error if they specified two or more of these flags
	const bool bAlwaysShow = TestProperty->HasMetaData(Schema->NAME_AlwaysAsPin) || bIsPoseInput;
	const bool bOptional_ShowByDefault = TestProperty->HasMetaData(Schema->NAME_PinShownByDefault);
	const bool bOptional_HideByDefault = TestProperty->HasMetaData(Schema->NAME_PinHiddenByDefault);
	const bool bNeverShow = TestProperty->HasMetaData(Schema->NAME_NeverAsPin);
	const bool bPropertyIsCustomized = TestProperty->HasMetaData(Schema->NAME_CustomizeProperty);

	Record.bCanToggleVisibility = bOptional_ShowByDefault || bOptional_HideByDefault;
	Record.bShowPin = bAlwaysShow || bOptional_ShowByDefault;
	Record.bPropertyIsCustomized = bPropertyIsCustomized;
}

void FAnimBlueprintNodeOptionalPinManager::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex, UProperty* Property) const
{
	if (BaseNode != NULL)
	{
		BaseNode->CustomizePinData(Pin, SourcePropertyName, ArrayIndex);
	}
}

void FAnimBlueprintNodeOptionalPinManager::PostInitNewPin(UEdGraphPin* Pin, FOptionalPinFromProperty& Record, int32 ArrayIndex, UProperty* Property, uint8* PropertyAddress) const
{
	check(PropertyAddress != NULL);
	check(Record.bShowPin);
		


	if (OldPins == NULL)
	{
		// Initial construction of a visible pin; copy values from the struct
		FBlueprintEditorUtils::ExportPropertyToKismetDefaultValue(Pin, Property, PropertyAddress, BaseNode);
	}
	else if (Record.bCanToggleVisibility)
	{
		if (UEdGraphPin* OldPin = OldPinMap.FindRef(Pin->PinName))
		{
			// Was already visible
		}
		else
		{
			// Showing a pin that was previously hidden, during a reconstruction

			UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);
			UAnimGraphNode_AssetPlayerBase* AssetPlayerNode = Cast<UAnimGraphNode_AssetPlayerBase>(BaseNode);
			const bool bIsAnimationAsset = AssetPlayerNode != nullptr && ObjectProperty != nullptr && ObjectProperty->PropertyClass->IsChildOf<UAnimationAsset>();

			// Store the old reference to an animation asset in the node's asset reference
			if (bIsAnimationAsset)
			{
				AssetPlayerNode->SetAssetReferenceForPinRestoration(ObjectProperty->GetObjectPropertyValue(PropertyAddress));
			}
		
			// Convert the struct property into DefaultValue/DefaultValueObject
			FBlueprintEditorUtils::ExportPropertyToKismetDefaultValue(Pin, Property, PropertyAddress, BaseNode);

			// clear the asset reference on the node
			if (bIsAnimationAsset)
			{
				ObjectProperty->SetObjectPropertyValue(PropertyAddress, nullptr);
			}
		}
	}
}

void FAnimBlueprintNodeOptionalPinManager::PostRemovedOldPin(FOptionalPinFromProperty& Record, int32 ArrayIndex, UProperty* Property, uint8* PropertyAddress) const
{
	check(PropertyAddress != NULL);
	check(!Record.bShowPin);

	if (Record.bCanToggleVisibility && (OldPins != NULL))
	{
		const FString OldPinName = (ArrayIndex != INDEX_NONE) ? FString::Printf(TEXT("%s_%d"), *(Record.PropertyName.ToString()), ArrayIndex) : Record.PropertyName.ToString();
		if (UEdGraphPin* OldPin = OldPinMap.FindRef(OldPinName))
		{
			// Pin was visible but it's now hidden
			UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);
			UAnimGraphNode_AssetPlayerBase* AssetPlayerNode = Cast<UAnimGraphNode_AssetPlayerBase>(BaseNode);
			if (AssetPlayerNode && ObjectProperty && ObjectProperty->PropertyClass->IsChildOf<UAnimationAsset>())
			{
				// If this is an anim asset pin, dont store a hard reference to the asset in the node, as this ends up referencing things we might not want to load any more
				UObject* AssetReference = AssetPlayerNode->GetAssetReferenceForPinRestoration();
				if (AssetReference)
				{
					ObjectProperty->SetObjectPropertyValue(PropertyAddress, AssetReference);
				}
					
			}
			else
			{
				// Convert DefaultValue/DefaultValueObject and push back into the struct
				FBlueprintEditorUtils::ImportKismetDefaultValueToProperty(OldPin, Property, PropertyAddress, BaseNode);
			}
		}
	}
}

void FAnimBlueprintNodeOptionalPinManager::AllocateDefaultPins(UStruct* SourceStruct, uint8* StructBasePtr)
{
	RebuildPropertyList(BaseNode->ShowPinForProperties, SourceStruct);
	CreateVisiblePins(BaseNode->ShowPinForProperties, SourceStruct, EGPD_Input, BaseNode, StructBasePtr);
}