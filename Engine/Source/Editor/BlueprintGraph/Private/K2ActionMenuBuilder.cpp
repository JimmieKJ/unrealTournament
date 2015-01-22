// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "Engine/LevelScriptBlueprint.h"
#include "K2ActionMenuBuilder.h"
#include "ComponentAssetBroker.h"
#include "KismetEditorUtilities.h"
#include "ObjectEditorUtils.h"
#include "Matinee/MatineeActor.h"

#include "K2Node_CastByteToEnum.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_EnumLiteral.h"
#include "K2Node_ForEachElementInEnum.h"
#include "K2Node_GetEnumeratorName.h"
#include "K2Node_GetEnumeratorNameAsString.h"
#include "K2Node_GetInputAxisValue.h"
#include "K2Node_GetInputVectorAxisValue.h"
#include "K2Node_GetNumEnumEntries.h"
#include "K2Node_InputAxisKeyEvent.h"
#include "K2Node_InputVectorAxisEvent.h"
#include "K2Node_Message.h"
#include "K2Node_MultiGate.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_SwitchString.h"
#include "K2Node_VariableSetRef.h"
#include "K2Node_SetFieldsInStruct.h"

#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_ClearDelegate.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_InputAxisEvent.h"
#include "K2Node_GetDataTableRow.h"
#include "K2Node_InputAction.h"
#include "K2Node_InputKey.h"
#include "K2Node_InputTouch.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_VariableSet.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "K2Node_CallMaterialParameterCollectionFunction.h"
#include "K2Node_CallDataTableFunction.h"
#include "K2Node_CallFunctionOnMember.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MakeArray.h"
#include "K2Node_FormatText.h"
#include "K2Node_EaseFunction.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_Composite.h"
#include "K2Node_Select.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchName.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Self.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_Timeline.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Literal.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_MatineeController.h"
#include "K2Node_TemporaryVariable.h"
#include "EditorStyleSettings.h"

#define LOCTEXT_NAMESPACE "KismetSchema"

/*******************************************************************************
* Static File Helpers
*******************************************************************************/

namespace K2ActionCategories
{
	static FString const SubCategoryDelim("|");

	static FString const AddEventCategory     = LOCTEXT("AddEventCategory",     "Add Event").ToString();
	static FString const AddEventForPrefix    = LOCTEXT("AddEventForPrefix",    "Add Event for ").ToString();
	static FString const AddEventForActorsCategory = LOCTEXT("EventForActorsCategory", "Add event for selected actors").ToString();
	static FString const AnimNotifyCategory   = LOCTEXT("AnimNotifyCategory",   "Add AnimNotify Event").ToString();
	static FString const BranchPointCategory  = LOCTEXT("BranchPointCategory",  "Add Montage Branching Point Event").ToString();
	static FString const StateMachineCategory = LOCTEXT("StateMachineCategory", "Add State Machine Event").ToString();
	static FString const DefaultEventDispatcherCategory = LOCTEXT("DispatchersCategory", "Event Dispatchers").ToString();
	static FString const DelegateCategory     = LOCTEXT("DelegateCategory",     "Event Dispatcher").ToString();
	static FString const DelegateBindingCategory = LOCTEXT("DelegateBindingCategory", "Assign Event Dispatcher").ToString();

	static FString const InputCategory = LOCTEXT("InputCategory", "Input").ToString();
	static FString const ActionEventsSubCategory = LOCTEXT("ActionEventsCategory", "Action Events").ToString();
	static FString const AxisValuesSubCategory   = LOCTEXT("ActionValuesCategory", "Axis Values").ToString();
	static FString const AxisEventsSubCategory   = LOCTEXT("AxisEventsCategory",   "Axis Events").ToString();
	static FString const KeyEventsSubCategory    = LOCTEXT("KeyEventsCategory",    "Key Events").ToString();
	static FString const KeyValuesSubCategory    = LOCTEXT("KeyValuesCategory", "Key Values").ToString();
	static FString const GamepadEventsSubCategory = LOCTEXT("GamepadEventsCategory", "Gamepad Events").ToString();
	static FString const GamepadValuesSubCategory = LOCTEXT("GamepadValuesCategory", "Gamepad Values").ToString();
	static FString const MouseEventsSubCategory = LOCTEXT("MouseEventsCategory", "Mouse Events").ToString();
	static FString const MouseValuesSubCategory = LOCTEXT("MouseValuesCategory", "Mouse Values").ToString();

	static FString const ActionEventsCategory = InputCategory + SubCategoryDelim + ActionEventsSubCategory;
	static FString const AxisCategory         = InputCategory + SubCategoryDelim + AxisValuesSubCategory;
	static FString const AxisEventCategory    = InputCategory + SubCategoryDelim + AxisEventsSubCategory;
	static FString const KeyEventsCategory    = InputCategory + SubCategoryDelim + KeyEventsSubCategory;
	static FString const KeyValuesCategory    = InputCategory + SubCategoryDelim + KeyValuesSubCategory;
	static FString const GamepadEventsCategory = InputCategory + SubCategoryDelim + GamepadEventsSubCategory;
	static FString const GamepadValuesCategory = InputCategory + SubCategoryDelim + GamepadValuesSubCategory;
	static FString const MouseEventsCategory = InputCategory + SubCategoryDelim + MouseEventsSubCategory;
	static FString const MouseValuesCategory = InputCategory + SubCategoryDelim + MouseValuesSubCategory;
	static FString const TouchEventsCategory = InputCategory;


	static FString const UtilitiesCategory      = LOCTEXT("UtilityCategory", "Utilities").ToString();
	static FString const ArraySubCategory       = LOCTEXT("ArrayCategory",   "Array").ToString();
	static FString const CastingSubCategory     = LOCTEXT("CastingCategory", "Casting").ToString();
	static FString const EnumSubCategory        = LOCTEXT("EnumCategory",    "Enum").ToString();
	static FString const FlowSubCategory        = LOCTEXT("FlowCategory",    "Flow Control").ToString();
	static FString const SwitchSubCategory      = LOCTEXT("SwitchCategory",  "Switch").ToString();
	static FString const MacroToolsSubCategory  = LOCTEXT("MacroCategory",   "Macro").ToString();
	static FString const NameSubCategory        = LOCTEXT("NameCategory",    "Name").ToString();
	static FString const StringSubCategory      = LOCTEXT("StringCategory",  "String").ToString();
	static FString const StructSubCategory      = LOCTEXT("StructCategory",  "Struct").ToString();
	static FString const MakeStructSubCategory  = LOCTEXT("MakeStruct",      "Make Struct").ToString();
	static FString const BreakStructSubCategory = LOCTEXT("BreakStruct",     "Break Struct").ToString();
	static FString const TextSubCategory        = LOCTEXT("TextCategory",    "Text").ToString();
	static FString const MathCategory           = LOCTEXT("MathCategory",    "Math").ToString();
	static FString const InterpSubCategory      = LOCTEXT("InterpCategory", "Interpolation").ToString();

	static FString const ArrayCategory         = UtilitiesCategory + SubCategoryDelim + ArraySubCategory;
	static FString const CastingCategory       = UtilitiesCategory + SubCategoryDelim + CastingSubCategory;
	static FString const EnumCategory          = UtilitiesCategory + SubCategoryDelim + EnumSubCategory;
	static FString const FlowControlCategory   = UtilitiesCategory + SubCategoryDelim + FlowSubCategory;
	static FString const MacroToolsCategory    = UtilitiesCategory + SubCategoryDelim + MacroToolsSubCategory;
	static FString const NameCategory          = UtilitiesCategory + SubCategoryDelim + NameSubCategory;
	static FString const MakeStructCategory    = UtilitiesCategory + SubCategoryDelim + StructSubCategory + SubCategoryDelim + MakeStructSubCategory;
	static FString const BreakStructCategory   = UtilitiesCategory + SubCategoryDelim + StructSubCategory + SubCategoryDelim + BreakStructSubCategory;
	static FString const StringCategory        = UtilitiesCategory + SubCategoryDelim + StringSubCategory;
	static FString const TextCategory          = UtilitiesCategory + SubCategoryDelim + TextSubCategory;
	static FString const FallbackMacroCategory = UtilitiesCategory;

	static FString const InterpCategory = MathCategory + SubCategoryDelim + InterpSubCategory;


	static FString const AddComponentCategory = LOCTEXT("AddComponentCategory", "Add Component").ToString();
	static FString const VariablesCategory    = LOCTEXT("VariablesCategory",    "Variables").ToString();
	static FString const GameCategory	      = LOCTEXT("GameCategory",         "Game").ToString();
	

	static FString const RootFunctionCategory = TEXT("");
	static FString const GenericFunctionCategory   = LOCTEXT("FunctionCategory",         "Call Function").ToString();
	static FString const CallFuncForEachCategory   = LOCTEXT("CallFuncForEachCategory",  "Call Function For Each").ToString();
	static FString const CallFuncOnPrefix	       = LOCTEXT("CallFuncOnPrefix",         "Call Function on ").ToString();
	static FString const CallFuncOnActorsCategory  = LOCTEXT("CallFuncOnSelectedActors", "Call function on selected actors").ToString();
	static FString const FallbackInterfaceCategory = LOCTEXT("InterfaceCategory",        "Interface Messages").ToString();
}


/** Helper function to add new action types to the actions array */
template<class ActionType>
static void GetRequestedAction(FGraphActionListBuilderBase& ActionMenuBuilder, const FString& Category)
{
	UK2Node* NodeTemplate = NewObject<ActionType>(ActionMenuBuilder.OwnerOfTemporaries);
	TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ActionMenuBuilder, Category, NodeTemplate->GetNodeTitle(ENodeTitleType::ListView), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
	Action->NodeTemplate = NodeTemplate;
}

/** Utility for creating spawn from archetype nodes */
static void AddSpawnActorNodeAction(FGraphActionListBuilderBase& ContextMenuBuilder, const FString& FunctionCategory)
{
	FString const SpawnActorCategory = FunctionCategory + K2ActionCategories::SubCategoryDelim + K2ActionCategories::GameCategory;

	// Add the new SpawnActorFromClass node
	{
		UK2Node* NodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_SpawnActorFromClass>();
		TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, SpawnActorCategory, LOCTEXT("SpawnActorFromClass", "Spawn Actor from Class"), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
		Action->NodeTemplate = NodeTemplate;
	}
}

/** Utility for getting a TableRow from a DataTable */
static void AddGetDataTableRowNodeAction(FGraphActionListBuilderBase& ContextMenuBuilder, const FString& FunctionCategory)
{
	FString const GetDataTableRowCategory = FunctionCategory + K2ActionCategories::SubCategoryDelim + K2ActionCategories::UtilitiesCategory;
	
	// Add the new GetDataTableRow node
	{
		UK2Node* NodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_GetDataTableRow>();
		TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, GetDataTableRowCategory, LOCTEXT("GetDataTableRow", "Get Data Table Row"), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
		Action->NodeTemplate = NodeTemplate;
	}
}

/** Gets all the input action and axis events and nodes */
static void GetInputNodes(FGraphActionListBuilderBase& ActionMenuBuilder, const bool bIncludeEvents)
{
	if (bIncludeEvents)
	{
		TArray<FName> ActionNames;
		GetDefault<UInputSettings>()->GetActionNames(ActionNames);
		for (const FName InputActionName : ActionNames)
		{
			TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ActionMenuBuilder, K2ActionCategories::ActionEventsCategory, FText::FromName(InputActionName), InputActionName.ToString());
			UK2Node_InputAction* InputActionNode = ActionMenuBuilder.CreateTemplateNode<UK2Node_InputAction>();
			InputActionNode->InputActionName = InputActionName;
			Action->NodeTemplate = InputActionNode;
			Action->Keywords = TEXT("InputAction");
		}

		TArray<FName> AxisNames;
		GetDefault<UInputSettings>()->GetAxisNames(AxisNames);
		for (const FName InputAxisName : AxisNames)
		{
			TSharedPtr<FEdGraphSchemaAction_K2NewNode> GetAction = FK2ActionMenuBuilder::AddNewNodeAction(ActionMenuBuilder, K2ActionCategories::AxisCategory, FText::FromName(InputAxisName), InputAxisName.ToString());
			UK2Node_GetInputAxisValue* GetInputAxisValue = ActionMenuBuilder.CreateTemplateNode<UK2Node_GetInputAxisValue>();
			GetInputAxisValue->Initialize(InputAxisName);
			GetAction->NodeTemplate = GetInputAxisValue;
			GetAction->Keywords = TEXT("InputAxis");

			TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ActionMenuBuilder, K2ActionCategories::AxisEventCategory, FText::FromName(InputAxisName), InputAxisName.ToString());
			UK2Node_InputAxisEvent* InputAxisEvent = ActionMenuBuilder.CreateTemplateNode<UK2Node_InputAxisEvent>();
			InputAxisEvent->Initialize(InputAxisName);
			Action->NodeTemplate = InputAxisEvent;
			Action->Keywords = TEXT("InputAxis");
		}

		TArray<FKey> AllKeys;
		EKeys::GetAllKeys(AllKeys);

		for (FKey Key : AllKeys)
		{
			if (Key.IsBindableInBlueprints())
			{
				const FText KeyName = Key.GetDisplayName();

				const bool bGamepad = Key.IsGamepadKey();
				const bool bMouse = Key.IsMouseButton();
				const FString& KeyEventCategory = (bGamepad ? K2ActionCategories::GamepadEventsCategory : (bMouse ? K2ActionCategories::MouseEventsCategory : K2ActionCategories::KeyEventsCategory));

				TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ActionMenuBuilder, KeyEventCategory, KeyName, KeyName.ToString());
				TSharedPtr<FEdGraphSchemaAction_K2NewNode> GetAction;

				if (bGamepad)
				{
					Action->Keywords = TEXT("Input Key InputKey Gamepad Joypad Joystick Stick");
				}
				else if (bMouse)
				{
					Action->Keywords = TEXT("Input Key InputKey Mouse");
				}
				else
				{
					Action->Keywords = TEXT("Input Key InputKey Keyboard");
				}

				if (Key.IsFloatAxis())
				{
					UK2Node_InputAxisKeyEvent* InputKeyAxisNode = ActionMenuBuilder.CreateTemplateNode<UK2Node_InputAxisKeyEvent>();
					InputKeyAxisNode->Initialize(Key);
					Action->NodeTemplate = InputKeyAxisNode;

					const FString& KeyValuesCategory = (bGamepad ? K2ActionCategories::GamepadValuesCategory : (bMouse ? K2ActionCategories::MouseValuesCategory : K2ActionCategories::KeyValuesCategory));
					GetAction = FK2ActionMenuBuilder::AddNewNodeAction(ActionMenuBuilder, KeyValuesCategory, KeyName, KeyName.ToString());
					UK2Node_GetInputAxisKeyValue* GetInputAxisKeyValue = ActionMenuBuilder.CreateTemplateNode<UK2Node_GetInputAxisKeyValue>();
					GetInputAxisKeyValue->Initialize(Key);
					GetAction->NodeTemplate = GetInputAxisKeyValue;
					GetAction->Keywords = Action->Keywords;
				}
				else if (Key.IsVectorAxis())
				{
					UK2Node_InputVectorAxisEvent* InputVectorAxisNode = ActionMenuBuilder.CreateTemplateNode<UK2Node_InputVectorAxisEvent>();
					InputVectorAxisNode->Initialize(Key);
					Action->NodeTemplate = InputVectorAxisNode;

					const FString& KeyValuesCategory = (bGamepad ? K2ActionCategories::GamepadValuesCategory : (bMouse ? K2ActionCategories::MouseValuesCategory : K2ActionCategories::KeyValuesCategory));
					GetAction = FK2ActionMenuBuilder::AddNewNodeAction(ActionMenuBuilder, KeyValuesCategory, KeyName, KeyName.ToString());
					UK2Node_GetInputVectorAxisValue* GetInputVectorAxisValue = ActionMenuBuilder.CreateTemplateNode<UK2Node_GetInputVectorAxisValue>();
					GetInputVectorAxisValue->Initialize(Key);
					GetAction->NodeTemplate = GetInputVectorAxisValue;
					GetAction->Keywords = Action->Keywords;
				}
				else
				{
					UK2Node_InputKey* InputKeyNode = ActionMenuBuilder.CreateTemplateNode<UK2Node_InputKey>();
					InputKeyNode->InputKey = Key;
					Action->NodeTemplate = InputKeyNode;
				}
			}
		}

		TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ActionMenuBuilder, K2ActionCategories::TouchEventsCategory, LOCTEXT("Touch", "Touch"), LOCTEXT("Touch", "Touch").ToString());
		UK2Node_InputTouch* InputTouchNode = ActionMenuBuilder.CreateTemplateNode<UK2Node_InputTouch>();
		Action->NodeTemplate = InputTouchNode;
		Action->Keywords = TEXT("Input Touch");
	}
}

