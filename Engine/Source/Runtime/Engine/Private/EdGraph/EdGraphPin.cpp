// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "BlueprintUtilities.h"
#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "SlateBasics.h"
#include "ScopedTransaction.h"
#include "Editor/UnrealEd/Public/Kismet2/Kismet2NameValidators.h"
#endif

#define LOCTEXT_NAMESPACE "EdGraph"

/////////////////////////////////////////////////////
// FEdGraphPinType

bool FEdGraphPinType::Serialize(FArchive& Ar)
{
	if (Ar.UE4Ver() < VER_UE4_EDGRAPHPINTYPE_SERIALIZATION)
	{
		return false;
	}

	Ar << PinCategory;
	Ar << PinSubCategory;

	// See: FArchive& operator<<( FArchive& Ar, FWeakObjectPtr& WeakObjectPtr )
	// The PinSubCategoryObject should be serialized into the package.
	if(!Ar.IsObjectReferenceCollector() || Ar.IsModifyingWeakAndStrongReferences() || Ar.IsPersistent())
	{
		UObject* Object = PinSubCategoryObject.Get(true);
		Ar << Object;
		if( Ar.IsLoading() || Ar.IsModifyingWeakAndStrongReferences() )
		{
			PinSubCategoryObject = Object;
		}
	}

	Ar << bIsArray;
	Ar << bIsReference;
	Ar << bIsWeakPointer;
	
	if (Ar.UE4Ver() >= VER_UE4_MEMBERREFERENCE_IN_PINTYPE)
	{
		Ar << PinSubCategoryMemberReference;
	}
	else if (Ar.IsLoading() && Ar.IsPersistent())
	{
		if ((PinCategory == TEXT("delegate")) || (PinCategory == TEXT("mcdelegate")))
		{
			if (const UFunction* Signature = Cast<const UFunction>(PinSubCategoryObject.Get()))
			{
				PinSubCategoryMemberReference.MemberName = Signature->GetFName();
				PinSubCategoryMemberReference.MemberParent = Signature->GetOwnerClass();
				PinSubCategoryObject = NULL;
			}
			else
			{
				ensure(true);
			}
		}
	}

	if (Ar.UE4Ver() >= VER_UE4_SERIALIZE_PINTYPE_CONST)
	{
		Ar << bIsConst;
	}
	else if (Ar.IsLoading())
	{
		bIsConst = false;
	}

	return true;
}

/////////////////////////////////////////////////////
// GraphPinHelpers
namespace GraphPinHelpers
{
	void EnableAllConnectedNodes(UEdGraphNode* InNode)
	{
		// Only enable when it has not been explicitly user-disabled
		if(InNode && InNode->EnabledState == ENodeEnabledState::Disabled && !InNode->bUserSetEnabledState)
		{
			// Enable the node and clear the comment
			InNode->Modify();
			InNode->EnableNode();
			InNode->NodeComment.Empty();

			// Go through all pin connections and enable the nodes. Enabled nodes will prevent further iteration
			for (UEdGraphPin* Pin : InNode->Pins)
			{
				for (UEdGraphPin* OtherPin : Pin->LinkedTo)
				{
					EnableAllConnectedNodes(OtherPin->GetOwningNode());
				}
			}
		}
	};
}

/////////////////////////////////////////////////////
// UEdGraphPin

UEdGraphPin::UEdGraphPin(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bHidden = false;
	bNotConnectable = false;
	bAdvancedView = false;
#endif // WITH_EDITORONLY_DATA
}

void UEdGraphPin::MakeLinkTo(UEdGraphPin* ToPin)
{
	Modify();

	if (ToPin != NULL)
	{
		ToPin->Modify();

		// Make sure we don't already link to it
		if (!LinkedTo.Contains(ToPin))
		{
			UEdGraphNode* MyNode = GetOwningNode();

			// Check that the other pin does not link to us
			ensureMsgf(!ToPin->LinkedTo.Contains(this), *GetLinkInfoString( LOCTEXT("MakeLinkTo", "MakeLinkTo").ToString(), LOCTEXT("IsLinked", "is linked with pin").ToString(), ToPin));			    
			ensureMsgf(MyNode->GetOuter() == ToPin->GetOwningNode()->GetOuter(), *GetLinkInfoString( LOCTEXT("MakeLinkTo", "MakeLinkTo").ToString(), LOCTEXT("OuterMismatch", "has a different outer than pin").ToString(), ToPin)); // Ensure both pins belong to the same graph

			// Add to both lists
			LinkedTo.Add(ToPin);
			ToPin->LinkedTo.Add(this);

			GraphPinHelpers::EnableAllConnectedNodes(MyNode);
			GraphPinHelpers::EnableAllConnectedNodes(ToPin->GetOwningNode());
		}
	}
}

