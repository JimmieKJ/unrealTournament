// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"

#include "UnrealEd.h"

#include "NodeFactory.h"

#include "SGraphNodeDefault.h"
#include "SGraphNodeComment.h"
#include "SGraphNodeDocumentation.h"
#include "EdGraph/EdGraphNode_Documentation.h"
#include "SGraphNodeKnot.h"

#include "KismetNodes/SGraphNodeK2Base.h"
#include "KismetNodes/SGraphNodeK2Default.h"
#include "KismetNodes/SGraphNodeK2Var.h"
#include "KismetNodes/SGraphNodeK2Terminator.h"
#include "KismetNodes/SGraphNodeK2Composite.h"
#include "KismetNodes/SGraphNodeSwitchStatement.h"
#include "KismetNodes/SGraphNodeK2Sequence.h"
#include "KismetNodes/SGraphNodeK2Timeline.h"
#include "KismetNodes/SGraphNodeSpawnActor.h"
#include "KismetNodes/SGraphNodeSpawnActorFromClass.h"
#include "KismetNodes/SGraphNodeK2CreateDelegate.h"
#include "KismetNodes/SGraphNodeCallParameterCollectionFunction.h"
#include "KismetNodes/SGraphNodeK2Event.h"
#include "KismetNodes/SGraphNodeFormatText.h"
#include "KismetNodes/SGraphNodeK2ArrayFunction.h"

#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimStateNode.h"
#include "AnimStateEntryNode.h"
#include "AnimStateConduitNode.h"
#include "AnimStateTransitionNode.h"

#include "AnimationStateMachineSchema.h"

#include "AnimationStateNodes/SGraphNodeAnimState.h"
#include "AnimationStateNodes/SGraphNodeAnimTransition.h"
#include "AnimationStateNodes/SGraphNodeAnimStateEntry.h"

#include "AnimationNodes/SGraphNodeSequencePlayer.h"
#include "AnimationNodes/SGraphNodeAnimationResult.h"
#include "AnimationNodes/SGraphNodeStateMachineInstance.h"
#include "AnimationNodes/SGraphNodeLayeredBoneBlend.h"
#include "AnimationPins/SGraphPinPose.h"

#include "KismetPins/SGraphPinBool.h"
#include "KismetPins/SGraphPinString.h"
#include "KismetPins/SGraphPinText.h"
#include "KismetPins/SGraphPinObject.h"
#include "KismetPins/SGraphPinClass.h"
#include "KismetPins/SGraphPinExec.h"
#include "KismetPins/SGraphPinNum.h"
#include "KismetPins/SGraphPinInteger.h"
#include "KismetPins/SGraphPinColor.h"
#include "KismetPins/SGraphPinEnum.h"
#include "KismetPins/SGraphPinKey.h"
#include "KismetPins/SGraphPinVector.h"
#include "KismetPins/SGraphPinVector2D.h"
#include "NiagaraPins/SGraphPinVector4.h"
#include "KismetPins/SGraphPinIndex.h"
#include "KismetPins/SGraphPinCollisionProfile.h"

#include "SoundNodes/SGraphNodeSoundBase.h"
#include "SoundNodes/SGraphNodeSoundResult.h"

#include "MaterialNodes/SGraphNodeMaterialBase.h"
#include "MaterialNodes/SGraphNodeMaterialComment.h"
#include "MaterialNodes/SGraphNodeMaterialResult.h"

#include "MaterialPins/SGraphPinMaterialInput.h"

#include "EdGraphUtilities.h"