/** Generate a set of 'AddComponent' actions from the selection set */
static void GetAddComponentActionsUsingSelectedAssets(FBlueprintGraphActionListBuilder& ContextMenuBuilder)
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	USelection* SelectedAssets = GEditor->GetSelectedObjects();
	for (FSelectionIterator Iter(*SelectedAssets); Iter; ++Iter)
	{
		UObject* Asset = *Iter;
		TArray< TSubclassOf<UActorComponent> > ComponentClasses = FComponentAssetBrokerage::GetComponentsForAsset(Asset);
		for (int32 i = 0; i < ComponentClasses.Num(); i++)
		{
			TSubclassOf<UActorComponent> DestinationComponentType = ComponentClasses[i];

			TSharedPtr<FEdGraphSchemaAction_K2AddComponent> NewAction = FK2ActionMenuBuilder::CreateAddComponentAction(ContextMenuBuilder.OwnerOfTemporaries, ContextMenuBuilder.Blueprint, DestinationComponentType, Asset);
			ContextMenuBuilder.AddAction(NewAction);
		}
	}
}

/** Generate a set of 'AddComponent' actions, one for each allowed component class */
static void GetAddComponentClasses(UBlueprint const* BlueprintIn, FGraphActionListBuilderBase& ActionMenuBuilder)
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;

		// If this is a subclass of ActorComponent, not abstract, and tagged as spawnable from Kismet
		if (Class->IsChildOf(UActorComponent::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract) && Class->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent) )
		{
			TSharedPtr<FEdGraphSchemaAction_K2AddComponent> NewAction = FK2ActionMenuBuilder::CreateAddComponentAction(ActionMenuBuilder.OwnerOfTemporaries, BlueprintIn, Class, /*Asset=*/ NULL);
			ActionMenuBuilder.AddAction(NewAction);
		}
	}
}

/** Gets the add comment action */
static void GetCommentAction(UBlueprint const* BlueprintIn, FGraphActionListBuilderBase& ActionMenuBuilder)
{
	const bool bIsManyNodesSelected = (FKismetEditorUtilities::GetNumberOfSelectedNodes(BlueprintIn) > 0);
	const FText MenuDescription = bIsManyNodesSelected ? LOCTEXT("AddCommentFromSelection", "Create Comment from Selection") : LOCTEXT("AddComment", "Add Comment...");
	const FString ToolTip = LOCTEXT("CreateCommentToolTip", "Create a resizable comment box.").ToString();
	TSharedPtr<FEdGraphSchemaAction_K2AddComment> NewAction = TSharedPtr<FEdGraphSchemaAction_K2AddComment>(new FEdGraphSchemaAction_K2AddComment( MenuDescription, ToolTip ));

	ActionMenuBuilder.AddAction( NewAction );
}

/** Gets the add documentation action */
static void GetDocumentationAction(UBlueprint const* BlueprintIn, FGraphActionListBuilderBase& ActionMenuBuilder)
{
	const FString MenuDescription = LOCTEXT("AddDocumentationNode", "Add Documentation Node...").ToString();
	const FString ToolTip = LOCTEXT("AddDocumentationToolTip", "Adds a node to display UDN documentation excerpts.").ToString();
	TSharedPtr<FEdGraphSchemaAction_K2AddDocumentation> NewAction = TSharedPtr<FEdGraphSchemaAction_K2AddDocumentation>(new FEdGraphSchemaAction_K2AddDocumentation( MenuDescription, ToolTip ));

	ActionMenuBuilder.AddAction( NewAction );
}