void UEdGraphPin::BreakLinkTo(UEdGraphPin* ToPin)
{
	Modify();

	if (ToPin != NULL)
	{
		ToPin->Modify();

		// If we do indeed link to the passed in pin...
		if (LinkedTo.Contains(ToPin))
		{
			LinkedTo.Remove(ToPin);

			// Check that the other pin links to us
			ensureMsgf(ToPin->LinkedTo.Contains(this), *GetLinkInfoString( LOCTEXT("BreakLinkTo", "BreakLinkTo").ToString(), LOCTEXT("NotLinked", "not reciprocally linked with pin").ToString(), ToPin) );
			ToPin->LinkedTo.Remove(this);
		}
		else
		{
			// Check that the other pin does not link to us
			ensureMsgf(!ToPin->LinkedTo.Contains(this), *GetLinkInfoString( LOCTEXT("MakeLinkTo", "MakeLinkTo").ToString(), LOCTEXT("IsLinked", "is linked with pin").ToString(), ToPin));
		}
	}
}

void UEdGraphPin::BreakAllPinLinks()
{
	TArray<UEdGraphPin*> LinkedToCopy = LinkedTo;

	for (int32 LinkIdx = 0; LinkIdx < LinkedToCopy.Num(); LinkIdx++)
	{
		BreakLinkTo(LinkedToCopy[LinkIdx]);
	}
}


void UEdGraphPin::CopyPersistentDataFromOldPin(const UEdGraphPin& SourcePin)
{
	// The name matches already, doesn't get copied here
	// The PinType, Direction, and bNotConnectable are properties generated from the schema

	// Only move the default value if it was modified; inherit the new default value otherwise
	if (SourcePin.DefaultValue != SourcePin.AutogeneratedDefaultValue || SourcePin.DefaultObject != NULL || SourcePin.DefaultTextValue.ToString() != SourcePin.AutogeneratedDefaultValue)
	{
		DefaultObject = SourcePin.DefaultObject;
		DefaultValue = SourcePin.DefaultValue;
		DefaultTextValue = SourcePin.DefaultTextValue;
	}

	// Copy the links
	for (int32 LinkIndex = 0; LinkIndex < SourcePin.LinkedTo.Num(); ++LinkIndex)
	{
		UEdGraphPin* OtherPin = SourcePin.LinkedTo[LinkIndex];
		check(NULL != OtherPin);

		Modify();
		OtherPin->Modify();

		LinkedTo.Add(OtherPin);

		// Unlike MakeLinkTo(), we attempt to ensure that the new pin (this) is inserted at the same position as the old pin (source)
		// in the OtherPin's LinkedTo array. This is necessary to ensure that the node's position in the execution order will remain
		// unchanged after nodes are reconstructed, because OtherPin may be connected to more than just this node.
		int32 Index = OtherPin->LinkedTo.Find(const_cast<UEdGraphPin*>(&SourcePin));
		if(Index != INDEX_NONE)
		{
			OtherPin->LinkedTo.Insert(this, Index);
		}
		else
		{
			// Fallback to "normal" add, just in case the old pin doesn't exist in the other pin's LinkedTo array for some reason.
			OtherPin->LinkedTo.Add(this);
		}
	}

	// If the source pin is split, then split the new one, but don't split multiple times, typically splitting is done
	// by UK2Node::ReallocatePinsDuringReconstruction or FBlueprintEditor::OnSplitStructPin, but there are several code
	// paths into this, and split state should be persistent:
	if (SourcePin.SubPins.Num() > 0 && SubPins.Num() == 0)
	{
		GetSchema()->SplitPin(this);
	}

#if WITH_EDITORONLY_DATA
	// Copy advanced visibility property, if it can be changed by user.
	// Otherwise we don't want to copy this, or we'd be ignoring new metadata that tries to hide old pins.
	UEdGraphNode* OuterNode = Cast<UEdGraphNode>(GetOuter());
	if (OuterNode != nullptr && OuterNode->CanUserEditPinAdvancedViewFlag())
	{
		bAdvancedView = SourcePin.bAdvancedView;
	}
#endif // WITH_EDITORONLY_DATA
}

void UEdGraphPin::AssignByRefPassThroughConnection(UEdGraphPin* InTargetPin)
{
	if (InTargetPin)
	{
		if (GetOwningNode() != InTargetPin->GetOwningNode())
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Pin '%s' is owned by node '%s' and pin '%s' is owned by '%s', they must be owned by the same node!"), *GetName(), *GetOwningNode()->GetName(), *InTargetPin->GetName(), *InTargetPin->GetOwningNode()->GetName());
		}
		else if (Direction == InTargetPin->Direction)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Both pin '%s' and pin '%s' on node '%s' go in the same direction, one must be an input and the other an output!"), *GetName(), *InTargetPin->GetName(), *GetOwningNode()->GetName());
		}
		else if (!PinType.bIsReference && !InTargetPin->PinType.bIsReference)
		{
			UEdGraphPin* InputPin = (Direction == EGPD_Input) ? this : InTargetPin;
			UEdGraphPin* OutputPin = (Direction == EGPD_Input) ? InTargetPin : this;
			UE_LOG(LogBlueprint, Warning, TEXT("Input pin '%s' is attempting to by-ref pass-through to output pin '%s' on node '%s', however neither pin is by-ref!"), *InputPin->GetName(), *OutputPin->GetName(), *InputPin->GetOwningNode()->GetName());
		}
		else if (!PinType.bIsReference)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Pin '%s' on node '%s' is not by-ref but it is attempting to pass-through to '%s'"), *GetName(), *GetOwningNode()->GetName(), *InTargetPin->GetName());
		}
		else if (!InTargetPin->PinType.bIsReference)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Pin '%s' on node '%s' is not by-ref but it is attempting to pass-through to '%s'"), *InTargetPin->GetName(), *InTargetPin->GetOwningNode()->GetName(), *GetName());
		}
		else
		{
			ReferencePassThroughConnection = InTargetPin;
			InTargetPin->ReferencePassThroughConnection = this;
		}
	}
}