TSharedPtr<SGraphNode> FNodeFactory::CreateNodeWidget(UEdGraphNode* InNode)
{
	check(InNode != NULL);

	// First give a shot to the node itself
	{
		TSharedPtr<SGraphNode> NodeCreatedResult = InNode->CreateVisualWidget();
		if (NodeCreatedResult.IsValid())
		{
			return NodeCreatedResult;
		}
	}

	// First give a shot to the registered node factories
	for (auto FactoryIt = FEdGraphUtilities::VisualNodeFactories.CreateIterator(); FactoryIt; ++FactoryIt)
	{
		TSharedPtr<FGraphPanelNodeFactory> FactoryPtr = *FactoryIt;
		if (FactoryPtr.IsValid())
		{
			TSharedPtr<SGraphNode> ResultVisualNode = FactoryPtr->CreateNode(InNode);
			if (ResultVisualNode.IsValid())
			{
				return ResultVisualNode;
			}
		}
	}

	//@TODO: Fold all of this code into registered factories for the various schemas!
	if (UAnimGraphNode_Base* BaseAnimNode = Cast<UAnimGraphNode_Base>(InNode))
	{
		if (UAnimGraphNode_Root* RootAnimNode = Cast<UAnimGraphNode_Root>(InNode))
		{
			return SNew(SGraphNodeAnimationResult, RootAnimNode);
		}
		else if (UAnimGraphNode_StateMachineBase* StateMachineInstance = Cast<UAnimGraphNode_StateMachineBase>(InNode))
		{
			return SNew(SGraphNodeStateMachineInstance, StateMachineInstance);
		}
		else if (UAnimGraphNode_SequencePlayer* SequencePlayer = Cast<UAnimGraphNode_SequencePlayer>(InNode))
		{
			return SNew(SGraphNodeSequencePlayer, SequencePlayer);
		}
		else if (UAnimGraphNode_LayeredBoneBlend* LayeredBlend = Cast<UAnimGraphNode_LayeredBoneBlend>(InNode))
		{
			return SNew(SGraphNodeLayeredBoneBlend, LayeredBlend);
		}
	}

	if (USoundCueGraphNode_Base* BaseSoundNode = Cast<USoundCueGraphNode_Base>(InNode))
	{
		if (USoundCueGraphNode_Root* RootSoundNode = Cast<USoundCueGraphNode_Root>(InNode))
		{
			return SNew(SGraphNodeSoundResult, RootSoundNode);
		}
		else if (USoundCueGraphNode* SoundNode = Cast<USoundCueGraphNode>(InNode))
		{
			return SNew(SGraphNodeSoundBase, SoundNode);
		}
	}

	if (UMaterialGraphNode_Base* BaseMaterialNode = Cast<UMaterialGraphNode_Base>(InNode))
	{
		if (UMaterialGraphNode_Root* RootMaterialNode = Cast<UMaterialGraphNode_Root>(InNode))
		{
			return SNew(SGraphNodeMaterialResult, RootMaterialNode);
		}
		else if (UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(InNode))
		{
			return SNew(SGraphNodeMaterialBase, MaterialNode);
		}
	}

	if (UK2Node* K2Node = Cast<UK2Node>(InNode))
	{
		if (UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(InNode))
		{
			return SNew(SGraphNodeK2Composite, CompositeNode);
		}
		else if (K2Node->DrawNodeAsVariable())
		{
			return SNew(SGraphNodeK2Var, K2Node);
		}
		else if (UK2Node_Switch* SwitchNode = Cast<UK2Node_Switch>(InNode))
		{
			return SNew(SGraphNodeSwitchStatement, SwitchNode);
		}
		else if (UK2Node_ExecutionSequence* SequenceNode = Cast<UK2Node_ExecutionSequence>(InNode))
		{
			return SNew(SGraphNodeK2Sequence, SequenceNode);
		}
		else if (UK2Node_MakeArray* MakeArrayNode = Cast<UK2Node_MakeArray>(InNode))
		{
			return SNew(SGraphNodeK2Sequence, MakeArrayNode);
		}
		else if (UK2Node_CommutativeAssociativeBinaryOperator* OperatorNode = Cast<UK2Node_CommutativeAssociativeBinaryOperator>(InNode))
		{
			return SNew(SGraphNodeK2Sequence, OperatorNode);
		}
		else if (UK2Node_DoOnceMultiInput* DoOnceMultiInputNode = Cast<UK2Node_DoOnceMultiInput>(InNode))
		{
			return SNew(SGraphNodeK2Sequence, DoOnceMultiInputNode);
		}
		else if (UK2Node_Timeline* TimelineNode = Cast<UK2Node_Timeline>(InNode))
		{
			return SNew(SGraphNodeK2Timeline, TimelineNode);
		}
		else if(UK2Node_SpawnActor* SpawnActorNode = Cast<UK2Node_SpawnActor>(InNode))
		{
			return SNew(SGraphNodeSpawnActor, SpawnActorNode);
		}
		else if(UK2Node_SpawnActorFromClass* SpawnActorNodeFromClass = Cast<UK2Node_SpawnActorFromClass>(InNode))
		{
			return SNew(SGraphNodeSpawnActorFromClass, SpawnActorNodeFromClass);
		}
		else if(UK2Node_CreateDelegate* CreateDelegateNode = Cast<UK2Node_CreateDelegate>(InNode))
		{
			return SNew(SGraphNodeK2CreateDelegate, CreateDelegateNode);
		}
		else if (UK2Node_CallMaterialParameterCollectionFunction* CallFunctionNode = Cast<UK2Node_CallMaterialParameterCollectionFunction>(InNode))
		{
			return SNew(SGraphNodeCallParameterCollectionFunction, CallFunctionNode);
		}
		else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(InNode))
		{
			return SNew(SGraphNodeK2Event, EventNode);
		}
		else if (UK2Node_FormatText* FormatTextNode = Cast<UK2Node_FormatText>(InNode))
		{
			return SNew(SGraphNodeFormatText, FormatTextNode);
		}
		else if (UK2Node_CallArrayFunction* CallFunction = Cast<UK2Node_CallArrayFunction>(InNode))
		{
			return SNew(SGraphNodeK2ArrayFunction, CallFunction);
		}
		else if (UK2Node_Knot* Knot = Cast<UK2Node_Knot>(InNode))
		{
			return SNew(SGraphNodeKnot, Knot);
		}
		else
		{
			return SNew(SGraphNodeK2Default, K2Node);
		}
	}
	else if (UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(InNode))
	{
		return SNew(SGraphNodeAnimTransition, TransitionNode);
	}
	else if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(InNode))
	{
		return SNew(SGraphNodeAnimState, StateNode);
	}
	else if (UAnimStateConduitNode* ConduitNode = Cast<UAnimStateConduitNode>(InNode))
	{
		return SNew(SGraphNodeAnimConduit, ConduitNode);
	}
	else if (UAnimStateEntryNode* EntryNode = Cast<UAnimStateEntryNode>(InNode))
	{
		return SNew(SGraphNodeAnimStateEntry, EntryNode);
	}
	else if(UEdGraphNode_Documentation* DocNode = Cast<UEdGraphNode_Documentation>(InNode))
	{
		return SNew(SGraphNodeDocumentation, DocNode);
	}
	else if (InNode->ShouldDrawNodeAsComment())
	{
		if (UMaterialGraphNode_Comment* MaterialCommentNode = Cast<UMaterialGraphNode_Comment>(InNode))
		{
			return SNew(SGraphNodeMaterialComment, MaterialCommentNode);
		}
		else
		{
			return SNew(SGraphNodeComment, InNode);
		}
	}
	else
	{
		return SNew(SGraphNodeDefault)
			.GraphNodeObj(InNode);
	}
}

