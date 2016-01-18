// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorPrivatePCH.h"

#include "Kismet/KismetMathLibrary.h"
#include "BlueprintGraphDefinitions.h"
#include "BlueprintUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"

#define LOCTEXT_NAMESPACE "UK2Node_LiveEditObject"

namespace UK2Node_LiveEditObjectStatics
{
	static const FString WorldContextPinName(TEXT("WorldContextObject"));
	static const FString BlueprintPinName(TEXT("Blueprint"));
	static const FString BaseClassPinName(TEXT("Base Class"));
	static const FString VariablePinName(TEXT("Variable"));
	static const FString DescriptionPinName(TEXT("Description"));
	static const FString PermittedBindingsPinName(TEXT("PermittedBindings"));
	static const FString EventPinName(TEXT("On MIDI Input"));
	static const FString DeltaMultPinName(TEXT("Delta Multiplier"));
	static const FString ShouldClampPinName(TEXT("Clamp Values"));
	static const FString ClampMinPinName(TEXT("Clamp Min"));
	static const FString ClampMaxPinName(TEXT("Clamp Max"));

	static UProperty *GetPropertyByNameRecurse( UStruct *InStruct, const FString &TokenString )
	{
		FString FirstToken;
		FString RemainingTokens;
		int32 SplitIndex;
		if ( TokenString.FindChar( '.', SplitIndex ) )
		{
			FirstToken = TokenString.LeftChop( TokenString.Len()-SplitIndex );
			RemainingTokens = TokenString.RightChop(SplitIndex+1);
		}
		else
		{
			FirstToken = TokenString;
			RemainingTokens = FString(TEXT(""));
		}

		if ( FirstToken.FindChar( '[', SplitIndex ) )
		{
			FirstToken = FirstToken.LeftChop( FirstToken.Len()-SplitIndex );
		}

		for (TFieldIterator<UProperty> PropertyIt(InStruct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			FName PropertyName = Property->GetFName();
			if ( FirstToken == PropertyName.ToString() )
			{
				if ( RemainingTokens.Len() == 0 )
				{
					return Property;
				}
				else
				{
					UStructProperty *StructProp = Cast<UStructProperty>(Property);
					if ( StructProp )
					{
						return GetPropertyByNameRecurse( StructProp->Struct, RemainingTokens );
					}
				}
			}
		}

		return NULL;
	}
	static UProperty *GetPropertyByName( UClass *InClass, const FName &Name )
	{
		if ( InClass == NULL )
			return NULL;

		UProperty *Property = GetPropertyByNameRecurse( InClass, Name.ToString() );
		if ( Property != NULL )
		{
			return Property;
		}

		AActor *AsActor = Cast<AActor>(InClass->ClassDefaultObject);
		if ( AsActor != NULL )
		{
			FString ComponentPropertyName = Name.ToString();
			int32 SplitIndex = 0;
			if ( ComponentPropertyName.FindChar( '.', SplitIndex ) )
			{
				//FString ComponentName = ComponentPropertyName.LeftChop(SplitIndex);
				ComponentPropertyName = ComponentPropertyName.RightChop(SplitIndex+1);

				TInlineComponentArray<UActorComponent*> ActorComponents;
				AsActor->GetComponents(ActorComponents);
				for ( auto ComponentIt = ActorComponents.CreateIterator(); ComponentIt; ++ComponentIt )
				{
					UActorComponent *Component = *ComponentIt;
					check( Component != NULL );
					/*
					if ( Component->GetName() != ComponentName )
					{
						continue;
					}
					*/

					Property = GetPropertyByNameRecurse( Component->GetClass(), ComponentPropertyName );
					if ( Property != NULL )
					{
						return Property;
					}
				}
			}
		}

		return NULL;
	}

	static bool SearchForConvertPinName( const UEdGraphSchema_K2 *Schema, const UEdGraphPin *InputPin, FName &TargetFunction )
	{
		if ( InputPin->PinType.PinCategory == Schema->PC_Int )
		{
			TargetFunction = TEXT("InInt");
			return true;
		}
		else if ( InputPin->PinType.PinCategory == Schema->PC_Float )
		{
			TargetFunction = TEXT("InFloat");
			return true;
		}
		else if ( InputPin->PinType.PinCategory == Schema->PC_Byte )
		{
			TargetFunction = TEXT("InByte");
			return true;
		}

		return false;
	}
}

FString UK2Node_LiveEditObject::LiveEditableVarPinCategory(TEXT("LiveEditableVar"));


UK2Node_LiveEditObject::UK2Node_LiveEditObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_LiveEditObject::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Add execution pins
	CreatePin(EGPD_Input, K2Schema->PC_Exec, TEXT(""), NULL, false, false, K2Schema->PN_Execute);
	CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, K2Schema->PN_Then);

	// If required add the world context pin
	if (GetBlueprint()->ParentClass->HasMetaData(FBlueprintMetadata::MD_ShowWorldContextPin))
	{
		CreatePin(EGPD_Input, K2Schema->PC_Object, TEXT(""), UObject::StaticClass(), false, false, UK2Node_LiveEditObjectStatics::WorldContextPinName);
	}

	// Add blueprint pin
	UEdGraphPin* BlueprintPin = CreatePin(EGPD_Input, K2Schema->PC_Object, TEXT(""), UBlueprint::StaticClass(), false, false, UK2Node_LiveEditObjectStatics::BlueprintPinName);
	SetPinToolTip(*BlueprintPin, LOCTEXT("BlueprintPinDescription", "The Blueprint Actor whose ClassDefaultObject you want to Live Edit"));

	// Add base class pin
	UEdGraphPin* BaseClassPin = CreatePin(EGPD_Input, K2Schema->PC_Class, TEXT(""), UObject::StaticClass(), false, false, UK2Node_LiveEditObjectStatics::BaseClassPinName);
	SetPinToolTip(*BaseClassPin, LOCTEXT("BaseClassPinDescription", "The BaseClass that this node will know how to tune"));
