// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "K2Node_InputKeyEvent.h"
#include "CompilerResultsLog.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "UK2Node_InputKey"

UK2Node_InputKey::UK2Node_InputKey(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bConsumeInput = true;
	bOverrideParentBinding = true;

#if PLATFORM_MAC && WITH_EDITOR
	if (IsTemplate())
	{
		UProperty* ControlProp = GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UK2Node_InputKey, bControl));
		UProperty* CommandProp = GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UK2Node_InputKey, bCommand));

		ControlProp->SetMetaData(TEXT("DisplayName"), TEXT("Command"));
		CommandProp->SetMetaData(TEXT("DisplayName"), TEXT("Control"));
	}
#endif
}

void UK2Node_InputKey::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerUE4Version() < VER_UE4_BLUEPRINT_INPUT_BINDING_OVERRIDES)
	{
		// Don't change existing behaviors
		bOverrideParentBinding = false;
	}
}

void UK2Node_InputKey::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	CachedNodeTitle.Clear();
	CachedTooltip.Clear();
}

void UK2Node_InputKey::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, TEXT("Pressed"));
	CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, TEXT("Released"));

	Super::AllocateDefaultPins();
}

FLinearColor UK2Node_InputKey::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->EventNodeTitleColor;
}

FName UK2Node_InputKey::GetModifierName() const
{
    FString ModName;
    bool bPlus = false;
    if (bControl)
    {
        ModName += TEXT("Ctrl");
        bPlus = true;
    }
    if (bCommand)
    {
        if (bPlus) ModName += TEXT("+");
        ModName += TEXT("Cmd");
        bPlus = true;
    }
    if (bAlt)
    {
        if (bPlus) ModName += TEXT("+");
        ModName += TEXT("Alt");
        bPlus = true;
    }
    if (bShift)
    {
        if (bPlus) ModName += TEXT("+");
        ModName += TEXT("Shift");
        bPlus = true;
    }

	return FName(*ModName);
}

FText UK2Node_InputKey::GetModifierText() const
{
#if PLATFORM_MAC
    const FText CommandText = NSLOCTEXT("UK2Node_InputKey", "KeyName_Control", "Ctrl");
    const FText ControlText = NSLOCTEXT("UK2Node_InputKey", "KeyName_Command", "Cmd");
#else
    const FText ControlText = NSLOCTEXT("UK2Node_InputKey", "KeyName_Control", "Ctrl");
    const FText CommandText = NSLOCTEXT("UK2Node_InputKey", "KeyName_Command", "Cmd");
#endif
    const FText AltText = NSLOCTEXT("UK2Node_InputKey", "KeyName_Alt", "Alt");
    const FText ShiftText = NSLOCTEXT("UK2Node_InputKey", "KeyName_Shift", "Shift");
    
	const FText AppenderText = NSLOCTEXT("UK2Node_InputKey", "ModAppender", "+");

	FFormatNamedArguments Args;
	int32 ModCount = 0;

    if (bControl)
    {
		Args.Add(FString::Printf(TEXT("Mod%d"),++ModCount), ControlText);
    }
    if (bCommand)
    {
		Args.Add(FString::Printf(TEXT("Mod%d"),++ModCount), CommandText);
    }
    if (bAlt)
    {
		Args.Add(FString::Printf(TEXT("Mod%d"),++ModCount), AltText);
    }
    if (bShift)
    {
		Args.Add(FString::Printf(TEXT("Mod%d"),++ModCount), ShiftText);
    }

	for (int32 i = 1; i <= 4; ++i)
	{
		if (i > ModCount)
		{
			Args.Add(FString::Printf(TEXT("Mod%d"), i), FText::GetEmpty());
		}

		Args.Add(FString::Printf(TEXT("Appender%d"), i), (i < ModCount ? AppenderText : FText::GetEmpty()));
	}

	Args.Add(TEXT("Key"), GetKeyText());

	return FText::Format(NSLOCTEXT("UK2Node_InputKey", "NodeTitle", "{Mod1}{Appender1}{Mod2}{Appender2}{Mod3}{Appender3}{Mod4}"), Args);
}

