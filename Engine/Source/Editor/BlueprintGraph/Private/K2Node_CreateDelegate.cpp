// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "BlueprintGraphPrivatePCH.h"

#include "DelegateNodeHandlers.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

struct FK2Node_CreateDelegate_Helper
{
	static FString ObjectInputName;
	static FString DelegateOutputName;
};
FString FK2Node_CreateDelegate_Helper::ObjectInputName(TEXT("InputObject"));
FString FK2Node_CreateDelegate_Helper::DelegateOutputName(TEXT("OutputDelegate"));

UK2Node_CreateDelegate::UK2Node_CreateDelegate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_CreateDelegate::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	if(UEdGraphPin* ObjPin = CreatePin(EGPD_Input, K2Schema->PC_Object, TEXT(""), UObject::StaticClass(), false, false, FK2Node_CreateDelegate_Helper::ObjectInputName))
	{
		ObjPin->PinFriendlyName = NSLOCTEXT("K2Node", "CreateDelegate_ObjectInputName", "Object");
	}

	if(UEdGraphPin* DelegatePin = CreatePin(EGPD_Output, K2Schema->PC_Delegate, TEXT(""), NULL, false, false, FK2Node_CreateDelegate_Helper::DelegateOutputName))
	{
		DelegatePin->PinFriendlyName = NSLOCTEXT("K2Node", "CreateDelegate_DelegateOutName", "Event");
	}

	Super::AllocateDefaultPins();
}

bool UK2Node_CreateDelegate::IsValid(FString* OutMsg, bool bDontUseSkeletalClassForSelf) const
{

	if (GetFunctionName() == NAME_None)
	{
		if (OutMsg)
		{
			*OutMsg = NSLOCTEXT("K2Node", "No_function_name", "No function name.").ToString();
		}
		return false;
	}

	const UEdGraphPin* DelegatePin = GetDelegateOutPin();
	if(!DelegatePin)
	{
		if (OutMsg)
		{
			*OutMsg = NSLOCTEXT("K2Node", "No_delegate_out_pin", "No delegate out pin.").ToString();
		}
		return false;
	}

	const UFunction* Signature = GetDelegateSignature();
	if(!Signature)
	{
		if (OutMsg)
		{
			*OutMsg = NSLOCTEXT("K2Node", "Signature_not_found", "Signature not found.").ToString();
		}
		return false;
	}

	for(int PinIter = 1; PinIter < DelegatePin->LinkedTo.Num(); PinIter++)
	{
		const UEdGraphPin* OtherPin = DelegatePin->LinkedTo[PinIter];
		const UFunction* OtherSignature = OtherPin ? 
			FMemberReference::ResolveSimpleMemberReference<UFunction>(OtherPin->PinType.PinSubCategoryMemberReference) : NULL;
		if(!OtherSignature || !Signature->IsSignatureCompatibleWith(OtherSignature))
		{
			if (OutMsg)
			{
				*OutMsg = NSLOCTEXT("K2Node", "No_delegate_out_pin", "No delegate out pin.").ToString();
			}
			return false;
		}
	}

	UClass* ScopeClass = GetScopeClass(bDontUseSkeletalClassForSelf);
	if(!ScopeClass)
	{
		if (OutMsg)
		{
			*OutMsg = NSLOCTEXT("K2Node", "Class_not_found", "Class not found.").ToString();
		}
		return false;
	}

	FMemberReference MemberReference;
	MemberReference.SetDirect(SelectedFunctionName, SelectedFunctionGuid, ScopeClass, false);
	const UFunction* FoundFunction = MemberReference.ResolveMember<UFunction>((UClass*) NULL);
	if (!FoundFunction)
	{
		if (OutMsg)
		{
			*OutMsg = NSLOCTEXT("K2Node", "Function_not_found", "Function not found.").ToString();
		}
		return false;
	}
	if (!Signature->IsSignatureCompatibleWith(FoundFunction))
	{
		if (OutMsg)
		{
			*OutMsg = NSLOCTEXT("K2Node", "Function_not_compatible", "Function not compatible.").ToString();
		}
		return false;
	}
	if (!UEdGraphSchema_K2::FunctionCanBeUsedInDelegate(FoundFunction))
	{
		if (OutMsg)
		{
			*OutMsg = NSLOCTEXT("K2Node", "Function_cannot_be_used_in_delegate", "Function cannot be used in delegate.").ToString();
		}
		return false;
	}

	if(!FoundFunction->HasAllFunctionFlags(FUNC_BlueprintAuthorityOnly))
	{
		for(int PinIter = 0; PinIter < DelegatePin->LinkedTo.Num(); PinIter++)
		{
			const UEdGraphPin* OtherPin = DelegatePin->LinkedTo[PinIter];
			const UK2Node_BaseMCDelegate* Node = OtherPin ? Cast<const UK2Node_BaseMCDelegate>(OtherPin->GetOwningNode()) : NULL;
			if (Node && Node->IsAuthorityOnly())
			{
				if(OutMsg)
				{
					*OutMsg = NSLOCTEXT("K2Node", "WrongDelegateAuthorityOnly", "No AuthorityOnly flag").ToString();
				}
				return false;
			}
		}
	}

	return true;
}

