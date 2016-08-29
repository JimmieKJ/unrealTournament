// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "BlueprintUtilities.h"
#include "Tickable.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "SlateBasics.h"
#include "ScopedTransaction.h"
#include "Editor/UnrealEd/Public/Kismet2/Kismet2NameValidators.h"
#include "TickableEditorObject.h"
#endif

#define LOCTEXT_NAMESPACE "EdGraph"

TArray<UEdGraphPin*> PinsToDelete;

class FPinDeletionQueue : 
#if WITH_EDITOR
	public FTickableEditorObject
#else
	public FTickableGameObject
#endif
{
public:
	virtual void Tick(float DeltaTime) override
	{
		for (UEdGraphPin* Pin : PinsToDelete)
		{
			delete Pin;
		}
		PinsToDelete.Empty();
	}

	virtual bool IsTickable() const override
	{
		return true;
	}

	/** return the stat id to use for this tickable **/
	virtual TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPinDeletionQueue, STATGROUP_Tickables);
	}

	virtual ~FPinDeletionQueue() {}
};

FPinDeletionQueue* PinDeletionQueue = new FPinDeletionQueue();;

//#define TRACK_PINS

#ifdef TRACK_PINS
TArray<TPair<UEdGraphPin*, FString>> PinAllocationTracking;
#endif //TRACK_PINS

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
// FEdGraphPinReference
void FEdGraphPinReference::SetPin(const UEdGraphPin* NewPin)
{
	if (NewPin)
	{
		OwningNode = NewPin->GetOwningNodeUnchecked();
		ensure(OwningNode.Get() || NewPin->IsPendingKill());
		PinId = NewPin->PinId;
	}
	else
	{
		OwningNode = nullptr;
		PinId.Invalidate();
	}
}

UEdGraphPin* FEdGraphPinReference::Get() const
{
	UEdGraphNode* ResolvedOwningNode = OwningNode.Get();
	if (ResolvedOwningNode && PinId.IsValid())
	{
		const FGuid& TargetId = PinId;
		UEdGraphPin** ExistingPin = ResolvedOwningNode->Pins.FindByPredicate([TargetId](const UEdGraphPin* TargetPin) { return TargetPin && TargetPin->PinId == TargetId; });
		if (ExistingPin)
		{
			return *ExistingPin;
		}
	}

	return nullptr;
}

/////////////////////////////////////////////////////
// Resolve enums and structs
enum class EPinResolveType : uint8
{
	OwningNode,
	LinkedTo,
	SubPins,
	ParentPin,
	ReferencePassThroughConnection
};

struct FUnresolvedPinData
{
	UEdGraphPin* ReferencingPin;
	int32 ArrayIdx;
	EPinResolveType ResolveType;
	bool bResolveSymmetrically;

	FUnresolvedPinData() : ReferencingPin(nullptr), ArrayIdx(INDEX_NONE), ResolveType(EPinResolveType::OwningNode) {}
	FUnresolvedPinData(UEdGraphPin* InReferencingPin, EPinResolveType InResolveType, int32 InArrayIdx, bool bInResolveSymmetrically = false)
		: ReferencingPin(InReferencingPin), ArrayIdx(InArrayIdx), ResolveType(InResolveType), bResolveSymmetrically(bInResolveSymmetrically)
	{}
};

struct FPinResolveId
{
	FGuid PinGuid;
	TWeakObjectPtr<UEdGraphNode> OwningNode;

	FPinResolveId() {}

	FPinResolveId(const FGuid& InPinGuid, const TWeakObjectPtr<UEdGraphNode>& InOwningNode)
		: PinGuid(InPinGuid), OwningNode(InOwningNode)
	{}

	bool operator==(const FPinResolveId& Other) const
	{
		return PinGuid == Other.PinGuid && OwningNode == Other.OwningNode;
	}

	friend inline uint32 GetTypeHash(const FPinResolveId& Id)
	{
		return HashCombine(GetTypeHash(Id.PinGuid), GetTypeHash(Id.OwningNode));
	}
};

/////////////////////////////////////////////////////
// PinHelpers
namespace PinHelpers
{
	static TMap<FPinResolveId, TArray<FUnresolvedPinData>> UnresolvedPins;
	static TMap<TWeakObjectPtr<UEdGraphPin_Deprecated>, FGuid> DeprecatedPinToNewPinGUIDMap;
	static TMap<TWeakObjectPtr<UEdGraphPin_Deprecated>, TArray<FUnresolvedPinData>> UnresolvedDeprecatedPins;

	static uint64 NumPinsInMemory = 0;

	static const TCHAR ExportTextPropDelimiter = ',';

	static FString PinIdName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, PinId);
	static FString PinNameName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, PinName);
#if WITH_EDITORONLY_DATA
	static FString PinFriendlyNameName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, PinFriendlyName);
#endif
	static FString PinToolTipName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, PinToolTip);
	static FString DirectionName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, Direction);
	static FString PinTypeName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, PinType);
	static FString DefaultValueName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, DefaultValue);
	static FString AutogeneratedDefaultValueName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, AutogeneratedDefaultValue);
	static FString DefaultObjectName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, DefaultObject);
	static FString DefaultTextValueName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, DefaultTextValue);
	static FString LinkedToName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, LinkedTo);
	static FString SubPinsName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, SubPins);
	static FString ParentPinName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, ParentPin);
	static FString ReferencePassThroughConnectionName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, ReferencePassThroughConnection);

#if WITH_EDITORONLY_DATA
	static FString PersistentGuidName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, PersistentGuid);
	static FString bHiddenName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, bHidden);
	static FString bNotConnectableName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, bNotConnectable);
	static FString bDefaultValueIsReadOnlyName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, bDefaultValueIsReadOnly);
	static FString bDefaultValueIsIgnoredName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, bDefaultValueIsIgnored);
	// Don't need bIsDiffing's name because it is transient
	static FString bAdvancedViewName = GET_MEMBER_NAME_STRING_CHECKED(UEdGraphPin, bAdvancedView);
	// Don't need bDisplayAsMutableRef's name because it is transient
	// Don't need bWasTrashed's name because it is transient
#endif
}

/////////////////////////////////////////////////////
// UEdGraphPin
UEdGraphPin* UEdGraphPin::CreatePin(UEdGraphNode* InOwningNode)
{
	check(InOwningNode);
	UEdGraphPin* NewPin = new UEdGraphPin(InOwningNode, FGuid::NewGuid());
	return NewPin;
}

void UEdGraphPin::MakeLinkTo(UEdGraphPin* ToPin)
{
	Modify();

	if (ToPin)
	{
		check(!bWasTrashed);
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

			UEdGraphPin::EnableAllConnectedNodes(MyNode);
			UEdGraphPin::EnableAllConnectedNodes(ToPin->GetOwningNode());
		}
	}
}

