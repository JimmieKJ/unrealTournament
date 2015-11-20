// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "Engine/ComponentDelegateBinding.h"
#include "K2Node_ComponentBoundEvent.h"

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_ComponentBoundEvent::UK2Node_ComponentBoundEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UK2Node_ComponentBoundEvent::Modify(bool bAlwaysMarkDirty)
{
	CachedNodeTitle.MarkDirty();

	return Super::Modify(bAlwaysMarkDirty);
}

FText UK2Node_ComponentBoundEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("DelegatePropertyName"), FText::FromName(DelegatePropertyName));
		Args.Add(TEXT("ComponentPropertyName"), FText::FromName(ComponentPropertyName));

		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("ComponentBoundEvent_Title", "{DelegatePropertyName} ({ComponentPropertyName})"), Args), this);
	}
	return CachedNodeTitle;
}

void UK2Node_ComponentBoundEvent::InitializeComponentBoundEventParams(UObjectProperty const* InComponentProperty, const UMulticastDelegateProperty* InDelegateProperty)
{
	if( InComponentProperty && InDelegateProperty )
	{
		ComponentPropertyName = InComponentProperty->GetFName();
		DelegatePropertyName = InDelegateProperty->GetFName();
		DelegateOwnerClass = CastChecked<UClass>(InDelegateProperty->GetOuter())->GetAuthoritativeClass();

		EventReference.SetFromField<UFunction>(InDelegateProperty->SignatureFunction, /*bIsConsideredSelfContext =*/false);

		CustomFunctionName = FName( *FString::Printf(TEXT("BndEvt__%s_%s_%s"), *InComponentProperty->GetName(), *GetName(), *EventReference.GetMemberName().ToString()) );
		bOverrideFunction = false;
		bInternalEvent = true;
		CachedNodeTitle.MarkDirty();
	}
}

UClass* UK2Node_ComponentBoundEvent::GetDynamicBindingClass() const
{
	return UComponentDelegateBinding::StaticClass();
}

void UK2Node_ComponentBoundEvent::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	UComponentDelegateBinding* ComponentBindingObject = CastChecked<UComponentDelegateBinding>(BindingObject);

	FBlueprintComponentDelegateBinding Binding;
	Binding.ComponentPropertyName = ComponentPropertyName;
	Binding.DelegatePropertyName = DelegatePropertyName;
	Binding.FunctionNameToBind = CustomFunctionName;

	CachedNodeTitle.MarkDirty();
	ComponentBindingObject->ComponentDelegateBindings.Add(Binding);
}

bool UK2Node_ComponentBoundEvent::IsUsedByAuthorityOnlyDelegate() const
{
	UMulticastDelegateProperty* TargetDelegateProp = GetTargetDelegateProperty();
	return (TargetDelegateProp && TargetDelegateProp->HasAnyPropertyFlags(CPF_BlueprintAuthorityOnly));
}

UMulticastDelegateProperty* UK2Node_ComponentBoundEvent::GetTargetDelegateProperty() const
{
	return Cast<UMulticastDelegateProperty>(FindField<UMulticastDelegateProperty>(DelegateOwnerClass, DelegatePropertyName));
}


FText UK2Node_ComponentBoundEvent::GetTooltipText() const
{
	UMulticastDelegateProperty* TargetDelegateProp = GetTargetDelegateProperty();
	if (TargetDelegateProp)
	{
		return TargetDelegateProp->GetToolTipText();
	}
	else
	{
		return FText::FromName(DelegatePropertyName);
	}
}

FString UK2Node_ComponentBoundEvent::GetDocumentationLink() const
{
	if (DelegateOwnerClass)
	{
		return FString::Printf(TEXT("Shared/GraphNodes/Blueprint/%s%s"), DelegateOwnerClass->GetPrefixCPP(), *EventReference.GetMemberName().ToString());
	}

	return FString();
}

FString UK2Node_ComponentBoundEvent::GetDocumentationExcerptName() const
{
	return DelegatePropertyName.ToString();
}

void UK2Node_ComponentBoundEvent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Fix up legacy nodes that may not yet have a delegate pin
	if(Ar.IsLoading())
	{
		bool bNeedsFixup = false;
		if(Ar.UE4Ver() < VER_UE4_K2NODE_EVENT_MEMBER_REFERENCE)
		{
			DelegateOwnerClass = EventSignatureClass_DEPRECATED;
			bNeedsFixup = true;
		}

		// Recover from the period where DelegateOwnerClass was transient
		if (!DelegateOwnerClass && HasValidBlueprint())
		{
			bNeedsFixup = true;
			// Search for a component property on the owning class, this should work in most cases
			UBlueprint* ParentBlueprint = GetBlueprint();
			UClass* ParentClass = ParentBlueprint ? ParentBlueprint->GeneratedClass : NULL;
			if (!ParentClass && ParentBlueprint)
			{
				// Try the skeleton class
				ParentClass = ParentBlueprint->SkeletonGeneratedClass;
			}

			UObjectProperty* ComponentProperty = ParentClass ? Cast<UObjectProperty>(ParentClass->FindPropertyByName(ComponentPropertyName)) : NULL;

			if (ParentClass && ComponentProperty)
			{
				UE_LOG(LogBlueprint, Warning, TEXT("Repaired invalid component bound event in node %s."), *GetPathName());
				DelegateOwnerClass = ComponentProperty->PropertyClass;
			}
		}

		if (bNeedsFixup)
		{
			// We need to fixup our event reference as it may have been saved incorrectly
			UMulticastDelegateProperty* TargetDelegateProp = GetTargetDelegateProperty();
			if (TargetDelegateProp && TargetDelegateProp->SignatureFunction)
			{
				FName ReferenceName = TargetDelegateProp->SignatureFunction->GetFName();
				UClass* ReferenceClass = TargetDelegateProp->SignatureFunction->GetOwnerClass();

				if (EventReference.GetMemberName() != ReferenceName || EventReference.GetMemberParentClass() != ReferenceClass)
				{
					// Set the reference if it wasn't already set properly, owner class may end up being NULL for native delegates
					EventReference.SetExternalMember(ReferenceName, ReferenceClass);
				}
			}
			else
			{
				UE_LOG(LogBlueprint, Warning, TEXT("Loaded invalid component bound event in node %s."), *GetPathName());
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE