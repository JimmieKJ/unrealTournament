// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyAllTypes.h"

UBTDecorator_Blackboard::UBTDecorator_Blackboard(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Blackboard Based Condition";
	NotifyObserver = EBTBlackboardRestart::ResultChange;
}

bool UBTDecorator_Blackboard::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	// note that this may produce unexpected logical results. FALSE is a valid return value here as well
	// @todo signal it
	return BlackboardComp && EvaluateOnBlackboard(*BlackboardComp);
}

bool UBTDecorator_Blackboard::EvaluateOnBlackboard(const UBlackboardComponent& BlackboardComp) const
{
	bool bResult = false;
	if (BlackboardKey.SelectedKeyType)
	{
		UBlackboardKeyType* KeyCDO = BlackboardKey.SelectedKeyType->GetDefaultObject<UBlackboardKeyType>();
		const uint8* KeyMemory = BlackboardComp.GetKeyRawData(BlackboardKey.GetSelectedKeyID());

		// KeyMemory can be NULL if the blackboard has its data setup wrong, so we must conditionally handle that case.
		if (ensure(KeyCDO != NULL) && (KeyMemory != NULL))
		{
			const EBlackboardKeyOperation::Type Op = KeyCDO->GetTestOperation();
			switch (Op)
			{
			case EBlackboardKeyOperation::Basic:
				bResult = KeyCDO->WrappedTestBasicOperation(BlackboardComp, KeyMemory, (EBasicKeyOperation::Type)OperationType);
				break;

			case EBlackboardKeyOperation::Arithmetic:
				bResult = KeyCDO->WrappedTestArithmeticOperation(BlackboardComp, KeyMemory, (EArithmeticKeyOperation::Type)OperationType, IntValue, FloatValue);
				break;

			case EBlackboardKeyOperation::Text:
				bResult = KeyCDO->WrappedTestTextOperation(BlackboardComp, KeyMemory, (ETextKeyOperation::Type)OperationType, StringValue);
				break;

			default:
				break;
			}
		}
	}

	return bResult;
}

EBlackboardNotificationResult UBTDecorator_Blackboard::OnBlackboardKeyValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID)
{
	UBehaviorTreeComponent* BehaviorComp = (UBehaviorTreeComponent*)Blackboard.GetBrainComponent();
	if (BehaviorComp == nullptr)
	{
		return EBlackboardNotificationResult::RemoveObserver;
	}

	if (BlackboardKey.GetSelectedKeyID() == ChangedKeyID && GetFlowAbortMode() != EBTFlowAbortMode::None)
	{
		const bool bIsExecutingBranch = BehaviorComp->IsExecutingBranch(this, GetChildIndex());
		const bool bPass = (EvaluateOnBlackboard(Blackboard) != IsInversed());
		const bool bAbortPending = BehaviorComp->IsAbortPending();

		UE_VLOG(BehaviorComp, LogBehaviorTree, Verbose, TEXT("%s, OnBlackboardChange[%s] pass:%d executingBranch:%d abortPending:%d => %s"),
			*UBehaviorTreeTypes::DescribeNodeHelper(this),
			*Blackboard.GetKeyName(ChangedKeyID).ToString(),
			bPass ? 1 : 0,
			bIsExecutingBranch ? 1 : 0,
			bAbortPending ? 1 : 0,
			(bAbortPending || (bIsExecutingBranch && !bPass) || (!bIsExecutingBranch && bPass)) ? TEXT("restart") : TEXT("skip"));

		if ((bIsExecutingBranch && !bPass) || (!bIsExecutingBranch && bPass))
		{
			BehaviorComp->RequestExecution(this);
		}
		else if (!bIsExecutingBranch && !bPass && GetParentNode() && GetParentNode()->Children.IsValidIndex(GetChildIndex()))
		{
			// this condition here is to remove all active observers _BELOW_ this node
			// because if this condition failed we no longer want to react to child-conditions
			// value changes anyway since their nodes execution will be blocked by this condition 
			// during tree search
			const UBTCompositeNode* BranchRoot = GetParentNode()->Children[GetChildIndex()].ChildComposite;
			BehaviorComp->UnregisterAuxNodesInBranch(BranchRoot);
		}
		else if (bIsExecutingBranch && bPass && (NotifyObserver == EBTBlackboardRestart::ValueChange || bAbortPending))
		{
			UE_VLOG(BehaviorComp, LogBehaviorTree, Verbose, TEXT("%s, OnBlackboardChange[%s] => restart"),
				*UBehaviorTreeTypes::DescribeNodeHelper(this),
				*Blackboard.GetKeyName(ChangedKeyID).ToString());

			// force result Aborted to restart from this decorator
			// can't use helper function

			const int32 InstanceIdx = BehaviorComp->FindInstanceContainingNode((UBTNode*)GetParentNode());
			if (InstanceIdx != INDEX_NONE)
			{
				BehaviorComp->RequestExecution(GetParentNode(), InstanceIdx, this, GetChildIndex(), EBTNodeResult::Aborted);
			}
		}
	}

	return EBlackboardNotificationResult::ContinueObserving;
}