void UEdGraphPin::BreakLinkTo(UEdGraphPin* ToPin)
{
	Modify();

	if (ToPin)
	{
		ToPin->Modify();

		// If we do indeed link to the passed in pin...
		if (LinkedTo.Contains(ToPin))
		{
			// Check that the other pin links to us
			ensureAlwaysMsgf(ToPin->LinkedTo.Contains(this), *GetLinkInfoString(LOCTEXT("BreakLinkTo", "BreakLinkTo").ToString(), LOCTEXT("NotLinked", "not reciprocally linked with pin").ToString(), ToPin));
			ToPin->LinkedTo.Remove(this);
			LinkedTo.Remove(ToPin);
		}
		else
		{
			// Check that the other pin does not link to us
			ensureAlwaysMsgf(!ToPin->LinkedTo.Contains(this), *GetLinkInfoString(LOCTEXT("MakeLinkTo", "MakeLinkTo").ToString(), LOCTEXT("IsLinked", "is linked with pin").ToString(), ToPin));
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

	PinId = SourcePin.PinId;

	// Only move the default value if it was modified; inherit the new default value otherwise
	if (SourcePin.DefaultValue != SourcePin.AutogeneratedDefaultValue || SourcePin.DefaultObject != nullptr || SourcePin.DefaultTextValue.ToString() != SourcePin.AutogeneratedDefaultValue)
	{
		DefaultObject = SourcePin.DefaultObject;
		DefaultValue = SourcePin.DefaultValue;
		DefaultTextValue = SourcePin.DefaultTextValue;
	}

	// Copy the links
	for (int32 LinkIndex = 0; LinkIndex < SourcePin.LinkedTo.Num(); ++LinkIndex)
	{
		UEdGraphPin* OtherPin = SourcePin.LinkedTo[LinkIndex];
		check(OtherPin);

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
	UEdGraphNode* LocalOwningNode = GetOwningNodeUnchecked();
	if (LocalOwningNode != nullptr && LocalOwningNode->CanUserEditPinAdvancedViewFlag())
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
			UE_LOG(LogBlueprint, Warning, TEXT("Pin '%s' is owned by node '%s' and pin '%s' is owned by '%s', they must be owned by the same node!"), *PinName, *GetOwningNode()->GetName(), *InTargetPin->PinName, *InTargetPin->GetOwningNode()->GetName());
		}
		else if (Direction == InTargetPin->Direction)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Both pin '%s' and pin '%s' on node '%s' go in the same direction, one must be an input and the other an output!"), *PinName, *InTargetPin->PinName, *GetOwningNode()->GetName());
		}
		else if (!PinType.bIsReference && !InTargetPin->PinType.bIsReference)
		{
			UEdGraphPin* InputPin = (Direction == EGPD_Input) ? this : InTargetPin;
			UEdGraphPin* OutputPin = (Direction == EGPD_Input) ? InTargetPin : this;
			UE_LOG(LogBlueprint, Warning, TEXT("Input pin '%s' is attempting to by-ref pass-through to output pin '%s' on node '%s', however neither pin is by-ref!"), *InputPin->PinName, *OutputPin->PinName, *InputPin->GetOwningNode()->GetName());
		}
		else if (!PinType.bIsReference)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Pin '%s' on node '%s' is not by-ref but it is attempting to pass-through to '%s'"), *PinName, *GetOwningNode()->GetName(), *InTargetPin->PinName);
		}
		else if (!InTargetPin->PinType.bIsReference)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Pin '%s' on node '%s' is not by-ref but it is attempting to pass-through to '%s'"), *InTargetPin->PinName, *InTargetPin->GetOwningNode()->GetName(), *PinName);
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
	auto OwnerNode = GetOwningNode();
	return OwnerNode ? OwnerNode->GetSchema() : NULL;
#else
	return NULL;
#endif	//WITH_EDITOR
}

FString UEdGraphPin::GetDefaultAsString() const
{
	if(DefaultObject)
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
	const UEdGraphNode* FromPinNode = GetOwningNodeUnchecked();
	const FString FromPinNodeName = (FromPinNode != nullptr) ? FromPinNode->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("Unknown");
	
	const FString ToPinName = InToPin->PinName;
	const UEdGraphNode* ToPinNode = InToPin->GetOwningNodeUnchecked();
	const FString ToPinNodeName = (ToPinNode != nullptr) ? ToPinNode->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("Unknown");
	const FString LinkInfo = FString::Printf( TEXT("UEdGraphPin::%s Pin '%s' on node '%s' %s '%s' on node '%s'"), *InFunctionName, *ToPinName, *ToPinNodeName, *InInfoData, *FromPinName, *FromPinNodeName);
	return LinkInfo;
#else
	return FString();
#endif
}

void UEdGraphPin::AddStructReferencedObjects(class FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PinType.PinSubCategoryMemberReference.MemberParent);
	Collector.AddReferencedObject(DefaultObject);
}

void UEdGraphPin::SerializeAsOwningNode(FArchive& Ar, TArray<UEdGraphPin*>& ArrayRef)
{
	UEdGraphPin::SerializePinArray(Ar, ArrayRef, nullptr, EPinResolveType::OwningNode);
}

bool UEdGraphPin::Modify(bool bAlwaysMarkDirty)
{
	UEdGraphNode* LocalOwningNode = GetOwningNodeUnchecked();
	if (LocalOwningNode)
	{
		return LocalOwningNode->Modify(bAlwaysMarkDirty);
	}

	return false;
}

void UEdGraphPin::SetOwningNode(UEdGraphNode* NewOwningNode)
{
	UEdGraphNode* OldOwningNode = GetOwningNodeUnchecked();
	if (OldOwningNode)
	{
		int32 ExistingPinIdx = OldOwningNode->Pins.IndexOfByKey(this);
		if (ExistingPinIdx != INDEX_NONE)
		{
			OldOwningNode->Pins.RemoveAt(ExistingPinIdx);
		}
	}

	OwningNode = NewOwningNode;

	if (NewOwningNode)
	{
		int32 ExistingPinIdx = NewOwningNode->Pins.IndexOfByPredicate([this](UEdGraphPin* Pin) { return Pin && Pin->PinId == this->PinId; });
		if (ExistingPinIdx == INDEX_NONE)
		{
			NewOwningNode->Pins.Add(this);
		}
		else
		{
			// If a pin with this ID is already in the node, it better be this pin
			check(NewOwningNode->Pins[ExistingPinIdx] == this);
		}
	}
}

UEdGraphPin* UEdGraphPin::CreatePinFromDeprecatedPin(UEdGraphPin_Deprecated* DeprecatedPin)
{
	if (!DeprecatedPin)
	{
		return nullptr;
	}

	UEdGraphNode* DeprecatedPinOwningNode = CastChecked<UEdGraphNode>(DeprecatedPin->GetOuter());

	UEdGraphPin* NewPin = CreatePin(DeprecatedPinOwningNode);
	DeprecatedPinOwningNode->Pins.Add(NewPin);
	NewPin->InitFromDeprecatedPin(DeprecatedPin);

	return NewPin;
}

UEdGraphPin* UEdGraphPin::FindPinCreatedFromDeprecatedPin(UEdGraphPin_Deprecated* DeprecatedPin)
{
	if (!DeprecatedPin)
	{
		return nullptr;
	}

	UEdGraphNode* DeprecatedPinOwningNode = CastChecked<UEdGraphNode>(DeprecatedPin->GetOuter());
	
	UEdGraphPin* ReturnPin = nullptr;
	FGuid PinGuidThatWasCreatedFromDeprecatedPin = PinHelpers::DeprecatedPinToNewPinGUIDMap.FindRef(DeprecatedPin);
	if (PinGuidThatWasCreatedFromDeprecatedPin.IsValid())
	{
		UEdGraphPin** ExistingPin = DeprecatedPinOwningNode->Pins.FindByPredicate([&PinGuidThatWasCreatedFromDeprecatedPin](const UEdGraphPin* Pin) { return Pin && Pin->PinId == PinGuidThatWasCreatedFromDeprecatedPin; });
		if (ExistingPin)
		{
			ReturnPin = *ExistingPin;
		}
	}

	return ReturnPin;
}