const class UEdGraphSchema* UEdGraphPin::GetSchema() const
{
#if WITH_EDITOR
	auto OwnerNode = GetOwningNodeUnchecked();
	return OwnerNode ? OwnerNode->GetSchema() : NULL;
#else
	return NULL;
#endif	//WITH_EDITOR
}

FString UEdGraphPin::GetDefaultAsString() const
{
	if(DefaultObject != NULL)
	{
		return DefaultObject->GetPathName();
	}
	else if(!DefaultTextValue.IsEmpty())
	{
		return DefaultTextValue.ToString();
	}
	else
	{
		return DefaultValue;
	}
}

#if WITH_EDITORONLY_DATA
FText UEdGraphPin::GetDisplayName() const
{
	FText DisplayName = FText::GetEmpty();
	auto Schema = GetSchema();
	if (Schema)
	{
		DisplayName = Schema->GetPinDisplayName(this);
	}
	else
	{
		DisplayName = (!PinFriendlyName.IsEmpty()) ? PinFriendlyName : FText::FromString(PinName);

		bool bShowNodesAndPinsUnlocalized = false;
		GConfig->GetBool( TEXT("Internationalization"), TEXT("ShowNodesAndPinsUnlocalized"), bShowNodesAndPinsUnlocalized, GEditorSettingsIni );
		if (bShowNodesAndPinsUnlocalized)
		{
			return FText::FromString(DisplayName.BuildSourceString());
		}
	}
	return DisplayName;
}
#endif // WITH_EDITORONLY_DATA

const FString UEdGraphPin::GetLinkInfoString( const FString& InFunctionName, const FString& InInfoData, const UEdGraphPin* InToPin ) const
{
#if WITH_EDITOR
	const FString FromPinName = PinName;
	const UEdGraphNode* FromPinNode = Cast<UEdGraphNode>(GetOuter());
	const FString FromPinNodeName = (FromPinNode != NULL) ? FromPinNode->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("Unknown");
	
	const FString ToPinName = InToPin->PinName;
	const UEdGraphNode* ToPinNode = Cast<UEdGraphNode>(InToPin->GetOuter());
	const FString ToPinNodeName = (ToPinNode != NULL) ? ToPinNode->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("Unknown");
	const FString LinkInfo = FString::Printf( TEXT("UEdGraphPin::%s Pin '%s' on node '%s' %s '%s' on node '%s'"), *InFunctionName, *ToPinName, *ToPinNodeName, *InInfoData, *FromPinName, *FromPinNodeName);
	return LinkInfo;
#else
	return FString();
#endif
}

void UEdGraphPin::PostLoad()
{
	Super::PostLoad();

	static FName GameplayTagName = TEXT("GameplayTag");
	static FName GameplayTagContainerName = TEXT("GameplayTagContainer");
	static FName GameplayTagsPathName = TEXT("/Script/GameplayTags");

	if (PinType.PinSubCategoryObject.IsValid())
	{
		UObject* PinSubCategoryObject = PinType.PinSubCategoryObject.Get();
		if (PinSubCategoryObject->GetOuter()->GetFName() == GameplayTagsPathName)
		{
			if (PinSubCategoryObject->GetFName() == GameplayTagName)
			{
				// Pins of type FGameplayTag were storing "()" for empty arrays and then importing that into ArrayProperty and expecting an empty array.
				// That it was working was a bug and has been fixed, so let's fixup pins. A pin that wants an array size of 1 will always fill the parenthesis
				// so there is no worry about breaking those cases.
				if (DefaultValue == TEXT("()"))
				{
					DefaultValue.Empty();
				}
			}
			else if (PinSubCategoryObject->GetFName() == GameplayTagContainerName)
			{
				// Pins of type FGameplayTagContainer were storing "GameplayTags=()" for empty arrays, which equates to having a single item, default generated as detailed above for FGameplayTag.
				// The solution is to replace occurances with an empty string, due to the item being a struct, we can't just empty the value and must replace only the section we need.
				DefaultValue.ReplaceInline(TEXT("GameplayTags=()"), TEXT("GameplayTags="));
			}
		}
	}
}

FString FGraphDisplayInfo::GetNotesAsString() const
{
	FString Result;

	if (Notes.Num() > 0)
	{
		Result = TEXT("(");
		for (int32 NoteIndex = 0; NoteIndex < Notes.Num(); ++NoteIndex)
		{
			if (NoteIndex > 0)
			{
				Result += TEXT(", ");
			}
			Result += Notes[NoteIndex];
		}
		Result += TEXT(")");
	}

	return Result;
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