#if WITH_EDITORONLY_DATA
	BaseClassPin->bNotConnectable = true;
#endif

	UEdGraphPin *DescriptionPin = CreatePin(EGPD_Input, K2Schema->PC_Text, TEXT(""), NULL, false, false, UK2Node_LiveEditObjectStatics::DescriptionPinName);
	SetPinToolTip(*DescriptionPin, LOCTEXT("DescriptionPinDescription", "Description, for binding wizard, of what this tunes"));

	UEnum * ControllerTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ELiveEditControllerType"), true);	
	UEdGraphPin *PermittedBindingsPin = CreatePin(EGPD_Input, K2Schema->PC_Byte, TEXT(""), ControllerTypeEnum, true, true, UK2Node_LiveEditObjectStatics::PermittedBindingsPinName);
	SetPinToolTip(*PermittedBindingsPin, LOCTEXT("PermittedBindingsPinDescription", "Restricts what types of MIDI Controls a user is allowed to bind to this tuning hook"));

	//output pin
	UEdGraphPin* OnMidiInputPin = CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, UK2Node_LiveEditObjectStatics::EventPinName);
	SetPinToolTip(*OnMidiInputPin, LOCTEXT("OnMidiInputPinDescription", "This output will fire after your value is tuned. Provided so you can do fallout logic of a value's change"));

	Super::AllocateDefaultPins();
}

FText UK2Node_LiveEditObject::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UEdGraphPin* BaseClassPin = GetBaseClassPin();
	if ((BaseClassPin == nullptr) || (BaseClassPin->DefaultObject == nullptr))
	{
		return NSLOCTEXT("K2Node", "LiveEditObject_NullTitle", "LiveEditObject NONE");
	}
	else if (CachedNodeTitle.IsOutOfDate(this))
	{
		FNumberFormattingOptions NumberOptions;
		NumberOptions.UseGrouping = false;
		FFormatNamedArguments Args;
		Args.Add(TEXT("SpawnString"), FText::FromName(BaseClassPin->DefaultObject->GetFName()));
		Args.Add(TEXT("ID"), FText::AsNumber(GetUniqueID(), &NumberOptions));

		CachedNodeTitle.SetCachedText(FText::Format(NSLOCTEXT("K2Node", "LiveEditObject", "LiveEditObject {SpawnString}_{ID}"), Args), this);
	}
	return CachedNodeTitle;
}

void UK2Node_LiveEditObject::PinDefaultValueChanged(UEdGraphPin* Pin) 
{
	if(Pin->PinName == UK2Node_LiveEditObjectStatics::BaseClassPinName)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		// Remove all pins related to archetype variables
		TArray<UEdGraphPin*> OldPins = Pins;
		for(int32 i=0; i<OldPins.Num(); i++)
		{
			UEdGraphPin* OldPin = OldPins[i];
			if(	IsSpawnVarPin(OldPin) )
			{
				Pin->BreakAllPinLinks();
				Pins.Remove(OldPin);
			}
		}

		UClass* UseSpawnClass = GetClassToSpawn();
		if(UseSpawnClass != NULL)
		{
			CreatePinsForClass(UseSpawnClass);
		}

		// Refresh the UI for the graph so the pin changes show up
		UEdGraph* Graph = GetGraph();
		Graph->NotifyGraphChanged();

		// Mark dirty
		FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
	}
}

void UK2Node_LiveEditObject::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) 
{
	AllocateDefaultPins();
	UClass* UseSpawnClass = GetClassToSpawn(&OldPins);
	if( UseSpawnClass != NULL )
	{
		CreatePinsForClass(UseSpawnClass);
	}
}

FNodeHandlingFunctor* UK2Node_LiveEditObject::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FNodeHandlingFunctor(CompilerContext);
}