bool UEdGraphPin::ExportTextItem(FString& ValueStr, int32 PortFlags) const
{
	const UStrProperty* StrPropCDO = GetDefault<UStrProperty>();
	const UTextProperty* TextPropCDO = GetDefault<UTextProperty>();
	const UBoolProperty* BoolPropCDO = GetDefault<UBoolProperty>();
	static UEdGraphPin DefaultPin(nullptr, FGuid());

	ValueStr += "(";

	ValueStr += PinHelpers::PinIdName + TEXT("=");
	PinId.ExportTextItem(ValueStr, FGuid(), nullptr, PortFlags, nullptr);
	ValueStr += PinHelpers::ExportTextPropDelimiter;

	if (PinName != DefaultPin.PinName)
	{
		ValueStr += PinHelpers::PinNameName + TEXT("=");
		StrPropCDO->ExportTextItem(ValueStr, &PinName, nullptr, nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;
	}

#if WITH_EDITORONLY_DATA
	if (!PinFriendlyName.EqualTo(DefaultPin.PinFriendlyName))
	{
		ValueStr += PinHelpers::PinFriendlyNameName + TEXT("=");
		TextPropCDO->ExportTextItem(ValueStr, &PinFriendlyName, nullptr, nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;
	}
#endif // WITH_EDITORONLY_DATA

	if (PinToolTip != DefaultPin.PinToolTip)
	{
		ValueStr += PinHelpers::PinToolTipName + TEXT("=");
		StrPropCDO->ExportTextItem(ValueStr, &PinToolTip, nullptr, nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;
	}

	if (Direction != DefaultPin.Direction)
	{
		const FString DirectionString = UEnum::GetValueAsString(TEXT("/Script/Engine.EEdGraphPinDirection"), Direction);
		ValueStr += PinHelpers::DirectionName + TEXT("=");
		StrPropCDO->ExportTextItem(ValueStr, &DirectionString, nullptr, nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;
	}

	for (TFieldIterator<UProperty> FieldIt(FEdGraphPinType::StaticStruct()); FieldIt; ++FieldIt)
	{
		FString PropertyStr;
		const uint8* PropertyAddr = FieldIt->ContainerPtrToValuePtr<uint8>(&PinType);
		const uint8* DefaultAddr = FieldIt->ContainerPtrToValuePtr<uint8>(&DefaultPin.PinType);
		FieldIt->ExportTextItem(PropertyStr, PropertyAddr, DefaultAddr, NULL, PortFlags, nullptr);

		if (!PropertyStr.IsEmpty())
		{
			ValueStr += PinHelpers::PinTypeName + TEXT(".") + FieldIt->GetName() + "=" + PropertyStr;
			ValueStr += PinHelpers::ExportTextPropDelimiter;
		}
	}

	if (DefaultValue != DefaultPin.DefaultValue)
	{
		ValueStr += PinHelpers::DefaultValueName + TEXT("=");
		StrPropCDO->ExportTextItem(ValueStr, &DefaultValue, nullptr, nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;
	}

	if (AutogeneratedDefaultValue != DefaultPin.AutogeneratedDefaultValue)
	{
		ValueStr += PinHelpers::AutogeneratedDefaultValueName + TEXT("=");
		StrPropCDO->ExportTextItem(ValueStr, &AutogeneratedDefaultValue, nullptr, nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;
	}

	if (DefaultObject != DefaultPin.DefaultObject)
	{
		const FString DefaultObjectPath = GetPathNameSafe(DefaultObject);
		ValueStr += PinHelpers::DefaultObjectName + TEXT("=");
		StrPropCDO->ExportTextItem(ValueStr, &DefaultObjectPath, nullptr, nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;
	}

	if (!DefaultTextValue.EqualTo(DefaultPin.DefaultTextValue))
	{
		ValueStr += PinHelpers::DefaultTextValueName + TEXT("=");
		TextPropCDO->ExportTextItem(ValueStr, &DefaultTextValue, nullptr, nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;
	}

	if (LinkedTo.Num() > 0)
	{
		ValueStr += PinHelpers::LinkedToName + TEXT("=") + UEdGraphPin::ExportText_PinArray(LinkedTo);
		ValueStr += PinHelpers::ExportTextPropDelimiter;
	}

	if (SubPins.Num() > 0)
	{
		ValueStr += PinHelpers::SubPinsName + TEXT("=") + UEdGraphPin::ExportText_PinArray(SubPins);
		ValueStr += PinHelpers::ExportTextPropDelimiter;
	}

	if (ParentPin)
	{
		ValueStr += PinHelpers::ParentPinName + TEXT("=") + UEdGraphPin::ExportText_PinReference(ParentPin);
		ValueStr += PinHelpers::ExportTextPropDelimiter;
	}

	if (ReferencePassThroughConnection)
	{
		ValueStr += PinHelpers::ReferencePassThroughConnectionName + TEXT("=") + UEdGraphPin::ExportText_PinReference(ReferencePassThroughConnection);
		ValueStr += PinHelpers::ExportTextPropDelimiter;
	}

#if WITH_EDITORONLY_DATA
	{
		ValueStr += PinHelpers::PersistentGuidName + TEXT("=");
		PersistentGuid.ExportTextItem(ValueStr, FGuid(), nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;

		ValueStr += PinHelpers::bHiddenName + TEXT("=");
		bool LocalHidden = bHidden;
		BoolPropCDO->ExportTextItem(ValueStr, &LocalHidden, nullptr, nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;

		ValueStr += PinHelpers::bNotConnectableName + TEXT("=");
		bool LocalNotConnectable = bNotConnectable;
		BoolPropCDO->ExportTextItem(ValueStr, &LocalNotConnectable, nullptr, nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;

		ValueStr += PinHelpers::bDefaultValueIsReadOnlyName + TEXT("=");
		bool LocalDefaultValueIsReadOnly = bDefaultValueIsReadOnly;
		BoolPropCDO->ExportTextItem(ValueStr, &LocalDefaultValueIsReadOnly, nullptr, nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;

		ValueStr += PinHelpers::bDefaultValueIsIgnoredName + TEXT("=");
		bool LocalDefaultValueIsIgnored = bDefaultValueIsIgnored;
		BoolPropCDO->ExportTextItem(ValueStr, &LocalDefaultValueIsIgnored, nullptr, nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;

		// Intentionally not exporting bIsDiffing as it is transient

		ValueStr += PinHelpers::bAdvancedViewName + TEXT("=");
		bool LocalAdvancedView = bAdvancedView;
		BoolPropCDO->ExportTextItem(ValueStr, &LocalAdvancedView, nullptr, nullptr, PortFlags, nullptr);
		ValueStr += PinHelpers::ExportTextPropDelimiter;

		// Intentionally not exporting bDisplayAsMutableRef as it is transient
		// Intentionally not exporting bWasTrashed as it is transient
	}
#endif //WITH_EDITORONLY_DATA
	ValueStr += ")";

	return true;
}

static void SkipWhitespace(const TCHAR*& Str)
{
	while (Str && FChar::IsWhitespace(*Str))
	{
		Str++;
	}
}

bool UEdGraphPin::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, class UObject* Parent, FOutputDevice* ErrorText)
{
	PinId = FGuid();

	const UStrProperty* StrPropCDO = GetDefault<UStrProperty>();
	const UTextProperty* TextPropCDO = GetDefault<UTextProperty>();
	const UBoolProperty* BoolPropCDO = GetDefault<UBoolProperty>();
	UEnum* PinDirectionEnum = FindObjectChecked<UEnum>(nullptr, TEXT("/Script/Engine.EEdGraphPinDirection"));

	SkipWhitespace(Buffer);
	if (*Buffer != '(')
	{
		// Parse error
		return false;
	}

	Buffer++;
	SkipWhitespace(Buffer);

	while (*Buffer != ')')
	{
		const TCHAR* StartBuffer = Buffer;
		if (*Buffer == 0)
		{
			// Parse error
			return false;
		}

		while (*Buffer != '=')
		{
			Buffer++;
		}

		if (*Buffer == 0)
		{
			// Parse error
			return false;
		}

		int32 NumCharsInToken = Buffer - StartBuffer;
		if (NumCharsInToken <= 0)
		{
			// Parse error
			return false;
		}

		// Advance over the '='
		Buffer++;

		FString PropertyToken(NumCharsInToken, StartBuffer);
		PropertyToken = PropertyToken.TrimTrailing();
		bool bParseSuccess = false;
		if (PropertyToken == PinHelpers::PinIdName)
		{
			bParseSuccess = PinId.ImportTextItem(Buffer, PortFlags, nullptr, ErrorText);
		}
		else if (PropertyToken == PinHelpers::PinNameName)
		{
			Buffer = StrPropCDO->ImportText(Buffer, &PinName, PortFlags, nullptr, ErrorText);
			bParseSuccess = (Buffer != nullptr);
		}
#if WITH_EDITORONLY_DATA
		else if (PropertyToken == PinHelpers::PinFriendlyNameName)
		{
			Buffer = TextPropCDO->ImportText(Buffer, &PinFriendlyName, PortFlags, nullptr, ErrorText);
			bParseSuccess = (Buffer != nullptr);
		}
#endif //WITH_EDITORONLY_DATA
		else if (PropertyToken == PinHelpers::PinToolTipName)
		{
			Buffer = StrPropCDO->ImportText(Buffer, &PinToolTip, PortFlags, nullptr, ErrorText);
			bParseSuccess = (Buffer != nullptr);
		}
		else if (PropertyToken == PinHelpers::DirectionName)
		{
			FString DirectionString;
			Buffer = StrPropCDO->ImportText(Buffer, &DirectionString, PortFlags, nullptr, ErrorText);
			bParseSuccess = (Buffer != nullptr);
			if (bParseSuccess)
			{
				int32 EnumIdx = PinDirectionEnum->GetIndexByName(FName(*DirectionString));
				if (EnumIdx != INDEX_NONE)
				{
					Direction = (EEdGraphPinDirection)EnumIdx;
				}
				else
				{
					// Invalid enum entry
					bParseSuccess = false;
				}
			}
		}
		else if (PropertyToken.StartsWith(PinHelpers::PinTypeName + TEXT(".")))
		{
			FString Left;
			FString Right;
			if (ensure(PropertyToken.Split(".", &Left, &Right, ESearchCase::CaseSensitive)))
			{
				UProperty* FoundProp = FEdGraphPinType::StaticStruct()->FindPropertyByName(FName(*Right));
				if (FoundProp)
				{
					uint8* PropertyAddr = FoundProp->ContainerPtrToValuePtr<uint8>(&PinType);
					Buffer = FoundProp->ImportText(Buffer, PropertyAddr, PortFlags, nullptr, ErrorText);
					bParseSuccess = (Buffer != nullptr);
				}
				else
				{
					// Couldn't find the prop
					bParseSuccess = false;
				}
			}
			else
			{
				// Eh, split failed to find the '.' we saw in the token?
				bParseSuccess = false;
			}
		}
		else if (PropertyToken == PinHelpers::DefaultValueName)
		{
			Buffer = StrPropCDO->ImportText(Buffer, &DefaultValue, PortFlags, nullptr, ErrorText);
			bParseSuccess = (Buffer != nullptr);
		}
		else if (PropertyToken == PinHelpers::AutogeneratedDefaultValueName)
		{
			Buffer = StrPropCDO->ImportText(Buffer, &AutogeneratedDefaultValue, PortFlags, nullptr, ErrorText);
			bParseSuccess = (Buffer != nullptr);
		}
		else if (PropertyToken == PinHelpers::DefaultObjectName)
		{
			FString DefaultObjectString;
			Buffer = StrPropCDO->ImportText(Buffer, &DefaultObjectString, PortFlags, nullptr, ErrorText);
			bParseSuccess = (Buffer != nullptr);
			if (bParseSuccess)
			{
				DefaultObject = FindObject<UObject>(nullptr, *DefaultObjectString);
			}
		}
		else if (PropertyToken == PinHelpers::DefaultTextValueName)
		{
			Buffer = TextPropCDO->ImportText(Buffer, &DefaultTextValue, PortFlags, nullptr, ErrorText);
			bParseSuccess = (Buffer != nullptr);
		}
		else if (PropertyToken == PinHelpers::LinkedToName)
		{
			bParseSuccess = UEdGraphPin::ImportText_PinArray(Buffer, LinkedTo, this, EPinResolveType::LinkedTo);
		}
		else if (PropertyToken == PinHelpers::SubPinsName)
		{
			bParseSuccess = UEdGraphPin::ImportText_PinArray(Buffer, SubPins, this, EPinResolveType::SubPins);
		}
		else if (PropertyToken == PinHelpers::ParentPinName)
		{
			bParseSuccess = UEdGraphPin::ImportText_PinReference(Buffer, ParentPin, INDEX_NONE, this, EPinResolveType::ParentPin);
		}
		else if (PropertyToken == PinHelpers::ReferencePassThroughConnectionName)
		{
			bParseSuccess = UEdGraphPin::ImportText_PinReference(Buffer, ReferencePassThroughConnection, INDEX_NONE, this, EPinResolveType::ReferencePassThroughConnection);
		}
#if WITH_EDITORONLY_DATA
		else if (PropertyToken == PinHelpers::PersistentGuidName)
		{
			bParseSuccess = PersistentGuid.ImportTextItem(Buffer, PortFlags, nullptr, ErrorText);
		}
		else if (PropertyToken == PinHelpers::bHiddenName)
		{
			bool LocalHidden = bHidden;
			Buffer = BoolPropCDO->ImportText(Buffer, &LocalHidden, PortFlags, nullptr, ErrorText);
			bParseSuccess = (Buffer != nullptr);
			bHidden = LocalHidden;
		}
		else if (PropertyToken == PinHelpers::bNotConnectableName)
		{
			bool LocalNotConnectable = bNotConnectable;
			Buffer = BoolPropCDO->ImportText(Buffer, &LocalNotConnectable, PortFlags, nullptr, ErrorText);
			bParseSuccess = (Buffer != nullptr);
			bNotConnectable = LocalNotConnectable;
		}
		else if (PropertyToken == PinHelpers::bDefaultValueIsReadOnlyName)
		{
			bool LocalDefaultValueIsReadOnly = bDefaultValueIsReadOnly;
			Buffer = BoolPropCDO->ImportText(Buffer, &LocalDefaultValueIsReadOnly, PortFlags, nullptr, ErrorText);
			bParseSuccess = (Buffer != nullptr);
			bDefaultValueIsReadOnly = LocalDefaultValueIsReadOnly;
		}
		else if (PropertyToken == PinHelpers::bDefaultValueIsIgnoredName)
		{
			bool LocalDefaultValueIsIgnored = bDefaultValueIsIgnored;
			Buffer = BoolPropCDO->ImportText(Buffer, &LocalDefaultValueIsIgnored, PortFlags, nullptr, ErrorText);
			bParseSuccess = (Buffer != nullptr);
			bDefaultValueIsIgnored = LocalDefaultValueIsIgnored;
		}
		// Intentionally not importing bIsDiffing as it is transient
		else if (PropertyToken == PinHelpers::bAdvancedViewName)
		{
			bool LocalAdvancedView = bAdvancedView;
			Buffer = BoolPropCDO->ImportText(Buffer, &LocalAdvancedView, PortFlags, nullptr, ErrorText);
			bParseSuccess = (Buffer != nullptr);
			bAdvancedView = LocalAdvancedView;
		}
		// Intentionally not importing bDisplayAsMutableRef as it is transient
		// Intentionally not importing bWasTrashed as it is transient
#endif

		if (!bParseSuccess)
		{
			// Parse error
			return false;
		}

		SkipWhitespace(Buffer);

		if (*Buffer == PinHelpers::ExportTextPropDelimiter)
		{
			Buffer++;
		}

		SkipWhitespace(Buffer);
	}

	// Advance over the last ')'
	Buffer++;

	if (!PinId.IsValid())
	{
		// Did not contain a valid PinId
		return false;
	}

	// Someone might have been waiting for this pin to be created. Let them know that this pin now exists.
	ResolveReferencesToPin(this, false);

	return true;
}

void UEdGraphPin::MarkPendingKill()
{
	if (!bWasTrashed)
	{
		DestroyImpl(true);
	}
}

void UEdGraphPin::ShutdownVerification()
{
	Purge();
	// There's a static UEdGraphPin in UEdGraphPin::ExportTextItem so if that code
	// has run we'll have a single pin 'in memory' on shutdown
	ensure(PinHelpers::NumPinsInMemory == 0 || PinHelpers::NumPinsInMemory == 1);
}

void UEdGraphPin::Purge()
{
	PinDeletionQueue->Tick(0.f);
}

UEdGraphPin::UEdGraphPin(UEdGraphNode* InOwningNode, const FGuid& PinIdGuid)
	: OwningNode(InOwningNode)
	, PinId(PinIdGuid)
	, PinName()
#if WITH_EDITORONLY_DATA
	, PinFriendlyName()
#endif // WITH_EDITORONLY_DATA
	, PinToolTip()
	, Direction(EGPD_Input)
	, PinType()
	, DefaultValue()
	, AutogeneratedDefaultValue()
	, DefaultObject(nullptr)
	, DefaultTextValue()
	, LinkedTo()
	, SubPins()
	, ParentPin(nullptr)
	, ReferencePassThroughConnection(nullptr)
#if WITH_EDITORONLY_DATA
	, PersistentGuid()
	, bHidden(false)
	, bNotConnectable(false)
	, bDefaultValueIsReadOnly(false)
	, bDefaultValueIsIgnored(false)
	, bIsDiffing(false)
	, bAdvancedView(false)
	, bDisplayAsMutableRef(false)
#endif
	, bWasTrashed(false)
{
	PinHelpers::NumPinsInMemory++;
#ifdef TRACK_PINS
	PinAllocationTracking.Add(TPairInitializer<UEdGraphPin*, FString>(this, InOwningNode ? InOwningNode->GetName() : FString(TEXT("UNOWNED"))));
#endif //TRACK_PINS
}

UEdGraphPin::~UEdGraphPin()
{
	check(PinHelpers::NumPinsInMemory > 0);
	PinHelpers::NumPinsInMemory--;
#ifdef TRACK_PINS
	PinAllocationTracking.RemoveAll([this](const TPair<UEdGraphPin*, FString>& Entry) { return Entry.Key == this; });
#endif //TRACK_PINS
}

void UEdGraphPin::ResolveAllPinReferences()
{
	for (auto& Entry : PinHelpers::UnresolvedPins)
	{
		FPinResolveId Key = Entry.Key;
		UEdGraphNode* Node = Key.OwningNode.Get();
		if (!Node)
		{
			continue;
		}

		const FGuid& LocalGuid = Key.PinGuid;
		UEdGraphPin** TargetPin = Node->Pins.FindByPredicate([&LocalGuid](const UEdGraphPin* Pin) { return Pin && Pin->PinId == LocalGuid; });
		if (TargetPin)
		{
			ResolveReferencesToPin(*TargetPin);
		}
	}
	PinHelpers::UnresolvedPins.Empty();
}

void UEdGraphPin::InitFromDeprecatedPin(class UEdGraphPin_Deprecated* DeprecatedPin)
{
	check(DeprecatedPin);
	PinHelpers::DeprecatedPinToNewPinGUIDMap.Add(DeprecatedPin, PinId);
	DeprecatedPin->FixupDefaultValue();

	OwningNode = CastChecked<UEdGraphNode>(DeprecatedPin->GetOuter());

	PinName = DeprecatedPin->PinName;

#if WITH_EDITORONLY_DATA
	PinFriendlyName = DeprecatedPin->PinFriendlyName;
#endif

	PinToolTip = DeprecatedPin->PinToolTip;
	Direction = DeprecatedPin->Direction;
	PinType = DeprecatedPin->PinType;
	DefaultValue = DeprecatedPin->DefaultValue;
	AutogeneratedDefaultValue = DeprecatedPin->AutogeneratedDefaultValue;
	DefaultObject = DeprecatedPin->DefaultObject;
	DefaultTextValue = DeprecatedPin->DefaultTextValue;
	
	LinkedTo.Empty(DeprecatedPin->LinkedTo.Num());
	LinkedTo.SetNum(DeprecatedPin->LinkedTo.Num());
	for (int32 PinIdx = 0; PinIdx < DeprecatedPin->LinkedTo.Num(); ++PinIdx)
	{
		UEdGraphPin_Deprecated* OldLinkedToPin = DeprecatedPin->LinkedTo[PinIdx];
		UEdGraphPin*& NewPin = LinkedTo[PinIdx];
		// check for corrupt data with cross graph links:
		if (OldLinkedToPin && OldLinkedToPin->GetTypedOuter<UEdGraph>() == DeprecatedPin->GetTypedOuter<UEdGraph>())
		{
			UEdGraphPin* ExistingPin = FindPinCreatedFromDeprecatedPin(OldLinkedToPin);
			if (ExistingPin)
			{
				NewPin = ExistingPin;
			}
			else
			{
				TArray<FUnresolvedPinData>& UnresolvedPinData = PinHelpers::UnresolvedDeprecatedPins.FindOrAdd(OldLinkedToPin);
				UnresolvedPinData.Add(FUnresolvedPinData(this, EPinResolveType::LinkedTo, PinIdx));
			}
		}
	}
	
	SubPins.Empty(DeprecatedPin->SubPins.Num());
	SubPins.SetNum(DeprecatedPin->SubPins.Num());
	for (int32 PinIdx = 0; PinIdx < DeprecatedPin->SubPins.Num(); ++PinIdx)
	{
		UEdGraphPin_Deprecated* OldSubPinsPin = DeprecatedPin->SubPins[PinIdx];
		UEdGraphPin*& NewPin = SubPins[PinIdx];
		if (OldSubPinsPin)
		{
			UEdGraphPin* ExistingPin = FindPinCreatedFromDeprecatedPin(OldSubPinsPin);
			if (ExistingPin)
			{
				NewPin = ExistingPin;
			}
			else
			{
				TArray<FUnresolvedPinData>& UnresolvedPinData = PinHelpers::UnresolvedDeprecatedPins.FindOrAdd(OldSubPinsPin);
				UnresolvedPinData.Add(FUnresolvedPinData(this, EPinResolveType::SubPins, PinIdx));
			}
		}
	}
	
	if (DeprecatedPin->ParentPin)
	{
		UEdGraphPin* ExistingPin = FindPinCreatedFromDeprecatedPin(DeprecatedPin->ParentPin);
		if (ExistingPin)
		{
			ParentPin = ExistingPin;
		}
		else
		{
			TArray<FUnresolvedPinData>& UnresolvedPinData = PinHelpers::UnresolvedDeprecatedPins.FindOrAdd(DeprecatedPin->ParentPin);
			UnresolvedPinData.Add(FUnresolvedPinData(this, EPinResolveType::ParentPin, INDEX_NONE));
		}
	}
	
	if (DeprecatedPin->ReferencePassThroughConnection)
	{
		UEdGraphPin* ExistingPin = FindPinCreatedFromDeprecatedPin(DeprecatedPin->ReferencePassThroughConnection);
		if (ExistingPin)
		{
			ReferencePassThroughConnection = ExistingPin;
		}
		else
		{
			TArray<FUnresolvedPinData>& UnresolvedPinData = PinHelpers::UnresolvedDeprecatedPins.FindOrAdd(DeprecatedPin->ReferencePassThroughConnection);
			UnresolvedPinData.Add(FUnresolvedPinData(this, EPinResolveType::ReferencePassThroughConnection, INDEX_NONE));
		}
	}

#if WITH_EDITORONLY_DATA
	bHidden = DeprecatedPin->bHidden;
	bNotConnectable = DeprecatedPin->bNotConnectable;
	bDefaultValueIsReadOnly = DeprecatedPin->bDefaultValueIsReadOnly;
	bDefaultValueIsIgnored = DeprecatedPin->bDefaultValueIsIgnored;
	bIsDiffing = DeprecatedPin->bIsDiffing;
	bAdvancedView = DeprecatedPin->bAdvancedView;
	bDisplayAsMutableRef = DeprecatedPin->bDisplayAsMutableRef;
	PersistentGuid = DeprecatedPin->PersistentGuid;
#endif // WITH_EDITORONLY_DATA

	// Now resolve any references this pin which was created from a deprecated pin. This should only be from other pins that were created from deprecated pins.
	TArray<FUnresolvedPinData>* UnresolvedPinData = PinHelpers::UnresolvedDeprecatedPins.Find(DeprecatedPin);
	if (UnresolvedPinData)
	{
		for (const FUnresolvedPinData& PinData : *UnresolvedPinData)
		{
			UEdGraphPin* ReferencingPin = PinData.ReferencingPin;
			if (ReferencingPin)
			{
				switch (PinData.ResolveType)
				{
				case EPinResolveType::LinkedTo:
					ReferencingPin->LinkedTo[PinData.ArrayIdx] = this;
					ensure(LinkedTo.Contains(ReferencingPin));
					break;

				case EPinResolveType::SubPins:
					ReferencingPin->SubPins[PinData.ArrayIdx] = this;
					break;

				case EPinResolveType::ParentPin:
					ReferencingPin->ParentPin = this;
					break;

				case EPinResolveType::ReferencePassThroughConnection:
					ReferencingPin->ReferencePassThroughConnection = this;
					break;

				case EPinResolveType::OwningNode:
				default:
					checkf(0, TEXT("Unhandled ResolveType %d"), PinData.ResolveType);
					break;
				}
			}
		}

		PinHelpers::UnresolvedDeprecatedPins.Remove(DeprecatedPin);
		UnresolvedPinData = nullptr;
	}
}

void UEdGraphPin::DestroyImpl(bool bClearLinks)
{
	PinsToDelete.Add(this);
	if (bClearLinks)
	{
		BreakAllPinLinks();
		if (ParentPin)
		{
			ParentPin->SubPins.Remove(this);
		}
	}
	else
	{
		LinkedTo.Empty();
	}
	OwningNode = nullptr;
	SubPins.Empty();
	ParentPin = nullptr;
	ReferencePassThroughConnection = nullptr;
	bWasTrashed = true;
}

bool UEdGraphPin::Serialize(FArchive& Ar)
{
	// These properties are in every pin and are unlikely to be removed, so they are native serialized for speed.
	Ar << OwningNode;
	Ar << PinId;

	Ar << PinName;

#if WITH_EDITORONLY_DATA
	if (!Ar.IsFilterEditorOnly())
	{
		Ar << PinFriendlyName;
	}
#endif

	Ar << PinToolTip;
	Ar << Direction;
	PinType.Serialize(Ar);
	Ar << DefaultValue;
	Ar << AutogeneratedDefaultValue;
	Ar << DefaultObject;
	Ar << DefaultTextValue;

	// There are several options to solve the problem wwe're fixing here. First,
	// the problem: we have two UObjects (A and B) that own Pin arrays, and within
	// those Pin arrays there are pins with references to pins owned by the other UObject
	// in LinkedTo. We could:
	//  * Use FEdGraphPinReference to 'softly' reference the other pins by outer+name
	//  * Defer resolving of UEdGraphPin* in volatile contexts (e.g. when serialzing)
	//  * Aggressively fix up references:
	// Our current approach is to defer resolving of UEdGraphPin*
	UEdGraphPin::SerializePinArray(Ar, LinkedTo, this, EPinResolveType::LinkedTo);

	// SubPin array is owned by a single UObject, so no complexity of keeping LinkedTo synchronized:
	UEdGraphPin::SerializePinArray(Ar, SubPins, this, EPinResolveType::SubPins);
	TArray<UEdGraphPin*> NoExistingValues; // unused for non-owner paths..
 	SerializePin(Ar, ParentPin, INDEX_NONE, this, EPinResolveType::ParentPin, NoExistingValues);
 	SerializePin(Ar, ReferencePassThroughConnection, INDEX_NONE, this, EPinResolveType::ReferencePassThroughConnection, NoExistingValues);

#if WITH_EDITORONLY_DATA
	if (!Ar.IsFilterEditorOnly())
	{
		Ar << PersistentGuid;

		// Do not reorder these! If you are adding a new one, add it to the end, and if you want to remove one, leave the others at the same offset in the uint32

		uint32 BitField = 0;
		BitField |= bHidden << 0;
		BitField |= bNotConnectable << 1;
		BitField |= bDefaultValueIsReadOnly << 2;
		BitField |= bDefaultValueIsIgnored << 3;
		// Intentionally not including bIsDiffing. It is transient.
		BitField |= bAdvancedView << 4;
		// Intentionally not including bDisplayAsMutableRef. It is transient.
		// Intentionally not including bWasTrashed. It is transient.

		Ar << BitField;

		bHidden = !!(BitField & 0x01);
		bNotConnectable = !!(BitField & 0x02);
		bDefaultValueIsReadOnly = !!(BitField & 0x04);
		bDefaultValueIsIgnored = !!(BitField & 0x08);
		// Intentionally not including bIsDiffing. It is transient.
		bAdvancedView = !!(BitField & 0x10);
		// Intentionally not including bDisplayAsMutableRef. It is transient.
		// Intentionally not including bWasTrashed. It is transient.
	}
#endif

	// Strictly speaking we could resolve now when transacting, but
	// we would still have to do the resolve step later to catch
	// any references that are created after we serialize a given node:
	if (Ar.IsLoading()
#if WITH_EDITOR 
		&& !Ar.IsTransacting()
#endif // WITH_EDITOR
		)
	{
		ResolveReferencesToPin(this);
	}

	return true;
}

void UEdGraphPin::EnableAllConnectedNodes(UEdGraphNode* InNode)
{
	// Only enable when it has not been explicitly user-disabled
	if (InNode && InNode->EnabledState == ENodeEnabledState::Disabled && !InNode->bUserSetEnabledState)
	{
		// Enable the node and clear the comment
		InNode->Modify();
		InNode->EnableNode();
		InNode->NodeComment.Empty();

		// Go through all pin connections and enable the nodes. Enabled nodes will prevent further iteration
		for (UEdGraphPin*  Pin : InNode->Pins)
		{
			for (UEdGraphPin* OtherPin : Pin->LinkedTo)
			{
				EnableAllConnectedNodes(OtherPin->GetOwningNode());
			}
		}
	}
}

void UEdGraphPin::ResolveReferencesToPin(UEdGraphPin* Pin, bool bStrictValidation)
{
	check(!Pin->bWasTrashed);
	FPinResolveId ResolveId(Pin->PinId, Pin->OwningNode);
	TArray<FUnresolvedPinData>* UnresolvedPinData = PinHelpers::UnresolvedPins.Find(ResolveId);
	if (UnresolvedPinData)
	{
		for (const FUnresolvedPinData& PinData : *UnresolvedPinData)
		{
			UEdGraphPin* ReferencingPin = PinData.ReferencingPin;
			switch (PinData.ResolveType)
			{
			case EPinResolveType::LinkedTo:
				if (ensure(ReferencingPin->LinkedTo.Num() > PinData.ArrayIdx))
				{
					ReferencingPin->LinkedTo[PinData.ArrayIdx] = Pin;
				}
				if (PinData.bResolveSymmetrically)
				{
					if (ensure(!Pin->LinkedTo.Contains(ReferencingPin)))
					{
						Pin->LinkedTo.Add(ReferencingPin);
					}
				}
				if (bStrictValidation)
				{
#if WITH_EDITOR
					// When in the middle of a transaction the LinkedTo lists will be in an 
					// indeterminate state. After PostEditUndo runs we could validate LinkedTo
					// coherence.
					ensureAlways(GIsTransacting || Pin->LinkedTo.Contains(ReferencingPin));
#else
					ensureAlways(Pin->LinkedTo.Contains(ReferencingPin));
#endif//WITH_EDITOR
				}
				break;

			case EPinResolveType::SubPins:
				ReferencingPin->SubPins[PinData.ArrayIdx] = Pin;
				break;

			case EPinResolveType::ParentPin:
				ReferencingPin->ParentPin = Pin;
				break;

			case EPinResolveType::ReferencePassThroughConnection:
				ReferencingPin->ReferencePassThroughConnection = Pin;
				break;

			case EPinResolveType::OwningNode:
			default:
				checkf(0, TEXT("Unhandled ResolveType %d"), PinData.ResolveType);
				break;
			}
		}

		PinHelpers::UnresolvedPins.Remove(ResolveId);
	}
}

void UEdGraphPin::SerializePinArray(FArchive& Ar, TArray<UEdGraphPin*>& ArrayRef, UEdGraphPin* RequestingPin, EPinResolveType ResolveType)
{
	const int32 NumCurrentEntries = ArrayRef.Num();
	int32 ArrayNum = NumCurrentEntries;
	Ar << ArrayNum;
	
	TArray<UEdGraphPin*> OldPins = Ar.IsLoading() && ResolveType == EPinResolveType::OwningNode ? ArrayRef : TArray<UEdGraphPin*>();

	if (Ar.IsLoading())
	{
		ArrayRef.SetNum(ArrayNum);
	}

	for (int32 PinIdx = 0; PinIdx < ArrayNum; ++PinIdx)
	{
		UEdGraphPin*& PinRef = ArrayRef[PinIdx];
		SerializePin(Ar, PinRef, PinIdx, RequestingPin, ResolveType, OldPins);
	}

	// free unused pins, this only happens when loading and we have already allocated pin data, which currently
	// means we're serializing for undo/redo:
	for (UEdGraphPin* Pin : OldPins)
	{
#if WITH_EDITOR
		// More complexity to handle asymmetry in the transaction buffer. If our peer node is not in the transaction
		// then we need to take ownership of the entire connection and clear both LinkedTo Arrays:
		extern UNREALED_API UEditorEngine* GEditor;
		for (int32 I = 0; I < Pin->LinkedTo.Num(); ++I )
		{
			UEdGraphPin* Peer = Pin->LinkedTo[I];
			// PeerNode will be null if the pin we were linked to was already thrown away:
			UEdGraphNode* PeerNode = Peer->GetOwningNodeUnchecked();
			if (PeerNode && !GEditor->Trans->IsObjectTransacting(PeerNode))
			{
				Pin->BreakLinkTo(Peer);
				--I;
			}
		}
#endif// WITH_EDITOR
		Pin->DestroyImpl(false);
	}
}

bool UEdGraphPin::SerializePin(FArchive& Ar, UEdGraphPin*& PinRef, int32 ArrayIdx, UEdGraphPin* RequestingPin, EPinResolveType ResolveType, TArray<UEdGraphPin*>& OldPins)
{
	bool bRetVal = true;

	bool bNullPtr = PinRef == nullptr;
	Ar << bNullPtr;
	if (bNullPtr)
	{
		if (Ar.IsLoading())
		{
			PinRef = nullptr;
		}
	}
	else
	{
		UEdGraphNode* LocalOwningNode = nullptr;
		FGuid PinGuid;
		if (!Ar.IsLoading())
		{
			check(!PinRef->bWasTrashed);
			LocalOwningNode = PinRef->OwningNode;
			PinGuid = PinRef->PinId;
		}

		Ar << LocalOwningNode;
		Ar << PinGuid;
		check(LocalOwningNode);
		check(PinGuid.IsValid());

		if (ResolveType != EPinResolveType::OwningNode)
		{
			if (Ar.IsLoading())
			{
				UEdGraphPin** ExistingPin = LocalOwningNode->Pins.FindByPredicate([&PinGuid](const UEdGraphPin* Pin) { return Pin && Pin->PinId == PinGuid; });
				if (ExistingPin
#if WITH_EDITOR
					// When transacting we're in a volatile state, and we cannot trust any pins we might find:
					&& !Ar.IsTransacting()
#endif//WITH_EDITOR
					)
				{
					PinRef = *ExistingPin;
					check(!PinRef->bWasTrashed);
				}
				else
				{
					check(RequestingPin);
#if WITH_EDITOR
					extern UNREALED_API UEditorEngine* GEditor;
					if(Ar.IsTransacting() && !GEditor->Trans->IsObjectTransacting(LocalOwningNode))
					{
						/* 
							Two possibilities:

							1. This transaction did not alter this node's LinkedTo list, and also did not
								alter ExistingPin's
							2. This transaction has altered this node's LinkedTo list, but did not alter
								ExistingPin's

							In case 2 we need to make sure that the LinkedTo connection is symmetrical, but
							in the case of 1 we have to assume that there is already a connection in ExistingPin.
							If ExistingPin's LinkedTo list *was* mutated IsObjectTransacting must return true
							for the owning node.
						*/

						check(ResolveType == EPinResolveType::LinkedTo);

						check(ExistingPin && *ExistingPin); // the transaction buffer is corrupt
						FGuid RequestingPinId = RequestingPin->PinId;
						UEdGraphPin** LinkedTo = (*ExistingPin)->LinkedTo.FindByPredicate([&RequestingPinId](const UEdGraphPin* Pin) { return Pin && Pin->PinId == RequestingPinId; });
						if (LinkedTo)
						{
							// case 1:
							// The second condition here is to avoid asserting when PinIds are reused.
							// This occur frequently due to current copy paste implementation, which does 
							// not generate/fixup PinIds at any point:
							check(*LinkedTo == RequestingPin || (*ExistingPin)->LinkedTo.FindByPredicate([RequestingPin](const UEdGraphPin* Pin) { return Pin == RequestingPin; }));
							PinRef = *ExistingPin;
						}
						else
						{
							// case 2:
							TArray<FUnresolvedPinData>& UnresolvedPinData = PinHelpers::UnresolvedPins.FindOrAdd(FPinResolveId(PinGuid, LocalOwningNode));
							UnresolvedPinData.Add(FUnresolvedPinData(RequestingPin, ResolveType, ArrayIdx, true));
							bRetVal = false;
						}
					}
					else
#endif // WITH_EDITOR
					{
						TArray<FUnresolvedPinData>& UnresolvedPinData = PinHelpers::UnresolvedPins.FindOrAdd(FPinResolveId(PinGuid, LocalOwningNode));
						UnresolvedPinData.Add(FUnresolvedPinData(RequestingPin, ResolveType, ArrayIdx));
						bRetVal = false;
					}
				}
			}
		}
		else
		{
			if (Ar.IsLoading())
			{
				UEdGraphPin** PinToReuse = OldPins.FindByPredicate([&PinGuid](const UEdGraphPin* Pin) { return Pin && Pin->PinId == PinGuid; });
				if (PinToReuse)
				{
					PinRef = *PinToReuse;
					check(!PinRef->bWasTrashed);
					OldPins.RemoveAtSwap(PinToReuse - OldPins.GetData());
				}
				else
				{
					PinRef = UEdGraphPin::CreatePin(LocalOwningNode);
				}
			}

			PinRef->Serialize(Ar);
			check(!PinRef->bWasTrashed);
		}
	}

	return bRetVal;
}

FString UEdGraphPin::ExportText_PinReference(const UEdGraphPin* Pin)
{
	if (Pin)
	{
		const FString OwningNodeString = Pin->GetOwningNodeUnchecked() ? Pin->OwningNode->GetName() : TEXT("null");
		return OwningNodeString + " " + Pin->PinId.ToString();
	}

	return FString();
}

FString UEdGraphPin::ExportText_PinArray(const TArray<UEdGraphPin*>& PinArray)
{
	FString RetVal;

	RetVal += "(";
	if (PinArray.Num() > 0)
	{
		for (UEdGraphPin* Pin : PinArray)
		{
			RetVal += ExportText_PinReference(Pin);
			RetVal += PinHelpers::ExportTextPropDelimiter;
		}
	}
	RetVal += ")";

	return RetVal;
}

bool UEdGraphPin::ImportText_PinReference(const TCHAR*& Buffer, UEdGraphPin*& PinRef, int32 ArrayIdx, UEdGraphPin* RequestingPin, EPinResolveType ResolveType)
{
	const TCHAR* BufferStart = Buffer;
	while (*Buffer != PinHelpers::ExportTextPropDelimiter && *Buffer != ')')
	{
		if (*Buffer == 0)
		{
			// Parse error
			return false;
		}

		Buffer++;
	}

	FString PinRefLine(Buffer - BufferStart, BufferStart);
	FString OwningNodeString;
	FString PinGuidString;
	if (PinRefLine.Split(TEXT(" "), &OwningNodeString, &PinGuidString, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		FGuid PinGuid;
		if (FGuid::Parse(PinGuidString, PinGuid))
		{
			UEdGraphNode* LocalOwningNode = FindObject<UEdGraphNode>(RequestingPin->GetOwningNode()->GetOuter(), *OwningNodeString);
			if (LocalOwningNode)
			{
				UEdGraphPin** ExistingPin = LocalOwningNode->Pins.FindByPredicate([&PinGuid](const UEdGraphPin* Pin) { return Pin && Pin->PinId == PinGuid; });
				if (ExistingPin)
				{
					PinRef = *ExistingPin;
				}
				else
				{
					TArray<FUnresolvedPinData>& UnresolvedPinData = PinHelpers::UnresolvedPins.FindOrAdd(FPinResolveId(PinGuid, LocalOwningNode));
					UnresolvedPinData.Add(FUnresolvedPinData(RequestingPin, ResolveType, ArrayIdx));
				}
			}
		}
	}

	return true;
}

bool UEdGraphPin::ImportText_PinArray(const TCHAR*& Buffer, TArray<UEdGraphPin*>& ArrayRef, UEdGraphPin* RequestingPin, EPinResolveType ResolveType)
{
	bool bSuccess = false;

	SkipWhitespace(Buffer);
	if (*Buffer == '(')
	{
		Buffer++;
		SkipWhitespace(Buffer);
		bool bParseError = false;
		while (*Buffer != ')')
		{
			if (*Buffer == 0)
			{
				return false;
			}

			int32 NewItemIdx = ArrayRef.Add(nullptr);
			bool bParseSuccess = ImportText_PinReference(Buffer, ArrayRef[NewItemIdx], NewItemIdx, RequestingPin, ResolveType);
			if (!bParseSuccess)
			{
				return false;
			}

			if (*Buffer == PinHelpers::ExportTextPropDelimiter)
			{
				// Advance over the ','
				Buffer++;
			}

			SkipWhitespace(Buffer);
		}

		// Advance over the last ')'
		Buffer++;

		return true;
	}

	return false;
}

/////////////////////////////////////////////////////
// UEdGraphPin

UEdGraphPin_Deprecated::UEdGraphPin_Deprecated(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	bHidden = false;
	bNotConnectable = false;
	bAdvancedView = false;
#endif // WITH_EDITORONLY_DATA
}

void UEdGraphPin_Deprecated::FixupDefaultValue()
{
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
