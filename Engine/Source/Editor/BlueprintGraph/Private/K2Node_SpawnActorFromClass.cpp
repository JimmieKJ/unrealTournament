// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "KismetCompiler.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_EnumLiteral.h"

struct FK2Node_SpawnActorFromClassHelper
{
	static FString WorldContextPinName;
	static FString ClassPinName;
	static FString SpawnTransformPinName;
	static FString NoCollisionFailPinName;
	static FString CollisionHandlingOverridePinName;
	static FString OwnerPinName;
};

FString FK2Node_SpawnActorFromClassHelper::WorldContextPinName(TEXT("WorldContextObject"));
FString FK2Node_SpawnActorFromClassHelper::ClassPinName(TEXT("Class"));
FString FK2Node_SpawnActorFromClassHelper::SpawnTransformPinName(TEXT("SpawnTransform"));
FString FK2Node_SpawnActorFromClassHelper::NoCollisionFailPinName(TEXT("SpawnEvenIfColliding"));		// deprecated pin, name kept for backwards compat
FString FK2Node_SpawnActorFromClassHelper::CollisionHandlingOverridePinName(TEXT("CollisionHandlingOverride"));
FString FK2Node_SpawnActorFromClassHelper::OwnerPinName(TEXT("Owner"));

#define LOCTEXT_NAMESPACE "K2Node_SpawnActorFromClass"

UK2Node_SpawnActorFromClass::UK2Node_SpawnActorFromClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeTooltip = LOCTEXT("NodeTooltip", "Attempts to spawn a new Actor with the specified transform");
}

void UK2Node_SpawnActorFromClass::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Add execution pins
	CreatePin(EGPD_Input, K2Schema->PC_Exec, TEXT(""), NULL, false, false, K2Schema->PN_Execute);
	CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, K2Schema->PN_Then);

	// If required add the world context pin
	if (GetBlueprint()->ParentClass->HasMetaData(FBlueprintMetadata::MD_ShowWorldContextPin))
	{
		CreatePin(EGPD_Input, K2Schema->PC_Object, TEXT(""), UObject::StaticClass(), false, false, FK2Node_SpawnActorFromClassHelper::WorldContextPinName);
	}

	// Add blueprint pin
	UEdGraphPin* ClassPin = CreatePin(EGPD_Input, K2Schema->PC_Class, TEXT(""), AActor::StaticClass(), false, false, FK2Node_SpawnActorFromClassHelper::ClassPinName);

	// Transform pin
	UScriptStruct* TransformStruct = TBaseStructure<FTransform>::Get();
	UEdGraphPin* TransformPin = CreatePin(EGPD_Input, K2Schema->PC_Struct, TEXT(""), TransformStruct, false, false, FK2Node_SpawnActorFromClassHelper::SpawnTransformPinName);

	// Collision handling method pin
	UEnum* const MethodEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ESpawnActorCollisionHandlingMethod"), true);
	UEdGraphPin* const CollisionHandlingOverridePin = CreatePin(EGPD_Input, K2Schema->PC_Byte, TEXT(""), MethodEnum, false, false, FK2Node_SpawnActorFromClassHelper::CollisionHandlingOverridePinName);
	CollisionHandlingOverridePin->DefaultValue = MethodEnum->GetEnumName(static_cast<int>(ESpawnActorCollisionHandlingMethod::Undefined));

	UEdGraphPin* OwnerPin = CreatePin(EGPD_Input, K2Schema->PC_Object, TEXT(""), AActor::StaticClass(),/*bIsArray =*/false, /*bIsReference =*/false, FK2Node_SpawnActorFromClassHelper::OwnerPinName);
	OwnerPin->bAdvancedView = true;
	if (ENodeAdvancedPins::NoPins == AdvancedPinDisplay)
	{
		AdvancedPinDisplay = ENodeAdvancedPins::Hidden;
	}

	// Result pin
	UEdGraphPin* ResultPin = CreatePin(EGPD_Output, K2Schema->PC_Object, TEXT(""), AActor::StaticClass(), false, false, K2Schema->PN_ReturnValue);

	Super::AllocateDefaultPins();
}