void UK2Node_LiveEditObject::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

	UEdGraphPin *SourceExecPin = GetExecPin();
	UEdGraphPin *SourceThenPin = GetThenPin();
	UEdGraphPin *SourceBlueprintPin = GetBlueprintPin();
	UEdGraphPin *SourceBaseClassPin = GetBaseClassPin();
	UEdGraphPin *SourceDescriptionPin = GetDescriptionPin();
	UEdGraphPin *SourcePermittedBindingsPin = GetPermittedBindingsPin();
	UEdGraphPin *SourceOnMidiInputPin = GetOnMidiInputPin();

	UEdGraphPin *SourceVariablePin = GetVariablePin();
	if(NULL == SourceVariablePin)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("LiveEditObjectNodeMissingBlueprint_Error", "LiveEdit node @@ must have a blueprint specified and a variable selected to tune.").ToString(), this);
		// we break exec links so this is the only error we get, don't want the SpawnActor node being considered and giving 'unexpected node' type warnings
		BreakAllNodeLinks();
		return;
	}

	UClass* SpawnClass = GetClassToSpawn();
	if(NULL == SpawnClass)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("LiveEditObjectNodeMissingBaseClass_Error", "LiveEdit node @@ must have a Base Class specified.").ToString(), this);
		// we break exec links so this is the only error we get, don't want the SpawnActor node being considered and giving 'unexpected node' type warnings
		BreakAllNodeLinks();
		return;
	}

	if ( SourcePermittedBindingsPin->LinkedTo.Num() == 0 )
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("LiveEditObjectNodeMissingBinding_Error", "LiveEdit node @@ must specify Permitted Bindings.").ToString(), this);
		// we break exec links so this is the only error we get, don't want the SpawnActor node being considered and giving 'unexpected node' type warnings
		BreakAllNodeLinks();
		return;
	}

	//sanity check the VariablePin value
	{
		UProperty *Property = UK2Node_LiveEditObjectStatics::GetPropertyByName( SpawnClass, *SourceVariablePin->DefaultValue );
		if ( Property == NULL || !Property->IsA(UNumericProperty::StaticClass()) )
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("LiveEditObjectNodeInvalidVariable_Error", "LiveEdit node @@ must have a valid variable selected.").ToString(), this);
			// we break exec links so this is the only error we get, don't want the SpawnActor node being considered and giving 'unexpected node' type warnings
			BreakAllNodeLinks();
			return;
		}
	}

	//hooks to pins that are generated after a BaseClass is set
	UEdGraphPin *DeltaMultPin = GetDeltaMultPin();
	UEdGraphPin *ShouldClampPin = GetShouldClampPin();
	UEdGraphPin *ClampMinPin = GetClampMinPin();
	UEdGraphPin *ClampMaxPin = GetClampMaxPin();

	UK2Node_Self *SelfNode  = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(this,SourceGraph);
	SelfNode->AllocateDefaultPins();
	UEdGraphPin *SelfNodeThenPin = SelfNode->FindPinChecked(Schema->PN_Self);

	FString EventNameGuid = GetEventName();
		
	//Create the registration part of the LiveEditor binding process
	{
		UK2Node_CallFunction *RegisterForMIDINode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,SourceGraph);
		RegisterForMIDINode->FunctionReference.SetExternalMember( TEXT("RegisterForLiveEditEvent"), ULiveEditorKismetLibrary::StaticClass() );
		RegisterForMIDINode->AllocateDefaultPins();

		UEdGraphPin *ExecPin = RegisterForMIDINode->GetExecPin();
		CompilerContext.MovePinLinksToIntermediate(*SourceExecPin, *ExecPin);

		UEdGraphPin *ThenPin = RegisterForMIDINode->GetThenPin();
		CompilerContext.MovePinLinksToIntermediate(*SourceThenPin, *ThenPin);

		UEdGraphPin *TargetPin = RegisterForMIDINode->FindPinChecked( FString(TEXT("Target")) );
		TargetPin->MakeLinkTo(SelfNodeThenPin);

		UEdGraphPin *EventNamePin = RegisterForMIDINode->FindPinChecked( FString(TEXT("EventName")) );
		EventNamePin->DefaultValue = EventNameGuid;
		
		UEdGraphPin *DescriptionPin = RegisterForMIDINode->FindPinChecked( FString(TEXT("Description")) );
		CompilerContext.CopyPinLinksToIntermediate( *SourceDescriptionPin, *DescriptionPin);

		UEdGraphPin *PermittedBindingsPin = RegisterForMIDINode->FindPinChecked( FString(TEXT("PermittedBindings")) );
		CompilerContext.CopyPinLinksToIntermediate( *SourcePermittedBindingsPin, *PermittedBindingsPin);
	}

	//Create the event handling part of the LiveEditor binding process
	{
		//
		//the event itself
		//
		UFunction *EventMIDISignature = GetEventMIDISignature();
		UK2Node_Event* EventNode = CompilerContext.SpawnIntermediateNode<UK2Node_Event>(this, SourceGraph);
		check(EventNode);
		EventNode->EventReference.SetFromField<UFunction>(EventMIDISignature, false);
		EventNode->CustomFunctionName = *EventNameGuid;
		EventNode->bInternalEvent = true;
		EventNode->AllocateDefaultPins();

		// Cache these out because we'll connect the sequence to it
		UEdGraphPin *EventThenPin = EventNode->FindPinChecked( Schema->PN_Then );
		UEdGraphPin *EventDeltaPin = EventNode->FindPinChecked( FString(TEXT("Delta")) );
		UEdGraphPin *EventMidiValuePin = EventNode->FindPinChecked( FString(TEXT("MidiValue")) );
		UEdGraphPin *EventControlTypePin = EventNode->FindPinChecked( FString(TEXT("ControlType")) );


		//
		// Check if Blueprint is NULL
		//
		UEdGraphPin *CompareBlueprintToNullBranchThenPin = NULL;
		{
			UK2Node_CallFunction *CompareBlueprintToNullNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,SourceGraph);
			CompareBlueprintToNullNode->FunctionReference.SetExternalMember( TEXT("NotEqual_ObjectObject"), UKismetMathLibrary::StaticClass() );
			CompareBlueprintToNullNode->AllocateDefaultPins();

			//Set A Pin to the Blueprint Pin
			UEdGraphPin *CompareBlueprintToNullAPin = CompareBlueprintToNullNode->FindPinChecked( FString(TEXT("A")) );
			CompilerContext.CopyPinLinksToIntermediate( *SourceBlueprintPin, *CompareBlueprintToNullAPin);

			// hook for Compare Blueprint to NULL result
			UEdGraphPin *CompareBlueprintToNullResultPin = CompareBlueprintToNullNode->GetReturnValuePin();

			// Create the BRANCH that will drive the comparison
			UK2Node_IfThenElse* CompareBlueprintToNullBranchNode = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
			CompareBlueprintToNullBranchNode->AllocateDefaultPins();

			//hook up the condition
			CompareBlueprintToNullResultPin->MakeLinkTo( CompareBlueprintToNullBranchNode->GetConditionPin() );

			//hook event to the branck input
			EventThenPin->MakeLinkTo( CompareBlueprintToNullBranchNode->GetExecPin() );

			//cache ot the THEN pin for later linkup
			CompareBlueprintToNullBranchThenPin = CompareBlueprintToNullBranchNode->GetThenPin();
		}

		//
		// Get Class Default Object
		//
		UK2Node_CallFunction *GetClassDefaultObjectNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,SourceGraph);
		GetClassDefaultObjectNode->FunctionReference.SetExternalMember( TEXT("GetBlueprintClassDefaultObject"), ULiveEditorKismetLibrary::StaticClass() );
		GetClassDefaultObjectNode->AllocateDefaultPins();

		UEdGraphPin *GetClassDefaultObjectBlueprintPin = GetClassDefaultObjectNode->FindPinChecked( TEXT("Blueprint") );
		CompilerContext.CopyPinLinksToIntermediate( *SourceBlueprintPin, *GetClassDefaultObjectBlueprintPin);

		//hook for later -> the pointer to the ClassDefaultObject of our BlueprintPin
		UEdGraphPin *GetClassDefaultObjectResultPin = GetClassDefaultObjectNode->GetReturnValuePin();


		//
		// Compare to BaseClass to make sure that the target Blueprint IsA(BaseClass)
		//
		UEdGraphPin *ClassIsChildOfBranchThenPin = NULL;
		{
			//
			//we need to get the class of the Blueprint pin
			UK2Node_CallFunction *GetClassNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,SourceGraph);
			GetClassNode->FunctionReference.SetExternalMember( TEXT("GetObjectClass"), UGameplayStatics::StaticClass() );
			GetClassNode->AllocateDefaultPins();

			//Pin in the GetClassDefaultObjectResultPin to the Object Parameter of the GetObjectClass FUNCTION
			//we want to make sure that the Class of the DEFAULT_OBJECT IsA( BaseClass )
			UEdGraphPin *GetClassObjectPin = GetClassNode->FindPinChecked( FString(TEXT("Object")) );
			GetClassDefaultObjectResultPin->MakeLinkTo( GetClassObjectPin );

			//hook for the Class result
			UEdGraphPin *GetClassReturnValuePin = GetClassNode->GetReturnValuePin();

			//
			//the ClassIsChildOf FUNCTION
			UK2Node_CallFunction *ClassIsChildOfNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,SourceGraph);
			ClassIsChildOfNode->FunctionReference.SetExternalMember( TEXT("ClassIsChildOf"), UKismetMathLibrary::StaticClass() );
			ClassIsChildOfNode->AllocateDefaultPins();

			//hook up the test pin
			UEdGraphPin *ClassIsChildOfTestPin = ClassIsChildOfNode->FindPinChecked( FString(TEXT("TestClass")) );
			GetClassReturnValuePin->MakeLinkTo( ClassIsChildOfTestPin );

			//copy our BaseClass Pin into the ClassIsChildOf Parameter
			UEdGraphPin *ClassIsChildOfParentPin = ClassIsChildOfNode->FindPinChecked( FString(TEXT("ParentClass")) );
			CompilerContext.CopyPinLinksToIntermediate( *SourceBaseClassPin, *ClassIsChildOfParentPin);

			//hook for return value
			UEdGraphPin *ClassIsChildOfResultPin = ClassIsChildOfNode->GetReturnValuePin();

			//
			// Create the BRANCH that will drive the comparison
			UK2Node_IfThenElse* ClassIsChildOfBranchNode = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
			ClassIsChildOfBranchNode->AllocateDefaultPins();

			//hook up the previous branch to this one
			check( CompareBlueprintToNullBranchThenPin != NULL );
			CompareBlueprintToNullBranchThenPin->MakeLinkTo( ClassIsChildOfBranchNode->GetExecPin() );

			//hook up our condition
			ClassIsChildOfResultPin->MakeLinkTo( ClassIsChildOfBranchNode->GetConditionPin() );

			//cache ot the THEN pin for later linkup
			ClassIsChildOfBranchThenPin = ClassIsChildOfBranchNode->GetThenPin();
		}


		//
		//The set variable function (to set LiveEdited new value)
		//
		UK2Node_CallFunction *ModifyVarNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,SourceGraph);
		ModifyVarNode->FunctionReference.SetExternalMember( TEXT("ModifyPropertyByName"), ULiveEditorKismetLibrary::StaticClass() );
		ModifyVarNode->AllocateDefaultPins();

		// Make link from the event to the Set variable node
		UEdGraphPin *ModifyVarExecPin = ModifyVarNode->GetExecPin();
		ClassIsChildOfBranchThenPin->MakeLinkTo( ModifyVarExecPin );

		//link up the Target Pin
		UEdGraphPin *ModifyVarNodeTargetPin = ModifyVarNode->FindPinChecked( TEXT("Target") );
		GetClassDefaultObjectResultPin->MakeLinkTo( ModifyVarNodeTargetPin );

		//link up the PropertyName Pin
		UEdGraphPin *ModifyVarNodePropertyNamePin = ModifyVarNode->FindPinChecked( TEXT("PropertyName") );
		ModifyVarNodePropertyNamePin->DefaultValue = SourceVariablePin->DefaultValue;

		//link up the MIDI Value Pin
		UEdGraphPin *ModifyVarNodeMidiValuePin = ModifyVarNode->FindPinChecked( TEXT("MidiValue") );
		EventMidiValuePin->MakeLinkTo(ModifyVarNodeMidiValuePin);

		//link up the ControlType Pin
		UEdGraphPin *ModifyVarNodeControlTypePin = ModifyVarNode->FindPinChecked( TEXT("ControlType") );
		EventControlTypePin->MakeLinkTo(ModifyVarNodeControlTypePin);

		//hook for the Delta Pin
		UEdGraphPin *ModifyVarNodeDeltaPin = ModifyVarNode->FindPinChecked( TEXT("Delta") );

		//Clamping
		if ( ShouldClampPin->DefaultValue == FString(TEXT("true")) )
		{
			UEdGraphPin *ModifyVarNodeShouldClampPin = ModifyVarNode->FindPinChecked( TEXT("bShouldClamp") );
			CompilerContext.CopyPinLinksToIntermediate( *ShouldClampPin, *ModifyVarNodeShouldClampPin);

			check( ClampMinPin != NULL );
			UEdGraphPin *ModifyVarNodeClampMinPin = ModifyVarNode->FindPinChecked( TEXT("ClampMin") );
			CompilerContext.CopyPinLinksToIntermediate( *ClampMinPin, *ModifyVarNodeClampMinPin);

			check( ClampMaxPin != NULL );
			UEdGraphPin *ModifyVarNodeClampMaxPin = ModifyVarNode->FindPinChecked( TEXT("ClampMax") );
			CompilerContext.CopyPinLinksToIntermediate( *ClampMaxPin, *ModifyVarNodeClampMaxPin);
		}

		//hook for ModifyVar THEN
		UEdGraphPin *ModifyVarNodeThenPin = ModifyVarNode->GetThenPin();

		//
		// The Multiply Delta * DeltaMult function
		//
		UK2Node_CallFunction *MultiplyNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,SourceGraph);
		MultiplyNode->FunctionReference.SetExternalMember( TEXT("Multiply_FloatFloat"), UKismetMathLibrary::StaticClass() );
		MultiplyNode->AllocateDefaultPins();

		//cache this out. it will be linked to from the output of the (int)Delta -> (float)Delta Conversion function
		UEdGraphPin *MultiplyNodeFirstPin = MultiplyNode->FindPinChecked( FString(TEXT("A")) );

		// 2nd input to the Add function comes from the Current variable value
		UEdGraphPin *MultiplyNodeSecondPin = MultiplyNode->FindPinChecked( FString(TEXT("B")) );
		CompilerContext.CopyPinLinksToIntermediate( *DeltaMultPin, *MultiplyNodeSecondPin);

		UEdGraphPin *MultiplyNodeReturnValuePin = MultiplyNode->GetReturnValuePin();
		MultiplyNodeReturnValuePin->MakeLinkTo( ModifyVarNodeDeltaPin );

		//
		// The Convert function to go from (int)Delta to ULiveEditorKismetLibrary::ModifyPropertyByName(... float Delta ...)
		//
		FName ConvertFunctionName;
		UClass* ConvertFunctionOwner = nullptr;
		bool success = Schema->SearchForAutocastFunction(EventDeltaPin, MultiplyNodeFirstPin, ConvertFunctionName, ConvertFunctionOwner);
		check( success );
		UK2Node_CallFunction *ConvertDeltaNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,SourceGraph);
		ConvertDeltaNode->FunctionReference.SetExternalMember(ConvertFunctionName, ConvertFunctionOwner ? ConvertFunctionOwner : UKismetMathLibrary::StaticClass());
		ConvertDeltaNode->AllocateDefaultPins();

		FName PinName;
		success = UK2Node_LiveEditObjectStatics::SearchForConvertPinName( Schema, EventDeltaPin, PinName );
		check( success );
		UEdGraphPin *ConvertDeltaInputPin = ConvertDeltaNode->FindPinChecked( PinName.ToString() );
		EventDeltaPin->MakeLinkTo( ConvertDeltaInputPin );

		UEdGraphPin *ConvertDeltaOutputPin = ConvertDeltaNode->GetReturnValuePin();
		ConvertDeltaOutputPin->MakeLinkTo( MultiplyNodeFirstPin );

		//
		// TODO - markDirty
		//

		//
		// send out the object value updates
		//
		UK2Node_CallFunction *ReplicationNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this,SourceGraph);
		ReplicationNode->FunctionReference.SetExternalMember( TEXT("ReplicateChangesToChildren"), ULiveEditorKismetLibrary::StaticClass() );
		ReplicationNode->AllocateDefaultPins();

		UEdGraphPin *ReplicationNodeVarNamePin = ReplicationNode->FindPinChecked( TEXT("PropertyName") );
		ReplicationNodeVarNamePin->DefaultValue = SourceVariablePin->DefaultValue;

		UEdGraphPin *ReplicationNodeArchetypePin = ReplicationNode->FindPinChecked( FString(TEXT("Archetype")) );
		GetClassDefaultObjectResultPin->MakeLinkTo( ReplicationNodeArchetypePin );

		UEdGraphPin *ReplicationNodeExecPin = ReplicationNode->GetExecPin();
		ModifyVarNodeThenPin->MakeLinkTo( ReplicationNodeExecPin );

		UEdGraphPin *ReplicationNodeThenPin = ReplicationNode->FindPinChecked( FString(TEXT("then")) );

		//
		// Finally, activate our OnMidiInput pin
		//
		CompilerContext.CopyPinLinksToIntermediate( *SourceOnMidiInputPin, *ReplicationNodeThenPin);
			
	}

	// Break any links to the expanded node
	BreakAllNodeLinks();
}