/** */
static void GetEnumUtilitiesNodes(FBlueprintGraphActionListBuilder& ContextMenuBuilder, bool bNumEnum, bool bForEach, bool bCastFromByte, bool bLiteralByte, UEnum* OnlyEnum = NULL)
{
	struct FGetEnumUtilitiesNodes
	{
		static void Get(FBlueprintGraphActionListBuilder& InContextMenuBuilder, bool bInNumEnum, bool bInForEach, bool bInCastFromByte, bool bInLiteralByte, UEnum* Enum, const FString& Category)
		{
			const bool bIsBlueprintType = UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Enum);
			if (bIsBlueprintType)
			{
				if (bInNumEnum)
				{
					UK2Node_GetNumEnumEntries* EnumTemplate = InContextMenuBuilder.CreateTemplateNode<UK2Node_GetNumEnumEntries>();
					EnumTemplate->Enum = Enum;
					UK2Node* NodeTemplate = EnumTemplate;
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(
						InContextMenuBuilder, Category, NodeTemplate->GetNodeTitle(ENodeTitleType::ListView), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
					Action->NodeTemplate = NodeTemplate;
				}

				if (bInForEach)
				{
					UK2Node_ForEachElementInEnum* EnumTemplate = InContextMenuBuilder.CreateTemplateNode<UK2Node_ForEachElementInEnum>();
					EnumTemplate->Enum = Enum;
					UK2Node* NodeTemplate = EnumTemplate;
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(
						InContextMenuBuilder, Category, NodeTemplate->GetNodeTitle(ENodeTitleType::ListView), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
					Action->NodeTemplate = NodeTemplate;
				}

				if (bInCastFromByte)
				{
					UK2Node_CastByteToEnum* EnumTemplate = InContextMenuBuilder.CreateTemplateNode<UK2Node_CastByteToEnum>();
					EnumTemplate->Enum = Enum;
					EnumTemplate->bSafe = true;
					UK2Node* NodeTemplate = EnumTemplate;
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(
						InContextMenuBuilder, Category, NodeTemplate->GetNodeTitle(ENodeTitleType::ListView), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
					Action->NodeTemplate = NodeTemplate;
				}

				if (bInLiteralByte)
				{
					UK2Node_EnumLiteral* EnumTemplate = InContextMenuBuilder.CreateTemplateNode<UK2Node_EnumLiteral>();
					EnumTemplate->Enum = Enum;
					UK2Node* NodeTemplate = EnumTemplate;
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(
						InContextMenuBuilder, Category, NodeTemplate->GetNodeTitle(ENodeTitleType::ListView), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
					Action->NodeTemplate = NodeTemplate;
				}
			}
		}
	};

	if (OnlyEnum)
	{
		FGetEnumUtilitiesNodes::Get(ContextMenuBuilder, bNumEnum, bForEach, bCastFromByte, bLiteralByte, OnlyEnum, K2ActionCategories::EnumCategory);
	}
	else
	{
		for (TObjectIterator<UEnum> EnumIt; EnumIt; ++EnumIt)
		{
			if (UEnum* CurrentEnum = *EnumIt)
			{
				FGetEnumUtilitiesNodes::Get(ContextMenuBuilder, bNumEnum, bForEach, bCastFromByte, bLiteralByte, CurrentEnum, K2ActionCategories::EnumCategory);
			}
		}
	}
}

/** Get functions you can call from the given interface */
static void GetInterfaceMessages(UClass* CurrentInterface, const FString& MessagesCategory, FBlueprintGraphActionListBuilder& ContextMenuBuilder)
{
	if (CurrentInterface && FKismetEditorUtilities::IsClassABlueprintInterface(CurrentInterface) && !FKismetEditorUtilities::IsClassABlueprintSkeleton(CurrentInterface))
	{
		const FString MessageSubCategory = FString::Printf(TEXT("%s|%s"), *MessagesCategory, *CurrentInterface->GetName());
		for (TFieldIterator<UFunction> FunctionIt(CurrentInterface, EFieldIteratorFlags::ExcludeSuper); FunctionIt; ++FunctionIt)
		{
			UFunction* Function = *FunctionIt;
			if (UEdGraphSchema_K2::CanUserKismetCallFunction(Function))
			{
				UK2Node_Message* MessageTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_Message>();
				MessageTemplate->FunctionReference.SetExternalMember(Function->GetFName(), CurrentInterface);
				TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, MessageSubCategory, FText::FromString(Function->GetName()), MessageTemplate->GetTooltipText().ToString(), 0, MessageTemplate->GetKeywords());
				Action->NodeTemplate = MessageTemplate;
			}
		}
	}
}

/** Helper method to get a reference variable setter node */
static void GetReferenceSetterItems(FBlueprintGraphActionListBuilder& ContextMenuBuilder)
{
	const UEdGraphPin* AssociatedPin = ContextMenuBuilder.FromPin;

	UK2Node_VariableSetRef* SetRefTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_VariableSetRef>();
	TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::VariablesCategory, SetRefTemplate->GetNodeTitle(ENodeTitleType::ListView), SetRefTemplate->GetTooltipText().ToString(), 0, SetRefTemplate->GetKeywords());

	Action->NodeTemplate = SetRefTemplate;
}

/** 
 * Check if function library can be used with specified scope
 *
 * @param	LibraryClass	Function library to test
 * @param	ScopeClass		Blueprint class used for restriction test
 */
static bool CanUseLibraryFunctionsForScope(UClass* LibraryClass, const UClass* ScopeClass)
{
	if (LibraryClass->HasMetaData(FBlueprintMetadata::MD_RestrictedToClasses))
	{
		const FString& ClassRestrictions = LibraryClass->GetMetaData(FBlueprintMetadata::MD_RestrictedToClasses);
		for (UClass* TestOwnerClass = (UClass*)ScopeClass; TestOwnerClass; TestOwnerClass = TestOwnerClass->GetSuperClass())
		{
			const bool bFound = (ClassRestrictions == TestOwnerClass->GetName()) || !!FCString::StrfindDelim(*ClassRestrictions, *ScopeClass->GetName(), TEXT(" "));
			if (bFound)
			{
				return true;
			}
		}

		return false;
	}

	return true;
}

/** */
static UClass* FindMostDerivedCommonActor( TArray<AActor*>& InObjects )
{
	check(InObjects.Num() > 0);

	// Find the most derived common class for all passed in Objects
	UClass* CommonClass = InObjects[0]->GetClass();
	for (int32 ObjIdx = 1; ObjIdx < InObjects.Num(); ++ObjIdx)
	{
		while (!InObjects[ObjIdx]->IsA(CommonClass))
		{
			CommonClass = CommonClass->GetSuperClass();
		}
	}
	return CommonClass;
}

/*******************************************************************************
* FBlueprintGraphActionListBuilder
*******************************************************************************/

//------------------------------------------------------------------------------
FBlueprintGraphActionListBuilder::FBlueprintGraphActionListBuilder(const UEdGraph* InGraph)
	: FGraphContextMenuBuilder(InGraph)
{
	check(CurrentGraph != NULL);
	Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(CurrentGraph);

	OwnerOfTemporaries = NewObject<UEdGraph>((Blueprint != NULL) ? (UObject*)Blueprint : (UObject*)GetTransientPackage());
	OwnerOfTemporaries->Schema = CurrentGraph->Schema;
	OwnerOfTemporaries->SetFlags(RF_Transient);
}

/*******************************************************************************
* FBlueprintPaletteListBuilder
*******************************************************************************/

//------------------------------------------------------------------------------
FBlueprintPaletteListBuilder::FBlueprintPaletteListBuilder(UBlueprint const* BlueprintIn, FString const& CategoryIn/* = TEXT("")*/)
	: FCategorizedGraphActionListBuilder(CategoryIn)
	, Blueprint(BlueprintIn)
{
	if (Blueprint != NULL)
	{
		OwnerOfTemporaries = NewObject<UEdGraph>((UObject*)Blueprint);
		OwnerOfTemporaries->Schema = UEdGraphSchema_K2::StaticClass();
		OwnerOfTemporaries->SetFlags(RF_Transient);
	}
}

/*******************************************************************************
* Static FK2ActionMenuBuilder Interface
*******************************************************************************/

/** */
int32 FK2ActionMenuBuilder::bAllowUnsafeCommands = 0;

//------------------------------------------------------------------------------
TSharedPtr<FEdGraphSchemaAction_K2AddComponent> FK2ActionMenuBuilder::CreateAddComponentAction(UObject* TemporaryOuter, const UBlueprint* Blueprint, TSubclassOf<UActorComponent> DestinationComponentType, UObject* Asset)
{
	// Make an add component node
	UK2Node_CallFunction* CallFuncNode = NewObject<UK2Node_AddComponent>(TemporaryOuter);

	UFunction* AddComponentFn = FindFieldChecked<UFunction>(AActor::StaticClass(), UK2Node_AddComponent::GetAddComponentFunctionName());
	CallFuncNode->FunctionReference.SetFromField<UFunction>(AddComponentFn, FBlueprintEditorUtils::IsActorBased(Blueprint));

	// Create an action to do it
	FString Description;
	FString ToolTip;

	if (Asset != NULL)
	{
		Description = FString::Printf(TEXT("Add %s (as %s)"), *Asset->GetName(), *DestinationComponentType->GetName());
		ToolTip = FString::Printf(TEXT("Spawn a %s using %s"), *DestinationComponentType->GetName(), *Asset->GetName());
	}
	else
	{
		Description = FString::Printf(TEXT("Add %s"), *DestinationComponentType->GetName());
		ToolTip = FString::Printf(TEXT("Spawn a %s"), *DestinationComponentType->GetName());
	}

	// Find class group name
	FString ClassGroup;
	{
		TArray<FString> ClassGroupNames;
		DestinationComponentType->GetClassGroupNames(ClassGroupNames);
		static const FString CommonClassGroup(TEXT("Common"));
		if(ClassGroupNames.Contains(CommonClassGroup)) // 'Common' takes priority
		{
			ClassGroup = CommonClassGroup;
		}
		else if (ClassGroupNames.Num())
		{
			ClassGroup = ClassGroupNames[0];
		}
	}

	// Create menu category
	FString MenuCategory = K2ActionCategories::AddComponentCategory + FString(TEXT("|")) + ClassGroup;
	TSharedPtr<FEdGraphSchemaAction_K2AddComponent> Action = TSharedPtr<FEdGraphSchemaAction_K2AddComponent>(new FEdGraphSchemaAction_K2AddComponent(MenuCategory, FText::FromString(Description), ToolTip, Asset != NULL ? 2 : 0));

	Action->NodeTemplate = CallFuncNode;
	Action->ComponentClass = DestinationComponentType;
	Action->ComponentAsset = Asset;

	return Action;
}

//------------------------------------------------------------------------------
TSharedPtr<FEdGraphSchemaAction_K2NewNode> FK2ActionMenuBuilder::AddNewNodeAction(FGraphActionListBuilderBase& ContextMenuBuilder, const FString& Category, const FText& MenuDesc, const FString& Tooltip, const int32 Grouping, const FString& Keywords)
{
	TSharedPtr<FEdGraphSchemaAction_K2NewNode> NewActionNode = TSharedPtr<FEdGraphSchemaAction_K2NewNode>(new FEdGraphSchemaAction_K2NewNode(Category, MenuDesc, Tooltip, Grouping));
	NewActionNode->Keywords = Keywords;

	ContextMenuBuilder.AddAction( NewActionNode );

	return NewActionNode;
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::AddEventForFunction(FGraphActionListBuilderBase& ActionMenuBuilder, UFunction* Function)
{
	if (Function != NULL)
	{
		check(!Function->HasAnyFunctionFlags(FUNC_Delegate));
		const FString SigName   = UEdGraphSchema_K2::GetFriendlySignatureName(Function);
		const FString EventName = FText::Format( LOCTEXT("EventWithSignatureName", "Event {0}"), FText::FromString(SigName)).ToString();

		UK2Node_Event* EventNodeTemplate = ActionMenuBuilder.CreateTemplateNode<UK2Node_Event>();
		EventNodeTemplate->EventSignatureName = Function->GetFName();
		EventNodeTemplate->EventSignatureClass = CastChecked<UClass>(Function->GetOuter());
		EventNodeTemplate->bOverrideFunction = true;

		const FString Category = UK2Node_CallFunction::GetDefaultCategoryForFunction(Function, K2ActionCategories::AddEventCategory);

		TSharedPtr<FEdGraphSchemaAction_K2AddEvent> Action(new FEdGraphSchemaAction_K2AddEvent(Category, FText::FromString(EventName), EventNodeTemplate->GetTooltipText().ToString(), 0));
		Action->Keywords     = EventNodeTemplate->GetKeywords();
		Action->NodeTemplate = EventNodeTemplate;

		ActionMenuBuilder.AddAction( Action );
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::AttachMacroGraphAction(FGraphActionListBuilderBase& ActionMenuBuilder, UEdGraph* MacroGraph)
{
	FKismetUserDeclaredFunctionMetadata* MacroGraphMetadata = UK2Node_MacroInstance::GetAssociatedGraphMetadata(MacroGraph);
	if (MacroGraphMetadata)
	{
		const FString CategoryName = (!MacroGraphMetadata->Category.IsEmpty()) ? MacroGraphMetadata->Category : K2ActionCategories::FallbackMacroCategory;

		UK2Node_MacroInstance* MacroTemplate = ActionMenuBuilder.CreateTemplateNode<UK2Node_MacroInstance>();
		MacroTemplate->SetMacroGraph(MacroGraph);
		TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ActionMenuBuilder, CategoryName, MacroTemplate->GetNodeTitle(ENodeTitleType::ListView), MacroTemplate->GetTooltipText().ToString());
		Action->NodeTemplate = MacroTemplate;
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::AddSpawnInfoForFunction(const UFunction* Function, bool bIsParentContext, const FFunctionTargetInfo& TargetInfo, const FMemberReference& CallOnMember, const FString& BaseCategory, int32 const ActionGrouping, FGraphActionListBuilderBase& ListBuilder, bool bCalledForEach, bool bPlaceOnTop)
{
	FString Tooltip = UK2Node_CallFunction::GetDefaultTooltipForFunction(Function);
	FString Description = UK2Node_CallFunction::GetUserFacingFunctionName(Function);
	FString Keywords = UK2Node_CallFunction::GetKeywordsForFunction(Function);

	if (bIsParentContext)
	{
		const FString ParentContextName = Function->GetOuter()->GetName();
		Tooltip = FString::Printf(TEXT("Call %s's version of %s.\n"), *ParentContextName, *Description) + Tooltip;
		Description = ParentContextName + TEXT("'s ") + Description;
	}

	FString NodeCategory;
	if (!bIsParentContext && !bPlaceOnTop)
	{
		NodeCategory = UK2Node_CallFunction::GetDefaultCategoryForFunction(Function, BaseCategory);
		if (NodeCategory.IsEmpty())
		{
			NodeCategory = K2ActionCategories::GenericFunctionCategory;
		}
	}

	TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action;
	// See if we want to create an actor ref node for the target
	if (TargetInfo.FunctionTarget == FFunctionTargetInfo::EFT_Actor)
	{
		TSharedPtr<FEdGraphSchemaAction_K2AddCallOnActor> ActorAction = TSharedPtr<FEdGraphSchemaAction_K2AddCallOnActor>(new FEdGraphSchemaAction_K2AddCallOnActor(NodeCategory, FText::FromString(Description), Tooltip, ActionGrouping));

		for ( int32 ActorIndex = 0; ActorIndex < TargetInfo.Actors.Num(); ActorIndex++ )
		{
			AActor* Actor = TargetInfo.Actors[ActorIndex].Get();
			if ( Actor != NULL )
			{
				ActorAction->LevelActors.Add( Actor );
			}
		}
		Action = ActorAction;

		ListBuilder.AddAction( ActorAction );
	}
	// Or create a component property ref node for the target
	else if(TargetInfo.FunctionTarget == FFunctionTargetInfo::EFT_Component)
	{
		TSharedPtr<FEdGraphSchemaAction_K2AddCallOnVariable> VarAction = TSharedPtr<FEdGraphSchemaAction_K2AddCallOnVariable>(new FEdGraphSchemaAction_K2AddCallOnVariable(NodeCategory, FText::FromString(Description), Tooltip, ActionGrouping));

		VarAction->VariableName = TargetInfo.ComponentPropertyName;
		Action = VarAction;

		ListBuilder.AddAction( VarAction );
	}
	// Default case (no additional nodes created for target)
	else 
	{
		Action = FK2ActionMenuBuilder::AddNewNodeAction(ListBuilder, NodeCategory, FText::FromString(Description), Tooltip, 0, Keywords);
	}

	Action->Keywords = Keywords;

	const bool bHasArrayPointerParms = Function->HasMetaData(TEXT("ArrayParm"));
	const bool bIsPure = Function->HasAllFunctionFlags(FUNC_BlueprintPure);
	const bool bCommutativeAssociativeBinaryOperator = Function->HasMetaData(FBlueprintMetadata::MD_CommutativeAssociativeBinaryOperator);
	const bool bMaterialParameterCollectionFunction = Function->HasMetaData(FBlueprintMetadata::MD_MaterialParameterCollectionFunction);
	bool const bIsDataTableFunc = Function->HasMetaData(FBlueprintMetadata::MD_DataTablePin);

	UK2Node_CallFunction* CallTemplate = NULL;

	if (bCalledForEach)
	{
		CallTemplate = ListBuilder.CreateTemplateNode<UK2Node_CallFunction>();
	}
	else if (bCommutativeAssociativeBinaryOperator && bIsPure)
	{
		CallTemplate = ListBuilder.CreateTemplateNode<UK2Node_CommutativeAssociativeBinaryOperator>();
	}
	else if (bMaterialParameterCollectionFunction)
	{
		CallTemplate = ListBuilder.CreateTemplateNode<UK2Node_CallMaterialParameterCollectionFunction>();
	}
	else if (bIsDataTableFunc)
	{
		CallTemplate = ListBuilder.CreateTemplateNode<UK2Node_CallDataTableFunction>();
	}
	else if (CallOnMember.GetMemberName() != NAME_None)
	{
		CallTemplate = ListBuilder.CreateTemplateNode<UK2Node_CallFunctionOnMember>();
		// Assign member to call on
		UK2Node_CallFunctionOnMember* CallFuncOnMemberTemplate = CastChecked<UK2Node_CallFunctionOnMember>(CallTemplate);
		CallFuncOnMemberTemplate->MemberVariableToCallOn = CallOnMember;
	}
	else if (bIsParentContext)
	{
		CallTemplate = ListBuilder.CreateTemplateNode<UK2Node_CallParentFunction>();
	}
	else if (!bHasArrayPointerParms)
	{
		CallTemplate = ListBuilder.CreateTemplateNode<UK2Node_CallFunction>();
	}
	else
	{
		CallTemplate = ListBuilder.CreateTemplateNode<UK2Node_CallArrayFunction>();
	}

	CallTemplate->SetFromFunction(Function);
	//@TODO: FUNCTIONREFACTOR:	CallFuncNode->FunctionReference.SetExternalMember(Function->GetFName(), Function->GetOuterUClass());

	Action->NodeTemplate = CallTemplate;
}

/*******************************************************************************
* Public FK2ActionMenuBuilder Interface
*******************************************************************************/

//------------------------------------------------------------------------------
FK2ActionMenuBuilder::FK2ActionMenuBuilder(UEdGraphSchema_K2 const* SchemaIn)
	: K2Schema(SchemaIn)
{
	check(K2Schema != NULL);
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetGraphContextActions(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const
{
	// Now do schema-specific stuff
	if (ContextMenuBuilder.FromPin != NULL)
	{
		GetPinAllowedNodeTypes(ContextMenuBuilder);
	}
	else
	{
		GetContextAllowedNodeTypes(ContextMenuBuilder);
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetAllActions(FBlueprintPaletteListBuilder& PaletteBuilder) const
{
	UClass const* ContextClass = PaletteBuilder.Blueprint ? PaletteBuilder.Blueprint->SkeletonGeneratedClass : NULL;

	// Get the list of valid classes
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = (*It);
		if (Class != ContextClass)
		{
			// Show protected members of parent classes
			const bool bShowProtected = ContextClass->IsChildOf(Class);

			GetAllActionsForClass(PaletteBuilder, Class, false, bShowProtected, false, true);
		}
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetPaletteActions(FBlueprintPaletteListBuilder& ActionMenuBuilder, TWeakObjectPtr<UClass> FilterClass/* = NULL*/) const
{
	if (FilterClass.IsValid())
	{
		UClass* SkelClass = ActionMenuBuilder.Blueprint ? ActionMenuBuilder.Blueprint->SkeletonGeneratedClass : NULL;

		const bool bShowProtected = (SkelClass != NULL) && SkelClass->IsChildOf(FilterClass.Get()); // show protected members of this class if our 'context' is a child of it
		GetAllActionsForClass(ActionMenuBuilder, FilterClass.Get(), true, bShowProtected, true, true);
	}
	else
	{
		GetAllActions(ActionMenuBuilder);
	}


	{
		// Actions that are part of Flow Control
		GetRequestedAction<UK2Node_IfThenElse>(ActionMenuBuilder, K2ActionCategories::FlowControlCategory);
		GetRequestedAction<UK2Node_ExecutionSequence>(ActionMenuBuilder, K2ActionCategories::FlowControlCategory);
		GetRequestedAction<UK2Node_MultiGate>(ActionMenuBuilder, K2ActionCategories::FlowControlCategory);
		GetSwitchMenuItems(ActionMenuBuilder, K2ActionCategories::FlowControlCategory);
	}
	GetRequestedAction<UK2Node_MakeArray>(ActionMenuBuilder, K2ActionCategories::ArrayCategory);
	AddSpawnActorNodeAction(ActionMenuBuilder, TEXT(""));

	GetAllEventActions(ActionMenuBuilder);

	GetAllInterfaceMessageActions(ActionMenuBuilder);

	GetInputNodes(ActionMenuBuilder, true);

	// Components are only valid in actor based blueprints, but not in the level script blueprint
	if(!FBlueprintEditorUtils::IsLevelScriptBlueprint(ActionMenuBuilder.Blueprint) && FBlueprintEditorUtils::IsActorBased(ActionMenuBuilder.Blueprint))
	{
		GetAddComponentClasses(ActionMenuBuilder.Blueprint, ActionMenuBuilder);
	}

	GetCommentAction(ActionMenuBuilder.Blueprint, ActionMenuBuilder);

	GetDocumentationAction(ActionMenuBuilder.Blueprint, ActionMenuBuilder);

	GetMacroTools(ActionMenuBuilder, true, NULL);

	GetRequestedAction<UK2Node_FormatText>(ActionMenuBuilder, K2ActionCategories::TextCategory);

	{
		// Actions that are part of Math
		GetRequestedAction<UK2Node_EaseFunction>(ActionMenuBuilder, K2ActionCategories::InterpCategory);
	}
}

/*******************************************************************************
* Private FK2ActionMenuBuilder Methods
*******************************************************************************/

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetContextAllowedNodeTypes(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const
{
	const UBlueprint* Blueprint = ContextMenuBuilder.Blueprint;
	UClass* Class = (Blueprint != NULL) ? Blueprint->SkeletonGeneratedClass : NULL;
	if (Class == NULL)
	{
		UE_LOG(LogBlueprint, Log, TEXT("GetContextAllowedNodeTypes: Failed to find class from Blueprint '%s'"), *Blueprint->GetPathName());
	}
	else
	{
		const EGraphType GraphType = K2Schema->GetGraphType(ContextMenuBuilder.CurrentGraph);
	
		const bool bSupportsEventGraphs = FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint);
		const bool bAllowEvents = ((GraphType == GT_Ubergraph) && bSupportsEventGraphs) ||
			((bAllowUnsafeCommands) && (Blueprint->BlueprintType == BPTYPE_MacroLibrary));
		const bool bIsConstructionScript = K2Schema->IsConstructionScript(ContextMenuBuilder.CurrentGraph);
		const bool bAllowImpureFuncs = K2Schema->DoesGraphSupportImpureFunctions(ContextMenuBuilder.CurrentGraph);
		
		// Flow control operations
		if (bAllowImpureFuncs)
		{
			GetRequestedAction<UK2Node_IfThenElse>(ContextMenuBuilder, K2ActionCategories::FlowControlCategory);
			GetRequestedAction<UK2Node_ExecutionSequence>(ContextMenuBuilder, K2ActionCategories::FlowControlCategory);
			GetRequestedAction<UK2Node_MultiGate>(ContextMenuBuilder, K2ActionCategories::FlowControlCategory);
			GetRequestedAction<UK2Node_MakeArray>(ContextMenuBuilder, K2ActionCategories::ArrayCategory);

			FBlueprintPaletteListBuilder PaletteBuilder(ContextMenuBuilder.Blueprint);
			GetSwitchMenuItems(PaletteBuilder, K2ActionCategories::FlowControlCategory, ContextMenuBuilder.FromPin);
			ContextMenuBuilder.Append(PaletteBuilder);
		}

		GetRequestedAction<UK2Node_GetEnumeratorName>(ContextMenuBuilder, K2ActionCategories::NameCategory);
		GetRequestedAction<UK2Node_GetEnumeratorNameAsString>(ContextMenuBuilder, K2ActionCategories::StringCategory);

		GetRequestedAction<UK2Node_FormatText>(ContextMenuBuilder, K2ActionCategories::TextCategory);

		{
			// Actions that are part of Math
			GetRequestedAction<UK2Node_EaseFunction>(ContextMenuBuilder, K2ActionCategories::InterpCategory);
		}

		GetAnimNotifyMenuItems(ContextMenuBuilder);
		GetVariableGettersSettersForClass(ContextMenuBuilder, Class, true, bAllowEvents);

		uint32 FunctionTypes = UEdGraphSchema_K2::FT_Pure | UEdGraphSchema_K2::FT_Const | UEdGraphSchema_K2::FT_Protected;
		if(bAllowImpureFuncs)
		{
			FunctionTypes |= UEdGraphSchema_K2::FT_Imperative;
		}

		GetFuncNodesForClass(ContextMenuBuilder, Class, Class, ContextMenuBuilder.CurrentGraph, FunctionTypes, K2ActionCategories::RootFunctionCategory);
		GetFuncNodesForClass(ContextMenuBuilder, NULL, Class, ContextMenuBuilder.CurrentGraph, FunctionTypes, K2ActionCategories::RootFunctionCategory);

		// spawn actor from archetype node
		if (!bIsConstructionScript)
		{
			AddSpawnActorNodeAction(ContextMenuBuilder, K2ActionCategories::RootFunctionCategory);

			if ( Blueprint->SupportsInputEvents() )
			{
				GetInputNodes(ContextMenuBuilder, true);
			}
		}
		
		AddGetDataTableRowNodeAction(ContextMenuBuilder, K2ActionCategories::RootFunctionCategory);

		// Add struct make/break nodes
		GetStructActions( ContextMenuBuilder );

		bool bCompositeOfUbberGraph = false;

		//If the composite has a ubergraph in its outer, it is allowed to have timelines/events added
		if (bSupportsEventGraphs && K2Schema->IsCompositeGraph(ContextMenuBuilder.CurrentGraph))
		{
			const UEdGraph* TestGraph = ContextMenuBuilder.CurrentGraph;
			while (TestGraph)
			{
				if (UK2Node_Composite* Composite = Cast<UK2Node_Composite>(TestGraph->GetOuter()))
				{
					TestGraph =  Cast<UEdGraph>(Composite->GetOuter());
				}
				else if (K2Schema->GetGraphType(TestGraph) == GT_Ubergraph)
				{
					bCompositeOfUbberGraph = true;
					break;
				}
				else
				{
					TestGraph = Cast<UEdGraph>(TestGraph->GetOuter());
				}
			}
		}

		if (bAllowEvents || bCompositeOfUbberGraph )
		{
			FBlueprintPaletteListBuilder PaletteBuilder(ContextMenuBuilder.Blueprint);
			GetAllEventActions(PaletteBuilder);
			ContextMenuBuilder.Append(PaletteBuilder);

			// PlayMovieScene
			// @todo sequencer: Currently don't allow "PlayMovieScene" to be added to scripts unless Sequencer is enabled
			// @todo sequencer: This block of logic causes MovieSceneTools to be circularly dependent with UnrealEd.  Ideally we don't want UnrealEd to statically depend on MovieSceneTools!
			/*
			if( FParse::Param( FCommandLine::Get(), TEXT( "Sequencer" ) ) )
			{
				if( FBlueprintEditorUtils::IsLevelScriptBlueprint( Blueprint ) && FBlueprintEditorUtils::DoesSupportEventGraphs( Blueprint ) )
				{
					// @todo sequencer: Should probably use drag and drop to place a new MovieScene node instead of placing an empty node
					TSharedPtr<FEdGraphSchemaAction_K2AddPlayMovieScene> Action( new FEdGraphSchemaAction_K2AddPlayMovieScene(TEXT(""), TEXT("Play MovieScene..."), TEXT(""), 0));
					OutTypes.Add(Action);

					UK2Node_PlayMovieScene* PlayMovieSceneNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_PlayMovieScene>();
					Action->NodeTemplate = PlayMovieSceneNode;
				}
			}*/
		}

		if (FKismetEditorUtilities::CanPasteNodes(ContextMenuBuilder.CurrentGraph))
		{
			TSharedPtr<FEdGraphSchemaAction_K2PasteHere> NewAction(new FEdGraphSchemaAction_K2PasteHere(TEXT(""), LOCTEXT("PasteHere", "Paste here"), TEXT(""), 0));
			ContextMenuBuilder.AddAction(NewAction);
		}

		// Level script
		if (FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint))
		{
			GetLiteralsFromActorSelection(ContextMenuBuilder);
			GetBoundEventsFromActorSelection(ContextMenuBuilder);
			GetMatineeControllers(ContextMenuBuilder);
			GetFunctionCallsOnSelectedActors(ContextMenuBuilder);
		}
		// Non level script
		else
		{
			if ( FBlueprintEditorUtils::IsActorBased(Blueprint) )
			{
				GetAddComponentActionsUsingSelectedAssets(ContextMenuBuilder);
				GetAddComponentClasses(ContextMenuBuilder.Blueprint, ContextMenuBuilder);
			}

			GetFunctionCallsOnSelectedComponents(ContextMenuBuilder);

			if ( bAllowEvents && Blueprint->AllowsDynamicBinding() )
			{
				GetBoundEventsFromComponentSelection(ContextMenuBuilder);			
			}
		}

		// Show local variables and assignment nodes only if it's *not* an ubergraph
		if ((GraphType != GT_Ubergraph) && (GraphType != GT_Animation))
		{
			// Assignment statement
			{
				UK2Node* NodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_AssignmentStatement>();
				TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::MacroToolsCategory, NodeTemplate->GetNodeTitle(ENodeTitleType::ListView), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
				Action->NodeTemplate = NodeTemplate;
			}

			// Temporary variables
			TArray<FEdGraphPinType> TemporaryTypes;
			// Standard Types
			TemporaryTypes.Add(FEdGraphPinType(K2Schema->PC_Int, TEXT(""), NULL, false, false));
			TemporaryTypes.Add(FEdGraphPinType(K2Schema->PC_Float, TEXT(""), NULL, false, false));
			TemporaryTypes.Add(FEdGraphPinType(K2Schema->PC_Boolean, TEXT(""), NULL, false, false));
			TemporaryTypes.Add(FEdGraphPinType(K2Schema->PC_String, TEXT(""), NULL, false, false));
			TemporaryTypes.Add(FEdGraphPinType(K2Schema->PC_Text, TEXT(""), NULL, false, false));

			// Array Variants
			TemporaryTypes.Add(FEdGraphPinType(K2Schema->PC_Int, TEXT(""), NULL, true, false));
			TemporaryTypes.Add(FEdGraphPinType(K2Schema->PC_Float, TEXT(""), NULL, true, false));
			TemporaryTypes.Add(FEdGraphPinType(K2Schema->PC_Boolean, TEXT(""), NULL, true, false));
			TemporaryTypes.Add(FEdGraphPinType(K2Schema->PC_String, TEXT(""), NULL, true, false));
			TemporaryTypes.Add(FEdGraphPinType(K2Schema->PC_Text, TEXT(""), NULL, true, false));

			FEdGraphPinType VectorPinType = FEdGraphPinType(K2Schema->PC_Struct, TEXT("Vector"), NULL, false, false);
			VectorPinType.PinSubCategoryObject = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("Vector"));
			TemporaryTypes.Add(VectorPinType);
			VectorPinType.bIsArray = true;
			TemporaryTypes.Add(VectorPinType);

			FEdGraphPinType RotatorPinType = FEdGraphPinType(K2Schema->PC_Struct, TEXT("Rotator"), NULL, false, false);
			RotatorPinType.PinSubCategoryObject = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("Rotator"));
			TemporaryTypes.Add(RotatorPinType);
			RotatorPinType.bIsArray = true;
			TemporaryTypes.Add(RotatorPinType);

			FEdGraphPinType TransformPinType = FEdGraphPinType(K2Schema->PC_Struct, TEXT("Transform"), NULL, false, false);
			TransformPinType.PinSubCategoryObject = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("Transform"));
			TemporaryTypes.Add(TransformPinType);
			TransformPinType.bIsArray = true;
			TemporaryTypes.Add(TransformPinType);

			FEdGraphPinType BlendSampleDataPinType = FEdGraphPinType(K2Schema->PC_Struct, TEXT("BlendSampleData"), NULL, false, false);
			BlendSampleDataPinType.PinSubCategoryObject = FindObjectChecked<UScriptStruct>(ANY_PACKAGE, TEXT("BlendSampleData"));
			BlendSampleDataPinType.bIsArray = true;
			TemporaryTypes.Add(BlendSampleDataPinType);

			if (bAllowUnsafeCommands)
			{
				for (FObjectIterator Iter(UScriptStruct::StaticClass()); Iter; ++Iter)
				{
					UScriptStruct* Struct = CastChecked<UScriptStruct>(*Iter);
					TemporaryTypes.Add(FEdGraphPinType(K2Schema->PC_Struct, TEXT(""), Struct, false, false));
				}
			}

			for (TArray<FEdGraphPinType>::TIterator TypeIt(TemporaryTypes); TypeIt; ++TypeIt)
			{
				// Add a standard version of the local
				{
					UK2Node_TemporaryVariable* NodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_TemporaryVariable>();
					NodeTemplate->VariableType = *TypeIt;
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::MacroToolsCategory, NodeTemplate->GetNodeTitle(ENodeTitleType::ListView), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
					Action->NodeTemplate = NodeTemplate;
				}
			}

			// Add in bool and int persistent pin types
			if(GraphType == GT_Macro)
			{
				// Persistent Integer
				UK2Node_TemporaryVariable* PersistentIntNodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_TemporaryVariable>();
				PersistentIntNodeTemplate->VariableType = FEdGraphPinType(K2Schema->PC_Int, TEXT(""), NULL, false, false);
				PersistentIntNodeTemplate->bIsPersistent = true;
				TSharedPtr<FEdGraphSchemaAction_K2NewNode> IntAction = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::MacroToolsCategory, PersistentIntNodeTemplate->GetNodeTitle(ENodeTitleType::ListView), PersistentIntNodeTemplate->GetTooltipText().ToString(), 0, PersistentIntNodeTemplate->GetKeywords());
				IntAction->NodeTemplate = PersistentIntNodeTemplate;

				// Persistent Bool
				UK2Node_TemporaryVariable* PersistentBoolNodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_TemporaryVariable>();
				PersistentBoolNodeTemplate->VariableType = FEdGraphPinType(K2Schema->PC_Boolean, TEXT(""), NULL, false, false);
				PersistentBoolNodeTemplate->bIsPersistent = true;
				TSharedPtr<FEdGraphSchemaAction_K2NewNode> BoolAction = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::MacroToolsCategory, PersistentBoolNodeTemplate->GetNodeTitle(ENodeTitleType::ListView), PersistentBoolNodeTemplate->GetTooltipText().ToString(), 0, PersistentBoolNodeTemplate->GetKeywords());
				BoolAction->NodeTemplate = PersistentBoolNodeTemplate;
			}
		}

		FBlueprintPaletteListBuilder PaletteBuilder(ContextMenuBuilder.Blueprint);
		// Show viable macros to instance
		GetMacroTools(PaletteBuilder, bAllowImpureFuncs, ContextMenuBuilder.CurrentGraph);
		ContextMenuBuilder.Append(PaletteBuilder);

		// Show messages that can potentially be called
		if (bAllowImpureFuncs) // Note: Bad naming, but intentional; messages could have any number of arbitrary side effects
		{
			for ( TObjectIterator<UClass> It; It; ++It )
			{
				GetInterfaceMessages(*It, TEXT("Interface Messages"), ContextMenuBuilder);
			}
		}
		GetCommentAction(ContextMenuBuilder.Blueprint, ContextMenuBuilder);
		GetDocumentationAction(ContextMenuBuilder.Blueprint, ContextMenuBuilder);

		{
			UK2Node* NodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_Select>();
			TSharedPtr<FEdGraphSchemaAction_K2NewNode> SelectionAction = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::UtilitiesCategory, NodeTemplate->GetNodeTitle(ENodeTitleType::ListView), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
			SelectionAction->NodeTemplate = NodeTemplate;
		}

		FEditorDelegates::OnBlueprintContextMenuCreated.Broadcast(ContextMenuBuilder);

		GetEnumUtilitiesNodes(ContextMenuBuilder, true, true, true, true);
	}
}

//------------------------------------------------------------------------------

struct FClassDynamicCastMenuUtils
{
	static bool CanCastToClass(const UClass* TestClass, const UEdGraphSchema_K2* K2Schema)
	{
		check(TestClass && K2Schema);
		const bool bIsSkeletonClass = FKismetEditorUtilities::IsClassABlueprintSkeleton(TestClass);
		const bool bProperClass = TestClass->HasAnyFlags(RF_Public) && !TestClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists);
		return !bIsSkeletonClass && bProperClass;
	}

	static void SpawnNode(UClass* TargetType, FBlueprintGraphActionListBuilder& ContextMenuBuilder)
	{
		static const FString CastCategory = K2ActionCategories::RootFunctionCategory + "|" + K2ActionCategories::CastingCategory;
		UK2Node_DynamicCast* CastNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_ClassDynamicCast>();
		CastNode->TargetType = TargetType;

		TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, CastCategory, CastNode->GetNodeTitle(ENodeTitleType::ListView), TEXT("Dynamic cast to a specific type"), 0, CastNode->GetKeywords());
		Action->NodeTemplate = CastNode;
	}

	static void GetClassDynamicCastNodes(FBlueprintGraphActionListBuilder& ContextMenuBuilder, const UEdGraphSchema_K2* K2Schema)
	{
		check(ContextMenuBuilder.FromPin && K2Schema);
		const UEdGraphPin& FromPin = *ContextMenuBuilder.FromPin;
		if ((FromPin.PinType.PinCategory == K2Schema->PC_Class))
		{
			UClass* PinClass = Cast<UClass>(FromPin.PinType.PinSubCategoryObject.Get());
			if (FromPin.Direction == EGPD_Output)
			{
				for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
				{
					UClass* TestClass = *ClassIt;
					const bool bCanCastFromPin = !PinClass || ((TestClass != PinClass) && TestClass->IsChildOf(PinClass));
					if (bCanCastFromPin && CanCastToClass(TestClass, K2Schema))
					{
						SpawnNode(TestClass, ContextMenuBuilder);
					}
				}
			}
			else if (PinClass 
				&& (FromPin.Direction == EGPD_Input) 
				&& (UObject::StaticClass() != PinClass) 
				&& CanCastToClass(PinClass, K2Schema))
			{
				SpawnNode(PinClass, ContextMenuBuilder);
			}
		}
	}
};

void FK2ActionMenuBuilder::GetPinAllowedNodeTypes(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const
{
	const UBlueprint* Blueprint = ContextMenuBuilder.Blueprint;
	UClass* Class = (Blueprint != NULL) ? Blueprint->SkeletonGeneratedClass : NULL;
	if (Class == NULL)
	{
		UE_LOG(LogBlueprint, Log, TEXT("GetPinAllowedNodeTypes: Failed to find class from Blueprint '%s'"), *Blueprint->GetPathName());
	}
	else
	{
		const UEdGraphPin& FromPin = *ContextMenuBuilder.FromPin;
		const EGraphType GraphType = K2Schema->GetGraphType(ContextMenuBuilder.CurrentGraph);
		const bool bAllowImpureFuncs = K2Schema->DoesGraphSupportImpureFunctions(ContextMenuBuilder.CurrentGraph);

		if ((FromPin.PinType.PinCategory == K2Schema->PC_Exec) && bAllowImpureFuncs)
		{
			GetRequestedAction<UK2Node_IfThenElse>(ContextMenuBuilder, K2ActionCategories::FlowControlCategory);
			GetRequestedAction<UK2Node_ExecutionSequence>(ContextMenuBuilder, K2ActionCategories::FlowControlCategory);
			GetRequestedAction<UK2Node_MultiGate>(ContextMenuBuilder, K2ActionCategories::FlowControlCategory);

			FBlueprintPaletteListBuilder PaletteBuilder(ContextMenuBuilder.Blueprint);
			GetSwitchMenuItems(PaletteBuilder, K2ActionCategories::FlowControlCategory, ContextMenuBuilder.FromPin);
			ContextMenuBuilder.Append(PaletteBuilder);

			// For Exec types, just show all imperative functions in current context 
			const bool bRequireDesiredPinType = true;
			const bool bWantBindableDelegates = (GraphType == GT_Ubergraph) || (GraphType == GT_Function) ||
				((bAllowUnsafeCommands) && (Blueprint->BlueprintType == BPTYPE_MacroLibrary));

			GetFuncNodesForClass(ContextMenuBuilder, Class, Class, ContextMenuBuilder.CurrentGraph, UEdGraphSchema_K2::FT_Imperative | UEdGraphSchema_K2::FT_Protected, K2ActionCategories::RootFunctionCategory);
			GetFuncNodesForClass(ContextMenuBuilder, NULL, Class, ContextMenuBuilder.CurrentGraph, UEdGraphSchema_K2::FT_Imperative | UEdGraphSchema_K2::FT_Protected, K2ActionCategories::RootFunctionCategory);

			GetVariableGettersSettersForClass(ContextMenuBuilder, Class, FromPin.PinType, true, false, true, bWantBindableDelegates, bRequireDesiredPinType);

			if (FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint))
			{
				GetFunctionCallsOnSelectedActors(ContextMenuBuilder);
			}
			else if (FBlueprintEditorUtils::IsActorBased(Blueprint))
			{
				GetAddComponentActionsUsingSelectedAssets(ContextMenuBuilder);
				GetAddComponentClasses(ContextMenuBuilder.Blueprint, ContextMenuBuilder);
			}

			if (!K2Schema->IsConstructionScript(ContextMenuBuilder.CurrentGraph))
			{
				AddSpawnActorNodeAction(ContextMenuBuilder, K2ActionCategories::RootFunctionCategory);

				if (Class->IsChildOf(AActor::StaticClass()))
				{
					const bool bIncludeEvents = FromPin.Direction == EGPD_Input;
					GetInputNodes(ContextMenuBuilder, bIncludeEvents);
				}
			}
			
			AddGetDataTableRowNodeAction(ContextMenuBuilder, K2ActionCategories::RootFunctionCategory);

			// for output pins, add macro flow control as a connection option
			if ( FromPin.Direction == EGPD_Output )
			{
				const bool bWantsOutputs = false;
				GetPinAllowedMacros(ContextMenuBuilder, Class, FromPin.PinType, bWantsOutputs, bAllowImpureFuncs);
			}

			// For input pins, add Events as a connection option
			if( FromPin.Direction == EGPD_Input )
			{
				GetEventsForBlueprint(PaletteBuilder);
				ContextMenuBuilder.Append(PaletteBuilder);
			}

			GetAllInterfaceMessageActions(ContextMenuBuilder);		
		}
		else if ((FromPin.PinType.PinCategory == K2Schema->PC_Object) || (FromPin.PinType.PinCategory == K2Schema->PC_Interface))
		{
			// For Object types, look for functions we can call on that object
			UClass* PinClass = Cast<UClass>(FromPin.PinType.PinSubCategoryObject.Get());
			if (PinClass)
			{
				GetInterfaceMessages(PinClass, TEXT("Interface Messages"), ContextMenuBuilder);

				if (FromPin.Direction == EGPD_Output)
				{
					const bool bWantBindableDelegates = (GraphType == GT_Ubergraph) || (GraphType == GT_Function) ||
						((bAllowUnsafeCommands) && (Blueprint->BlueprintType == BPTYPE_MacroLibrary));

					if( !FromPin.PinType.bIsArray )
					{
						FEdGraphPinType DontCareType;
						const bool bRequireDesiredPinType = false;

						GetVariableGettersSettersForClass(ContextMenuBuilder, PinClass, DontCareType, false, true, bAllowImpureFuncs, bWantBindableDelegates, bRequireDesiredPinType);

						// Get functions you can call on this kind of pin
						uint32 FunctionTypes = UEdGraphSchema_K2::FT_Const;
						if(bAllowImpureFuncs)
						{
							FunctionTypes |= UEdGraphSchema_K2::FT_Imperative;
						}

						GetFuncNodesForClass(ContextMenuBuilder, PinClass, PinClass, ContextMenuBuilder.CurrentGraph, FunctionTypes, K2ActionCategories::RootFunctionCategory);

						if( FromPin.PinType.bIsReference )
						{
							GetReferenceSetterItems(ContextMenuBuilder);
						}
					}
					else if(bAllowImpureFuncs && PinClass)
					{
						GetFuncNodesForClass(ContextMenuBuilder, PinClass, PinClass, ContextMenuBuilder.CurrentGraph, UEdGraphSchema_K2::FT_Imperative, K2ActionCategories::CallFuncForEachCategory, FFunctionTargetInfo(), false, true);
						if(bWantBindableDelegates)
						{
							GetEventDispatcherNodesForClass(ContextMenuBuilder, PinClass, false);
						}
					}

					GetFuncNodesWithPinType(ContextMenuBuilder, NULL, Class, FromPin.PinType, false, !bAllowImpureFuncs);
					GetPinAllowedMacros(ContextMenuBuilder, Class, FromPin.PinType, false, bAllowImpureFuncs);
				}

				if (FromPin.Direction == EGPD_Input)
				{
					const bool bRequireDesiredPinType = true;
					const bool bWantBindableDelegates = false;

					GetVariableGettersSettersForClass(ContextMenuBuilder, Class, FromPin.PinType, true, true, false, bWantBindableDelegates, bRequireDesiredPinType);
					GetFuncNodesWithPinType(ContextMenuBuilder, Class, Class, FromPin.PinType, true, !bAllowImpureFuncs);
					GetFuncNodesWithPinType(ContextMenuBuilder, NULL, Class, FromPin.PinType, true, !bAllowImpureFuncs);
					GetPinAllowedMacros(ContextMenuBuilder, Class, FromPin.PinType, true, bAllowImpureFuncs);
				}

				// Dynamic casts
				{
					FString const CastCategory = K2ActionCategories::RootFunctionCategory + "|" + K2ActionCategories::CastingCategory;

					for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
					{
						UClass* TestClass = *ClassIt;
						bool bIsSkeletonClass = FKismetEditorUtilities::IsClassABlueprintSkeleton(TestClass);

						if (TestClass->HasAnyFlags(RF_Public) && !bIsSkeletonClass && !TestClass->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists))
						{
							bool bIsCastable = false;
							if (TestClass != PinClass)
							{
								// from Parent(pin) to Child(test)  (output); offer cast
								// from Parent(test) to Child(pin)  (input); offer cast
								if (FromPin.Direction == EGPD_Output)
								{
									bIsCastable = TestClass->IsChildOf(PinClass);
								}
								else if (FromPin.Direction == EGPD_Input)
								{
									bIsCastable = PinClass->IsChildOf(TestClass);
								}
							}

							if (bIsCastable)
							{
								UK2Node_DynamicCast* CastNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_DynamicCast>();
								CastNode->TargetType = TestClass;

								TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder,  CastCategory, CastNode->GetNodeTitle(ENodeTitleType::ListView), TEXT("Dynamic cast to a specific type"), 0, CastNode->GetKeywords());
								Action->NodeTemplate = CastNode;
							}
						}
					}
				}
			}
		}
		else if (FromPin.PinType.PinCategory != K2Schema->PC_Wildcard)
		{
			const bool bWantOutputs = FromPin.Direction == EGPD_Input;

			// List variables that match the type
			const bool bWantGetters = bWantOutputs;
			const bool bWantSetters = (FromPin.Direction == EGPD_Output) && bAllowImpureFuncs;
			const bool bRequirePinType = true;
			GetVariableGettersSettersForClass(ContextMenuBuilder, Class, FromPin.PinType, true, bWantGetters, bWantSetters, false, bRequirePinType);

			// For POD types, look for a mix of callable and pure functions (in current context and in function libraries) that have an input/output of the correct type
			GetFuncNodesWithPinType(ContextMenuBuilder, Class, Class, FromPin.PinType, bWantOutputs, !bAllowImpureFuncs);
			GetFuncNodesWithPinType(ContextMenuBuilder, NULL, Class, FromPin.PinType, bWantOutputs, !bAllowImpureFuncs);

			// Add if flow control block to pin for bool pin type
			if ((FromPin.PinType.PinCategory == K2Schema->PC_Boolean) && (bWantSetters))
			{
				GetRequestedAction<UK2Node_IfThenElse>(ContextMenuBuilder, K2ActionCategories::FlowControlCategory);
			}

			// Add in any suitable macros for pin
			GetPinAllowedMacros(ContextMenuBuilder, Class, FromPin.PinType, bWantOutputs, bAllowImpureFuncs);

			if (FromPin.Direction == EGPD_Output)
			{
				// Try looking for switches that switch on this pin type
				FBlueprintPaletteListBuilder PaletteBuilder(ContextMenuBuilder.Blueprint);
				GetSwitchMenuItems(PaletteBuilder, K2ActionCategories::FlowControlCategory, ContextMenuBuilder.FromPin);
				ContextMenuBuilder.Append(PaletteBuilder);

				// If this is a reference pin, grab a setter node as well
				if( FromPin.PinType.bIsReference && !FromPin.PinType.bIsArray)
				{
					GetReferenceSetterItems(ContextMenuBuilder);
				}
			}
			else if(FromPin.Direction == EGPD_Input)
			{
				if(FromPin.PinType.bIsArray)
				{
					GetRequestedAction<UK2Node_MakeArray>(ContextMenuBuilder, K2ActionCategories::ArrayCategory);
				}
			}

			if ((K2Schema->PC_Struct == FromPin.PinType.PinCategory) && !FromPin.PinType.bIsArray)
			{
				UScriptStruct* PinStruct = Cast<UScriptStruct>(FromPin.PinType.PinSubCategoryObject.Get());
				if( PinStruct )
				{
					const bool bCanBeMade = UK2Node_MakeStruct::CanBeMade(PinStruct);
					if ((FromPin.Direction == EGPD_Output) && UK2Node_BreakStruct::CanBeBroken(PinStruct))
					{
						UK2Node_BreakStruct* NodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_BreakStruct>();
						NodeTemplate->StructType = PinStruct;
						TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, FString(), NodeTemplate->GetNodeTitle(ENodeTitleType::ListView), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
						Action->NodeTemplate = NodeTemplate;
					}
					else if ((FromPin.Direction == EGPD_Input) && bCanBeMade)
					{
						UK2Node_MakeStruct* NodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_MakeStruct>();
						NodeTemplate->StructType = PinStruct;
						TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, FString(), NodeTemplate->GetNodeTitle(ENodeTitleType::ListView), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
						Action->NodeTemplate = NodeTemplate;
					}

					if ((FromPin.Direction == EGPD_Output) && bCanBeMade)
					{
						UK2Node_SetFieldsInStruct* NodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_SetFieldsInStruct>();
						NodeTemplate->StructType = PinStruct;
						TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, FString(), NodeTemplate->GetNodeTitle(ENodeTitleType::ListView), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
						Action->NodeTemplate = NodeTemplate;
					}
				}

			}

			if ((FromPin.Direction == EGPD_Input) && (FromPin.PinType.PinCategory == K2Schema->PC_Delegate))
			{
				UK2Node_CreateDelegate* NodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_CreateDelegate>();
				TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, FString(), NodeTemplate->GetNodeTitle(ENodeTitleType::ListView), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
				Action->NodeTemplate = NodeTemplate;

				UFunction* Signature = FMemberReference::ResolveSimpleMemberReference<UFunction>(FromPin.PinType.PinSubCategoryMemberReference);
				const bool bSupportsEventGraphs = (Blueprint && FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint));
				const bool bAllowEvents = (GraphType == GT_Ubergraph);
				if(Signature && bSupportsEventGraphs && bAllowEvents)
				{
					TSharedPtr<FEdGraphSchemaAction_EventFromFunction> DelegateEvent(new FEdGraphSchemaAction_EventFromFunction( FString(), LOCTEXT("AddCustomEventForDispatcher", "Add Custom Event for Dispatcher"), LOCTEXT("AddCustomEventForDispatcher_Tooltip", "Add a new event for given Dispatcher").ToString(), 0));
					ContextMenuBuilder.AddAction( DelegateEvent );
					DelegateEvent->SignatureFunction = Signature;
				}
			}
		}

		FClassDynamicCastMenuUtils::GetClassDynamicCastNodes(ContextMenuBuilder, K2Schema);

		if ((FromPin.Direction == EGPD_Input) && (FromPin.PinType.PinCategory == K2Schema->PC_Int))
		{
			GetEnumUtilitiesNodes(ContextMenuBuilder, true, false, false, true);
		}
		else if(FromPin.PinType.PinCategory == K2Schema->PC_Text)
		{
			GetRequestedAction<UK2Node_FormatText>(ContextMenuBuilder, K2ActionCategories::ArrayCategory);
		}
		else if (FromPin.PinType.PinCategory == K2Schema->PC_Float ||
			(FromPin.PinType.PinCategory == K2Schema->PC_Struct &&
			FromPin.PinType.PinSubCategoryObject.IsValid() && 
			(FromPin.PinType.PinSubCategoryObject.Get()->GetName() == TEXT("Vector") ||
			FromPin.PinType.PinSubCategoryObject.Get()->GetName() == TEXT("Rotator") ||
			FromPin.PinType.PinSubCategoryObject.Get()->GetName() == TEXT("Transform"))))
		{
			GetRequestedAction<UK2Node_EaseFunction>(ContextMenuBuilder, K2ActionCategories::InterpCategory);
		}

		UEnum* Enum = Cast<UEnum>(FromPin.PinType.PinSubCategoryObject.Get());
		if ((FromPin.PinType.PinCategory == K2Schema->PC_Byte) && Enum)
		{
			if (FromPin.Direction == EGPD_Output)
			{
				FString ConversionsCategory = TEXT("Conversions");
				GetRequestedAction<UK2Node_GetEnumeratorName>(ContextMenuBuilder, K2ActionCategories::NameCategory);
				GetRequestedAction<UK2Node_GetEnumeratorNameAsString>(ContextMenuBuilder, K2ActionCategories::StringCategory);
			}
			if (FromPin.Direction == EGPD_Input)
			{
				GetEnumUtilitiesNodes(ContextMenuBuilder, false, true, true, false, Enum);
			}
		}
		else if ((FromPin.PinType.PinCategory == K2Schema->PC_Byte) && !Enum && FromPin.Direction == EGPD_Input)
		{
			GetEnumUtilitiesNodes(ContextMenuBuilder, false, false, false, true);
		}
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetFuncNodesForClass(FGraphActionListBuilderBase& ListBuilder, const UClass* Class, const UClass* OwnerClass, const UEdGraph* DestGraph, uint32 FunctionTypes, const FString& BaseCategory, const FFunctionTargetInfo& TargetInfo, bool bShowInherited, bool bCalledForEach) const
{
	if (Class != NULL)
	{
		bool bLatentFuncs = true;
		bool bIsConstructionScript = false;

		if(DestGraph != NULL)
		{
			bLatentFuncs = (K2Schema->GetGraphType(DestGraph) == GT_Ubergraph);
			bIsConstructionScript = K2Schema->IsConstructionScript(DestGraph);
		}

		// Find all functions on the current class
		EFieldIteratorFlags::SuperClassFlags SuperClassFlag = bShowInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;
		for (TFieldIterator<UFunction> FunctionIt(Class, SuperClassFlag); FunctionIt; ++FunctionIt)
		{
			UFunction* Function = *FunctionIt;
			if( K2Schema->CanFunctionBeUsedInGraph(Class, Function, DestGraph, FunctionTypes, bCalledForEach, TargetInfo) )
			{
				// Check if the function is hidden
				const bool bFunctionHidden = FObjectEditorUtils::IsFunctionHiddenFromClass(Function, Class);
				if (!bFunctionHidden)
				{
					FK2ActionMenuBuilder::AddSpawnInfoForFunction(Function, false, TargetInfo, FMemberReference(), BaseCategory, K2Schema->AG_LevelReference, ListBuilder, bCalledForEach);
				}
			}
		}

		// If looking for 'inherited functions', search for object properties we can call functions _on_
		if(bShowInherited)
		{
			for (TFieldIterator<UObjectProperty> PropIt(Class, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
			{
				UObjectProperty* ObjProp = *PropIt;
				// See if this object property is exposed to BP and has function categories we can call on it
				FString ExposeFuncCatsString = ObjProp->GetMetaData(FBlueprintMetadata::MD_ExposeFunctionCategories);
				if(ObjProp->HasAllPropertyFlags(CPF_BlueprintVisible) && ExposeFuncCatsString.Len() > 0)
				{
					TArray<FString> ExposeFuncCats;
					ExposeFuncCatsString.ParseIntoArray(&ExposeFuncCats, TEXT( "," ), true);

					// Look for functions in those categories
					UClass* ObjClass = ObjProp->PropertyClass;
					for (TFieldIterator<UFunction> FunctionIt(ObjClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
					{
						UFunction* Function = *FunctionIt;
						// Get category of function and see if its in our list
						FString FunctionCategory = Function->GetMetaData(FBlueprintMetadata::MD_FunctionCategory);
						if(ExposeFuncCats.Contains(FunctionCategory))
						{
							// It is, create menu option for it
							FMemberReference MemberVarRef;
							MemberVarRef.SetFromField<UProperty>(ObjProp, false);

							FK2ActionMenuBuilder::AddSpawnInfoForFunction(Function, false, TargetInfo, MemberVarRef, BaseCategory, K2Schema->AG_LevelReference, ListBuilder);
						}
					}
				}
			}
		}
	}
	else
	{
		// Find all functions in each function library
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			UClass* TestClass = *ClassIt;
			const bool bTransient = (GetTransientPackage() == TestClass->GetOutermost());
			const UClass* AuthoritativeOwnerClass = OwnerClass ? const_cast<UClass*>(OwnerClass)->GetAuthoritativeClass() : NULL;
			const bool bCanShowInOwnerClass = !AuthoritativeOwnerClass || !TestClass->IsChildOf(AuthoritativeOwnerClass);
			const bool bIsProperFucntionLibrary = TestClass->IsChildOf(UBlueprintFunctionLibrary::StaticClass()) && CanUseLibraryFunctionsForScope(TestClass, OwnerClass);
			const bool bIsSkeletonClass = FKismetEditorUtilities::IsClassABlueprintSkeleton(TestClass);
			if (bCanShowInOwnerClass && bIsProperFucntionLibrary && !bTransient && !bIsSkeletonClass)
			{
				GetFuncNodesForClass(ListBuilder, TestClass, OwnerClass, DestGraph, FunctionTypes, BaseCategory, TargetInfo);
			}
		}
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetAllActionsForClass(FBlueprintPaletteListBuilder& PaletteBuilder, const UClass* InClass, bool bShowInherited, bool bShowProtected, bool bShowVariables, bool bShowDelegates) const
{
	UBlueprint* ClassBlueprint = UBlueprint::GetBlueprintFromClass(InClass);
	// @TODO Don't show other blueprints yet...
	if(!InClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) && (ClassBlueprint == NULL))
	{
		int32 FuncFlags = UEdGraphSchema_K2::FT_Const | UEdGraphSchema_K2::FT_Imperative | UEdGraphSchema_K2::FT_Pure;
		if(bShowProtected)
		{
			FuncFlags |= UEdGraphSchema_K2::FT_Protected;
		}

		GetFuncNodesForClass(PaletteBuilder, InClass, InClass, NULL, FuncFlags, TEXT(""), FFunctionTargetInfo(), bShowInherited);

		// Find vars
		if (bShowVariables)
		{
			EFieldIteratorFlags::SuperClassFlags SuperClassFlag = bShowInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;
			for (TFieldIterator<UProperty> PropertyIt(InClass, SuperClassFlag); PropertyIt; ++PropertyIt)
			{
				UProperty* Property = *PropertyIt;

				if (K2Schema->CanUserKismetAccessVariable(Property, InClass, UEdGraphSchema_K2::CannotBeDelegate))
				{
					const FString PropertyTooltip = Property->GetToolTipText().ToString();
					const FString PropertyDesc = GetDefault<UEditorStyleSettings>()->bShowFriendlyNames ? UEditorEngine::GetFriendlyName(Property) : Property->GetName();
					const FString PropertyCategory = FObjectEditorUtils::GetCategory(Property);

					TSharedPtr<FEdGraphSchemaAction_K2Var> NewVarAction = TSharedPtr<FEdGraphSchemaAction_K2Var>(new FEdGraphSchemaAction_K2Var(PropertyCategory, FText::FromString(PropertyDesc), PropertyTooltip, 0));
					NewVarAction->SetVariableInfo(Property->GetFName(), InClass);

					PaletteBuilder.AddAction(NewVarAction);
				}
			}
		}	

		if(bShowDelegates)
		{
			const EFieldIteratorFlags::SuperClassFlags SuperClassFlag = bShowInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;
			for (TFieldIterator<UMulticastDelegateProperty> PropertyIt(InClass, SuperClassFlag); PropertyIt; ++PropertyIt)
			{
				UMulticastDelegateProperty* Property = *PropertyIt;
				const bool bHidden = FObjectEditorUtils::IsVariableCategoryHiddenFromClass(Property, InClass);
				const bool bShow = !Property->HasAnyPropertyFlags(CPF_Parm) && Property->HasAnyPropertyFlags(CPF_BlueprintAssignable | CPF_BlueprintCallable) && !bHidden;
				if(bShow)
				{
					const FString PropertyTooltip = Property->GetToolTipText().ToString();
					const FString PropertyDesc = GetDefault<UEditorStyleSettings>()->bShowFriendlyNames ? UEditorEngine::GetFriendlyName(Property) : Property->GetName();

					FString PropertyCategory = FObjectEditorUtils::GetCategory(Property);
					if(PropertyCategory.IsEmpty())
					{
						PropertyCategory = K2ActionCategories::DefaultEventDispatcherCategory;
					}
					PropertyCategory = FName::NameToDisplayString(PropertyCategory, /*bIsBool =*/false);

					TSharedPtr<FEdGraphSchemaAction_K2Delegate> NewFuncAction = MakeShareable(new FEdGraphSchemaAction_K2Delegate(PropertyCategory, FText::FromString(PropertyDesc), PropertyTooltip, 0));
					NewFuncAction->SetDelegateInfo(Property->GetFName(), InClass);

					UClass* CurrentPropertyOwner = Cast<UClass>(Property->GetOuterUField());
					if(CurrentPropertyOwner && ClassBlueprint && ClassBlueprint == CurrentPropertyOwner->ClassGeneratedBy)
					{
						NewFuncAction->EdGraph = FBlueprintEditorUtils::GetDelegateSignatureGraphByName(ClassBlueprint, Property->GetFName());
					}
					else
					{
						NewFuncAction->EdGraph = NULL;
					}

					PaletteBuilder.AddAction(NewFuncAction);
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetSwitchMenuItems(FBlueprintPaletteListBuilder& ActionMenuBuilder, FString const& RootCategory, UEdGraphPin const* PinContext) const
{
	FString const SwitchControlCategory = RootCategory + K2ActionCategories::SubCategoryDelim + K2ActionCategories::SwitchSubCategory;

	// Determine if we have an enum restriction
	UEnum* RequiredEnum = NULL;
	if (PinContext != NULL)
	{
		RequiredEnum = Cast<UEnum>(PinContext->PinType.PinSubCategoryObject.Get());
	}

	// Add integer switch
	if ((PinContext == NULL) || (PinContext->PinType.PinCategory == K2Schema->PC_Int)) //@TODO: Give byte->int casting a shot
	{
		GetRequestedAction<UK2Node_SwitchInteger>(ActionMenuBuilder, SwitchControlCategory);
	}

	// Add string switch
	if ((PinContext == NULL) || (PinContext->PinType.PinCategory == K2Schema->PC_String))
	{
		GetRequestedAction<UK2Node_SwitchString>(ActionMenuBuilder, SwitchControlCategory);
	}

	// Add Name switch
	if ((PinContext == NULL) || (PinContext->PinType.PinCategory == K2Schema->PC_Name))
	{
		GetRequestedAction<UK2Node_SwitchName>(ActionMenuBuilder, SwitchControlCategory);
	}

	// Add a switch entry for any enum types that pass the filter
	if ((PinContext == NULL) || (RequiredEnum != NULL))
	{
		for (TObjectIterator<UEnum> EnumIt; EnumIt; ++EnumIt)
		{
			UEnum* CurrentEnum = *EnumIt;

			const bool bIsBlueprintType = UEdGraphSchema_K2::IsAllowableBlueprintVariableType(CurrentEnum);
			const bool bMatchesEnumFilter = (RequiredEnum == NULL) || (CurrentEnum == RequiredEnum);
			if (bIsBlueprintType && bMatchesEnumFilter)
			{
				UK2Node_SwitchEnum* EnumTemplate = ActionMenuBuilder.CreateTemplateNode<UK2Node_SwitchEnum>();
				EnumTemplate->SetEnum(CurrentEnum);

				UK2Node* NodeTemplate = EnumTemplate;
				TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ActionMenuBuilder, SwitchControlCategory, NodeTemplate->GetNodeTitle(ENodeTitleType::ListView), NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
				Action->NodeTemplate = NodeTemplate;
			}
		}
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetAnimNotifyMenuItems(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const
{
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(ContextMenuBuilder.CurrentGraph));
	if (K2Schema->DoesSupportAnimNotifyActions() && (AnimBlueprint != NULL))
	{
		// find all animnotifies it can use
		if (AnimBlueprint->TargetSkeleton)
		{
			for (int32 I = 0; I < AnimBlueprint->TargetSkeleton->AnimationNotifies.Num(); ++I)
			{
				FName NotifyName = AnimBlueprint->TargetSkeleton->AnimationNotifies[I];
				FString Label = NotifyName.ToString();

				TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::AnimNotifyCategory, FText::FromString(Label), TEXT(""));

				UK2Node_Event* EventNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_Event>();
				FString SignatureName = FString::Printf(TEXT("AnimNotify_%s"), *Label);
				EventNode->EventSignatureName = FName(*SignatureName);
				EventNode->EventSignatureClass = UAnimInstance::StaticClass();
				EventNode->CustomFunctionName = EventNode->EventSignatureName;
				Action->NodeTemplate = EventNode;
			}
		}

		// State machine events
		if (UAnimBlueprintGeneratedClass* GeneratedClass = AnimBlueprint->GetAnimBlueprintGeneratedClass())
		{
			for(int32 NotifyIdx=0; NotifyIdx < GeneratedClass->AnimNotifies.Num(); NotifyIdx++)
			{
				FName NotifyName = GeneratedClass->AnimNotifies[NotifyIdx].NotifyName;
				if(NotifyName != NAME_None)
				{
					FString Label = NotifyName.ToString();

					TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::StateMachineCategory, FText::FromString(Label), TEXT(""));

					UK2Node_Event* EventNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_Event>();
					FString SignatureName = FString::Printf(TEXT("AnimNotify_%s"), *Label);
					EventNode->EventSignatureName = FName(*SignatureName);
					EventNode->EventSignatureClass = UAnimInstance::StaticClass();
					EventNode->CustomFunctionName = EventNode->EventSignatureName;
					Action->NodeTemplate = EventNode;
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetVariableGettersSettersForClass(FBlueprintGraphActionListBuilder& ContextMenuBuilder, const UClass* Class, bool bSelfContext, bool bIncludeDelegates) const
{
	FEdGraphPinType DontCareType;
	const bool bWantGetters = true;
	const bool bWantSetters = true;
	const bool bRequireDesiredPinType = false;

	GetVariableGettersSettersForClass(ContextMenuBuilder, Class, DontCareType, bSelfContext, bWantGetters, bWantSetters, bIncludeDelegates, bRequireDesiredPinType);
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetVariableGettersSettersForClass(FBlueprintGraphActionListBuilder& ContextMenuBuilder, const UClass* Class, const FEdGraphPinType& DesiredPinType, bool bSelfContext, bool bWantGetters, bool bWantSetters, bool bWantDelegates, bool bRequireDesiredPinType) const
{
	check(Class);
	FString GetString(TEXT("Get "));
	FString SetString(TEXT("Set "));
	FString SelfString(TEXT("Get a reference to self"));
	FString SelfTooltip(TEXT("Get a reference to this object"));
	FString GetStructMemberString(TEXT("Get members in "));
	FString SetStructMemberString(TEXT("Set members in "));
	FString MakeStructString(TEXT("Make "));

	if(Class->HasAnyClassFlags(CLASS_Const))
	{
		bWantSetters=false;
	}

	for (TFieldIterator<UProperty> PropertyIt(Class, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;
		UClass* PropertyClass = CastChecked<UClass>(Property->GetOuter());
		const bool bIsDelegate = Property->IsA(UMulticastDelegateProperty::StaticClass());

		if (UEdGraphSchema_K2::CanUserKismetAccessVariable(Property, Class, UEdGraphSchema_K2::VariablesAndDelegates))
		{
			//@TODO: To properly filter protected and private variables, we need to know the calling context too

			FString Category = FObjectEditorUtils::GetCategory(Property);
			FString PropertyTooltip = Property->GetToolTipText().ToString();
			if ( PropertyTooltip.IsEmpty() )
			{
				PropertyTooltip = FString::Printf(TEXT("Category %s"), *Category);
			}
			const FString PropertyDescription = GetDefault<UEditorStyleSettings>()->bShowFriendlyNames ? UEditorEngine::GetFriendlyName(Property) : Property->GetName();

			FEdGraphPinType PropertyTypeAsPinType;
			bool bTypesMatchForRead = !bRequireDesiredPinType;
			bool bTypesMatchForWrite = !bRequireDesiredPinType;
			if (bRequireDesiredPinType && K2Schema->ConvertPropertyToPinType(Property, /*out*/ PropertyTypeAsPinType))
			{
				// reading (property out -> pin in)
				bTypesMatchForRead = K2Schema->ArePinTypesCompatible(PropertyTypeAsPinType, DesiredPinType, Class);

				// writing (pin out -> property in)
				// Always allow exec pins to write
				bTypesMatchForWrite = DesiredPinType.PinCategory == K2Schema->PC_Exec || K2Schema->ArePinTypesCompatible(DesiredPinType, PropertyTypeAsPinType, Class);
			}

			FString FullCategoryName = K2ActionCategories::VariablesCategory;
			if(Category.IsEmpty() == false)
			{
				FullCategoryName += K2ActionCategories::SubCategoryDelim;
				FullCategoryName += Category;
			}

			const bool bIsReadOnly = FBlueprintEditorUtils::IsPropertyReadOnlyInCurrentBlueprint(ContextMenuBuilder.Blueprint, Property);
			const bool bCanRead = bWantGetters && bTypesMatchForRead;
			const bool bCanWrite = bWantSetters && bTypesMatchForWrite && !bIsReadOnly;
			const bool bCanModifyDelegate = !bRequireDesiredPinType && !bIsReadOnly;

			if (!bIsDelegate)
			{
				// Variable getters and setters
				if (bCanRead)
				{
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> ReadVar = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, FullCategoryName, FText::FromString(GetString + PropertyDescription), PropertyTooltip);

					UK2Node_VariableGet* VarGetNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_VariableGet>();
					VarGetNode->SetFromProperty(Property, bSelfContext);
					ReadVar->NodeTemplate = VarGetNode;
				}

				if (bCanWrite)
				{
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> WriteVar = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, FullCategoryName, FText::FromString(SetString + PropertyDescription), PropertyTooltip);

					UK2Node_VariableSet* VarSetNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_VariableSet>();
					VarSetNode->SetFromProperty(Property, bSelfContext);
					WriteVar->NodeTemplate = VarSetNode;
				}
			}
		}
	}

	// Add any local variables to the list as well
	UEdGraph* FunctionGraph = FBlueprintEditorUtils::GetTopLevelGraph(ContextMenuBuilder.CurrentGraph);
	TArray<UK2Node_FunctionEntry*> FunctionEntryNodes;
	FunctionGraph->GetNodesOfClass<UK2Node_FunctionEntry>(FunctionEntryNodes);
	if(FunctionEntryNodes.Num() == 1)
	{
		// Setting them without the property, cannot guarantee that properties in the UFunction are valid as local variables
		for(auto& LocalVariable : FunctionEntryNodes[0]->LocalVariables)
		{
			bool bTypesMatchForRead = !bRequireDesiredPinType;
			bool bTypesMatchForWrite = !bRequireDesiredPinType;
			if (bRequireDesiredPinType)
			{
				// reading (property out -> pin in)
				bTypesMatchForRead = K2Schema->ArePinTypesCompatible(LocalVariable.VarType, DesiredPinType, Class);

				// writing (pin out -> property in)
				// Always allow exec pins to write
				bTypesMatchForWrite = DesiredPinType.PinCategory == K2Schema->PC_Exec || K2Schema->ArePinTypesCompatible(DesiredPinType, LocalVariable.VarType, Class);
			}

			FString Category = LocalVariable.Category.ToString();
			FString PropertyTooltip;
			if(LocalVariable.HasMetaData(TEXT("tooltip")))
			{
				PropertyTooltip = LocalVariable.GetMetaData(TEXT("tooltip"));
			}
			FString FullCategoryName = K2ActionCategories::VariablesCategory;
			if(Category.IsEmpty() == false)
			{
				FullCategoryName += K2ActionCategories::SubCategoryDelim;
				FullCategoryName += Category;
			}

			const FString PropertyDescription = GetDefault<UEditorStyleSettings>()->bShowFriendlyNames ? LocalVariable.FriendlyName : LocalVariable.VarName.ToString();
			const bool bCanRead = bWantGetters && bTypesMatchForRead;
			const bool bCanWrite = bWantSetters && bTypesMatchForWrite;
			{
				// Variable getters and setters
				if (bCanRead)
				{
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> ReadVar = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, FullCategoryName, FText::FromString(GetString + PropertyDescription), PropertyTooltip);

					UK2Node_VariableGet* VarGetNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_VariableGet>();
					VarGetNode->VariableReference.SetLocalMember(LocalVariable.VarName, FunctionGraph->GetName(), LocalVariable.VarGuid);
					ReadVar->NodeTemplate = VarGetNode;
				}

				if (bCanWrite)
				{
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> WriteVar = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, FullCategoryName, FText::FromString(SetString + PropertyDescription), PropertyTooltip);

					UK2Node_VariableSet* VarSetNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_VariableSet>();
					VarSetNode->VariableReference.SetLocalMember(LocalVariable.VarName, FunctionGraph->GetName(), LocalVariable.VarGuid);
					WriteVar->NodeTemplate = VarSetNode;
				}
			}
		}
	}

	if (bWantDelegates)
	{
		GetEventDispatcherNodesForClass(ContextMenuBuilder,Class, bSelfContext);
	}

	// Add in self-context variable getter, if needed
	const bool bSelfIsAllowed = !Class->IsChildOf(UBlueprintFunctionLibrary::StaticClass());
	if (bSelfContext && bWantGetters && bSelfIsAllowed)
	{
		TSharedPtr<FEdGraphSchemaAction_K2NewNode> SelfVar = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::VariablesCategory, FText::FromString(SelfString), SelfTooltip);

		UK2Node_Self* SelfNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_Self>();
		SelfVar->NodeTemplate = SelfNode;
		SelfVar->Keywords = SelfNode->GetKeywords();
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetEventDispatcherNodesForClass(FBlueprintGraphActionListBuilder& ContextMenuBuilder, const UClass* Class, bool bSelfContext) const
{
	check(Class);
	static const FString BindString(TEXT("Assign "));

	for (TFieldIterator<UProperty> PropertyIt(Class, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;
		UClass* PropertyClass = CastChecked<UClass>(Property->GetOuter());
		const bool bIsDelegate = Property->IsA(UMulticastDelegateProperty::StaticClass());

		if (UEdGraphSchema_K2::CanUserKismetAccessVariable(Property, Class, UEdGraphSchema_K2::VariablesAndDelegates) && bIsDelegate)
		{
			const FString Category = FObjectEditorUtils::GetCategory(Property);
			FString PropertyTooltip = Property->GetToolTipText().ToString();
			if ( PropertyTooltip.IsEmpty() )
			{
				PropertyTooltip = FString::Printf(TEXT("Category %s"), *Category);
			}

			const bool bCanHaveBlueprintImplementableEventInActorClass = bSelfContext && (PropertyClass == AActor::StaticClass()); 
			// e.g. Actor::OnActorBeginOverlap should not be assigned as a delegate, when one can implement Actor::ReceiveActorBeginOverlap

			if (Property->HasAllPropertyFlags(CPF_BlueprintAssignable) && !bCanHaveBlueprintImplementableEventInActorClass)
			{
				{
					UK2Node_AddDelegate* DelegateEventNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_AddDelegate>();
					DelegateEventNode->SetFromProperty(Property, bSelfContext);

					const EGraphType GraphType = K2Schema->GetGraphType(ContextMenuBuilder.CurrentGraph);
					const bool bSupportsEventGraphs = (ContextMenuBuilder.Blueprint && FBlueprintEditorUtils::DoesSupportEventGraphs(ContextMenuBuilder.Blueprint));
					const bool bAllowEvents = ((GraphType == GT_Ubergraph) && bSupportsEventGraphs);
					if(bAllowEvents)
					{
						const FString PropertyDescription = GetDefault<UEditorStyleSettings>()->bShowFriendlyNames ? UEditorEngine::GetFriendlyName(Property) : Property->GetName();
						TSharedPtr<FEdGraphSchemaAction_K2AssignDelegate> AssignDelegateEvent(new FEdGraphSchemaAction_K2AssignDelegate(K2ActionCategories::DelegateBindingCategory, FText::FromString(BindString + PropertyDescription), PropertyTooltip, 0));
						AssignDelegateEvent->NodeTemplate = DelegateEventNode;
						ContextMenuBuilder.AddAction( AssignDelegateEvent );
					}
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> DelegateEvent = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::DelegateCategory, DelegateEventNode->GetNodeTitle(ENodeTitleType::FullTitle), PropertyTooltip);
					DelegateEvent->NodeTemplate = DelegateEventNode;
				}

				{
					UK2Node_RemoveDelegate* DelegateEventNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_RemoveDelegate>();
					DelegateEventNode->SetFromProperty(Property, bSelfContext);
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> DelegateEvent = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::DelegateCategory, DelegateEventNode->GetNodeTitle(ENodeTitleType::FullTitle), PropertyTooltip);
					DelegateEvent->NodeTemplate = DelegateEventNode;
				}

				{
					UK2Node_ClearDelegate* DelegateEventNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_ClearDelegate>();
					DelegateEventNode->SetFromProperty(Property, bSelfContext);
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> DelegateEvent = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::DelegateCategory, DelegateEventNode->GetNodeTitle(ENodeTitleType::FullTitle), PropertyTooltip);
					DelegateEvent->NodeTemplate = DelegateEventNode;
				}
			}

			if(Property->HasAllPropertyFlags(CPF_BlueprintCallable))
			{
				UK2Node_CallDelegate* DelegateEventNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_CallDelegate>();
				DelegateEventNode->SetFromProperty(Property, bSelfContext);
				TSharedPtr<FEdGraphSchemaAction_K2NewNode> DelegateEvent = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::DelegateCategory, DelegateEventNode->GetNodeTitle(ENodeTitleType::FullTitle), PropertyTooltip);
				DelegateEvent->NodeTemplate = DelegateEventNode;
			}
		}
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetStructActions(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const
{
	// Iterate through all script struct types	
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		// If this is a blueprintable variable add a make and break for it
		if (K2Schema->IsAllowableBlueprintVariableType(Struct))
		{
			if (UK2Node_BreakStruct::CanBeBroken(Struct))
			{
				UK2Node_BreakStruct* BreakNodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_BreakStruct>();
				if( BreakNodeTemplate != NULL )
				{
					BreakNodeTemplate->StructType = Struct;
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::BreakStructCategory, BreakNodeTemplate->GetNodeTitle(ENodeTitleType::ListView), BreakNodeTemplate->GetTooltipText().ToString(), 0, BreakNodeTemplate->GetKeywords());
					Action->NodeTemplate = BreakNodeTemplate;
				}
			}

			if (UK2Node_MakeStruct::CanBeMade(Struct))
			{
				{
					UK2Node_MakeStruct* MakeNodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_MakeStruct>();
					if (MakeNodeTemplate != NULL)
					{
						MakeNodeTemplate->StructType = Struct;
						TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::MakeStructCategory, MakeNodeTemplate->GetNodeTitle(ENodeTitleType::ListView), MakeNodeTemplate->GetTooltipText().ToString(), 0, MakeNodeTemplate->GetKeywords());
						Action->NodeTemplate = MakeNodeTemplate;
					}
				}

				{
					UK2Node_SetFieldsInStruct* SetFieldsTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_SetFieldsInStruct>();
					if (SetFieldsTemplate != NULL)
					{
						SetFieldsTemplate->StructType = Struct;
						TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, K2ActionCategories::MakeStructCategory, SetFieldsTemplate->GetNodeTitle(ENodeTitleType::ListView), SetFieldsTemplate->GetTooltipText().ToString(), 0, SetFieldsTemplate->GetKeywords());
						Action->NodeTemplate = SetFieldsTemplate;
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetAllEventActions(FBlueprintPaletteListBuilder& ActionMenuBuilder) const
{
	// Events
	GetEventsForBlueprint(ActionMenuBuilder);

	// Timeline
	if (FBlueprintEditorUtils::DoesSupportTimelines(ActionMenuBuilder.Blueprint))
	{
		const FString ToolTip = LOCTEXT("Timeline_Tooltip", "Add a Timeline node to modify values over time.").ToString();
		TSharedPtr<FEdGraphSchemaAction_K2AddTimeline> Action = TSharedPtr<FEdGraphSchemaAction_K2AddTimeline>(new FEdGraphSchemaAction_K2AddTimeline(TEXT(""), LOCTEXT("AddTimeline", "Add Timeline..."), ToolTip, 0));
		ActionMenuBuilder.AddAction( Action );

		UK2Node_Timeline* TimelineNodeTemplate = ActionMenuBuilder.CreateTemplateNode<UK2Node_Timeline>();
		TimelineNodeTemplate->TimelineName = FBlueprintEditorUtils::FindUniqueTimelineName(ActionMenuBuilder.Blueprint);
		Action->NodeTemplate = TimelineNodeTemplate;
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetEventsForBlueprint(FBlueprintPaletteListBuilder& ActionMenuBuilder) const
{
	const UBlueprint* Blueprint = ActionMenuBuilder.Blueprint;
	check(ActionMenuBuilder.Blueprint);

	// Keep track of the names of the functions being added for uniqueness
	TArray<FName> ExistingFuncNames;
	// Keep track of the names of the functions that have already been added to the "show event" list
	TArray<FName> ShowEventFuncNames;

	if (Blueprint->SkeletonGeneratedClass != NULL)
	{
		// Grab the list of events to be excluded from the override list
		const FString ExclusionListKeyName = TEXT("KismetHideOverrides");
		TArray<FString> ExcludedEventNames;
		if( Blueprint->ParentClass->HasMetaData(*ExclusionListKeyName) )
		{
			const FString ExcludedEventNameString = Blueprint->ParentClass->GetMetaData(*ExclusionListKeyName);
			ExcludedEventNameString.ParseIntoArray(&ExcludedEventNames, TEXT(","), true);
		}

		// Start the name list by first adding all events already on the graph
		TArray<UK2Node_Event*> AllEvents;
		FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(Blueprint, AllEvents);
		for (auto It = AllEvents.CreateConstIterator(); It; It++)
		{
			ExistingFuncNames.Add((*It)->EventSignatureName);
		}

		for (TFieldIterator<UFunction> FunctionIt(Blueprint->ParentClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
		{
			UFunction* Function = *FunctionIt;

			UClass* FuncClass = CastChecked<UClass>(Function->GetOuter());
			UK2Node_Event* ExistingOverride = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, FuncClass, Function->GetFName());

			// See if it can be placed as an event
			if (K2Schema->FunctionCanBePlacedAsEvent(Function) && !ExcludedEventNames.Contains(Function->GetName()))
			{
				if (ExistingOverride)
				{
					if (ShowEventFuncNames.Find(Function->GetFName()) == INDEX_NONE)
					{
						FK2ActionMenuBuilder::AddEventForFunction(ActionMenuBuilder, Function);	
						ShowEventFuncNames.Add(Function->GetFName());
					}
				}
				else if (ExistingFuncNames.Find(Function->GetFName()) == INDEX_NONE)
				{
					FK2ActionMenuBuilder::AddEventForFunction(ActionMenuBuilder, Function);	
					ExistingFuncNames.Add(Function->GetFName());
				}	
			}
		}
	}

	// Build a list of all interface classes either implemented by this blueprint or through inheritance
	TArray<UClass*> InterfaceClasses;
	FBlueprintEditorUtils::FindImplementedInterfaces(Blueprint, true, InterfaceClasses);

	// Add all potential interface events using the class list we build above
	for(auto It = InterfaceClasses.CreateConstIterator(); It; It++)
	{
		const UClass* Interface = (*It);
		for (TFieldIterator<UFunction> FunctionIt(Interface, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
		{
			UFunction* Function = *FunctionIt;

			UK2Node_Event* ExistingOverride = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, Interface, Function->GetFName());

			// See if it can be placed as an event
			if( K2Schema->FunctionCanBePlacedAsEvent(Function) )
			{
				if(ExistingOverride)
				{
					if (ShowEventFuncNames.Find(Function->GetFName()) == INDEX_NONE)
					{
						FK2ActionMenuBuilder::AddEventForFunction(ActionMenuBuilder, Function);	
					}
				}
				else if (ExistingFuncNames.Find(Function->GetFName()) == INDEX_NONE)
				{
					FK2ActionMenuBuilder::AddEventForFunction(ActionMenuBuilder, Function);
					ExistingFuncNames.Add(Function->GetFName());
				}				
			}
		}
	}

	// Add support for custom events, if applicable
	TSharedPtr<FEdGraphSchemaAction_K2AddCustomEvent> Action = TSharedPtr<FEdGraphSchemaAction_K2AddCustomEvent>(new FEdGraphSchemaAction_K2AddCustomEvent(TEXT(""), LOCTEXT("AddCustomEvent", "Custom Event..."), TEXT(""), 0));
	ActionMenuBuilder.AddAction(Action, K2ActionCategories::AddEventCategory);

	UK2Node_CustomEvent* CustomEventNode = ActionMenuBuilder.CreateTemplateNode<UK2Node_CustomEvent>();
	CustomEventNode->CustomFunctionName = FBlueprintEditorUtils::FindUniqueCustomEventName(Blueprint);
	CustomEventNode->bIsEditable = true;
	Action->NodeTemplate = CustomEventNode;
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetLiteralsFromActorSelection(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const
{
	const ULevelScriptBlueprint* LevelBlueprint = CastChecked<ULevelScriptBlueprint>(ContextMenuBuilder.Blueprint);
	check(LevelBlueprint);

	USelection* SelectedActors = GEditor->GetSelectedActors();

	TArray<AActor*> ValidActors;
	for(FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if( K2Schema->IsActorValidForLevelScriptRefs(Actor, LevelBlueprint) )
		{
			ValidActors.Add( Actor );
		}
	}

	if ( ValidActors.Num() )
	{
		TArray<TSharedPtr<FEdGraphSchemaAction> > NewActions;
		for ( int32 ActorIndex = 0; ActorIndex < ValidActors.Num(); ActorIndex++ )
		{
			AActor* Actor = ValidActors[ActorIndex];
			UK2Node_Literal* LiteralNodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_Literal>();
			LiteralNodeTemplate->SetObjectRef(Actor);

			const FString Description = (ValidActors.Num() == 1) ?
				FString::Printf(*LOCTEXT("AddReferenceToActor", "Add Reference to %s").ToString(), *Actor->GetActorLabel()) :
				LOCTEXT("AddReferencesToSelectedActors", "Add references to selected actors").ToString();
			const FString ToolTip = (ValidActors.Num() == 1 ) ?
				FString::Printf(*LOCTEXT("AddReferenceToActorToolTip", "Add a reference to %s").ToString(), *Actor->GetActorLabel()) :
				LOCTEXT("AddReferencesToSelectedActorsToolTip", "Add references to currently selected actors in the scene").ToString();

			TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action( new FEdGraphSchemaAction_K2NewNode(TEXT(""), FText::FromString(Description), ToolTip, K2Schema->AG_LevelReference) );
			Action->Keywords = LiteralNodeTemplate->GetKeywords();
			Action->NodeTemplate = LiteralNodeTemplate;

			NewActions.Add( Action );
		}

		ContextMenuBuilder.AddActionList( NewActions );
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetBoundEventsFromActorSelection(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const
{
	const ULevelScriptBlueprint* LevelBlueprint = CastChecked<ULevelScriptBlueprint>(ContextMenuBuilder.Blueprint);
	check(LevelBlueprint != NULL);

	USelection* SelectedActors = GEditor->GetSelectedActors();

	TArray<AActor*> ValidActors;
	for(FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		// We only care about actors that are referenced in the world for bound events
		AActor* Actor = Cast<AActor>(*Iter);
		if( K2Schema->IsActorValidForLevelScriptRefs(Actor, LevelBlueprint) )
		{
			ValidActors.Add( Actor );
		}
	}

	if ( ValidActors.Num() > 0 )
	{
		UClass* CommonClass = FindMostDerivedCommonActor( ValidActors );

		// If only one actor is selected then we build a different string using that actor's label else we show a generic string
		const FString CurrentCategory = (ValidActors.Num() == 1) ?
			K2ActionCategories::AddEventForPrefix + ValidActors[0]->GetActorLabel() :
			K2ActionCategories::AddEventForActorsCategory;

		AddBoundEventActionsForClass( ContextMenuBuilder, CommonClass, CurrentCategory, ValidActors, NULL );
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::AddBoundEventActionsForClass(FBlueprintGraphActionListBuilder& ContextMenuBuilder, UClass* BindClass, const FString& CurrentCategory, TArray<AActor*>& Actors, UObjectProperty* ComponentProperty) const
{
	const FString AddString = FString(TEXT("Add "));
	const FString ViewString = FString(TEXT("View "));

	check(Actors.Num() != 0 || ComponentProperty != NULL);

	for (TFieldIterator<UProperty> PropertyIt(BindClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;

		// Check for multicast delegates that we can safely assign
		if (K2Schema->CanUserKismetAccessVariable(Property, BindClass, UEdGraphSchema_K2::MustBeDelegate))
		{
			if (UMulticastDelegateProperty* DelegateProperty = Cast<UMulticastDelegateProperty>(Property))
			{
				FString PropertyTooltip = DelegateProperty->GetToolTipText().ToString();
				if( PropertyTooltip.IsEmpty() )
				{
					PropertyTooltip = Property->GetName();
				}

				// Add on category for delegate property
				const FString EventCategory = CurrentCategory + TEXT("|") + FObjectEditorUtils::GetCategory(Property);

				if ( ComponentProperty != NULL )
				{
					const UK2Node* EventNode = FKismetEditorUtilities::FindBoundEventForComponent(ContextMenuBuilder.Blueprint, DelegateProperty->GetFName(), ComponentProperty->GetFName());

					if (EventNode)
					{
						TSharedPtr<FEdGraphSchemaAction_K2ViewNode> NewDelegateNode = TSharedPtr<FEdGraphSchemaAction_K2ViewNode>( new FEdGraphSchemaAction_K2ViewNode(EventCategory, FText::FromString(ViewString + DelegateProperty->GetName()), PropertyTooltip, K2Schema->AG_LevelReference));
						NewDelegateNode->NodePtr = EventNode;

						ContextMenuBuilder.AddAction( NewDelegateNode );
					}
					else
					{
						TSharedPtr<FEdGraphSchemaAction_K2NewNode> NewDelegateNode = TSharedPtr<FEdGraphSchemaAction_K2NewNode>( new FEdGraphSchemaAction_K2NewNode(EventCategory, FText::FromString(AddString + DelegateProperty->GetName()), PropertyTooltip, K2Schema->AG_LevelReference));

						UK2Node_ComponentBoundEvent* NewComponentEvent = ContextMenuBuilder.CreateTemplateNode<UK2Node_ComponentBoundEvent>();
						NewComponentEvent->InitializeComponentBoundEventParams( ComponentProperty, DelegateProperty );
						NewDelegateNode->NodeTemplate = NewComponentEvent;

						ContextMenuBuilder.AddAction( NewDelegateNode );
					}
				}
				else if ( Actors.Num() > 0 )
				{
					TArray<TSharedPtr<FEdGraphSchemaAction> > NewActions;
					for ( int32 ActorIndex = 0; ActorIndex < Actors.Num(); ActorIndex++ )
					{
						const UK2Node* ActorEventNode = FKismetEditorUtilities::FindBoundEventForActor(Actors[ActorIndex], DelegateProperty->GetFName());

						if (ActorEventNode)
						{
							TSharedPtr<FEdGraphSchemaAction_K2ViewNode> NewDelegateNode = TSharedPtr<FEdGraphSchemaAction_K2ViewNode>( new FEdGraphSchemaAction_K2ViewNode(EventCategory, FText::FromString(ViewString + DelegateProperty->GetName()), PropertyTooltip, K2Schema->AG_LevelReference));
							NewDelegateNode->NodePtr = ActorEventNode;

							NewActions.Add( NewDelegateNode );
						}
						else
						{
							TSharedPtr<FEdGraphSchemaAction_K2NewNode> NewDelegateNode = TSharedPtr<FEdGraphSchemaAction_K2NewNode>(new FEdGraphSchemaAction_K2NewNode(EventCategory, FText::FromString(AddString + DelegateProperty->GetName()), PropertyTooltip, K2Schema->AG_LevelReference));

							UK2Node_ActorBoundEvent* NewActorEvent = ContextMenuBuilder.CreateTemplateNode<UK2Node_ActorBoundEvent>();
							NewActorEvent->InitializeActorBoundEventParams( Actors[ActorIndex], DelegateProperty );
							NewDelegateNode->NodeTemplate = NewActorEvent;

							NewActions.Add( NewDelegateNode );
						}
					}

					ContextMenuBuilder.AddActionList( NewActions );
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetMatineeControllers(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const
{
	const ULevelScriptBlueprint* LevelBlueprint = CastChecked<ULevelScriptBlueprint>(ContextMenuBuilder.Blueprint);
	ULevel* BlueprintLevel = LevelBlueprint->GetLevel();
	check(BlueprintLevel != NULL);

	bool bShowForSelected = true;

	TArray<AMatineeActor*> MatineeActors;
	if(bShowForSelected)
	{
		// Find selected matinee actors in this level
		USelection* SelectedActors = GEditor->GetSelectedActors();
		for(FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			AMatineeActor* MatActor = Cast<AMatineeActor>(*Iter);
			if( MatActor != NULL && (MatActor->GetLevel() == BlueprintLevel) )
			{
				MatineeActors.Add(MatActor);
			}
		}
	}
	else
	{
		// Find all matinee actors in the level
		for ( TArray<AActor*>::TConstIterator LevelActorIterator( BlueprintLevel->Actors ); LevelActorIterator; ++LevelActorIterator )
		{
			AMatineeActor* MatActor = Cast<AMatineeActor>(*LevelActorIterator);
			if (MatActor != NULL)
			{
				MatineeActors.Add(MatActor);
			}
		}
	}

	// Then remove any for which we already have a MatineeController node
	TArray<UK2Node_MatineeController*> ExistingMatineeControllers;
	FBlueprintEditorUtils::GetAllNodesOfClass(ContextMenuBuilder.Blueprint, ExistingMatineeControllers);
	for (int32 i=0; i<ExistingMatineeControllers.Num(); i++)
	{
		UK2Node_MatineeController* MatController = ExistingMatineeControllers[i];
		if(MatController->MatineeActor != NULL)
		{
			MatineeActors.Remove(MatController->MatineeActor);
		}
	}

	// Then offer option for all that remain
	for (int32 i=0; i<MatineeActors.Num(); i++)
	{
		AMatineeActor* MatActor = MatineeActors[i];

		FText Description = FText::Format(LOCTEXT("AddMatineeControllerForActor", "Add MatineeController for {actorName}"), FText::FromString(MatActor->GetName()));
		FString ToolTip = FText::Format(LOCTEXT("AddMatineeControllerForActor_Tooltip", "Add Matinee controller node for {actorName}"), FText::FromString(MatActor->GetName())).ToString();

		UK2Node_MatineeController* MatControlNode = ContextMenuBuilder.CreateTemplateNode<UK2Node_MatineeController>();
		MatControlNode->MatineeActor = MatActor;

		TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, TEXT(""), Description, ToolTip, K2Schema->AG_LevelReference, MatControlNode->GetKeywords());
		Action->NodeTemplate = MatControlNode;
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetFunctionCallsOnSelectedActors(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const
{
	const ULevelScriptBlueprint* LevelBlueprint = CastChecked<ULevelScriptBlueprint>(ContextMenuBuilder.Blueprint);
	ULevel* BlueprintLevel = LevelBlueprint->GetLevel();
	check(BlueprintLevel != NULL);

	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<AActor*> ValidActors;
	for(FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if ( Actor != NULL && Actor->GetLevel() == BlueprintLevel && K2Schema->IsActorValidForLevelScriptRefs(Actor, LevelBlueprint) )
		{
			ValidActors.Add( Actor );
		}
	}

	if ( ValidActors.Num() )
	{
		UClass* CommonClass = FindMostDerivedCommonActor( ValidActors );

		// If only one actor is selected then we build a different string using that actor's label else we show a generic string
		const FString BaseCategory = (ValidActors.Num() == 1) ?
			K2ActionCategories::CallFuncOnPrefix + ValidActors[0]->GetActorLabel() :
			K2ActionCategories::CallFuncOnActorsCategory;
		GetFuncNodesForClass(ContextMenuBuilder, CommonClass, NULL, ContextMenuBuilder.CurrentGraph, UEdGraphSchema_K2::FT_Imperative | UEdGraphSchema_K2::FT_Const, BaseCategory, FFunctionTargetInfo(ValidActors));
	}
	else
	{
		FText SelectActorsMsg = LOCTEXT("SelectActorForEvents", "Select Actor(s) to see available Events & Functions");
		FText SelectActorsToolTip = LOCTEXT("SelectActorForEventsTooltip", "Select Actor(s) in the level to see available Events and Functions in this menu.");
		TSharedPtr<FEdGraphSchemaAction_Dummy> MsgAction = TSharedPtr<FEdGraphSchemaAction_Dummy>(new FEdGraphSchemaAction_Dummy(TEXT(""), SelectActorsMsg, SelectActorsToolTip.ToString(), K2Schema->AG_LevelReference));
		ContextMenuBuilder.AddAction(MsgAction);
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetBoundEventsFromComponentSelection(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const
{
	for (int32 SelectIdx = 0; SelectIdx < ContextMenuBuilder.SelectedObjects.Num(); ++SelectIdx)
	{
		UObjectProperty* CompProperty = Cast<UObjectProperty>(ContextMenuBuilder.SelectedObjects[SelectIdx]);
		if(	CompProperty != NULL && 
			CompProperty->PropertyClass != NULL )
		{
			const FString PropertyName = GetDefault<UEditorStyleSettings>()->bShowFriendlyNames ? UEditorEngine::GetFriendlyName(CompProperty) : CompProperty->GetName();
			const FString CurrentCategory = K2ActionCategories::AddEventForPrefix + PropertyName;

			TArray<AActor*> DummyActorList;
			AddBoundEventActionsForClass(ContextMenuBuilder, CompProperty->PropertyClass, CurrentCategory, DummyActorList, CompProperty);
		}
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetFunctionCallsOnSelectedComponents(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const
{
	UObjectProperty* SelectedCompProperty = NULL;

	for (int32 SelectIdx = 0; SelectIdx < ContextMenuBuilder.SelectedObjects.Num(); ++SelectIdx)
	{
		UObjectProperty* CompProperty = Cast<UObjectProperty>(ContextMenuBuilder.SelectedObjects[SelectIdx]);
		if(	CompProperty != NULL && 
			CompProperty->PropertyClass != NULL )
		{
			SelectedCompProperty = CompProperty;
			break;
		}
	}

	if(SelectedCompProperty != NULL)
	{
		const FString PropertyName = GetDefault<UEditorStyleSettings>()->bShowFriendlyNames ? UEditorEngine::GetFriendlyName(SelectedCompProperty) : SelectedCompProperty->GetName();
		const FString BaseCategory = K2ActionCategories::CallFuncOnPrefix + PropertyName;
		GetFuncNodesForClass(ContextMenuBuilder, SelectedCompProperty->PropertyClass, NULL, ContextMenuBuilder.CurrentGraph, UEdGraphSchema_K2::FT_Imperative | UEdGraphSchema_K2::FT_Const, BaseCategory, FFunctionTargetInfo(SelectedCompProperty->GetFName()));
	}
	else
	{
		FText SelectComponentMsg = LOCTEXT("SelectComponentForEvents", "Select a Component to see available Events & Functions");
		FText SelectComponentToolTip = LOCTEXT("SelectComponentForEventsTooltip", "Select a Component in the MyBlueprint tab to see available Events and Functions in this menu.");
		TSharedPtr<FEdGraphSchemaAction_Dummy> MsgAction = TSharedPtr<FEdGraphSchemaAction_Dummy>(new FEdGraphSchemaAction_Dummy(TEXT(""), SelectComponentMsg, SelectComponentToolTip.ToString(), K2Schema->AG_LevelReference));
		ContextMenuBuilder.AddAction(MsgAction);
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetMacroTools(FBlueprintPaletteListBuilder& ActionMenuBuilder, const bool bInAllowImpureFuncs, const UEdGraph* InCurrentGraph) const
{
	// Macro instances
	for (TObjectIterator<UBlueprint> BlueprintIt; BlueprintIt; ++BlueprintIt)
	{
		UBlueprint* MacroBP = *BlueprintIt;
		if ((ActionMenuBuilder.Blueprint == MacroBP) || //add local Macros
			((MacroBP->BlueprintType == BPTYPE_MacroLibrary) && (ActionMenuBuilder.Blueprint->ParentClass->IsChildOf(MacroBP->ParentClass))))
		{
			for (TArray<UEdGraph*>::TIterator GraphIt(MacroBP->MacroGraphs); GraphIt; ++GraphIt)
			{
				UEdGraph* MacroGraph = *GraphIt;

				//Check whether the current graph is same as the blueprint macro graph that we are trying to add instance of.
				//If so, then don't allow adding action menu to instance the macro graph to its own definition.
				if (InCurrentGraph != MacroGraph)
				{
					// Do not allow any macros with latent functions to appear in the list
					if(InCurrentGraph && InCurrentGraph->GetSchema()->GetGraphType(InCurrentGraph) == GT_Function && FBlueprintEditorUtils::CheckIfGraphHasLatentFunctions(MacroGraph))
					{
						continue;
					}

					UK2Node_Tunnel* FunctionEntryNode = NULL;
					UK2Node_Tunnel* FunctionResultNode = NULL;
					bool bIsMacroPure = false;
					FKismetEditorUtilities::GetInformationOnMacro(MacroGraph, /*out*/ FunctionEntryNode, /*out*/ FunctionResultNode, /*out*/ bIsMacroPure);

					if (bIsMacroPure || bInAllowImpureFuncs)
					{
						FK2ActionMenuBuilder::AttachMacroGraphAction(ActionMenuBuilder, MacroGraph);
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetPinAllowedMacros(FBlueprintGraphActionListBuilder& ContextMenuBuilder, const UClass* Class, const FEdGraphPinType& DesiredPinType, bool bWantOutputs, bool bAllowImpureMacros) const
{
	// Macro instances
	for (TObjectIterator<UBlueprint> BlueprintIt; BlueprintIt; ++BlueprintIt)
	{
		UBlueprint* MacroBP = *BlueprintIt;
		if ((MacroBP == ContextMenuBuilder.Blueprint) || //let local macros be added
			((MacroBP->BlueprintType == BPTYPE_MacroLibrary) && (ContextMenuBuilder.Blueprint->ParentClass->IsChildOf(MacroBP->ParentClass))))
		{
			// getting 'top-level' of the macros
			for (TArray<UEdGraph*>::TIterator GraphIt(MacroBP->MacroGraphs); GraphIt; ++GraphIt)
			{
				UEdGraph* MacroGraph = *GraphIt;

				//Check whether the current graph is same as the blueprint macro graph that we are trying to add instance of.
				//If so, then don't allow adding action menu to instance the macro graph to its own definition.
				if (ContextMenuBuilder.CurrentGraph != MacroGraph)
				{
					UK2Node_Tunnel* FunctionEntryNode = NULL;
					UK2Node_Tunnel* FunctionResultNode = NULL;
					bool bIsMacroPure = false;
					FKismetEditorUtilities::GetInformationOnMacro(MacroGraph, /*out*/ FunctionEntryNode, /*out*/ FunctionResultNode, /*out*/ bIsMacroPure);

					bool bCompatible = false;
					UK2Node_Tunnel* FunctionNode = bWantOutputs ? FunctionResultNode : FunctionEntryNode;

					if ((FunctionNode != NULL) && (bIsMacroPure || bAllowImpureMacros))
					{
						// Search for matching pin between macro and desired pin
						for (TArray<UEdGraphPin*>::TIterator PinIterator(FunctionNode->Pins); PinIterator; ++PinIterator)
						{
							UEdGraphPin* CurrentPin = *PinIterator;

							// Tunnels have all output pins on the entrance, and all input pins on the exit.
							// Need to recheck direction for IsChildOf checks to occur correctly.
							if(!bWantOutputs)
							{
								// Going from an output to an input pin.
								if (K2Schema->ArePinTypesCompatible(DesiredPinType, CurrentPin->PinType, Class)) 
								{
									bCompatible = true;
									break;
								}
							}
							else
							{
								// Going from an input to an output pin.
								if (K2Schema->ArePinTypesCompatible(CurrentPin->PinType, DesiredPinType, Class)) 
								{
									bCompatible = true;
									break;
								}
							}
						}
					}

					// Pull macro into action menu if its compatible
					if (bCompatible) 
					{
						FK2ActionMenuBuilder::AttachMacroGraphAction(ContextMenuBuilder, MacroGraph);
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetFuncNodesWithPinType(FBlueprintGraphActionListBuilder& ContextMenuBuilder, const UClass* Class, class UClass* OwnerClass, const FEdGraphPinType& DesiredPinType, bool bWantOutput, bool bPureOnly) const
{
	if (Class != NULL)
	{
		UBlueprint* ClassBlueprint = UBlueprint::GetBlueprintFromClass(OwnerClass);

		static const FName NativeMakeFunc(TEXT("NativeMakeFunc"));
		static const FName NativeBrakeFunc(TEXT("NativeBreakFunc"));
		static const FName HasNativeMake(TEXT("HasNativeMake"));
		static const FName HasNativeBreak(TEXT("HasNativeBreak"));
		const UScriptStruct* ScriptStruct = Cast<const UScriptStruct>(DesiredPinType.PinSubCategoryObject.Get());
		const bool bUseNativeMake = bWantOutput && ScriptStruct && ScriptStruct->HasMetaData(HasNativeMake);
		const bool bUseNativeBrake = !bWantOutput && ScriptStruct && ScriptStruct->HasMetaData(HasNativeBreak);

		for (TFieldIterator<UFunction> FunctionIt(Class, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
		{
			UFunction* Function = *FunctionIt;
			if (K2Schema->CanUserKismetCallFunction(Function))
			{
				if ((false == bPureOnly || Function->HasAnyFunctionFlags(FUNC_BlueprintPure)) && 
					K2Schema->FunctionHasParamOfType(Function, ContextMenuBuilder.CurrentGraph, DesiredPinType, bWantOutput)) 
				{
					const bool bShowMakeOnTop = bUseNativeMake && Function->HasMetaData(NativeMakeFunc);
					const bool bShowBrakeOnTop = bUseNativeBrake && Function->HasMetaData(NativeBrakeFunc);
					FK2ActionMenuBuilder::AddSpawnInfoForFunction(Function, false, FFunctionTargetInfo(), FMemberReference(), K2ActionCategories::RootFunctionCategory, K2Schema->AG_LevelReference, ContextMenuBuilder, false, bShowMakeOnTop || bShowBrakeOnTop);
				}
			}
		}
	}
	else
	{
		// Try each function library for functions matching the criteria
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			UClass* TestClass = *ClassIt;
			const bool bIsSkeletonClass = FKismetEditorUtilities::IsClassABlueprintSkeleton(TestClass);
			if (TestClass->IsChildOf(UBlueprintFunctionLibrary::StaticClass()) &&
				CanUseLibraryFunctionsForScope(OwnerClass, TestClass) && !bIsSkeletonClass)
			{
				GetFuncNodesWithPinType(ContextMenuBuilder, TestClass, OwnerClass, DesiredPinType, bWantOutput, bPureOnly);
			}
		}
	}
}

//------------------------------------------------------------------------------
void FK2ActionMenuBuilder::GetAllInterfaceMessageActions(FGraphActionListBuilderBase& ActionMenuBuilder) const
{
	// Add interface classes if relevant
	for ( TObjectIterator<UClass> it; it; ++it )
	{
		UClass* CurrentInterface = *it;
		if( FKismetEditorUtilities::IsClassABlueprintInterface(CurrentInterface) && !FKismetEditorUtilities::IsClassABlueprintSkeleton(CurrentInterface) )
		{
			for (TFieldIterator<UFunction> FunctionIt(CurrentInterface, EFieldIteratorFlags::ExcludeSuper); FunctionIt; ++FunctionIt)
			{
				UFunction* Function = *FunctionIt;
				if( K2Schema->CanUserKismetCallFunction(Function) )
				{
					FString MessageCategory = UK2Node_CallFunction::GetDefaultCategoryForFunction(Function, TEXT(""));
					if (MessageCategory.IsEmpty())
					{
						MessageCategory = K2ActionCategories::FallbackInterfaceCategory;
					}

					UK2Node_Message* MessageTemplate = ActionMenuBuilder.CreateTemplateNode<UK2Node_Message>();
					MessageTemplate->FunctionReference.SetExternalMember(Function->GetFName(), CurrentInterface);
					TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ActionMenuBuilder, MessageCategory, FText::FromString(Function->GetName()), MessageTemplate->GetTooltipText().ToString(), 0, MessageTemplate->GetKeywords());
					Action->NodeTemplate = MessageTemplate;
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