FText UK2Node_InputKey::GetKeyText() const
{
	return InputKey.GetDisplayName();
}

FText UK2Node_InputKey::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (bControl || bAlt || bShift || bCommand)
	{
		if (CachedNodeTitle.IsOutOfDate(this))
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ModifierKey"), GetModifierText());
			Args.Add(TEXT("Key"), GetKeyText());
			
			// FText::Format() is slow, so we cache this to save on performance
			CachedNodeTitle.SetCachedText(FText::Format(NSLOCTEXT("K2Node", "InputKey_Name_WithModifiers", "{ModifierKey} {Key}"), Args), this);
		}
		return CachedNodeTitle;
	}
	else
	{
		return GetKeyText();
	}
}

FText UK2Node_InputKey::GetTooltipText() const
{
	if (CachedTooltip.IsOutOfDate(this))
	{
		FText ModifierText = GetModifierText();
		FText KeyText = GetKeyText();

		// FText::Format() is slow, so we cache this to save on performance
		if (!ModifierText.IsEmpty())
		{
			CachedTooltip.SetCachedText(FText::Format(NSLOCTEXT("K2Node", "InputKey_Tooltip_Modifiers", "Events for when the {0} key is pressed or released while {1} is also held."), KeyText, ModifierText), this);
		}
		else
		{
			CachedTooltip.SetCachedText(FText::Format(NSLOCTEXT("K2Node", "InputKey_Tooltip", "Events for when the {0} key is pressed or released."), KeyText), this);
		}
	}
	return CachedTooltip;
}

FName UK2Node_InputKey::GetPaletteIcon(FLinearColor& OutColor) const
{
	return EKeys::GetMenuCategoryPaletteIcon(InputKey.GetMenuCategory());
}

bool UK2Node_InputKey::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);

	UEdGraphSchema_K2 const* K2Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
	bool const bIsConstructionScript = (K2Schema != nullptr) ? K2Schema->IsConstructionScript(Graph) : false;

	return (Blueprint != nullptr) && Blueprint->SupportsInputEvents() && !bIsConstructionScript && Super::IsCompatibleWithGraph(Graph);
}

UEdGraphPin* UK2Node_InputKey::GetPressedPin() const
{
	return FindPin(TEXT("Pressed"));
}

UEdGraphPin* UK2Node_InputKey::GetReleasedPin() const
{
	return FindPin(TEXT("Released"));
}

void UK2Node_InputKey::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);
	
	if (!InputKey.IsValid())
	{
		MessageLog.Warning(*FText::Format(NSLOCTEXT("KismetCompiler", "Invalid_InputKey_Warning", "InputKey Event specifies invalid FKey'{0}' for @@"), FText::FromString(InputKey.ToString())).ToString(), this);
	}
	else if (InputKey.IsFloatAxis())
	{
		MessageLog.Warning(*FText::Format(NSLOCTEXT("KismetCompiler", "Axis_InputKey_Warning", "InputKey Event specifies axis FKey'{0}' for @@"), FText::FromString(InputKey.ToString())).ToString(), this);
	}
	else if (!InputKey.IsBindableInBlueprints())
	{
		MessageLog.Warning( *FText::Format( NSLOCTEXT("KismetCompiler", "NotBindanble_InputKey_Warning", "InputKey Event specifies FKey'{0}' that is not blueprint bindable for @@"), FText::FromString(InputKey.ToString())).ToString(), this);
	}
}

