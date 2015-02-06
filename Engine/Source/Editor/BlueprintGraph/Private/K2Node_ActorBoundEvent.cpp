// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"

#include "KismetCompiler.h"
#include "EventEntryHandler.h"

#define LOCTEXT_NAMESPACE "K2Node_ActorBoundEvent"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_ActorBoundEventEntry

class FKCHandler_ActorBoundEventEntry : public FKCHandler_EventEntry
{
public:
	FKCHandler_ActorBoundEventEntry(FKismetCompilerContext& InCompilerContext)
		: FKCHandler_EventEntry(InCompilerContext)
	{
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		// Check to make sure that the object the event is bound to is valid
		const UK2Node_ActorBoundEvent* BoundEventNode = Cast<UK2Node_ActorBoundEvent>(Node);
		if( BoundEventNode && BoundEventNode->EventOwner )
		{
			FKCHandler_EventEntry::Compile(Context, Node);
		}
		else
		{
			CompilerContext.MessageLog.Error(*FString(*LOCTEXT("FindNodeBoundEvent_Error", "Couldn't find object for bound event node @@").ToString()), Node);
		}
	}
};

UK2Node_ActorBoundEvent::UK2Node_ActorBoundEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FNodeHandlingFunctor* UK2Node_ActorBoundEvent::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_ActorBoundEventEntry(CompilerContext);
}

void UK2Node_ActorBoundEvent::ReconstructNode()
{
	// Ensure that we try to update the delegate we're bound to if its moved
	GetTargetDelegate();

	CachedNodeTitle.MarkDirty();

	Super::ReconstructNode();
}

void UK2Node_ActorBoundEvent::DestroyNode()
{
	if( EventOwner )
	{
		// If we have an event owner, remove the delegate referencing this event, if any
		const ULevel* TargetLevel = Cast<ULevel>(EventOwner->GetOuter());
		if( TargetLevel )
		{
			ALevelScriptActor* LSA = TargetLevel->GetLevelScriptActor();
			if( LSA )
			{
				// Create a delegate of the correct signature to remove
				FScriptDelegate Delegate;
				Delegate.BindUFunction(LSA, CustomFunctionName);
				
				// Attempt to remove it from the target's MC delegate
				if (FMulticastScriptDelegate* TargetDelegate = GetTargetDelegate())
				{
					TargetDelegate->Remove(Delegate);
				}				
			}
		}
		
	}

	Super::DestroyNode();
}

FText UK2Node_ActorBoundEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (EventOwner == nullptr)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("DelegatePropertyName"), FText::FromName(DelegatePropertyName));
		return FText::Format(LOCTEXT("ActorBoundEventTitle", "{DelegatePropertyName} (None)"), Args);
	}
	else if (CachedNodeTitle.IsOutOfDate())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("DelegatePropertyName"), FText::FromName(DelegatePropertyName));
		Args.Add(TEXT("TargetName"), FText::FromString(EventOwner->GetActorLabel()));

		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle = FText::Format(LOCTEXT("ActorBoundEventTitle", "{DelegatePropertyName} ({TargetName})"), Args);
	}
	return CachedNodeTitle;
}

FText UK2Node_ActorBoundEvent::GetTooltipText() const
{
	UMulticastDelegateProperty* TargetDelegateProp = GetTargetDelegatePropertyConst();
	if(TargetDelegateProp)
	{
		return TargetDelegateProp->GetToolTipText();
	}
	else
	{
		return FText::FromName(DelegatePropertyName);
	}
}

FString UK2Node_ActorBoundEvent::GetDocumentationLink() const
{
	if (EventSignatureClass)
	{
		return FString::Printf(TEXT("Shared/GraphNodes/Blueprint/%s%s"), EventSignatureClass->GetPrefixCPP(), *EventSignatureClass->GetName());
	}

	return FString();
}

FString UK2Node_ActorBoundEvent::GetDocumentationExcerptName() const
{
	return DelegatePropertyName.ToString();
}


AActor* UK2Node_ActorBoundEvent::GetReferencedLevelActor() const
{
	return EventOwner;
}

void UK2Node_ActorBoundEvent::InitializeActorBoundEventParams(AActor* InEventOwner, const UMulticastDelegateProperty* InDelegateProperty)
{
	if( InEventOwner && InDelegateProperty )
	{
		EventOwner = InEventOwner;
		DelegatePropertyName = InDelegateProperty->GetFName();
		EventSignatureName = InDelegateProperty->SignatureFunction->GetFName();
		EventSignatureClass = CastChecked<UClass>(InDelegateProperty->SignatureFunction->GetOuter())->GetAuthoritativeClass();
		CustomFunctionName = FName( *FString::Printf(TEXT("BndEvt__%s_%s_%s"), *InEventOwner->GetName(), *GetName(), *EventSignatureName.ToString()) );
		bOverrideFunction = false;
		bInternalEvent = true;
		CachedNodeTitle.MarkDirty();
	}
}

UMulticastDelegateProperty* UK2Node_ActorBoundEvent::GetTargetDelegatePropertyConst() const
{
	return Cast<UMulticastDelegateProperty>(FindField<UMulticastDelegateProperty>(EventSignatureClass, DelegatePropertyName));
}

UMulticastDelegateProperty* UK2Node_ActorBoundEvent::GetTargetDelegateProperty()
{
	UMulticastDelegateProperty* TargetDelegateProp = Cast<UMulticastDelegateProperty>(FindField<UMulticastDelegateProperty>(EventSignatureClass, DelegatePropertyName));

	// If we couldn't find the target delegate, then try to find it in the property remap table
	if (!TargetDelegateProp)
	{
		UMulticastDelegateProperty* NewProperty = Cast<UMulticastDelegateProperty>(FindRemappedField(EventSignatureClass, DelegatePropertyName));
		if (NewProperty)
		{
			// Found a remapped property, update the node
			TargetDelegateProp = NewProperty;
			DelegatePropertyName = NewProperty->GetFName();
			EventSignatureClass = Cast<UClass>(NewProperty->GetOuter());
			CachedNodeTitle.MarkDirty();
		}
	}

	return TargetDelegateProp;
}

FMulticastScriptDelegate* UK2Node_ActorBoundEvent::GetTargetDelegate()
{
	if( EventOwner )
	{
		UMulticastDelegateProperty* TargetDelegateProp = GetTargetDelegateProperty();
		if( TargetDelegateProp )
		{
			return TargetDelegateProp->GetPropertyValuePtr_InContainer(EventOwner);
		}
	}
	
	return NULL;
}

bool UK2Node_ActorBoundEvent::IsUsedByAuthorityOnlyDelegate() const
{
	const UMulticastDelegateProperty* TargetDelegateProp = Cast<const UMulticastDelegateProperty>(FindField<UMulticastDelegateProperty>( EventSignatureClass, DelegatePropertyName ));
	return (TargetDelegateProp && TargetDelegateProp->HasAnyPropertyFlags(CPF_BlueprintAuthorityOnly));
}

#undef LOCTEXT_NAMESPACE
