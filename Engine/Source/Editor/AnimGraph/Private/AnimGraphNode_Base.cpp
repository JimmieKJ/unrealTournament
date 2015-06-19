// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_Base.h"
#include "AnimationGraphSchema.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"

/////////////////////////////////////////////////////
// FA3NodeOptionalPinManager

struct FA3NodeOptionalPinManager : public FOptionalPinManager
{
protected:
	class UAnimGraphNode_Base* BaseNode;
	TArray<UEdGraphPin*>* OldPins;

	TMap<FString, UEdGraphPin*> OldPinMap;

public:
	FA3NodeOptionalPinManager(class UAnimGraphNode_Base* Node, TArray<UEdGraphPin*>* InOldPins)
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

	virtual void GetRecordDefaults(UProperty* TestProperty, FOptionalPinFromProperty& Record) const override
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

	virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex, UProperty* Property) const override
	{
		if (BaseNode != NULL)
		{
			BaseNode->CustomizePinData(Pin, SourcePropertyName, ArrayIndex);
		}
	}

	void PostInitNewPin(UEdGraphPin* Pin, FOptionalPinFromProperty& Record, int32 ArrayIndex, UProperty* Property, uint8* PropertyAddress) const override
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
				// Convert the struct property into DefaultValue/DefaultValueObject
				FBlueprintEditorUtils::ExportPropertyToKismetDefaultValue(Pin, Property, PropertyAddress, BaseNode);
			}
		}
	}

	void PostRemovedOldPin(FOptionalPinFromProperty& Record, int32 ArrayIndex, UProperty* Property, uint8* PropertyAddress) const override
	{
		check(PropertyAddress != NULL);
		check(!Record.bShowPin);

		if (Record.bCanToggleVisibility && (OldPins != NULL))
		{
			const FString OldPinName = (ArrayIndex != INDEX_NONE) ? FString::Printf(TEXT("%s_%d"), *(Record.PropertyName.ToString()), ArrayIndex) : Record.PropertyName.ToString();
			if (UEdGraphPin* OldPin = OldPinMap.FindRef(OldPinName))
			{
				// Pin was visible but it's now hidden
				// Convert DefaultValue/DefaultValueObject and push back into the struct
				FBlueprintEditorUtils::ImportKismetDefaultValueToProperty(OldPin, Property, PropertyAddress, BaseNode);
			}
		}
	}

	void AllocateDefaultPins(UStruct* SourceStruct, uint8* StructBasePtr)
	{
		RebuildPropertyList(BaseNode->ShowPinForProperties, SourceStruct);
		CreateVisiblePins(BaseNode->ShowPinForProperties, SourceStruct, EGPD_Input, BaseNode, StructBasePtr);
	}
};

/////////////////////////////////////////////////////
// UAnimGraphNode_Base

UAnimGraphNode_Base::UAnimGraphNode_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimGraphNode_Base::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if ((PropertyName == TEXT("bShowPin")))
	{
		GetSchema()->ReconstructNode(*this);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UAnimGraphNode_Base::CreateOutputPins()
{
	if (!IsSinkNode())
	{
		const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
		CreatePin(EGPD_Output, Schema->PC_Struct, TEXT(""), FPoseLink::StaticStruct(), /*bIsArray=*/ false, /*bIsReference=*/ false, TEXT("Pose"));
	}
}

void UAnimGraphNode_Base::InternalPinCreation(TArray<UEdGraphPin*>* OldPins)
{
	// preload required assets first before creating pins
	PreloadRequiredAssets();

	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
	if (const UStructProperty* NodeStruct = GetFNodeProperty())
	{
		// Display any currently visible optional pins
		{
			FA3NodeOptionalPinManager OptionalPinManager(this, OldPins);
			OptionalPinManager.AllocateDefaultPins(NodeStruct->Struct, NodeStruct->ContainerPtrToValuePtr<uint8>(this));
		}

		// Create the output pin, if needed
		CreateOutputPins();
	}
}

void UAnimGraphNode_Base::AllocateDefaultPins()
{
	InternalPinCreation(NULL);
}

void UAnimGraphNode_Base::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	InternalPinCreation(&OldPins);
}

FLinearColor UAnimGraphNode_Base::GetNodeTitleColor() const
{
	return FLinearColor::Black;
}

UScriptStruct* UAnimGraphNode_Base::GetFNodeType() const
{
	UScriptStruct* BaseFStruct = FAnimNode_Base::StaticStruct();

	for (TFieldIterator<UProperty> PropIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		if (UStructProperty* StructProp = Cast<UStructProperty>(*PropIt))
		{
			if (StructProp->Struct->IsChildOf(BaseFStruct))
			{
				return StructProp->Struct;
			}
		}
	}

	return NULL;
}

UStructProperty* UAnimGraphNode_Base::GetFNodeProperty() const
{
	UScriptStruct* BaseFStruct = FAnimNode_Base::StaticStruct();

	for (TFieldIterator<UProperty> PropIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		if (UStructProperty* StructProp = Cast<UStructProperty>(*PropIt))
		{
			if (StructProp->Struct->IsChildOf(BaseFStruct))
			{
				return StructProp;
			}
		}
	}

	return NULL;
}

FString UAnimGraphNode_Base::GetNodeCategory() const
{
	return TEXT("Misc.");
}

void UAnimGraphNode_Base::GetNodeAttributes( TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes ) const
{
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Type" ), TEXT( "AnimGraphNode" ) ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Class" ), GetClass()->GetName() ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Name" ), GetName() ));
}