void UK2Node_InputKey::CreateInputKeyEvent(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin* InputKeyPin, const EInputEvent KeyEvent)
{
	if (InputKeyPin->LinkedTo.Num() > 0)
	{
		UK2Node_InputKeyEvent* InputKeyEvent = CompilerContext.SpawnIntermediateNode<UK2Node_InputKeyEvent>(this, SourceGraph);
		const FName ModifierName = GetModifierName();
		if ( ModifierName != NAME_None )
		{
			InputKeyEvent->CustomFunctionName = FName( *FString::Printf(TEXT("InpActEvt_%s_%s_%s"), *ModifierName.ToString(), *InputKey.ToString(), *InputKeyEvent->GetName()));
		}
		else
		{
			InputKeyEvent->CustomFunctionName = FName( *FString::Printf(TEXT("InpActEvt_%s_%s"), *InputKey.ToString(), *InputKeyEvent->GetName()));
		}
		InputKeyEvent->InputChord.Key = InputKey;
		InputKeyEvent->InputChord.bCtrl = bControl;
		InputKeyEvent->InputChord.bAlt = bAlt;
		InputKeyEvent->InputChord.bShift = bShift;
		InputKeyEvent->InputChord.bCmd = bCommand;
		InputKeyEvent->bConsumeInput = bConsumeInput;
		InputKeyEvent->bExecuteWhenPaused = bExecuteWhenPaused;
		InputKeyEvent->bOverrideParentBinding = bOverrideParentBinding;
		InputKeyEvent->InputKeyEvent = KeyEvent;
		InputKeyEvent->EventReference.SetExternalDelegateMember(FName(TEXT("InputActionHandlerDynamicSignature__DelegateSignature")));
		InputKeyEvent->bInternalEvent = true;
		InputKeyEvent->AllocateDefaultPins();

		// Move any exec links from the InputActionNode pin to the InputActionEvent node
		UEdGraphPin* EventOutput = CompilerContext.GetSchema()->FindExecutionPin(*InputKeyEvent, EGPD_Output);

		if(EventOutput != NULL)
		{
			CompilerContext.MovePinLinksToIntermediate(*InputKeyPin, *EventOutput);
		}
	}
}

void UK2Node_InputKey::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	CreateInputKeyEvent(CompilerContext, SourceGraph, GetPressedPin(), IE_Pressed);
	CreateInputKeyEvent(CompilerContext, SourceGraph, GetReleasedPin(), IE_Released);
}

void UK2Node_InputKey::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	TArray<FKey> AllKeys;
	EKeys::GetAllKeys(AllKeys);

	auto CustomizeInputNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, FKey Key)
	{
		UK2Node_InputKey* InputNode = CastChecked<UK2Node_InputKey>(NewNode);
		InputNode->InputKey = Key;
	};

	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();

	for (FKey const Key : AllKeys)
	{
		// these will be handled by UK2Node_GetInputAxisKeyValue and UK2Node_GetInputVectorAxisValue respectively
		if (!Key.IsBindableInBlueprints() || Key.IsFloatAxis() || Key.IsVectorAxis())
		{
			continue;
		}

		// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
		// check to make sure that the registrar is looking for actions of this type
		// (could be regenerating actions for a specific asset, and therefore the 
		// registrar would only accept actions corresponding to that asset)
		if (!ActionRegistrar.IsOpenForRegistration(ActionKey))
		{
			continue;
		}

		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeInputNodeLambda, Key);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_InputKey::GetMenuCategory() const
{
	static TMap<FName, FNodeTextCache> CachedCategories;

	const FName KeyCategory = InputKey.GetMenuCategory();
	const FText SubCategoryDisplayName = FText::Format(LOCTEXT("EventsCategory", "{0} Events"), EKeys::GetMenuCategoryDisplayName(KeyCategory));
	FNodeTextCache& NodeTextCache = CachedCategories.FindOrAdd(KeyCategory);

	if (NodeTextCache.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		NodeTextCache.SetCachedText(FEditorCategoryUtils::BuildCategoryString(FCommonEditorCategory::Input, SubCategoryDisplayName), this);
	}
	return NodeTextCache;
}

FBlueprintNodeSignature UK2Node_InputKey::GetSignature() const
{
	FBlueprintNodeSignature NodeSignature = Super::GetSignature();
	NodeSignature.AddKeyValue(InputKey.ToString());

	return NodeSignature;
}

#undef LOCTEXT_NAMESPACE