void UK2Node_CreateDelegate::ValidationAfterFunctionsAreCreated(class FCompilerResultsLog& MessageLog, bool bFullCompile) const
{
	FString Msg;
	if(!IsValid(&Msg, bFullCompile))
	{
		MessageLog.Error(*FString::Printf( TEXT("%s %s @@"), *NSLOCTEXT("K2Node", "WrongDelegate", "Event signature error: ").ToString(), *Msg), this);
	}
}

void UK2Node_CreateDelegate::HandleAnyChangeWithoutNotifying()
{
	const auto Blueprint = GetBlueprint();
	const auto SelfScopeClass = Blueprint ? Blueprint->SkeletonGeneratedClass : NULL;
	const auto ParentClass = GetScopeClass();
	const bool bIsSelfScope = SelfScopeClass && ParentClass && ((SelfScopeClass->IsChildOf(ParentClass)) || (SelfScopeClass->ClassGeneratedBy == ParentClass->ClassGeneratedBy));

	FMemberReference FunctionReference;
	FunctionReference.SetDirect(SelectedFunctionName, SelectedFunctionGuid, GetScopeClass(), bIsSelfScope);

	if (FunctionReference.ResolveMember<UFunction>(SelfScopeClass))
	{
		SelectedFunctionName = FunctionReference.GetMemberName();
		SelectedFunctionGuid = FunctionReference.GetMemberGuid();

		if (!SelectedFunctionGuid.IsValid())
		{
			UBlueprint::GetGuidFromClassByFieldName<UFunction>(ParentClass, SelectedFunctionName, SelectedFunctionGuid);
		}
	}

	if(!IsValid())
	{
		SelectedFunctionName = NAME_None;
		SelectedFunctionGuid.Invalidate();
	}
}

void UK2Node_CreateDelegate::HandleAnyChange(UEdGraph* & OutGraph, UBlueprint* & OutBlueprint)
{
	const FName OldSelectedFunctionName = GetFunctionName();
	HandleAnyChangeWithoutNotifying();
	if (OldSelectedFunctionName != GetFunctionName())
	{
		OutGraph = GetGraph();
		OutBlueprint = GetBlueprint();
	}
	else
	{
		OutGraph = NULL;
		OutBlueprint = NULL;
	}
}

void UK2Node_CreateDelegate::HandleAnyChange(bool bForceModify)
{
	const FName OldSelectedFunctionName = GetFunctionName();
	HandleAnyChangeWithoutNotifying();
	if (bForceModify || (OldSelectedFunctionName != GetFunctionName()))
	{
		if(UEdGraph* Graph = GetGraph())
		{
			Graph->NotifyGraphChanged();
		}

		UBlueprint* Blueprint = GetBlueprint();
		if(Blueprint && !Blueprint->bBeingCompiled)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			Blueprint->BroadcastChanged();
		}
	}
	else if (GetFunctionName() == NAME_None)
	{
		if(UEdGraph* Graph = GetGraph())
		{
			Graph->NotifyGraphChanged();
		}
	}
}

void UK2Node_CreateDelegate::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);
	UBlueprint* Blueprint = GetBlueprint();
	if(Blueprint && !Blueprint->bBeingCompiled)
	{
		HandleAnyChange();
	}
	else
	{
		HandleAnyChangeWithoutNotifying();
	}
}

void UK2Node_CreateDelegate::PinTypeChanged(UEdGraphPin* Pin)
{
	Super::PinTypeChanged(Pin);

	HandleAnyChangeWithoutNotifying();
}

void UK2Node_CreateDelegate::NodeConnectionListChanged()
{
	Super::NodeConnectionListChanged();
	UBlueprint* Blueprint = GetBlueprint();
	if(Blueprint && !Blueprint->bBeingCompiled)
	{
		HandleAnyChange();
	}
	else
	{
		HandleAnyChangeWithoutNotifying();
	}
}

void UK2Node_CreateDelegate::PostReconstructNode()
{
	Super::PostReconstructNode();
	HandleAnyChange();
}

UFunction* UK2Node_CreateDelegate::GetDelegateSignature() const
{
	UEdGraphPin* Pin = GetDelegateOutPin();
	check(Pin != NULL);
	if(Pin->LinkedTo.Num())
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if(UEdGraphPin* ResultPin = Pin->LinkedTo[0])
		{
			ensure(K2Schema->PC_Delegate == ResultPin->PinType.PinCategory);
			return FMemberReference::ResolveSimpleMemberReference<UFunction>(ResultPin->PinType.PinSubCategoryMemberReference);
		}
	}
	return NULL;
}