void UK2Node_SpawnActorFromClass::CreatePinsForClass(UClass* InClass, TArray<UEdGraphPin*>& OutClassPins)
{
	check(InClass != NULL);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	const UObject* const ClassDefaultObject = InClass->GetDefaultObject(false);

	for (TFieldIterator<UProperty> PropertyIt(InClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;
		UClass* PropertyClass = CastChecked<UClass>(Property->GetOuter());
		const bool bIsDelegate = Property->IsA(UMulticastDelegateProperty::StaticClass());
		const bool bIsExposedToSpawn = UEdGraphSchema_K2::IsPropertyExposedOnSpawn(Property);
		const bool bIsSettableExternally = !Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance);

		if(	bIsExposedToSpawn &&
			!Property->HasAnyPropertyFlags(CPF_Parm) && 
			bIsSettableExternally &&
			Property->HasAllPropertyFlags(CPF_BlueprintVisible) &&
			!bIsDelegate &&
			(NULL == FindPin(Property->GetName()) ) )
		{
			UEdGraphPin* Pin = CreatePin(EGPD_Input, TEXT(""), TEXT(""), NULL, false, false, Property->GetName());
			const bool bPinGood = (Pin != NULL) && K2Schema->ConvertPropertyToPinType(Property, /*out*/ Pin->PinType);
			OutClassPins.Add(Pin);

			if (ClassDefaultObject && Pin != NULL && K2Schema->PinDefaultValueIsEditable(*Pin))
			{
				FString DefaultValueAsString;
				const bool bDefaultValueSet = FBlueprintEditorUtils::PropertyValueToString(Property, reinterpret_cast<const uint8*>(ClassDefaultObject), DefaultValueAsString);
				check( bDefaultValueSet );
				K2Schema->TrySetDefaultValue(*Pin, DefaultValueAsString);
			}

			// Copy tooltip from the property.
			if (Pin != nullptr)
			{
				K2Schema->ConstructBasicPinTooltip(*Pin, Property->GetToolTipText(), Pin->PinToolTip);
			}
		}
	}

	// Change class of output pin
	UEdGraphPin* ResultPin = GetResultPin();
	ResultPin->PinType.PinSubCategoryObject = InClass;
}

UClass* UK2Node_SpawnActorFromClass::GetClassToSpawn(const TArray<UEdGraphPin*>* InPinsToSearch /*=NULL*/) const
{
	UClass* UseSpawnClass = nullptr;
	const TArray<UEdGraphPin*>* PinsToSearch = InPinsToSearch ? InPinsToSearch : &Pins;

	UEdGraphPin* ClassPin = GetClassPin(PinsToSearch);
	if(ClassPin && ClassPin->DefaultObject != NULL && ClassPin->LinkedTo.Num() == 0)
	{
		UseSpawnClass = CastChecked<UClass>(ClassPin->DefaultObject);
	}
	else if (ClassPin && (1 == ClassPin->LinkedTo.Num()))
	{
		auto SourcePin = ClassPin->LinkedTo[0];
		UseSpawnClass = SourcePin ? Cast<UClass>(SourcePin->PinType.PinSubCategoryObject.Get()) : nullptr;
	}

	return UseSpawnClass;
}