void UK2Node_LiveEditObject::GetNodeAttributes( TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes ) const
{
	UClass* ClassToSpawn = GetClassToSpawn();
	const FString ClassToSpawnStr = ClassToSpawn ? ClassToSpawn->GetName() : TEXT( "UnknownClass" );
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Type" ), TEXT( "LiveEditObject" ) ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Class" ), GetClass()->GetName() ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Name" ), GetName() ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "ClassToSpawn" ), ClassToSpawnStr ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "EventName" ), GetEventName() ));
}

void UK2Node_LiveEditObject::CreatePinsForClass(UClass* InClass)
{
	check(InClass != NULL);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	FEdGraphPinType LiveEditVarPinType;
	LiveEditVarPinType.bIsArray = false;
	LiveEditVarPinType.bIsReference = false;
	LiveEditVarPinType.bIsWeakPointer = false;
	LiveEditVarPinType.PinCategory = K2Schema->PC_String;
	LiveEditVarPinType.PinSubCategory = LiveEditableVarPinCategory; //this will trigger a SGraphPinLiveEditVar to be constructed
	LiveEditVarPinType.PinSubCategoryObject = NULL;

	UEdGraphPin* LiveEditVarPin = CreatePin(EGPD_Input, TEXT(""), TEXT(""), NULL, false, false, UK2Node_LiveEditObjectStatics::VariablePinName);
	LiveEditVarPin->PinType = LiveEditVarPinType;
	SetPinToolTip(*LiveEditVarPin, LOCTEXT("LiveEditVarPinDescription", "The specific Variable in the Assigned Blueprint that this node will LiveEdit"));

	// Add pin for Delta Multiplier
	UEdGraphPin* DeltaMultPin = CreatePin(EGPD_Input, K2Schema->PC_Float, TEXT(""), NULL, false, false, UK2Node_LiveEditObjectStatics::DeltaMultPinName);
	SetPinToolTip(*DeltaMultPin, LOCTEXT("BlueprintPinDescription", "Amount by which to multiply the raw MIDI input Delta before modifying the tuned variable"));
	DeltaMultPin->DefaultValue = DeltaMultPin->AutogeneratedDefaultValue = TEXT("1.0");

	// Add pin for whether or not we should clamp values
	UEdGraphPin* ShouldClampPin = CreatePin(EGPD_Input, K2Schema->PC_Boolean, TEXT(""), NULL, false, false, UK2Node_LiveEditObjectStatics::ShouldClampPinName);
	SetPinToolTip(*ShouldClampPin, LOCTEXT("BlueprintPinDescription", "Do you want the value clamped to a specified range"));

	// Add pin for clamp MIN
	UEdGraphPin* ClampMinPin = CreatePin(EGPD_Input, K2Schema->PC_Float, TEXT(""), NULL, false, false, UK2Node_LiveEditObjectStatics::ClampMinPinName);
	ClampMinPin->DefaultValue = ClampMinPin->AutogeneratedDefaultValue = TEXT("0.0");
	SetPinToolTip(*ClampMinPin, LOCTEXT("BlueprintPinDescription", "MIN value to which you want to clamp the tuned value (ONLY active if ClampValues == TRUE)"));

	// Add pin for clamp MAX
	UEdGraphPin* ClampMaxPin = CreatePin(EGPD_Input, K2Schema->PC_Float, TEXT(""), NULL, false, false, UK2Node_LiveEditObjectStatics::ClampMaxPinName);
	ClampMaxPin->DefaultValue = ClampMaxPin->AutogeneratedDefaultValue = TEXT("0.0");
	SetPinToolTip(*ClampMaxPin, LOCTEXT("BlueprintPinDescription", "MAX value to which you want to clamp the tuned value (ONLY active if ClampValues == TRUE)"));
}