TSharedPtr<SGraphPin> FNodeFactory::CreatePinWidget(UEdGraphPin* InPin)
{
	check(InPin != NULL);

	// First give a shot to the registered pin factories
	for (auto FactoryIt = FEdGraphUtilities::VisualPinFactories.CreateIterator(); FactoryIt; ++FactoryIt)
	{
		TSharedPtr<FGraphPanelPinFactory> FactoryPtr = *FactoryIt;
		if (FactoryPtr.IsValid())
		{
			TSharedPtr<SGraphPin> ResultVisualPin = FactoryPtr->CreatePin(InPin);
			if (ResultVisualPin.IsValid())
			{
				return ResultVisualPin;
			}
		}
	}

	if (const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(InPin->GetSchema()))
	{
		if (InPin->PinType.PinCategory == K2Schema->PC_Boolean)
		{
			return SNew(SGraphPinBool, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Text)
		{
			return SNew(SGraphPinText, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Exec)
		{
			return SNew(SGraphPinExec, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Object)
		{
			return SNew(SGraphPinObject, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Interface)
		{
			return SNew(SGraphPinObject, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Asset)
		{
			return SNew(SGraphPinObject, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Class)
		{
			return SNew(SGraphPinClass, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_AssetClass)
		{
			return SNew(SGraphPinClass, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Int)
		{
			return SNew(SGraphPinInteger, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Float)
		{
			return SNew(SGraphPinNum, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_String || InPin->PinType.PinCategory == K2Schema->PC_Name)
		{
			return SNew(SGraphPinString, InPin);
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Struct)
		{
			// If you update this logic you'll probably need to update UEdGraphSchema_K2::ShouldHidePinDefaultValue!
			UScriptStruct* ColorStruct = TBaseStructure<FLinearColor>::Get();
			UScriptStruct* VectorStruct = TBaseStructure<FVector>::Get();
			UScriptStruct* Vector2DStruct = TBaseStructure<FVector2D>::Get();
			UScriptStruct* RotatorStruct = TBaseStructure<FRotator>::Get();

			if (InPin->PinType.PinSubCategoryObject == ColorStruct)
			{
				return SNew(SGraphPinColor, InPin);
			}
			else if ((InPin->PinType.PinSubCategoryObject == VectorStruct) || (InPin->PinType.PinSubCategoryObject == RotatorStruct))
			{
				return SNew(SGraphPinVector, InPin);
			}
			else if (InPin->PinType.PinSubCategoryObject == Vector2DStruct)
			{
				return SNew(SGraphPinVector2D, InPin);
			}
			else if (InPin->PinType.PinSubCategoryObject == FKey::StaticStruct())
			{
				return SNew(SGraphPinKey, InPin);
			}
			else if ((InPin->PinType.PinSubCategoryObject == FPoseLink::StaticStruct()) || (InPin->PinType.PinSubCategoryObject == FComponentSpacePoseLink::StaticStruct()))
			{
				return SNew(SGraphPinPose, InPin);
			}
			else if (InPin->PinType.PinSubCategoryObject == FCollisionProfileName::StaticStruct())
			{
				return SNew(SGraphPinCollisionProfile, InPin);
			}
		}
		else if (InPin->PinType.PinCategory == K2Schema->PC_Byte)
		{
			// Check for valid enum object reference
			if ((InPin->PinType.PinSubCategoryObject != NULL) && (InPin->PinType.PinSubCategoryObject->IsA(UEnum::StaticClass())))
			{
				return SNew(SGraphPinEnum, InPin);
			}
			else
			{
				return SNew(SGraphPinInteger, InPin);
			}
		}
		else if ((InPin->PinType.PinCategory == K2Schema->PC_Wildcard) && (InPin->PinType.PinSubCategory == K2Schema->PSC_Index))
		{
			return SNew(SGraphPinIndex, InPin);
		}
		else if(InPin->PinType.PinCategory == K2Schema->PC_MCDelegate)
		{
			return SNew(SGraphPinString, InPin);
		}
	}

	const UAnimationStateMachineSchema* AnimStateMachineSchema = Cast<const UAnimationStateMachineSchema>(InPin->GetSchema());
	if (AnimStateMachineSchema != NULL)
	{
		if (InPin->PinType.PinCategory == AnimStateMachineSchema->PC_Exec)
		{
			return SNew(SGraphPinExec, InPin);
		}
	}

	if (const UMaterialGraphSchema* MaterialGraphSchema = Cast<const UMaterialGraphSchema>(InPin->GetSchema()))
	{
		if (InPin->PinType.PinCategory == MaterialGraphSchema->PC_MaterialInput)
		{
			return SNew(SGraphPinMaterialInput, InPin);
		}
		else
		{
			return SNew(SGraphPin, InPin);
		}
	}

	if (const UEdGraphSchema_Niagara* NSchema = Cast<const UEdGraphSchema_Niagara>(InPin->GetSchema()))
	{
		if (InPin->PinType.PinCategory == NSchema->PC_Float)
		{
			return SNew(SGraphPinNum, InPin);
		}
		if (InPin->PinType.PinCategory == NSchema->PC_Vector)
		{
			return SNew(SGraphPinVector4, InPin);
		}
		if (InPin->PinType.PinCategory == NSchema->PC_Matrix)
		{
			return SNew(SGraphPin, InPin);
		}
		if (InPin->PinType.PinCategory == NSchema->PC_Curve)
		{
			return SNew(SGraphPin, InPin);
		}

	}


	// If we didn't pick a custom pin widget, use an uncustomized basic pin
	return SNew(SGraphPin, InPin);
}