void UK2Node_SpawnActorFromClass::MaybeUpdateCollisionPin(TArray<UEdGraphPin*>& OldPins)
{
	// see if there's a bNoCollisionFail pin
	for (UEdGraphPin* Pin : OldPins)
	{
		if (Pin->PinName == FK2Node_SpawnActorFromClassHelper::NoCollisionFailPinName)
		{
			bool bHadOldCollisionPin = true;
			if (Pin->LinkedTo.Num() == 0)
			{
				// no links, use the default value of the pin
				bool const bOldCollisionPinValue = (Pin->DefaultValue == FString(TEXT("true")));

				UEdGraphPin* const CollisionHandlingOverridePin = GetCollisionHandlingOverridePin();
				if (CollisionHandlingOverridePin)
				{
					UEnum const* const MethodEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ESpawnActorCollisionHandlingMethod"), true);
					CollisionHandlingOverridePin->DefaultValue =
						bOldCollisionPinValue
						? MethodEnum->GetEnumName(static_cast<int>(ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
						: MethodEnum->GetEnumName(static_cast<int>(ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding));
				}
			}
			else
			{
				// something was linked. we will just move the links to the new pin
				// #note: this will be an invalid linkage and the BP compiler will complain, and that's intentional
				// so that users will be able to see and fix issues
				UEdGraphPin* const CollisionHandlingOverridePin = GetCollisionHandlingOverridePin();
				check(CollisionHandlingOverridePin);

				UEnum* const MethodEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ESpawnActorCollisionHandlingMethod"), true);
				
				FGraphNodeCreator<UK2Node_EnumLiteral> AlwaysSpawnLiteralCreator(*GetGraph());
				UK2Node_EnumLiteral* const AlwaysSpawnLiteralNode = AlwaysSpawnLiteralCreator.CreateNode();
				AlwaysSpawnLiteralNode->Enum = MethodEnum;
				AlwaysSpawnLiteralNode->NodePosX = NodePosX;
				AlwaysSpawnLiteralNode->NodePosY = NodePosY;
				AlwaysSpawnLiteralCreator.Finalize();
			
				FGraphNodeCreator<UK2Node_EnumLiteral> AdjustIfNecessaryLiteralCreator(*GetGraph());
				UK2Node_EnumLiteral* const AdjustIfNecessaryLiteralNode = AdjustIfNecessaryLiteralCreator.CreateNode();
				AdjustIfNecessaryLiteralNode->Enum = MethodEnum;
				AdjustIfNecessaryLiteralNode->NodePosX = NodePosX;
				AdjustIfNecessaryLiteralNode->NodePosY = NodePosY;
				AdjustIfNecessaryLiteralCreator.Finalize();

				FGraphNodeCreator<UK2Node_Select> SelectCreator(*GetGraph());
				UK2Node_Select* const SelectNode = SelectCreator.CreateNode();
				SelectNode->NodePosX = NodePosX;
				SelectNode->NodePosY = NodePosY;
				SelectCreator.Finalize();

				// find pins we want to set and link up
				auto FindEnumInputPin = [](UK2Node_EnumLiteral const* Node)
				{
					for (UEdGraphPin* NodePin : Node->Pins)
					{
						if (NodePin->PinName == Node->GetEnumInputPinName())
						{
							return NodePin;
						}
					}
					return (UEdGraphPin*)nullptr;
				};

				UEdGraphPin* const AlwaysSpawnLiteralNodeInputPin = FindEnumInputPin(AlwaysSpawnLiteralNode);
				UEdGraphPin* const AdjustIfNecessaryLiteralInputPin = FindEnumInputPin(AdjustIfNecessaryLiteralNode);

				TArray<UEdGraphPin*> SelectOptionPins;
				SelectNode->GetOptionPins(SelectOptionPins);
				UEdGraphPin* const SelectIndexPin = SelectNode->GetIndexPin();

				auto FindResultPin = [](UK2Node const* Node)
				{
					for (UEdGraphPin* NodePin : Node->Pins)
					{
						if (EEdGraphPinDirection::EGPD_Output == NodePin->Direction)
						{
							return NodePin;
						}
					}
					return (UEdGraphPin*)nullptr;
				};
				UEdGraphPin* const AlwaysSpawnLiteralNodeResultPin = FindResultPin(AlwaysSpawnLiteralNode);
				check(AlwaysSpawnLiteralNodeResultPin);
				UEdGraphPin* const AdjustIfNecessaryLiteralResultPin = FindResultPin(AdjustIfNecessaryLiteralNode);
				check(AdjustIfNecessaryLiteralResultPin);

				UEdGraphPin* const OldBoolPin = Pin->LinkedTo[0];
				check(OldBoolPin);

				//
				// now set data and links that we want to set
				//

				AlwaysSpawnLiteralNodeInputPin->DefaultValue = MethodEnum->GetEnumName(static_cast<int>(ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
				AdjustIfNecessaryLiteralInputPin->DefaultValue = MethodEnum->GetEnumName(static_cast<int>(ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding));

				OldBoolPin->BreakLinkTo(Pin);
				OldBoolPin->MakeLinkTo(SelectIndexPin);

				AlwaysSpawnLiteralNodeResultPin->MakeLinkTo(SelectOptionPins[0]);
				AdjustIfNecessaryLiteralResultPin->MakeLinkTo(SelectOptionPins[1]);
				
				UEdGraphPin* const SelectOutputPin = SelectNode->GetReturnValuePin();
				check(SelectOutputPin);
				SelectOutputPin->MakeLinkTo(CollisionHandlingOverridePin);

				// tell select node to update its wildcard status
				SelectNode->NotifyPinConnectionListChanged(SelectIndexPin);
				SelectNode->NotifyPinConnectionListChanged(SelectOptionPins[0]);
				SelectNode->NotifyPinConnectionListChanged(SelectOptionPins[1]);
				SelectNode->NotifyPinConnectionListChanged(SelectOutputPin);

			}
		}
	}
}


void UK2Node_SpawnActorFromClass::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) 
{
	AllocateDefaultPins();
	UClass* UseSpawnClass = GetClassToSpawn(&OldPins);
	if( UseSpawnClass != NULL )
	{
		TArray<UEdGraphPin*> ClassPins;
		CreatePinsForClass(UseSpawnClass, ClassPins);
	}

	MaybeUpdateCollisionPin(OldPins);
	RestoreSplitPins(OldPins);
}

void UK2Node_SpawnActorFromClass::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();

	UClass* UseSpawnClass = GetClassToSpawn();
	if( UseSpawnClass != NULL )
	{
		TArray<UEdGraphPin*> ClassPins;
		CreatePinsForClass(UseSpawnClass, ClassPins);
	}
}

bool UK2Node_SpawnActorFromClass::IsSpawnVarPin(UEdGraphPin* Pin)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* ParentPin = Pin->ParentPin;
	while (ParentPin)
	{
		if (ParentPin->PinName == FK2Node_SpawnActorFromClassHelper::SpawnTransformPinName)
		{
			return false;
		}
		ParentPin = ParentPin->ParentPin;
	}

	return(	Pin->PinName != K2Schema->PN_Execute &&
			Pin->PinName != K2Schema->PN_Then &&
			Pin->PinName != K2Schema->PN_ReturnValue &&
			Pin->PinName != FK2Node_SpawnActorFromClassHelper::ClassPinName &&
			Pin->PinName != FK2Node_SpawnActorFromClassHelper::WorldContextPinName &&
			Pin->PinName != FK2Node_SpawnActorFromClassHelper::CollisionHandlingOverridePinName &&
			Pin->PinName != FK2Node_SpawnActorFromClassHelper::SpawnTransformPinName && 
			Pin->PinName != FK2Node_SpawnActorFromClassHelper::OwnerPinName );
}

void UK2Node_SpawnActorFromClass::OnClassPinChanged()
{
 	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Remove all pins related to archetype variables
	TArray<UEdGraphPin*> OldPins = Pins;
	TArray<UEdGraphPin*> OldClassPins;

	for (int32 i = 0; i < OldPins.Num(); i++)
	{
		UEdGraphPin* OldPin = OldPins[i];
		if (IsSpawnVarPin(OldPin))
		{
			Pins.Remove(OldPin);
			OldClassPins.Add(OldPin);
		}
	}

	CachedNodeTitle.MarkDirty();

	UClass* UseSpawnClass = GetClassToSpawn();
	TArray<UEdGraphPin*> NewClassPins;
	if (UseSpawnClass != NULL)
	{
		CreatePinsForClass(UseSpawnClass, NewClassPins);
	}

	UEdGraphPin* ResultPin = GetResultPin();
	// Cache all the pin connections to the ResultPin, we will attempt to recreate them
	TArray<UEdGraphPin*> ResultPinConnectionList = ResultPin->LinkedTo;
	// Because the archetype has changed, we break the output link as the output pin type will change
	ResultPin->BreakAllPinLinks();

	// Recreate any pin links to the Result pin that are still valid
	for (UEdGraphPin* Connections : ResultPinConnectionList)
	{
		K2Schema->TryCreateConnection(ResultPin, Connections);
	}

	// Rewire the old pins to the new pins so connections are maintained if possible
	RewireOldPinsToNewPins(OldClassPins, NewClassPins);

	// Destroy the old pins
	DestroyPinList(OldClassPins);

	// Refresh the UI for the graph so the pin changes show up
	UEdGraph* Graph = GetGraph();
	Graph->NotifyGraphChanged();

	// Mark dirty
	FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
}

void UK2Node_SpawnActorFromClass::PinConnectionListChanged(UEdGraphPin* ChangedPin)
{
	if (ChangedPin && (ChangedPin->PinName == FK2Node_SpawnActorFromClassHelper::ClassPinName))
	{
		OnClassPinChanged();
	}
}

void UK2Node_SpawnActorFromClass::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	if (UEdGraphPin* ClassPin = GetClassPin())
	{
		K2Schema->ConstructBasicPinTooltip(*ClassPin, LOCTEXT("ClassPinDescription", "The Actor class you want to spawn"), ClassPin->PinToolTip);
	}
	if (UEdGraphPin* TransformPin = GetSpawnTransformPin())
	{
		K2Schema->ConstructBasicPinTooltip(*TransformPin, LOCTEXT("TransformPinDescription", "The transform to spawn the Actor with"), TransformPin->PinToolTip);
	}
	if (UEdGraphPin* CollisionHandlingOverridePin = GetCollisionHandlingOverridePin())
	{
		K2Schema->ConstructBasicPinTooltip(*CollisionHandlingOverridePin, LOCTEXT("CollisionHandlingOverridePinDescription", "Specifies how to handle collisions at the spawn point. If undefined, uses actor class settings."), CollisionHandlingOverridePin->PinToolTip);
	}
	if (UEdGraphPin* OwnerPin = GetOwnerPin())
	{
		K2Schema->ConstructBasicPinTooltip(*OwnerPin, LOCTEXT("OwnerPinDescription", "Can be left empty; primarily used for replication (bNetUseOwnerRelevancy and bOnlyRelevantToOwner), or visibility (PrimitiveComponent's bOwnerNoSee/bOnlyOwnerSee)"), OwnerPin->PinToolTip);
	}
	if (UEdGraphPin* ResultPin = GetResultPin())
	{
		K2Schema->ConstructBasicPinTooltip(*ResultPin, LOCTEXT("ResultPinDescription", "The spawned Actor"), ResultPin->PinToolTip);
	}

	return Super::GetPinHoverText(Pin, HoverTextOut);
}