void UAnimGraphNode_Base::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UAnimGraphNode_Base::GetMenuCategory() const
{
	return FText::FromString(GetNodeCategory());
}

void UAnimGraphNode_Base::GetPinAssociatedProperty(const UScriptStruct* NodeType, UEdGraphPin* InputPin, UProperty*& OutProperty, int32& OutIndex)
{
	OutProperty = NULL;
	OutIndex = INDEX_NONE;

	//@TODO: Name-based hackery, avoid the roundtrip and better indicate when it's an array pose pin
	int32 UnderscoreIndex = InputPin->PinName.Find(TEXT("_"), ESearchCase::CaseSensitive);
	if (UnderscoreIndex != INDEX_NONE)
	{
		FString ArrayName = InputPin->PinName.Left(UnderscoreIndex);
		int32 ArrayIndex = FCString::Atoi(*(InputPin->PinName.Mid(UnderscoreIndex + 1)));

		if (UArrayProperty* ArrayProperty = FindField<UArrayProperty>(NodeType, *ArrayName))
		{
			OutProperty = ArrayProperty;
			OutIndex = ArrayIndex;
		}
	}
	else
	{
		if (UProperty* Property = FindField<UProperty>(NodeType, *(InputPin->PinName)))
		{
			OutProperty = Property;
			OutIndex = INDEX_NONE;
		}
	}
}

FPoseLinkMappingRecord UAnimGraphNode_Base::GetLinkIDLocation(const UScriptStruct* NodeType, UEdGraphPin* SourcePin)
{
	if (SourcePin->LinkedTo.Num() > 0)
	{
		if (UAnimGraphNode_Base* LinkedNode = Cast<UAnimGraphNode_Base>(FBlueprintEditorUtils::FindFirstCompilerRelevantNode(SourcePin->LinkedTo[0])))
		{
			//@TODO: Name-based hackery, avoid the roundtrip and better indicate when it's an array pose pin
			int32 UnderscoreIndex = SourcePin->PinName.Find(TEXT("_"), ESearchCase::CaseSensitive);
			if (UnderscoreIndex != INDEX_NONE)
			{
				FString ArrayName = SourcePin->PinName.Left(UnderscoreIndex);
				int32 ArrayIndex = FCString::Atoi(*(SourcePin->PinName.Mid(UnderscoreIndex + 1)));

				if (UArrayProperty* ArrayProperty = FindField<UArrayProperty>(NodeType, *ArrayName))
				{
					if (UStructProperty* Property = Cast<UStructProperty>(ArrayProperty->Inner))
					{
						if (Property->Struct->IsChildOf(FPoseLinkBase::StaticStruct()))
						{
							return FPoseLinkMappingRecord::MakeFromArrayEntry(this, LinkedNode, ArrayProperty, ArrayIndex);
						}
					}
				}
			}
			else
			{
				if (UStructProperty* Property = FindField<UStructProperty>(NodeType, *(SourcePin->PinName)))
				{
					if (Property->Struct->IsChildOf(FPoseLinkBase::StaticStruct()))
					{
						return FPoseLinkMappingRecord::MakeFromMember(this, LinkedNode, Property);
					}
				}
			}
		}
	}

	return FPoseLinkMappingRecord::MakeInvalid();
}

void UAnimGraphNode_Base::CreatePinsForPoseLink(UProperty* PoseProperty, int32 ArrayIndex)
{
	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
	UScriptStruct* A2PoseStruct = FA2Pose::StaticStruct();

	// pose input
	const FString NewPinName = (ArrayIndex == INDEX_NONE) ? PoseProperty->GetName() : FString::Printf(TEXT("%s_%d"), *(PoseProperty->GetName()), ArrayIndex);
	CreatePin(EGPD_Input, Schema->PC_Struct, TEXT(""), A2PoseStruct, /*bIsArray=*/ false, /*bIsReference=*/ false, NewPinName);
}

void UAnimGraphNode_Base::PostProcessPinName(const UEdGraphPin* Pin, FString& DisplayName) const
{
	if (Pin->Direction == EGPD_Output)
	{
		if (Pin->PinName == TEXT("Pose"))
		{
			DisplayName = TEXT("");
		}
	}
}

bool UAnimGraphNode_Base::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const
{
	return DesiredSchema->GetClass()->IsChildOf(UAnimationGraphSchema::StaticClass());
}

FString UAnimGraphNode_Base::GetDocumentationLink() const
{
	return TEXT("Shared/GraphNodes/Animation");
}

void UAnimGraphNode_Base::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	if (UAnimationGraphSchema::IsLocalSpacePosePin(Pin.PinType))
	{
		HoverTextOut = TEXT("Animation Pose");
	}
	else if (UAnimationGraphSchema::IsComponentSpacePosePin(Pin.PinType))
	{
		HoverTextOut = TEXT("Animation Pose (Component Space)");
	}
	else
	{
		Super::GetPinHoverText(Pin, HoverTextOut);
	}
}

void UAnimGraphNode_Base::HandleAnimReferenceCollection(UAnimationAsset* AnimAsset, TArray<UAnimationAsset*>& ComplexAnims, TArray<UAnimSequence*>& AnimationSequences) const
{
	if(UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimAsset))
	{
		AnimationSequences.AddUnique(AnimSequence);
	}
	else
	{
		ComplexAnims.AddUnique(AnimAsset);
		AnimAsset->GetAllAnimationSequencesReferred(AnimationSequences);
	}
}