UClass* UK2Node_CreateDelegate::GetScopeClass(bool bDontUseSkeletalClassForSelf/* = false*/) const
{
	UEdGraphPin* Pin = FindPin(FK2Node_CreateDelegate_Helper::ObjectInputName);
	if (Pin == nullptr)
	{
		// The BlueprintNodeTemplateCache creates nodes but doesn't call allocate default pins.
		// SMyBlueprint::OnDeleteGraph calls this function on every UK2Node_CreateDelegate. Each of 
		// these systems is violating some first principles, so I've settled on this null check.
		return nullptr;
	}
	check(Pin->LinkedTo.Num() <= 1);
	if(Pin->LinkedTo.Num())
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if(UEdGraphPin* ResultPin = Pin->LinkedTo[0])
		{
			ensure(K2Schema->PC_Object == ResultPin->PinType.PinCategory);
			if (K2Schema->PN_Self == ResultPin->PinType.PinSubCategory)
			{
				if (UBlueprint* ScopeClassBlueprint = GetBlueprint())
				{
					return bDontUseSkeletalClassForSelf ? ScopeClassBlueprint->GeneratedClass : ScopeClassBlueprint->SkeletonGeneratedClass;
				}
			}
			if(UClass* TrueScopeClass = Cast<UClass>(ResultPin->PinType.PinSubCategoryObject.Get()))
			{
				if(UBlueprint* ScopeClassBlueprint = Cast<UBlueprint>(TrueScopeClass->ClassGeneratedBy))
				{
					if(ScopeClassBlueprint->SkeletonGeneratedClass)
					{
						return ScopeClassBlueprint->SkeletonGeneratedClass;
					}
				}
				return TrueScopeClass;
			}
		}
	}
	return NULL;
}

FName UK2Node_CreateDelegate::GetFunctionName() const
{
	return SelectedFunctionName;
}

UEdGraphPin* UK2Node_CreateDelegate::GetDelegateOutPin() const
{
	return FindPin(FK2Node_CreateDelegate_Helper::DelegateOutputName);
}

UEdGraphPin* UK2Node_CreateDelegate::GetObjectInPin() const
{
	return FindPinChecked(FK2Node_CreateDelegate_Helper::ObjectInputName);
}

FText UK2Node_CreateDelegate::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "CreateDelegate", "Create Event");
}

UObject* UK2Node_CreateDelegate::GetJumpTargetForDoubleClick() const
{
	UBlueprint* ScopeClassBlueprint = NULL;

	UEdGraphPin* Pin = FindPinChecked(FK2Node_CreateDelegate_Helper::ObjectInputName);
	UEdGraphPin* ResultPin = Pin->LinkedTo.Num() ? Pin->LinkedTo[0] : NULL;
	if (ResultPin)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		ensure(K2Schema->PC_Object == ResultPin->PinType.PinCategory);
		if (UClass* TrueScopeClass = Cast<UClass>(ResultPin->PinType.PinSubCategoryObject.Get()))
		{
			ScopeClassBlueprint = Cast<UBlueprint>(TrueScopeClass->ClassGeneratedBy);
		}
		else if (K2Schema->PN_Self == ResultPin->PinType.PinSubCategory)
		{
			ScopeClassBlueprint = GetBlueprint();
		}
	}

	if (ScopeClassBlueprint)
	{
		if (UEdGraph* FoundGraph = FindObject<UEdGraph>(ScopeClassBlueprint, *GetFunctionName().ToString()))
		{
			if(!FBlueprintEditorUtils::IsGraphIntermediate(FoundGraph))
			{
				return FoundGraph;
			}
		}
		for (auto UbergraphIt = ScopeClassBlueprint->UbergraphPages.CreateIterator(); UbergraphIt; ++UbergraphIt)
		{
			UEdGraph* Graph = (*UbergraphIt);
			if(!FBlueprintEditorUtils::IsGraphIntermediate(Graph))
			{
				TArray<UK2Node_Event*> EventNodes;
				Graph->GetNodesOfClass(EventNodes);
				for (auto EventIt = EventNodes.CreateIterator(); EventIt; ++EventIt)
				{
					if(GetFunctionName() == (*EventIt)->GetFunctionName())
					{
						return *EventIt;
					}
				}
			}
		}
	}
	return NULL;
}

FNodeHandlingFunctor* UK2Node_CreateDelegate::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_CreateDelegate(CompilerContext);
}

void UK2Node_CreateDelegate::SetFunction(FName Name)
{
	SelectedFunctionName = Name;
	SelectedFunctionGuid.Invalidate();
}

FText UK2Node_CreateDelegate::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Delegates);
}

void UK2Node_CreateDelegate::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(NodeClass))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(NodeClass);
		check(NodeSpawner);
		ActionRegistrar.AddBlueprintAction(NodeClass, NodeSpawner);
	}
}