void UK2Node_SpawnActorFromClass::PinDefaultValueChanged(UEdGraphPin* ChangedPin) 
{
	if (ChangedPin && (ChangedPin->PinName == FK2Node_SpawnActorFromClassHelper::ClassPinName))
	{
		OnClassPinChanged();
	}
}

FText UK2Node_SpawnActorFromClass::GetTooltipText() const
{
	return NodeTooltip;
}

FSlateIcon UK2Node_SpawnActorFromClass::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.SpawnActor_16x");
	return Icon;
}

UEdGraphPin* UK2Node_SpawnActorFromClass::GetThenPin()const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPinChecked(K2Schema->PN_Then);
	check(Pin->Direction == EGPD_Output);
	return Pin;
}

UEdGraphPin* UK2Node_SpawnActorFromClass::GetClassPin(const TArray<UEdGraphPin*>* InPinsToSearch /*= NULL*/) const
{
	const TArray<UEdGraphPin*>* PinsToSearch = InPinsToSearch ? InPinsToSearch : &Pins;

	UEdGraphPin* Pin = NULL;
	for( auto PinIt = PinsToSearch->CreateConstIterator(); PinIt; ++PinIt )
	{
		UEdGraphPin* TestPin = *PinIt;
		if( TestPin && TestPin->PinName == FK2Node_SpawnActorFromClassHelper::ClassPinName )
		{
			Pin = TestPin;
			break;
		}
	}
	check(Pin == NULL || Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin* UK2Node_SpawnActorFromClass::GetSpawnTransformPin() const
{
	UEdGraphPin* Pin = FindPinChecked(FK2Node_SpawnActorFromClassHelper::SpawnTransformPinName);
	check(Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin* UK2Node_SpawnActorFromClass::GetCollisionHandlingOverridePin() const
{
	UEdGraphPin* const Pin = FindPinChecked(FK2Node_SpawnActorFromClassHelper::CollisionHandlingOverridePinName);
	check(Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin* UK2Node_SpawnActorFromClass::GetOwnerPin() const
{
	UEdGraphPin* Pin = FindPin(FK2Node_SpawnActorFromClassHelper::OwnerPinName);
	check(Pin == NULL || Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin* UK2Node_SpawnActorFromClass::GetWorldContextPin() const
{
	UEdGraphPin* Pin = FindPin(FK2Node_SpawnActorFromClassHelper::WorldContextPinName);
	check(Pin == NULL || Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin* UK2Node_SpawnActorFromClass::GetResultPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPinChecked(K2Schema->PN_ReturnValue);
	check(Pin->Direction == EGPD_Output);
	return Pin;
}

FLinearColor UK2Node_SpawnActorFromClass::GetNodeTitleColor() const
{
	return Super::GetNodeTitleColor();
}


FText UK2Node_SpawnActorFromClass::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FText NodeTitle = NSLOCTEXT("K2Node", "SpawnActor_BaseTitle", "Spawn Actor from Class");
	if (TitleType != ENodeTitleType::MenuTitle)
	{
		if (UEdGraphPin* ClassPin = GetClassPin())
		{
			if (ClassPin->LinkedTo.Num() > 0)
			{
				// Blueprint will be determined dynamically, so we don't have the name in this case
				NodeTitle = NSLOCTEXT("K2Node", "SpawnActor_Title_Unknown", "SpawnActor");
			}
			else if (ClassPin->DefaultObject == nullptr)
			{
				NodeTitle = NSLOCTEXT("K2Node", "SpawnActor_Title_NONE", "SpawnActor NONE");
			}
			else
			{
				if (CachedNodeTitle.IsOutOfDate(this))
				{
					FText ClassName;
					if (UClass* PickedClass = Cast<UClass>(ClassPin->DefaultObject))
					{
						ClassName = PickedClass->GetDisplayNameText();
					}

					FFormatNamedArguments Args;
					Args.Add(TEXT("ClassName"), ClassName);

					// FText::Format() is slow, so we cache this to save on performance
					CachedNodeTitle.SetCachedText(FText::Format(NSLOCTEXT("K2Node", "SpawnActor_Title_Class", "SpawnActor {ClassName}"), Args), this);
				}
				NodeTitle = CachedNodeTitle;
			} 
		}
		else
		{
			NodeTitle = NSLOCTEXT("K2Node", "SpawnActor_Title_NONE", "SpawnActor NONE");
		}
	}
	return NodeTitle;
}

bool UK2Node_SpawnActorFromClass::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const 
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	return Super::IsCompatibleWithGraph(TargetGraph) && (!Blueprint || (FBlueprintEditorUtils::FindUserConstructionScript(Blueprint) != TargetGraph && Blueprint->GeneratedClass->GetDefaultObject()->ImplementsGetWorld()));
}

void UK2Node_SpawnActorFromClass::GetNodeAttributes( TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes ) const
{
	UClass* ClassToSpawn = GetClassToSpawn();
	const FString ClassToSpawnStr = ClassToSpawn ? ClassToSpawn->GetName() : TEXT( "InvalidClass" );
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Type" ), TEXT( "SpawnActorFromClass" ) ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Class" ), GetClass()->GetName() ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Name" ), GetName() ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "ActorClass" ), ClassToSpawnStr ));
}

void UK2Node_SpawnActorFromClass::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

FText UK2Node_SpawnActorFromClass::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Gameplay);
}

FNodeHandlingFunctor* UK2Node_SpawnActorFromClass::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FNodeHandlingFunctor(CompilerContext);
}

void UK2Node_SpawnActorFromClass::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	static FName BeginSpawningBlueprintFuncName = GET_FUNCTION_NAME_CHECKED(UGameplayStatics, BeginDeferredActorSpawnFromClass);
	static FString ActorClassParamName = FString(TEXT("ActorClass"));
	static FString WorldContextParamName = FString(TEXT("WorldContextObject"));

	static FName FinishSpawningFuncName = GET_FUNCTION_NAME_CHECKED(UGameplayStatics, FinishSpawningActor);
	static FString ActorParamName = FString(TEXT("Actor"));
	static FString TransformParamName = FString(TEXT("SpawnTransform"));
	static FString CollisionHandlingOverrideParamName = FString(TEXT("CollisionHandlingOverride"));
	static FString OwnerParamName = FString(TEXT("Owner"));

	static FString ObjectParamName = FString(TEXT("Object"));
	static FString ValueParamName = FString(TEXT("Value"));
	static FString PropertyNameParamName = FString(TEXT("PropertyName"));

	UK2Node_SpawnActorFromClass* SpawnNode = this;
	UEdGraphPin* SpawnNodeExec = SpawnNode->GetExecPin();
	UEdGraphPin* SpawnNodeTransform = SpawnNode->GetSpawnTransformPin();
	UEdGraphPin* SpawnNodeCollisionHandlingOverride = GetCollisionHandlingOverridePin();
	UEdGraphPin* SpawnWorldContextPin = SpawnNode->GetWorldContextPin();
	UEdGraphPin* SpawnClassPin = SpawnNode->GetClassPin();
	UEdGraphPin* SpawnNodeOwnerPin = SpawnNode->GetOwnerPin();
	UEdGraphPin* SpawnNodeThen = SpawnNode->GetThenPin();
	UEdGraphPin* SpawnNodeResult = SpawnNode->GetResultPin();

	// Cache the class to spawn. Note, this is the compile time class that the pin was set to or the variable type it was connected to. Runtime it could be a child.
	UClass* ClassToSpawn = GetClassToSpawn();

	UClass* SpawnClass = (SpawnClassPin != NULL) ? Cast<UClass>(SpawnClassPin->DefaultObject) : NULL;
	if((0 == SpawnClassPin->LinkedTo.Num()) && (NULL == SpawnClass))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("SpawnActorNodeMissingClass_Error", "Spawn node @@ must have a @@ specified.").ToString(), SpawnNode, SpawnClassPin);
		// we break exec links so this is the only error we get, don't want the SpawnActor node being considered and giving 'unexpected node' type warnings
		SpawnNode->BreakAllNodeLinks();
		return;
	}

	//////////////////////////////////////////////////////////////////////////
	// create 'begin spawn' call node
	UK2Node_CallFunction* CallBeginSpawnNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SpawnNode, SourceGraph);
	CallBeginSpawnNode->FunctionReference.SetExternalMember(BeginSpawningBlueprintFuncName, UGameplayStatics::StaticClass());
	CallBeginSpawnNode->AllocateDefaultPins();

	UEdGraphPin* CallBeginExec = CallBeginSpawnNode->GetExecPin();
	UEdGraphPin* CallBeginWorldContextPin = CallBeginSpawnNode->FindPinChecked(WorldContextParamName);
	UEdGraphPin* CallBeginActorClassPin = CallBeginSpawnNode->FindPinChecked(ActorClassParamName);
	UEdGraphPin* CallBeginTransform = CallBeginSpawnNode->FindPinChecked(TransformParamName);
	UEdGraphPin* CallBeginCollisionHandlingOverride = CallBeginSpawnNode->FindPinChecked(CollisionHandlingOverrideParamName);

	UEdGraphPin* CallBeginOwnerPin = CallBeginSpawnNode->FindPinChecked(FK2Node_SpawnActorFromClassHelper::OwnerPinName);
	UEdGraphPin* CallBeginResult = CallBeginSpawnNode->GetReturnValuePin();	

	// Move 'exec' connection from spawn node to 'begin spawn'
	CompilerContext.MovePinLinksToIntermediate(*SpawnNodeExec, *CallBeginExec);

	if(SpawnClassPin->LinkedTo.Num() > 0)
	{
		// Copy the 'blueprint' connection from the spawn node to 'begin spawn'
		CompilerContext.MovePinLinksToIntermediate(*SpawnClassPin, *CallBeginActorClassPin);
	}
	else
	{
		// Copy blueprint literal onto begin spawn call 
		CallBeginActorClassPin->DefaultObject = SpawnClass;
	}

	// Copy the world context connection from the spawn node to 'begin spawn' if necessary
	if (SpawnWorldContextPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*SpawnWorldContextPin, *CallBeginWorldContextPin);
	}

	if (SpawnNodeOwnerPin != nullptr)
	{
		CompilerContext.MovePinLinksToIntermediate(*SpawnNodeOwnerPin, *CallBeginOwnerPin);
	}

	// Copy the 'transform' connection from the spawn node to 'begin spawn'
	CompilerContext.MovePinLinksToIntermediate(*SpawnNodeTransform, *CallBeginTransform);

	// Copy the 'bNoCollisionFail' connection from the spawn node to 'begin spawn'
	CompilerContext.MovePinLinksToIntermediate(*SpawnNodeCollisionHandlingOverride, *CallBeginCollisionHandlingOverride);

	//////////////////////////////////////////////////////////////////////////
	// create 'finish spawn' call node
	UK2Node_CallFunction* CallFinishSpawnNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SpawnNode, SourceGraph);
	CallFinishSpawnNode->FunctionReference.SetExternalMember(FinishSpawningFuncName, UGameplayStatics::StaticClass());
	CallFinishSpawnNode->AllocateDefaultPins();

	UEdGraphPin* CallFinishExec = CallFinishSpawnNode->GetExecPin();
	UEdGraphPin* CallFinishThen = CallFinishSpawnNode->GetThenPin();
	UEdGraphPin* CallFinishActor = CallFinishSpawnNode->FindPinChecked(ActorParamName);
	UEdGraphPin* CallFinishTransform = CallFinishSpawnNode->FindPinChecked(TransformParamName);
	UEdGraphPin* CallFinishResult = CallFinishSpawnNode->GetReturnValuePin();

	// Move 'then' connection from spawn node to 'finish spawn'
	CompilerContext.MovePinLinksToIntermediate(*SpawnNodeThen, *CallFinishThen);

	// Copy transform connection
	CompilerContext.CopyPinLinksToIntermediate(*CallBeginTransform, *CallFinishTransform);

	// Connect output actor from 'begin' to 'finish'
	CallBeginResult->MakeLinkTo(CallFinishActor);

	// Move result connection from spawn node to 'finish spawn'
	CallFinishResult->PinType = SpawnNodeResult->PinType; // Copy type so it uses the right actor subclass
	CompilerContext.MovePinLinksToIntermediate(*SpawnNodeResult, *CallFinishResult);

	//////////////////////////////////////////////////////////////////////////
	// create 'set var' nodes

	// Get 'result' pin from 'begin spawn', this is the actual actor we want to set properties on
	UEdGraphPin* LastThen = FKismetCompilerUtilities::GenerateAssignmentNodes(CompilerContext, SourceGraph, CallBeginSpawnNode, SpawnNode, CallBeginResult, ClassToSpawn );

	// Make exec connection between 'then' on last node and 'finish'
	LastThen->MakeLinkTo(CallFinishExec);

	// Break any links to the expanded node
	SpawnNode->BreakAllNodeLinks();
}

bool UK2Node_SpawnActorFromClass::HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const
{
	UClass* SourceClass = GetClassToSpawn();
	const UBlueprint* SourceBlueprint = GetBlueprint();
	const bool bResult = (SourceClass != NULL) && (SourceClass->ClassGeneratedBy != SourceBlueprint);
	if (bResult && OptionalOutput)
	{
		OptionalOutput->AddUnique(SourceClass);
	}
	const bool bSuperResult = Super::HasExternalDependencies(OptionalOutput);
	return bSuperResult || bResult;
}

#undef LOCTEXT_NAMESPACE