UFunction *UK2Node_LiveEditObject::GetEventMIDISignature() const
{
	FMemberReference ReferenceToUse;
	FGuid DelegateGuid;
	UBlueprint::GetGuidFromClassByFieldName<UFunction>(ULiveEditorBroadcaster::StaticClass(), TEXT("OnEventMIDI"), DelegateGuid);
	ReferenceToUse.SetDirect( TEXT("OnEventMIDI"), DelegateGuid, ULiveEditorBroadcaster::StaticClass(), false );

	UMulticastDelegateProperty* DelegateProperty = ReferenceToUse.ResolveMember<UMulticastDelegateProperty>(GetBlueprintClassFromNode());
	if (DelegateProperty != NULL)
	{
		return DelegateProperty->SignatureFunction;
	}
	return NULL;
}

bool UK2Node_LiveEditObject::IsSpawnVarPin(UEdGraphPin* Pin)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	return(	Pin->PinName != K2Schema->PN_Execute &&
			Pin->PinName != K2Schema->PN_Then &&
			Pin->PinName != K2Schema->PN_ReturnValue &&
			Pin->PinName != UK2Node_LiveEditObjectStatics::BlueprintPinName &&
			Pin->PinName != UK2Node_LiveEditObjectStatics::BaseClassPinName &&
			Pin->PinName != UK2Node_LiveEditObjectStatics::WorldContextPinName &&
			Pin->PinName != UK2Node_LiveEditObjectStatics::DescriptionPinName &&
			Pin->PinName != UK2Node_LiveEditObjectStatics::PermittedBindingsPinName &&
			Pin->PinName != UK2Node_LiveEditObjectStatics::EventPinName );
}