void UBTDecorator_Blackboard::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);

	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	FString DescKeyValue;

	if (BlackboardComp)
	{
		DescKeyValue = BlackboardComp->DescribeKeyValue(BlackboardKey.GetSelectedKeyID(), EBlackboardDescription::OnlyValue);
	}

	const bool bResult = BlackboardComp && EvaluateOnBlackboard(*BlackboardComp);
	Values.Add(FString::Printf(TEXT("value: %s (%s)"), *DescKeyValue, bResult ? TEXT("pass") : TEXT("fail")));
}

FString UBTDecorator_Blackboard::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: %s"), *Super::GetStaticDescription(), *CachedDescription);
}

#if WITH_EDITOR
static UEnum* BasicOpEnum = NULL;
static UEnum* ArithmeticOpEnum = NULL;
static UEnum* TextOpEnum = NULL;

static void CacheOperationEnums()
{
	if (BasicOpEnum == NULL)
	{
		BasicOpEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EBasicKeyOperation"));
		check(BasicOpEnum);
	}

	if (ArithmeticOpEnum == NULL)
	{
		ArithmeticOpEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EArithmeticKeyOperation"));
		check(ArithmeticOpEnum);
	}

	if (TextOpEnum == NULL)
	{
		TextOpEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ETextKeyOperation"));
		check(TextOpEnum);
	}
}

void UBTDecorator_Blackboard::BuildDescription()
{
	UBlackboardData* BlackboardAsset = GetBlackboardAsset();
	const FBlackboardEntry* EntryInfo = BlackboardAsset ? BlackboardAsset->GetKey(BlackboardKey.GetSelectedKeyID()) : NULL;

	FString BlackboardDesc = "invalid";
	if (EntryInfo)
	{
		// safety feature to not crash when changing couple of properties on a bunch
		// while "post edit property" triggers for every each of them
		if (EntryInfo->KeyType->GetClass() == BlackboardKey.SelectedKeyType)
		{
			const FString KeyName = EntryInfo->EntryName.ToString();
			CacheOperationEnums();		

			const EBlackboardKeyOperation::Type Op = EntryInfo->KeyType->GetTestOperation();
			switch (Op)
			{
			case EBlackboardKeyOperation::Basic:
				BlackboardDesc = FString::Printf(TEXT("%s is %s"), *KeyName, *BasicOpEnum->GetEnumName(OperationType));
				break;

			case EBlackboardKeyOperation::Arithmetic:
				BlackboardDesc = FString::Printf(TEXT("%s %s %s"), *KeyName, *ArithmeticOpEnum->GetDisplayNameText(OperationType).ToString(),
					*EntryInfo->KeyType->DescribeArithmeticParam(IntValue, FloatValue));
				break;

			case EBlackboardKeyOperation::Text:
				BlackboardDesc = FString::Printf(TEXT("%s %s [%s]"), *KeyName, *TextOpEnum->GetEnumName(OperationType), *StringValue);
				break;

			default: break;
			}
		}
	}

	CachedDescription = BlackboardDesc;
}

void UBTDecorator_Blackboard::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property == NULL)
	{
		return;
	}

	const FName ChangedPropName = PropertyChangedEvent.Property->GetFName();
	if (ChangedPropName == GET_MEMBER_NAME_CHECKED(UBTDecorator_Blackboard, BlackboardKey.SelectedKeyName))
	{
		if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Enum::StaticClass() ||
			BlackboardKey.SelectedKeyType == UBlackboardKeyType_NativeEnum::StaticClass())
		{
			IntValue = 0;
		}
	}

#if WITH_EDITORONLY_DATA

	UBlackboardKeyType* KeyCDO = BlackboardKey.SelectedKeyType ? BlackboardKey.SelectedKeyType->GetDefaultObject<UBlackboardKeyType>() : NULL;
	if (ChangedPropName == GET_MEMBER_NAME_CHECKED(UBTDecorator_Blackboard, BasicOperation))
	{
		if (KeyCDO && KeyCDO->GetTestOperation() == EBlackboardKeyOperation::Basic)
		{
			OperationType = BasicOperation;
		}
	}
	else if (ChangedPropName == GET_MEMBER_NAME_CHECKED(UBTDecorator_Blackboard, ArithmeticOperation))
	{
		if (KeyCDO && KeyCDO->GetTestOperation() == EBlackboardKeyOperation::Arithmetic)
		{
			OperationType = ArithmeticOperation;
		}
	}
	else if (ChangedPropName == GET_MEMBER_NAME_CHECKED(UBTDecorator_Blackboard, TextOperation))
	{
		if (KeyCDO && KeyCDO->GetTestOperation() == EBlackboardKeyOperation::Text)
		{
			OperationType = TextOperation;
		}
	}

#endif // WITH_EDITORONLY_DATA

	BuildDescription();
}

void UBTDecorator_Blackboard::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	BuildDescription();
}

#endif	// WITH_EDITOR