FString UK2Node_LiveEditObject::GetEventName() const
{
	UEdGraphPin *VariablePin = GetVariablePin();
	UEdGraphPin *BaseClassPin = GetBaseClassPin();
	if ( BaseClassPin->DefaultObject == NULL )
	{
		return FString(TEXT("Invalid"));
	}

	FString EventName = FString::Printf(TEXT("%s_%s_%d%d"), *VariablePin->DefaultValue, *BaseClassPin->DefaultObject->GetName(), NodePosX,NodePosY);
	return EventName;
}

UEdGraphPin* UK2Node_LiveEditObject::GetBlueprintPin(const TArray<UEdGraphPin*>* InPinsToSearch /*= NULL*/) const
{
	const TArray<UEdGraphPin*>* PinsToSearch = InPinsToSearch ? InPinsToSearch : &Pins;

	UEdGraphPin* Pin = NULL;
	for( auto PinIt = PinsToSearch->CreateConstIterator(); PinIt; ++PinIt )
	{
		UEdGraphPin* TestPin = *PinIt;
		if( TestPin && TestPin->PinName == UK2Node_LiveEditObjectStatics::BlueprintPinName )
		{
			Pin = TestPin;
			break;
		}
	}
	check(Pin == NULL || Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin *UK2Node_LiveEditObject::GetBaseClassPin(const TArray<UEdGraphPin*>* InPinsToSearch) const
{
	const TArray<UEdGraphPin*>* PinsToSearch = InPinsToSearch ? InPinsToSearch : &Pins;

	UEdGraphPin* Pin = NULL;
	for( auto PinIt = PinsToSearch->CreateConstIterator(); PinIt; ++PinIt )
	{
		UEdGraphPin* TestPin = *PinIt;
		if( TestPin && TestPin->PinName == UK2Node_LiveEditObjectStatics::BaseClassPinName )
		{
			Pin = TestPin;
			break;
		}
	}
	check(Pin == NULL || Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin* UK2Node_LiveEditObject::GetThenPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPinChecked(K2Schema->PN_Then);
	check(Pin->Direction == EGPD_Output);
	return Pin;
}

UEdGraphPin *UK2Node_LiveEditObject::GetVariablePin() const
{
	UEdGraphPin *Pin = FindPin( UK2Node_LiveEditObjectStatics::VariablePinName );
	check(!Pin || Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin *UK2Node_LiveEditObject::GetDescriptionPin() const
{
	UEdGraphPin *Pin = FindPinChecked( UK2Node_LiveEditObjectStatics::DescriptionPinName );
	check(Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin *UK2Node_LiveEditObject::GetPermittedBindingsPin() const
{
	UEdGraphPin *Pin = FindPinChecked( UK2Node_LiveEditObjectStatics::PermittedBindingsPinName );
	check(Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin *UK2Node_LiveEditObject::GetOnMidiInputPin() const
{
	UEdGraphPin *Pin = FindPinChecked( UK2Node_LiveEditObjectStatics::EventPinName );
	check(Pin->Direction == EGPD_Output);
	return Pin;
}

UEdGraphPin *UK2Node_LiveEditObject::GetDeltaMultPin() const
{
	for ( int32 i = 0; i < Pins.Num(); ++i )
	{
		UEdGraphPin *Pin = Pins[i];
		if ( Pin->PinName == UK2Node_LiveEditObjectStatics::DeltaMultPinName )
		{
			return Pin;
		}
	}
	
	return NULL;
}

UEdGraphPin *UK2Node_LiveEditObject::GetShouldClampPin() const
{
	UEdGraphPin *Pin = FindPinChecked( UK2Node_LiveEditObjectStatics::ShouldClampPinName );
	check(Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin *UK2Node_LiveEditObject::GetClampMinPin() const
{
	UEdGraphPin *Pin = FindPinChecked( UK2Node_LiveEditObjectStatics::ClampMinPinName );
	check(Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin *UK2Node_LiveEditObject::GetClampMaxPin() const
{
	UEdGraphPin *Pin = FindPinChecked( UK2Node_LiveEditObjectStatics::ClampMaxPinName );
	check(Pin->Direction == EGPD_Input);
	return Pin;
}

void UK2Node_LiveEditObject::SetPinToolTip(UEdGraphPin& MutatablePin, const FText& PinDescription) const
{
	MutatablePin.PinToolTip = UEdGraphSchema_K2::TypeToText(MutatablePin.PinType).ToString();

	UEdGraphSchema_K2 const* const K2Schema = Cast<const UEdGraphSchema_K2>(GetSchema());
	if (K2Schema != nullptr)
	{
		MutatablePin.PinToolTip += TEXT(" ");
		MutatablePin.PinToolTip += K2Schema->GetPinDisplayName(&MutatablePin).ToString();
	}

	MutatablePin.PinToolTip += FString(TEXT("\n")) + PinDescription.ToString();
}

UClass* UK2Node_LiveEditObject::GetClassToSpawn(const TArray<UEdGraphPin*>* InPinsToSearch /*=NULL*/) const
{
	UClass* UseSpawnClass = NULL;
	const TArray<UEdGraphPin*>* PinsToSearch = InPinsToSearch ? InPinsToSearch : &Pins;

	UEdGraphPin *BaseClassPin = GetBaseClassPin(PinsToSearch);
	if ( BaseClassPin && BaseClassPin->DefaultObject != NULL )
	{
		UseSpawnClass = CastChecked<UClass>(BaseClassPin->DefaultObject);
	}

	return UseSpawnClass;
}

#undef LOCTEXT_NAMESPACE
