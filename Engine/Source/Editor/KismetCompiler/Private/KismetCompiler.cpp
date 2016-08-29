// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	KismetCompiler.cpp
=============================================================================*/


#include "KismetCompilerPrivatePCH.h"
#include "Engine/LevelScriptBlueprint.h"
#include "KismetCompilerBackend.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetDebugUtilities.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetReinstanceUtilities.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetEditorUtilities.h"
#include "Editor/UnrealEd/Public/ScriptDisassembler.h"
#include "Editor/UnrealEd/Public/ComponentTypeRegistry.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "UserDefinedStructureCompilerUtils.h"
#include "K2Node_EnumLiteral.h"
#include "K2Node_SetVariableOnPersistentFrame.h"
#include "EdGraph/EdGraphNode_Documentation.h"
#include "Engine/DynamicBlueprintBinding.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/InheritableComponentHandler.h"
#include "BlueprintCompilerCppBackendInterface.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/TimelineTemplate.h"
#include "Components/TimelineComponent.h"

static bool bDebugPropertyPropagation = false;

#define LOCTEXT_NAMESPACE "KismetCompiler"

//////////////////////////////////////////////////////////////////////////
// Stats for this module
DECLARE_CYCLE_STAT(TEXT("Create Schema"), EKismetCompilerStats_CreateSchema, STATGROUP_KismetCompiler );
DECLARE_CYCLE_STAT(TEXT("Create Function List"), EKismetCompilerStats_CreateFunctionList, STATGROUP_KismetCompiler );
DECLARE_CYCLE_STAT(TEXT("Expansion"), EKismetCompilerStats_Expansion, STATGROUP_KismetCompiler )
DECLARE_CYCLE_STAT(TEXT("Process uber"), EKismetCompilerStats_ProcessUbergraph, STATGROUP_KismetCompiler );
DECLARE_CYCLE_STAT(TEXT("Process func"), EKismetCompilerStats_ProcessFunctionGraph, STATGROUP_KismetCompiler );
DECLARE_CYCLE_STAT(TEXT("Precompile Function"), EKismetCompilerStats_PrecompileFunction, STATGROUP_KismetCompiler );
DECLARE_CYCLE_STAT(TEXT("Compile Function"), EKismetCompilerStats_CompileFunction, STATGROUP_KismetCompiler );
DECLARE_CYCLE_STAT(TEXT("Postcompile Function"), EKismetCompilerStats_PostcompileFunction, STATGROUP_KismetCompiler );
DECLARE_CYCLE_STAT(TEXT("Finalization"), EKismetCompilerStats_FinalizationWork, STATGROUP_KismetCompiler );
DECLARE_CYCLE_STAT(TEXT("Code Gen"), EKismetCompilerStats_CodeGenerationTime, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Clean and Sanitize Class"), EKismetCompilerStats_CleanAndSanitizeClass, STATGROUP_KismetCompiler );
DECLARE_CYCLE_STAT(TEXT("Create Class Properties"), EKismetCompilerStats_CreateClassVariables, STATGROUP_KismetCompiler );
DECLARE_CYCLE_STAT(TEXT("Bind and Link Class"), EKismetCompilerStats_BindAndLinkClass, STATGROUP_KismetCompiler );
DECLARE_CYCLE_STAT(TEXT("Calculate checksum of CDO"), EKismetCompilerStats_ChecksumCDO, STATGROUP_KismetCompiler );
DECLARE_CYCLE_STAT(TEXT("Analyze execution path"), EKismetCompilerStats_AnalyzeExecutionPath, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Calculate checksum of signature"), EKismetCompilerStats_ChecksumSignature, STATGROUP_KismetCompiler);
//////////////////////////////////////////////////////////////////////////
// FKismetCompilerContext

FKismetCompilerContext::FKismetCompilerContext(UBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions, TArray<UObject*>* InObjLoaded)
	: FGraphCompilerContext(InMessageLog)
	, Schema(NULL)
	, CompileOptions(InCompilerOptions)
	, ObjLoaded(InObjLoaded)
	, Blueprint(SourceSketch)
	, NewClass(NULL)
	, ConsolidatedEventGraph(NULL)
	, UbergraphContext(NULL)
{
	MacroRowMaxHeight = 0;

	MinimumSpawnX = -2000;
	MaximumSpawnX = 2000;

	AverageNodeWidth = 200;
	AverageNodeHeight = 150;

	HorizontalSectionPadding = 250;
	VerticalSectionPadding = 250;
	HorizontalNodePadding = 40;

	MacroSpawnX = MinimumSpawnX;
	MacroSpawnY = -2000;
	
	VectorStruct = TBaseStructure<FVector>::Get();
	RotatorStruct = TBaseStructure<FRotator>::Get();
	TransformStruct = TBaseStructure<FTransform>::Get();
	LinearColorStruct = TBaseStructure<FLinearColor>::Get();
}

FKismetCompilerContext::~FKismetCompilerContext()
{
	for (TMap< TSubclassOf<UEdGraphNode>, FNodeHandlingFunctor*>::TIterator It(NodeHandlers); It; ++It)
	{
		FNodeHandlingFunctor* FPtr = It.Value();
		delete FPtr;
	}
	NodeHandlers.Empty();
	DefaultPropertyValueMap.Empty();
}

UEdGraphSchema_K2* FKismetCompilerContext::CreateSchema()
{
	return NewObject<UEdGraphSchema_K2>();
}

void FKismetCompilerContext::EnsureProperGeneratedClass(UClass*& TargetUClass)
{
	if( TargetUClass && !((UObject*)TargetUClass)->IsA(UBlueprintGeneratedClass::StaticClass()) )
	{
		FKismetCompilerUtilities::ConsignToOblivion(TargetUClass, Blueprint->bIsRegeneratingOnLoad);
		TargetUClass = NULL;
	}
}

void FKismetCompilerContext::SpawnNewClass(const FString& NewClassName)
{
	// First, attempt to find the class, in case it hasn't been serialized in yet
	NewClass = FindObject<UBlueprintGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);
	if (NewClass == NULL)
	{
		// If the class hasn't been found, then spawn a new one
		NewClass = NewObject<UBlueprintGeneratedClass>(Blueprint->GetOutermost(), FName(*NewClassName), RF_Public | RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked in the Blueprint yet due to load ordering issues
		NewClass->ClassGeneratedBy = Blueprint;
		FBlueprintCompileReinstancer::Create(NewClass);
	}
}

void FKismetCompilerContext::FSubobjectCollection::AddObject(const UObject* const InObject)
{
	if ( InObject )
	{
		Collection.Add(InObject);
		TArray<UObject*> Subobjects;
		GetObjectsWithOuter(InObject, Subobjects, true);
		for ( auto SubObject : Subobjects )
		{
			Collection.Add(SubObject);
		}
	}
}

bool FKismetCompilerContext::FSubobjectCollection::operator()(const UObject* const RemovalCandidate) const
{
	return ( NULL != Collection.Find(RemovalCandidate) );
}

void FKismetCompilerContext::CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& OldCDO)
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_CleanAndSanitizeClass);

	const bool bRecompilingOnLoad = Blueprint->bIsRegeneratingOnLoad;
	FString TransientClassString = FString::Printf(TEXT("TRASHCLASS_%s"), *Blueprint->GetName());
	FName TransientClassName = MakeUniqueObjectName(GetTransientPackage(), UBlueprintGeneratedClass::StaticClass(), FName(*TransientClassString));
	UClass* TransientClass = NewObject<UBlueprintGeneratedClass>(GetTransientPackage(), TransientClassName, RF_Public | RF_Transient);
	
	UClass* ParentClass = Blueprint->ParentClass;

	if(CompileOptions.CompileType == EKismetCompileType::SkeletonOnly)
	{
		if(UBlueprint* BlueprintParent = Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy))
		{
			ParentClass = BlueprintParent->SkeletonGeneratedClass;
		}
	}

	if( ParentClass == NULL )
	{
		ParentClass = UObject::StaticClass();
	}
	TransientClass->ClassAddReferencedObjects = ParentClass->AddReferencedObjects;
	
	NewClass = ClassToClean;
	OldCDO = ClassToClean->ClassDefaultObject; // we don't need to create the CDO at this point
	
	const ERenameFlags RenFlags = REN_DontCreateRedirectors | (bRecompilingOnLoad ? REN_ForceNoResetLoaders : 0) | REN_NonTransactional | REN_DoNotDirty;

	if( OldCDO )
	{
		FString TransientCDOString = FString::Printf(TEXT("TRASH_%s"), *OldCDO->GetName());
		FName TransientCDOName = MakeUniqueObjectName(GetTransientPackage(), TransientClass, FName(*TransientCDOString));
		OldCDO->Rename(*TransientCDOName.ToString(), GetTransientPackage(), RenFlags);
		FLinkerLoad::InvalidateExport(OldCDO);
	}

	// Purge all subobjects (properties, functions, params) of the class, as they will be regenerated
	TArray<UObject*> ClassSubObjects;
	GetObjectsWithOuter(ClassToClean, ClassSubObjects, true);

	{
		// Save subobjects, that won't be regenerated.
		FSubobjectCollection SubObjectsToSave;
		SaveSubObjectsFromCleanAndSanitizeClass(SubObjectsToSave, ClassToClean, OldCDO);

		ClassSubObjects.RemoveAllSwap(SubObjectsToSave);
	}

	for( auto SubObjIt = ClassSubObjects.CreateIterator(); SubObjIt; ++SubObjIt )
	{
		UObject* CurrSubObj = *SubObjIt;
		CurrSubObj->Rename(NULL, TransientClass, RenFlags);
		if( UProperty* Prop = Cast<UProperty>(CurrSubObj) )
		{
			FKismetCompilerUtilities::InvalidatePropertyExport(Prop);
		}
		else
		{
			FLinkerLoad::InvalidateExport(CurrSubObj);
		}
	}

	// Purge the class to get it back to a "base" state
	ClassToClean->PurgeClass(bRecompilingOnLoad);

	// Set properties we need to regenerate the class with
	ClassToClean->PropertyLink = ParentClass->PropertyLink;
	ClassToClean->ClassWithin = ParentClass;
	ClassToClean->ClassConfigName = ClassToClean->IsNative() ? FName(ClassToClean->StaticConfigName()) : FName(*ParentClass->GetConfigName());
	ClassToClean->DebugData = FBlueprintDebugData();
}

void FKismetCompilerContext::SaveSubObjectsFromCleanAndSanitizeClass(FSubobjectCollection& SubObjectsToSave, UBlueprintGeneratedClass* ClassToClean, UObject*& OldCDO)
{
	SubObjectsToSave.AddObjects(Blueprint->ComponentTemplates);
	SubObjectsToSave.AddObjects(Blueprint->Timelines);

	if ( Blueprint->SimpleConstructionScript )
	{
		SubObjectsToSave.AddObject(Blueprint->SimpleConstructionScript);
		if ( const USCS_Node* DefaultScene = Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode() )
		{
			SubObjectsToSave.AddObject(DefaultScene->ComponentTemplate);
		}

		for ( USCS_Node* SCSNode : Blueprint->SimpleConstructionScript->GetAllNodes() )
		{
			SubObjectsToSave.AddObject(SCSNode->ComponentTemplate);
		}
	}

	{
		TSet<class UCurveBase*> Curves;
		for ( auto Timeline : Blueprint->Timelines )
		{
			if ( Timeline )
			{
				Timeline->GetAllCurves(Curves);
			}
		}
		for ( auto Component : Blueprint->ComponentTemplates )
		{
			if ( auto TimelineComponent = Cast<UTimelineComponent>(Component) )
			{
				TimelineComponent->GetAllCurves(Curves);
			}
		}
		for ( auto Curve : Curves )
		{
			SubObjectsToSave.AddObject(Curve);
		}
	}

	if (Blueprint->InheritableComponentHandler)
	{
		SubObjectsToSave.AddObject(Blueprint->InheritableComponentHandler);
		TArray<UActorComponent*> AllTemplates;
		Blueprint->InheritableComponentHandler->GetAllTemplates(AllTemplates);
		SubObjectsToSave.AddObjects(AllTemplates);
	}
}

void FKismetCompilerContext::PostCreateSchema()
{
	NodeHandlers.Add(UEdGraphNode_Comment::StaticClass(), new FNodeHandlingFunctor(*this));

	TArray<UClass*> ClassesOfUK2Node;
	GetDerivedClasses(UK2Node::StaticClass(), ClassesOfUK2Node, true);
	for ( auto ClassIt = ClassesOfUK2Node.CreateConstIterator(); ClassIt; ++ClassIt )
	{
		if( !(*ClassIt)->HasAnyClassFlags(CLASS_Abstract) )
		{
			UClass* Class = (*ClassIt);
			UObject* CDO = Class->GetDefaultObject();
			UK2Node* K2CDO = Class->GetDefaultObject<UK2Node>();
			FNodeHandlingFunctor* HandlingFunctor = K2CDO->CreateNodeHandler(*this);
			if (HandlingFunctor)
			{
				NodeHandlers.Add(*ClassIt, HandlingFunctor);
			}
		}
	}
}	


/** Validates that the interconnection between two pins is schema compatible */
void FKismetCompilerContext::ValidateLink(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	Super::ValidateLink(PinA, PinB);

	// At this point we can assume the pins are linked, and as such the connection response should not be to disallow
	// @todo: Potentially revisit this later.
	// This API is intended to describe how to handle a potentially new connection to a pin that may already have a connection.
	// However it also checks all necessary constraints for a valid connection to exist. We rely on the fact that the "disallow"
	// response will be returned if the pins are not compatible; any other response here then means that the connection is valid.
	const FPinConnectionResponse ConnectResponse = Schema->CanCreateConnection(PinA, PinB);

	const bool bForbiddenConnection = (ConnectResponse.Response == CONNECT_RESPONSE_DISALLOW);
	const bool bMissingConversion   = (ConnectResponse.Response == CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE);
	if (bForbiddenConnection || bMissingConversion)
	{
		const FString ErrorMessage = FString::Printf(*LOCTEXT("PinTypeMismatch_Error", "Can't connect pins @@ and @@: %s").ToString(), *ConnectResponse.Message.ToString());
		if (ConnectResponse.IsFatal())
		{
			MessageLog.Error(*ErrorMessage, PinA, PinB);
		}
		else
		{
			MessageLog.Warning(*ErrorMessage, PinA, PinB);
		}
	}

	if (PinA && PinB && PinA->Direction != PinB->Direction)
	{
		const UEdGraphPin* InputPin = (EEdGraphPinDirection::EGPD_Input == PinA->Direction) ? PinA : PinB;
		const UEdGraphPin* OutputPin = (EEdGraphPinDirection::EGPD_Output == PinA->Direction) ? PinA : PinB;
		const bool bInvalidConnection = InputPin && OutputPin && (OutputPin->PinType.PinCategory == Schema->PC_Interface) && (InputPin->PinType.PinCategory == Schema->PC_Object);
		if (bInvalidConnection)
		{
			MessageLog.Error(*LOCTEXT("PinTypeMismatch_Error_UseExplictCast", "Can't connect pins @@ (Interface) and @@ (Object). Use an explicit cast node.").ToString(), OutputPin, InputPin);
		}
	}
}

/** Validate that the wiring for a single pin is schema compatible */
void FKismetCompilerContext::ValidatePin(const UEdGraphPin* Pin) const
{
	Super::ValidatePin(Pin);

	auto OwningNodeUnchecked = Pin ? Pin->GetOwningNodeUnchecked() : NULL;
	if (!OwningNodeUnchecked)
	{
		//handled by Super::ValidatePin
		return;
	}

	if (Pin->LinkedTo.Num() > 1)
	{
		if (Pin->Direction == EGPD_Output)
		{
			if (Schema->IsExecPin(*Pin))
			{
				// Multiple outputs are not OK, since they don't have a clear defined order of execution
				MessageLog.Error(*LOCTEXT("TooManyOutputPinConnections_Error", "Exec output pin @@ cannot have more than one connection").ToString(), Pin);
			}
		}
		else if (Pin->Direction == EGPD_Input)
		{
			if (Schema->IsExecPin(*Pin))
			{
				// Multiple inputs to an execution wire are ok, it means we get executed from more than one path
			}
			else if( Schema->IsSelfPin(*Pin) )
			{
				// Pure functions and latent functions cannot have more than one self connection
				UK2Node_CallFunction* OwningNode = Cast<UK2Node_CallFunction>(OwningNodeUnchecked);
				if( OwningNode )
				{
					if( OwningNode->IsNodePure() )
					{
						MessageLog.Error(*LOCTEXT("PureFunction_OneSelfPin_Error", "Pure function call node @@ cannot have more than one self pin connection").ToString(), OwningNode);
					}
					else if( OwningNode->IsLatentFunction() )
					{
						MessageLog.Error(*LOCTEXT("LatentFunction_OneSelfPin_Error", "Latent function call node @@ cannot have more than one self pin connection").ToString(), OwningNode);
					}
				}
			}
			else
			{
				MessageLog.Error(*LOCTEXT("InputPin_OneConnection_Error", "Input pin @@ cannot have more than one connection").ToString(), Pin);
			}
		}
		else
		{
			MessageLog.Error(*LOCTEXT("UnexpectedPiNDirection_Error", "Unexpected pin direction encountered on @@").ToString(), Pin);
		}
	}

	//function return node exec pin should be connected to something
	if(Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() == 0 && Schema->IsExecPin(*Pin) )
	{
		if (UK2Node_FunctionResult* OwningNode = Cast<UK2Node_FunctionResult>(OwningNodeUnchecked))
		{
			MessageLog.Warning(*LOCTEXT("ReturnNodeExecPinUnconnected", "ReturnNode Exec pin has no connections on @@").ToString(), Pin);
		}
	}
}

/** Validates that the node is schema compatible */
void FKismetCompilerContext::ValidateNode(const UEdGraphNode* Node) const
{
	//@TODO: Validate the node type is a known one
	Super::ValidateNode(Node);
}

/** Creates a class variable */
UProperty* FKismetCompilerContext::CreateVariable(const FName VarName, const FEdGraphPinType& VarType)
{
	if (BPTYPE_FunctionLibrary == Blueprint->BlueprintType)
	{
		MessageLog.Error(*FString::Printf(*LOCTEXT("VariableInFunctionLibrary_Error", "The variable %s cannot be declared in FunctionLibrary @@").ToString(),
			*VarName.ToString()), Blueprint);
	}

	UProperty* NewProperty = FKismetCompilerUtilities::CreatePropertyOnScope(NewClass, VarName, VarType, NewClass, 0, Schema, MessageLog);
	if (NewProperty != NULL)
	{
		FKismetCompilerUtilities::LinkAddedProperty(NewClass, NewProperty);
	}
	else
	{
		MessageLog.Error(*FString::Printf(*LOCTEXT("VariableInvalidType_Error", "The variable %s declared in @@ has an invalid type %s").ToString(),
			*VarName.ToString(), *UEdGraphSchema_K2::TypeToText(VarType).ToString()), Blueprint);
	}

	return NewProperty;
}

/** Determines if a node is pure */
bool FKismetCompilerContext::IsNodePure(const UEdGraphNode* Node) const
{
	if (const UK2Node* K2Node = Cast<const UK2Node>(Node))
	{
		return K2Node->IsNodePure();
	}
	// Only non K2Nodes are comments and documentation nodes, which are pure
	ensure(Node->IsA(UEdGraphNode_Comment::StaticClass())||Node->IsA(UEdGraphNode_Documentation::StaticClass()));
	return true;
}

void FKismetCompilerContext::ValidateVariableNames()
{
	UClass* ParentClass = Blueprint->ParentClass;
	if (ParentClass != nullptr)
	{
		TSharedPtr<FKismetNameValidator> ParentBPNameValidator;
		if (UBlueprint* ParentBP = Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy))
		{
			ParentBPNameValidator = MakeShareable(new FKismetNameValidator(ParentBP));
		}

		for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
		{
			FName OldVarName = VarDesc.VarName;
			FName NewVarName = OldVarName;

			FString VarNameStr = OldVarName.ToString();
			if (ParentBPNameValidator.IsValid() && (ParentBPNameValidator->IsValid(VarNameStr) != EValidatorResult::Ok))
			{
				NewVarName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, VarNameStr);
			}
			else if (ParentClass->IsNative()) // the above case handles when the parent is a blueprint
			{
				if (auto ExisingField = FindField<UField>(ParentClass, *VarNameStr))
				{
					UE_LOG(LogK2Compiler, Warning, TEXT("ValidateVariableNames name %s (used in %s) is already taken by %s")
						, *VarNameStr, *Blueprint->GetPathName(), *ExisingField->GetPathName());
					NewVarName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, VarNameStr);
				}
			}

			if (OldVarName != NewVarName)
			{
				MessageLog.Warning(*FString::Printf(*LOCTEXT("MemberVariableConflictWarning", "Found a member variable with a conflicting name (%s) - changed to %s.").ToString(), *VarNameStr, *NewVarName.ToString()));
				FBlueprintEditorUtils::RenameMemberVariable(Blueprint, OldVarName, NewVarName);
			}
		}
	}
}

void FKismetCompilerContext::ValidateTimelineNames()
{
	TSharedPtr<FKismetNameValidator> ParentBPNameValidator;
	if( Blueprint->ParentClass != NULL )
	{
		UBlueprint* ParentBP = Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy);
		if( ParentBP != NULL )
		{
			ParentBPNameValidator = MakeShareable(new FKismetNameValidator(ParentBP));
		}
	}

	for (int32 TimelineIndex=0; TimelineIndex < Blueprint->Timelines.Num(); ++TimelineIndex)
	{
		UTimelineTemplate* TimelineTemplate = Blueprint->Timelines[TimelineIndex];
		if( TimelineTemplate )
		{
			if( ParentBPNameValidator.IsValid() && ParentBPNameValidator->IsValid(TimelineTemplate->GetName()) != EValidatorResult::Ok )
			{
				// Use the viewer displayed Timeline name (without the _Template suffix) because it will be added later for appropriate checks.
				FString TimelineName = UTimelineTemplate::TimelineTemplateNameToVariableName(TimelineTemplate->GetFName());

				FName NewName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, TimelineName);
				MessageLog.Warning(*FString::Printf(*LOCTEXT("TimelineConflictWarning", "Found a timeline with a conflicting name (%s) - changed to %s.").ToString(), *TimelineTemplate->GetName(), *NewName.ToString()));
				FBlueprintEditorUtils::RenameTimeline(Blueprint, FName(*TimelineName), NewName);
			}
		}
	}
}

void FKismetCompilerContext::CreateClassVariablesFromBlueprint()
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_CreateClassVariables);

	// Grab the blueprint variables
	NewClass->NumReplicatedProperties = 0;	// Keep track of how many replicated variables this blueprint adds
	// Clear out any existing property guids
	const bool bRebuildPropertyMap = bIsFullCompile && !Blueprint->bIsRegeneratingOnLoad;
	if (bRebuildPropertyMap)
	{
		NewClass->PropertyGuids.Reset();
		// Add any chained parent blueprint map values
		UBlueprint* ParentBP = Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy);
		while (ParentBP)
		{
			if (UBlueprintGeneratedClass* ParentBPGC = Cast<UBlueprintGeneratedClass>(ParentBP->GeneratedClass))
			{
				NewClass->PropertyGuids.Append(ParentBPGC->PropertyGuids);
			}
			ParentBP = Cast<UBlueprint>(ParentBP->ParentClass->ClassGeneratedBy);
		}
	}

	for (int32 i = 0; i < Blueprint->NewVariables.Num(); ++i)
	{
		FBPVariableDescription& Variable = Blueprint->NewVariables[Blueprint->NewVariables.Num() - (i + 1)];

		UProperty* NewProperty = CreateVariable(Variable.VarName, Variable.VarType);
		if (NewProperty != NULL)
		{
			NewProperty->SetPropertyFlags(Variable.PropertyFlags);
			NewProperty->SetMetaData(TEXT("DisplayName"), *Variable.FriendlyName);
			NewProperty->SetMetaData(TEXT("Category"), *Variable.Category.ToString());
			NewProperty->RepNotifyFunc = Variable.RepNotifyFunc;

			if(!Variable.DefaultValue.IsEmpty())
			{
				SetPropertyDefaultValue(NewProperty, Variable.DefaultValue);
			}

			if (NewProperty->HasAnyPropertyFlags(CPF_Net))
			{
				NewClass->NumReplicatedProperties++;
			}

			// Set metadata on property
			for (FBPVariableMetaDataEntry& Entry : Variable.MetaDataArray)
			{
				NewProperty->SetMetaData(Entry.DataKey, *Entry.DataValue);
				if (Entry.DataKey == FBlueprintMetadata::MD_ExposeOnSpawn)
				{
					NewProperty->SetPropertyFlags(CPF_ExposeOnSpawn);
					if (NewProperty->HasAnyPropertyFlags(CPF_DisableEditOnInstance))
					{
						MessageLog.Warning(*FString::Printf(*LOCTEXT("ExposeToSpawnButPrivateWarning", "Variable %s is marked as 'Expose on Spawn' but not marked as 'Editable'; please make it 'Editable'").ToString(), *NewProperty->GetName()));
					}
				}
			}
			if (bRebuildPropertyMap)
			{
				// Update new class property guid map
				NewClass->PropertyGuids.Add(Variable.VarName, Variable.VarGuid);
			}
		}
	}

	// Ensure that timeline names are valid and that there are no collisions with a parent class
	ValidateTimelineNames();

	// Create a class property for each timeline instance contained in the blueprint
	for (int32 TimelineIndex = 0; TimelineIndex < Blueprint->Timelines.Num(); ++TimelineIndex)
	{
		UTimelineTemplate* Timeline = Blueprint->Timelines[TimelineIndex];
		// Not fatal if NULL, but shouldn't happen
		if (!Timeline)
		{
			continue;
		}

		FEdGraphPinType TimelinePinType(Schema->PC_Object, TEXT(""), UTimelineComponent::StaticClass(), false, false);

		// Previously UTimelineComponent object has exactly the same name as UTimelineTemplate object (that obj was in blueprint)
		const FString TimelineVariableName = UTimelineTemplate::TimelineTemplateNameToVariableName(Timeline->GetFName());
		UProperty* TimelineProperty = CreateVariable(*TimelineVariableName, TimelinePinType);
		if (TimelineProperty != NULL)
		{
			TimelineProperty->SetMetaData( TEXT("Category"), *Blueprint->GetName() );
			TimelineProperty->SetPropertyFlags(CPF_BlueprintVisible);

			TimelineToMemberVariableMap.Add(Timeline, TimelineProperty);
		}

		FEdGraphPinType DirectionPinType(Schema->PC_Byte, TEXT(""), FTimeline::GetTimelineDirectionEnum(), false, false);
		CreateVariable(Timeline->GetDirectionPropertyName(), DirectionPinType);

		FEdGraphPinType FloatPinType(Schema->PC_Float, TEXT(""), NULL, false, false);
		for(int32 i=0; i<Timeline->FloatTracks.Num(); i++)
		{
			CreateVariable(Timeline->GetTrackPropertyName(Timeline->FloatTracks[i].TrackName), FloatPinType);
		}

		FEdGraphPinType VectorPinType(Schema->PC_Struct, TEXT(""), VectorStruct, false, false);
		for(int32 i=0; i<Timeline->VectorTracks.Num(); i++)
		{
			CreateVariable(Timeline->GetTrackPropertyName(Timeline->VectorTracks[i].TrackName), VectorPinType);
		}

		FEdGraphPinType LinearColorPinType(Schema->PC_Struct, TEXT(""), LinearColorStruct, false, false);
		for(int32 i=0; i<Timeline->LinearColorTracks.Num(); i++)
		{
			CreateVariable(Timeline->GetTrackPropertyName(Timeline->LinearColorTracks[i].TrackName), LinearColorPinType);
		}
	}

	// Create a class property for any simple-construction-script created components that should be exposed
	if (Blueprint->SimpleConstructionScript != NULL)
	{
		// Ensure that nodes have valid templates (This will remove nodes that have had the classes the inherited from removed
		Blueprint->SimpleConstructionScript->ValidateNodeTemplates(MessageLog);

		// Ensure that variable names are valid and that there are no collisions with a parent class
		Blueprint->SimpleConstructionScript->ValidateNodeVariableNames(MessageLog);

		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node)
			{
				FName VarName = Node->GetVariableName();
				if ((VarName != NAME_None) && (Node->ComponentTemplate != NULL))
				{
					FEdGraphPinType Type(Schema->PC_Object, TEXT(""), Node->ComponentTemplate->GetClass(), false, false);
					UProperty* NewProperty = CreateVariable(VarName, Type);
					if (NewProperty != NULL)
					{
						const FText CategoryName = Node->CategoryName.IsEmpty() ? FText::FromString(Blueprint->GetName()) : Node->CategoryName ;
					
						NewProperty->SetMetaData(TEXT("Category"), *CategoryName.ToString());
						NewProperty->SetPropertyFlags(CPF_BlueprintVisible | CPF_NonTransactional );
					}
				}
			}
		}
	}
}

void FKismetCompilerContext::CreatePropertiesFromList(UStruct* Scope, UField**& PropertyStorageLocation, TIndirectArray<FBPTerminal>& Terms, uint64 PropertyFlags, bool bPropertiesAreLocal, bool bPropertiesAreParameters)
{
	for (int32 i = 0; i < Terms.Num(); ++i)
	{
		FBPTerminal& Term = Terms[i];

		if(NULL != Term.AssociatedVarProperty)
		{
			if(Term.Context && !Term.Context->IsObjectContextType())
			{
				continue;
			}
			MessageLog.Warning(*FString::Printf(*LOCTEXT("AssociatedVarProperty_Error", "AssociatedVarProperty property overridden %s from @@ type (%s)").ToString(), *Term.Name, *UEdGraphSchema_K2::TypeToText(Term.Type).ToString()), Term.Source);
		}

		if (Term.bIsLiteral)
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("PropertyForLiteral_Error", "Cannot create property for a literal: %s from @@ type (%s)").ToString(), *Term.Name, *UEdGraphSchema_K2::TypeToText(Term.Type).ToString()), Term.Source);
		}

		UProperty* NewProperty = FKismetCompilerUtilities::CreatePropertyOnScope(Scope, FName(*Term.Name), Term.Type, NewClass, PropertyFlags, Schema, MessageLog);
		if (NewProperty != NULL)
		{
			NewProperty->PropertyFlags |= PropertyFlags;

			if (bPropertiesAreParameters && Term.Type.bIsConst)
			{
				NewProperty->SetPropertyFlags(CPF_ConstParm);
			}

			if (Term.bPassedByReference)
			{
				// special case for BlueprintImplementableEvent
				if (NewProperty->HasAnyPropertyFlags(CPF_Parm) && !NewProperty->HasAnyPropertyFlags(CPF_OutParm))
				{
					NewProperty->SetPropertyFlags(CPF_OutParm | CPF_ReferenceParm);
				}
			}

			if (Term.bIsSavePersistent)
			{
				NewProperty->SetPropertyFlags(CPF_SaveGame);
			}

			// Imply read only for input object pointer parameters to a const class
			//@TODO: UCREMOVAL: This should really happen much sooner, and isn't working here
			if (bPropertiesAreParameters && ((PropertyFlags & CPF_OutParm) == 0))
			{
				if (UObjectProperty* ObjProp = Cast<UObjectProperty>(NewProperty))
				{
					UClass* EffectiveClass = NULL;
					if (ObjProp->PropertyClass != NULL)
					{
						EffectiveClass = ObjProp->PropertyClass;
					}
					else if (UClassProperty* ClassProp = Cast<UClassProperty>(ObjProp))
					{
						EffectiveClass = ClassProp->MetaClass;
					}


					if ((EffectiveClass != NULL) && (EffectiveClass->HasAnyClassFlags(CLASS_Const)))
					{
						NewProperty->PropertyFlags |= CPF_ConstParm;
					}
				}
				else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(NewProperty))
				{
					NewProperty->PropertyFlags |= CPF_ReferenceParm;

					// ALWAYS pass array parameters as out params, so they're set up as passed by ref
					if( (PropertyFlags & CPF_Parm) != 0 )
					{
						NewProperty->PropertyFlags |= CPF_OutParm;
					}
				}
			}
			
			// Link this object to the tail of the list (so properties remain in the desired order)
			*PropertyStorageLocation = NewProperty;
			PropertyStorageLocation = &(NewProperty->Next);

			Term.AssociatedVarProperty = NewProperty;
			Term.SetVarTypeLocal(bPropertiesAreLocal);

			// Record in the debugging information
			//@TODO: Rename RegisterClassPropertyAssociation, etc..., to better match that indicate it works with locals
			{
				if (Term.SourcePin)
				{
					UEdGraphPin* TrueSourcePin = MessageLog.FindSourcePin(Term.SourcePin);
					NewClass->GetDebugData().RegisterClassPropertyAssociation(TrueSourcePin, NewProperty);
				}
				else
				{
					UObject* TrueSourceObject = MessageLog.FindSourceObject(Term.Source);
					NewClass->GetDebugData().RegisterClassPropertyAssociation(TrueSourceObject, NewProperty);
				}
			}

			// Record the desired default value for this, if specified by the term
			if (!Term.PropertyDefault.IsEmpty())
			{
				if (bPropertiesAreParameters)
				{
					const bool bInputParameter = (0 == (PropertyFlags & CPF_OutParm)) && (0 != (PropertyFlags & CPF_Parm));
					if (bInputParameter)
					{
						Scope->SetMetaData(NewProperty->GetFName(), *Term.PropertyDefault);
					}
					else
					{
						MessageLog.Warning(
							*FString::Printf(*LOCTEXT("UnusedDefaultValue_Warn", "Default value for '%s' cannot be used.").ToString(), *NewProperty->GetName()), 
							Term.Source);
					}
				}
				else
				{
					SetPropertyDefaultValue(NewProperty, Term.PropertyDefault);
				}
			}
		}
		else
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("FailedCreateProperty_Error", "Failed to create property %s from @@ due to a bad or unknown type (%s)").ToString(), *Term.Name, *UEdGraphSchema_K2::TypeToText(Term.Type).ToString()),
				Term.Source);
		}

	}
}

static void SwapElementsInSingleLinkedList(UField* & PtrToFirstElement, UField* & PtrToSecondElement)
{
	check(PtrToFirstElement && PtrToSecondElement);
	UField* TempSecond = PtrToSecondElement;
	UField* TempSecondNext = PtrToSecondElement->Next;

	if (PtrToFirstElement->Next == PtrToSecondElement)
	{
		PtrToSecondElement->Next = PtrToFirstElement;
	}
	else
	{
		PtrToSecondElement->Next = PtrToFirstElement->Next;
		PtrToSecondElement = PtrToFirstElement;
	}

	PtrToFirstElement->Next = TempSecondNext;
	PtrToFirstElement = TempSecond;
}

void FKismetCompilerContext::CreateLocalVariablesForFunction(FKismetFunctionContext& Context, UFunction* ParameterSignature)
{
	ensure(Context.IsEventGraph() || !Context.EventGraphLocals.Num());
	ensure(!Context.IsEventGraph() || !Context.Locals.Num() || !UsePersistentUberGraphFrame());

	const bool bPersistentUberGraphFrame = UsePersistentUberGraphFrame() && Context.bIsUbergraph;
	// Local stack frame (or maybe class for the ubergraph)
	{
		const bool bArePropertiesLocal = true;

		// Pull the local properties generated out of the function, they will be put at the end of the list
		UField* LocalProperties = Context.Function->Children;
		Context.Function->Children = nullptr;

		UField** PropertyStorageLocation = &Context.Function->Children;
		CreatePropertiesFromList(Context.Function, PropertyStorageLocation, Context.Parameters, CPF_Parm, bArePropertiesLocal, /*bPropertiesAreParameters=*/ true);
		CreatePropertiesFromList(Context.Function, PropertyStorageLocation, Context.Results, CPF_Parm | CPF_OutParm, bArePropertiesLocal, /*bPropertiesAreParameters=*/ true);

		//MAKE SURE THE PARAMETERS ORDER MATCHES THE OVERRIDEN FUNCTION
		if (ParameterSignature)
		{
			UField** CurrentFieldStorageLocation = &Context.Function->Children;
			for (TFieldIterator<UProperty> SignatureIt(ParameterSignature); SignatureIt && ((SignatureIt->PropertyFlags & CPF_Parm) != 0); ++SignatureIt)
			{
				const FName WantedName = SignatureIt->GetFName();
				if (!*CurrentFieldStorageLocation || (WantedName != (*CurrentFieldStorageLocation)->GetFName()))
				{
					//Find Field with the proper name
					UField** FoundFieldStorageLocation = *CurrentFieldStorageLocation ? &((*CurrentFieldStorageLocation)->Next) : nullptr;
					while (FoundFieldStorageLocation && *FoundFieldStorageLocation && (WantedName != (*FoundFieldStorageLocation)->GetFName()))
					{
						FoundFieldStorageLocation = &((*FoundFieldStorageLocation)->Next);
					}

					if (FoundFieldStorageLocation && *FoundFieldStorageLocation)
					{
						// swap the found field and OverridenIt
						SwapElementsInSingleLinkedList(*CurrentFieldStorageLocation, *FoundFieldStorageLocation); //FoundFieldStorageLocation points now a random element 
					}
					else
					{
						MessageLog.Error(*FString::Printf(*LOCTEXT("WrongParameterOrder_Error", "Cannot order parameters %s in function %s.").ToString(), *WantedName.ToString(), *Context.Function->GetName()));
						break;
					}
				}
				CurrentFieldStorageLocation = &((*CurrentFieldStorageLocation)->Next);
			}
			PropertyStorageLocation = CurrentFieldStorageLocation;
			// There is no guarantee that CurrentFieldStorageLocation points the last parameter's next. We need to ensure that.
			while (*PropertyStorageLocation)
			{
				PropertyStorageLocation = &((*PropertyStorageLocation)->Next);
			}
		}

		CreatePropertiesFromList(Context.Function, PropertyStorageLocation, Context.Locals, 0, bArePropertiesLocal, /*bPropertiesAreParameters=*/ true);

		if (bPersistentUberGraphFrame)
		{
			CreatePropertiesFromList(Context.Function, PropertyStorageLocation, Context.EventGraphLocals, 0, bArePropertiesLocal, true);
		}

		// If there were local properties, place them at the end of the property storage location
		if(LocalProperties)
		{
			*PropertyStorageLocation = LocalProperties;
		}

		// Create debug data for variable reads/writes
		if (Context.bCreateDebugData)
		{
			for (int32 VarAccessIndex = 0; VarAccessIndex < Context.VariableReferences.Num(); ++VarAccessIndex)
			{
				FBPTerminal& Term = Context.VariableReferences[VarAccessIndex];

				if (Term.AssociatedVarProperty != NULL)
				{
					if (Term.SourcePin)
					{
						UEdGraphPin* TrueSourcePin = MessageLog.FindSourcePin(Term.SourcePin);
						NewClass->GetDebugData().RegisterClassPropertyAssociation(TrueSourcePin, Term.AssociatedVarProperty);
					}
					else
					{
						UObject* TrueSourceObject = MessageLog.FindSourceObject(Term.Source);
						NewClass->GetDebugData().RegisterClassPropertyAssociation(TrueSourceObject, Term.AssociatedVarProperty);
					}
				}
			}
		}

		// Fix up the return value
		//@todo:  Is there a better way of doing this without mangling code?
		const FName RetValName = FName(TEXT("ReturnValue"));
		for (TFieldIterator<UProperty> It(Context.Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			UProperty* Property = *It;
			if ((Property->GetFName() == RetValName) && Property->HasAnyPropertyFlags(CPF_OutParm))
			{
				Property->SetPropertyFlags(CPF_ReturnParm);
			}
		}
	}

	// Class
	{
		int32 PropertySafetyCounter = 100000;
		UField** PropertyStorageLocation = &(NewClass->Children);
		while (*PropertyStorageLocation != NULL)
		{
			if (--PropertySafetyCounter == 0)
			{
				checkf(false, TEXT("Property chain is corrupted;  The most likely causes are multiple properties with the same name.") );
			}

			PropertyStorageLocation = &((*PropertyStorageLocation)->Next);
		}

		const bool bArePropertiesLocal = false;
		const uint64 UbergraphHiddenVarFlags = CPF_Transient | CPF_DuplicateTransient;
		if (!bPersistentUberGraphFrame)
		{
			CreatePropertiesFromList(NewClass, PropertyStorageLocation, Context.EventGraphLocals, UbergraphHiddenVarFlags, bArePropertiesLocal);
		}

		// Handle level actor references
		const uint64 LevelActorReferenceVarFlags = 0/*CPF_Edit*/;
		CreatePropertiesFromList(NewClass, PropertyStorageLocation, Context.LevelActorReferences, LevelActorReferenceVarFlags, false);
	}
}

void FKismetCompilerContext::CreateUserDefinedLocalVariablesForFunction(FKismetFunctionContext& Context, UField**& PropertyStorageLocation)
{
	check(Context.Function->Children == NULL);

	// Create local variables from the Context entry point
	for (int32 i = 0; i < Context.EntryPoint->LocalVariables.Num(); ++i)
	{
		FBPVariableDescription& Variable = Context.EntryPoint->LocalVariables[Context.EntryPoint->LocalVariables.Num() - (i + 1)];

		// Create the property based on the variable description, scoped to the function
		UProperty* NewProperty = FKismetCompilerUtilities::CreatePropertyOnScope(Context.Function, Variable.VarName, Variable.VarType, NewClass, 0, Schema, MessageLog);
		if (NewProperty != NULL)
		{
			// Link this object to the tail of the list (so properties remain in the desired order)
			*PropertyStorageLocation = NewProperty;
			PropertyStorageLocation = &(NewProperty->Next);
		}

		if (NewProperty != NULL)
		{
			NewProperty->SetPropertyFlags(Variable.PropertyFlags);
			NewProperty->SetMetaData(TEXT("FriendlyName"), *Variable.FriendlyName);
			NewProperty->SetMetaData(TEXT("Category"), *Variable.Category.ToString());
			NewProperty->RepNotifyFunc = Variable.RepNotifyFunc;
			NewProperty->SetPropertyFlags(Variable.PropertyFlags);

			if(!Variable.DefaultValue.IsEmpty())
			{
				SetPropertyDefaultValue(NewProperty, Variable.DefaultValue);
			}
		}
	}
}

void FKismetCompilerContext::SetPropertyDefaultValue(const UProperty* PropertyToSet, FString& Value)
{
	DefaultPropertyValueMap.Add(PropertyToSet->GetFName(), Value);
}

/** Copies default values cached for the terms in the DefaultPropertyValueMap to the final CDO */
void FKismetCompilerContext::CopyTermDefaultsToDefaultObject(UObject* DefaultObject)
{
	// Assign all default object values from the map to the new CDO
	for( TMap<FName, FString>::TIterator PropIt(DefaultPropertyValueMap); PropIt; ++PropIt )
	{
		FName TargetPropName = PropIt.Key();
		FString Value = PropIt.Value();

		for (TFieldIterator<UProperty> It(DefaultObject->GetClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			UProperty* Property = *It;
			if (Property->GetFName() == TargetPropName)
			{
				const bool bParseSuccedded = FBlueprintEditorUtils::PropertyValueFromString(Property, Value, reinterpret_cast<uint8*>(DefaultObject));
				if(!bParseSuccedded)
				{
					const FString ErrorMessage = *FString::Printf(*LOCTEXT("ParseDefaultValueError", "Can't parse default value '%s' for @@. Property: %s.").ToString(), *Value, *Property->GetName());
					UObject* InstigatorObject = NewClass->GetDebugData().FindObjectThatCreatedProperty(Property);
					if(InstigatorObject)
					{
						MessageLog.Warning(*ErrorMessage, InstigatorObject);
					}
					else
					{
						UEdGraphPin* InstigatorPin = NewClass->GetDebugData().FindPinThatCreatedProperty(Property);
						MessageLog.Warning(*ErrorMessage, InstigatorPin);
					}
				}

				break;
			}
		}			
	}
}

void FKismetCompilerContext::PrintVerboseInfoStruct(UStruct* Struct) const
{
	for (TFieldIterator<UProperty> PropIt(Struct); PropIt; ++PropIt)
	{
		UProperty* Prop = *PropIt;

		MessageLog.Note(*FString::Printf(*LOCTEXT("StructInfo_Note", "  %s named %s at offset %d with size %d [dim = %d] and flags %x").ToString(),
			*Prop->GetClass()->GetDescription(),
			*Prop->GetName(),
			Prop->GetOffset_ForDebug(),
			Prop->ElementSize,
			Prop->ArrayDim,
			Prop->PropertyFlags));
	}
}

void FKismetCompilerContext::PrintVerboseInformation(UClass* Class) const
{
	MessageLog.Note(*FString::Printf(*LOCTEXT("ClassHasMembers_Note", "Class %s has members:").ToString(), *Class->GetName()));
	PrintVerboseInfoStruct(Class);

	for (int32 i = 0; i < FunctionList.Num(); ++i)
	{
		const FKismetFunctionContext& Context = FunctionList[i];

		if (Context.IsValid())
		{
			MessageLog.Note(*FString::Printf(*LOCTEXT("FunctionHasMembers_Note", "Function %s has members:").ToString(), *Context.Function->GetName()));
			PrintVerboseInfoStruct(Context.Function);
		}
		else
		{
			MessageLog.Note(*FString::Printf(*LOCTEXT("FunctionCompileFailed_Note", "Function #%d failed to compile and is not valid.").ToString(), i));
		}
	}
}

void FKismetCompilerContext::CheckConnectionResponse(const FPinConnectionResponse &Response, const UEdGraphNode *Node)
{
	if (!Response.CanSafeConnect())
	{
		MessageLog.Error(*FString::Printf(*LOCTEXT("FailedBuildingConnection_Error", "COMPILER ERROR: failed building connection with '%s' at @@").ToString(), *Response.Message.ToString()), Node);
	}
}

/**
 * Performs transformations on specific nodes that require it according to the schema
 */
void FKismetCompilerContext::TransformNodes(FKismetFunctionContext& Context)
{
	// Give every node a chance to transform itself
	for (int32 NodeIndex = 0; NodeIndex < Context.SourceGraph->Nodes.Num(); ++NodeIndex)
	{
		UEdGraphNode* Node = Context.SourceGraph->Nodes[NodeIndex];

		if (FNodeHandlingFunctor* Handler = NodeHandlers.FindRef(Node->GetClass()))
		{
			Handler->Transform(Context, Node);
		}
		else
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("UnexpectedNodeType_Error", "Unexpected node type %s encountered at @@").ToString(), *(Node->GetClass()->GetName())), Node);
		}
	}
}


struct FNodeVisitorDownExecWires
{
	TSet<UEdGraphNode*> VisitedNodes;
	UEdGraphSchema_K2* Schema;

	void TouchNode(UEdGraphNode* Node)
	{
	}

	void TraverseNodes(UEdGraphNode* Node)
	{
		VisitedNodes.Add(Node);
		TouchNode(Node);

		// Follow every exec output pin
		for (int32 i = 0; i < Node->Pins.Num(); ++i)
		{
			UEdGraphPin* MyPin = Node->Pins[i];

			if ((MyPin->Direction == EGPD_Output) && (Schema->IsExecPin(*MyPin)))
			{
				for (int32 j = 0; j < MyPin->LinkedTo.Num(); ++j)
				{
					UEdGraphPin* OtherPin = MyPin->LinkedTo[j];
					if( OtherPin )
					{
						UEdGraphNode* OtherNode = OtherPin->GetOwningNode();
						if (!VisitedNodes.Contains(OtherNode))
						{
							TraverseNodes(OtherNode);
						}
					}
				}
			}
		}
	}
};

bool FKismetCompilerContext::CanIgnoreNode(const UEdGraphNode* Node) const
{
	if (const UK2Node* K2Node = Cast<const UK2Node>(Node))
	{
		return K2Node->IsNodeSafeToIgnore();
	}
	return false;
}

bool FKismetCompilerContext::ShouldForceKeepNode(const UEdGraphNode* Node) const
{
	if (Node->IsA(UEdGraphNode_Comment::StaticClass()) && CompileOptions.bSaveIntermediateProducts)
	{
		// Preserve comment nodes when debugging the compiler
		return true;
	}
	else
	{
		return false;
	}
}

/** Prunes any nodes that weren't visited from the graph, printing out a warning */
void FKismetCompilerContext::PruneIsolatedNodes(const TArray<UEdGraphNode*>& RootSet, TArray<UEdGraphNode*>& GraphNodes)
{
	//@TODO: This function crawls the graph twice (once here and once in Super, could potentially combine them, with a bitflag for flows reached via exec wires)

	// Prune the impure nodes that aren't reachable via any (even impossible, e.g., a branch never taken) execution flow
	FNodeVisitorDownExecWires Visitor;
	Visitor.Schema = Schema;

	for (TArray<UEdGraphNode*>::TConstIterator It(RootSet); It; ++It)
	{
		UEdGraphNode* RootNode = *It;
		Visitor.TraverseNodes(RootNode);
	}

	for (int32 NodeIndex = 0; NodeIndex < GraphNodes.Num(); ++NodeIndex)
	{
		UEdGraphNode* Node = GraphNodes[NodeIndex];
		if (!Node || (!Visitor.VisitedNodes.Contains(Node) && !IsNodePure(Node)))
		{
			if (!CanIgnoreNode(Node))
			{
				// Disabled this warning, because having orphaned chains is standard workflow for LDs
				//MessageLog.Warning(TEXT("Node @@ will never be executed and is being pruned"), Node);
			}

			if (!Node || !ShouldForceKeepNode(Node))
			{
				if (Node)
				{
					Node->BreakAllNodeLinks();
				}
				GraphNodes.RemoveAtSwap(NodeIndex);
				--NodeIndex;
			}
		}
	}

	// Prune the nodes that aren't even reachable via data dependencies
	Super::PruneIsolatedNodes(RootSet, GraphNodes);
}

/**
 *	Checks if self pins are connected.
 */
void FKismetCompilerContext::ValidateSelfPinsInGraph(FKismetFunctionContext& Context)
{
	const UEdGraph* SourceGraph = Context.SourceGraph;

	check(NULL != Schema);
	for (int32 NodeIndex = 0; NodeIndex < SourceGraph->Nodes.Num(); ++NodeIndex)
	{
		if(const UEdGraphNode* Node = SourceGraph->Nodes[NodeIndex])
		{
			for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
			{
				if (const UEdGraphPin* Pin = Node->Pins[PinIndex])
				{
					if (Schema->IsSelfPin(*Pin) && (Pin->LinkedTo.Num() == 0) && Pin->DefaultObject == nullptr)
					{
						FKismetCompilerUtilities::ValidateSelfCompatibility(Pin, Context);
					}
				}
			}
		}
	}
}

void FKismetCompilerContext::ValidateNoWildcardPinsInGraph(const UEdGraph* SourceGraph)
{
	for (int32 NodeIndex = 0; NodeIndex < SourceGraph->Nodes.Num(); ++NodeIndex)
	{
		if (const UEdGraphNode* Node = SourceGraph->Nodes[NodeIndex])
		{
			for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
			{
				if (const UEdGraphPin* Pin = Node->Pins[PinIndex])
				{
					if (Pin->PinType.PinCategory == Schema->PC_Wildcard)
					{
						// Wildcard pins should never be seen by the compiler; they should always be forced into a particular type by wiring.
						MessageLog.Error(*LOCTEXT("UndeterminedPinType_Error", "The type of @@ is undetermined.  Connect something to @@ to imply a specific type.").ToString(), Pin, Pin->GetOwningNodeUnchecked());
					}
				}
			}
		}
	}
}

/**
 * First phase of compiling a function graph
 *   - Prunes the 'graph' to only included the connected portion that contains the function entry point 
 *   - Schedules execution of each node based on data dependencies
 *   - Creates a UFunction object containing parameters and local variables (but no script code yet)
 */
void FKismetCompilerContext::PrecompileFunction(FKismetFunctionContext& Context)
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_PrecompileFunction);

	// Find the root node, which will drive everything else
	check(Context.RootSet.Num() == 0);
	FindNodesByClass(Context.SourceGraph, UK2Node_FunctionEntry::StaticClass(), Context.RootSet);

	if (Context.RootSet.Num())
	{
		Context.EntryPoint = CastChecked<UK2Node_FunctionEntry>(Context.RootSet[0]);

		// Make sure there was only one function entry node
		for (int32 i = 1; i < Context.RootSet.Num(); ++i)
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("ExpectedOneFunctionEntry_Error", "Expected only one function entry node in graph @@, but found both @@ and @@").ToString()),
				Context.SourceGraph,
				Context.EntryPoint,
				Context.RootSet[i]);
		}

		// Find any other entry points caused by special nodes
		FindNodesByClass(Context.SourceGraph, UK2Node_Event::StaticClass(), Context.RootSet);
		FindNodesByClass(Context.SourceGraph, UK2Node_Timeline::StaticClass(), Context.RootSet);

		// Find the connected subgraph starting at the root node and prune out unused nodes
		PruneIsolatedNodes(Context.RootSet, Context.SourceGraph->Nodes);

		if (bIsFullCompile)
		{
			// Check if self pins are connected and types are resolved after PruneIsolatedNodes, to avoid errors from isolated nodes.
			ValidateSelfPinsInGraph(Context);
			ValidateNoWildcardPinsInGraph(Context.SourceGraph);

			for (int32 ChildIndex = 0; ChildIndex < Context.SourceGraph->Nodes.Num(); ++ChildIndex)
			{
				if (const UK2Node* K2Node = Cast<const UK2Node>(Context.SourceGraph->Nodes[ChildIndex]))
				{
					K2Node->ValidateNodeAfterPrune(Context.MessageLog);
				}
			}

			// Transforms
			TransformNodes(Context);
		}

		// Create the function stub
		FName NewFunctionName = (Context.EntryPoint->CustomGeneratedFunctionName != NAME_None) ? Context.EntryPoint->CustomGeneratedFunctionName : Context.EntryPoint->SignatureName;
		if(Context.IsDelegateSignature())
		{
			// prefix with the the blueprint name to avoid conflicts with natively defined delegate signatures
			FString Name = NewFunctionName.ToString();
			Name += HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX;
			NewFunctionName = FName(*Name);
		}

		// Determine if this is a new function or if it overrides a parent function
			//@TODO: Does not support multiple overloads for a parent virtual function
		UClass* SuperClass = Context.NewClass->GetSuperClass();
		UFunction* ParentFunction = Context.NewClass->GetSuperClass()->FindFunctionByName(NewFunctionName);
		
		const FString NewFunctionNameString = NewFunctionName.ToString();
		if (CreatedFunctionNames.Contains(NewFunctionNameString))
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("DuplicateFunctionName_Error", "Found more than one function with the same name %s; second occurance at @@").ToString(), *NewFunctionNameString), Context.EntryPoint);
			return;
		}
		else if(NULL != FindField<UProperty>(NewClass, NewFunctionName))
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("DuplicateFieldName_Error", "Name collision - function and property have the same name - '%s'. @@").ToString(), *NewFunctionNameString), Context.EntryPoint);
			return;
		}
		else
		{
			CreatedFunctionNames.Add(NewFunctionNameString);
		}

		Context.Function = NewObject<UFunction>(NewClass, NewFunctionName, RF_Public);

#if USE_TRANSIENT_SKELETON
		// Propagate down transient settings from the class
		if (NewClass->HasAnyFlags(RF_Transient))
		{
			Context.Function->SetFlags(RF_Transient);
		}
#endif

		Context.Function->SetSuperStruct( ParentFunction );
		Context.Function->RepOffset = MAX_uint16;
		Context.Function->ReturnValueOffset = MAX_uint16;
		Context.Function->FirstPropertyToInit = NULL;

		// Set up the function category
		FKismetUserDeclaredFunctionMetadata& FunctionMetaData = Context.EntryPoint->MetaData;
		if( !FunctionMetaData.Category.IsEmpty() )
		{
			Context.Function->SetMetaData(FBlueprintMetadata::MD_FunctionCategory, *FunctionMetaData.Category.ToString());
		}

		// Set up the function keywords
		if( !FunctionMetaData.Keywords.IsEmpty() )
		{
			Context.Function->SetMetaData(FBlueprintMetadata::MD_FunctionKeywords, *FunctionMetaData.Keywords.ToString());
		}

		// Set up the function compact node title
		if( !FunctionMetaData.CompactNodeTitle.IsEmpty() )
		{
			Context.Function->SetMetaData(FBlueprintMetadata::MD_CompactNodeTitle, *FunctionMetaData.CompactNodeTitle.ToString());
		}

		// Set as blutility function
		if( FunctionMetaData.bCallInEditor )
		{
			Context.Function->SetMetaData(FBlueprintMetadata::MD_CallInEditor, TEXT( "true" ));
		}

		// Set the required function flags
		if (Context.CanBeCalledByKismet())
		{
			Context.Function->FunctionFlags |= FUNC_BlueprintCallable;
		}

		if (Context.IsInterfaceStub())
		{
			Context.Function->FunctionFlags |= FUNC_BlueprintEvent;
		}

		// Inherit extra flags from the entry node
		if (Context.EntryPoint)
		{
			Context.Function->FunctionFlags |= Context.EntryPoint->GetExtraFlags();
		}

		// First try to get the overriden function from the super class
		UFunction* OverridenFunction = Context.Function->GetSuperFunction();
		// If we couldn't find it, see if we can find an interface class in our inheritance to get it from
		if (!OverridenFunction && Context.Blueprint)
		{
			bool bInvalidInterface = false;
			OverridenFunction = FBlueprintEditorUtils::FindFunctionInImplementedInterfaces( Context.Blueprint, Context.Function->GetFName(), &bInvalidInterface );
			if(bInvalidInterface)
			{
				MessageLog.Warning(TEXT("Blueprint tried to implement invalid interface."));
			}
		}

		// Inherit flags and validate against overridden function if it exists
		if (OverridenFunction)
		{
			Context.Function->FunctionFlags |= (OverridenFunction->FunctionFlags & (FUNC_FuncInherit | FUNC_Public | FUNC_Protected | FUNC_Private | FUNC_BlueprintPure));

			if ((Context.Function->FunctionFlags & FUNC_AccessSpecifiers) != (OverridenFunction->FunctionFlags & FUNC_AccessSpecifiers))
			{
				MessageLog.Error(*LOCTEXT("IncompatibleAccessSpecifier_Error", "Access specifier is not compatible the parent function @@").ToString(), Context.EntryPoint);
			}

			const uint32 OverrideFlagsToCheck = (FUNC_FuncOverrideMatch & ~FUNC_AccessSpecifiers);
			if ((Context.Function->FunctionFlags & OverrideFlagsToCheck) != (OverridenFunction->FunctionFlags & OverrideFlagsToCheck))
			{
				MessageLog.Error(*LOCTEXT("IncompatibleOverrideFlags_Error", "Overriden function is not compatible with the parent function @@. Check flags: Exec, Final, Static.").ToString(), Context.EntryPoint);
			}

			// Copy metadata from parent function as well
			UMetaData::CopyMetadata(OverridenFunction, Context.Function);
		}
		else
		{
			// If this is the root of a blueprint-defined function or event, and if it's public, make it overrideable
			if( !Context.IsEventGraph() && !Context.Function->HasAnyFunctionFlags(FUNC_Private) )
			{
				Context.Function->FunctionFlags |= FUNC_BlueprintEvent;
			}
		}
		
		// Link it
		//@TODO: should this be in regular or reverse order?
		Context.Function->Next = Context.NewClass->Children;
		Context.NewClass->Children = Context.Function;

		// Add the function to it's owner class function name -> function map
		Context.NewClass->AddFunctionToFunctionMap(Context.Function);
		if (UsePersistentUberGraphFrame() && Context.bIsUbergraph)
		{
			ensure(!NewClass->UberGraphFunction);
			NewClass->UberGraphFunction = Context.Function;
		}

		// Create any user defined variables, this must occur before registering nets so that the properties are in place
		UField** PropertyStorageLocation = &(Context.Function->Children);
		CreateUserDefinedLocalVariablesForFunction(Context, PropertyStorageLocation);

		//@TODO: Prune pure functions that don't have any consumers
		if (bIsFullCompile)
		{
			if (!Blueprint->bIsRegeneratingOnLoad)
			{
				BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_AnalyzeExecutionPath);
				FKismetCompilerUtilities::ValidateProperEndExecutionPath(Context);
			}

			// Find the execution path (and make sure it has no cycles)
			CreateExecutionSchedule(Context.SourceGraph->Nodes, Context.LinearExecutionList);

			for (int32 NodeIndex = 0; NodeIndex < Context.LinearExecutionList.Num(); ++NodeIndex)
			{
				UEdGraphNode* Node = Context.LinearExecutionList[NodeIndex];

				// Register nets in the schedule
				if (FNodeHandlingFunctor* Handler = NodeHandlers.FindRef(Node->GetClass()))
				{
					Handler->RegisterNets(Context, Node);
				}
				else
				{
					MessageLog.Error(*FString::Printf(*LOCTEXT("UnexpectedNodeType_Error", "Unexpected node type %s encountered at @@").ToString(), *(Node->GetClass()->GetName())), Node);
				}
			}
		}
		else
		{
			for (auto Node : Context.SourceGraph->Nodes)
			{
				const bool bGenerateParameters = Node->IsA<UK2Node_FunctionEntry>();
				const bool bGenerateResult = Node->IsA<UK2Node_FunctionResult>();
				if (bGenerateParameters || bGenerateResult)
				{
					if (FNodeHandlingFunctor* Handler = NodeHandlers.FindRef(Node->GetClass()))
					{
						Handler->RegisterNets(Context, Node);
					}
				}
			}
		}
	
		// Create variable declarations
		CreateLocalVariablesForFunction(Context, ParentFunction ? ParentFunction : OverridenFunction);

		//Validate AccessSpecifier
		const uint32 AccessSpecifierFlag = FUNC_AccessSpecifiers & Context.EntryPoint->GetExtraFlags();
		const bool bAcceptedAccessSpecifier = 
			(0 == AccessSpecifierFlag) || (FUNC_Public == AccessSpecifierFlag) || (FUNC_Protected == AccessSpecifierFlag) || (FUNC_Private == AccessSpecifierFlag);
		if(!bAcceptedAccessSpecifier)
		{
			MessageLog.Warning(*LOCTEXT("WrongAccessSpecifier_Error", "Wrong access specifier @@").ToString(), Context.EntryPoint);
		}

		Context.Function->FunctionFlags |= Context.GetNetFlags();

		// Parameter list needs to be linked before signatures are compared. 
		Context.Function->StaticLink(true);

		// Make sure the function signature is valid if this is an override
		if (ParentFunction)
		{
			// Verify the signature
			if (!ParentFunction->IsSignatureCompatibleWith(Context.Function))
			{
				FString SignatureClassName("");
				if (Context.EntryPoint->SignatureClass)
				{
					SignatureClassName = Context.EntryPoint->SignatureClass->GetName();
				}
				MessageLog.Error(*FString::Printf(*LOCTEXT("OverrideFunctionDifferentSignature_Error", "Cannot override '%s::%s' at @@ which was declared in a parent with a different signature").ToString(), *SignatureClassName, *NewFunctionNameString), Context.EntryPoint);
			}
			const bool bEmptyCase = (0 == AccessSpecifierFlag);
			const bool bDifferentAccessSpecifiers = AccessSpecifierFlag != (ParentFunction->FunctionFlags & FUNC_AccessSpecifiers);
			if( !bEmptyCase && bDifferentAccessSpecifiers )
			{
				MessageLog.Warning(*LOCTEXT("IncompatibleAccessSpecifier_Error", "Access specifier is not compatible the parent function @@").ToString(), Context.EntryPoint);
			}

			uint32 const ParentNetFlags = (ParentFunction->FunctionFlags & FUNC_NetFuncFlags);
			if (ParentNetFlags != Context.GetNetFlags())
			{
				MessageLog.Error(*LOCTEXT("MismatchedNetFlags_Error", "@@ function's net flags don't match parent function's flags").ToString(), Context.EntryPoint);
				
				// clear the existing net flags
				Context.Function->FunctionFlags &= ~(FUNC_NetFuncFlags);
				// have to replace with the parent's net flags, or this will   
				// trigger an assert in Link()
				Context.Function->FunctionFlags |= ParentNetFlags;
			}
		}

		////////////////////////////////////////

		if(Context.IsDelegateSignature())
		{
			Context.Function->FunctionFlags |= FUNC_Delegate;
			UMulticastDelegateProperty* Property = FindObject<UMulticastDelegateProperty>(Context.NewClass, *Context.DelegateSignatureName.ToString());
			if(Property)
			{
				Property->SignatureFunction = Context.Function;
			}
			else
			{
				MessageLog.Warning(*LOCTEXT("NoDelegateProperty_Error", "No delegate property found for @@").ToString(), Context.SourceGraph);
			}
		}

	}
	else
	{
		MessageLog.Error(*LOCTEXT("NoRootNodeFound_Error", "Could not find a root node for the graph @@").ToString(), Context.SourceGraph);
	}
}

/** Inserts a new item into an array in a sorted position; using an externally stored sort index map */
template<typename DataType, typename SortKeyType>
void OrderedInsertIntoArray(TArray<DataType>& Array, const TMap<DataType, SortKeyType>& SortKeyMap, const DataType& NewItem)
{
	const SortKeyType NewItemKey = SortKeyMap.FindChecked(NewItem);

	for (int32 i = 0; i < Array.Num(); ++i)
	{
		DataType& TestItem = Array[i];
		const SortKeyType TestItemKey = SortKeyMap.FindChecked(TestItem);

		if (TestItemKey > NewItemKey)
		{
			Array.Insert(NewItem, i);
			return;
		}
	}

	Array.Add(NewItem);
}

/**
 * Second phase of compiling a function graph
 *   - Generates executable code and performs final validation
 */
void FKismetCompilerContext::CompileFunction(FKismetFunctionContext& Context)
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_CompileFunction);

	check(Context.IsValid());

	// Generate statements for each node in the linear execution order (which should roughly correspond to the final execution order)
	TMap<UEdGraphNode*, int32> SortKeyMap;
	int32 NumNodesAtStart = Context.LinearExecutionList.Num();
	for (int32 i = 0; i < Context.LinearExecutionList.Num(); ++i)
	{
		UEdGraphNode* Node = Context.LinearExecutionList[i];
		SortKeyMap.Add(Node, i);

		const FString NodeComment = Node->NodeComment.IsEmpty() ? Node->GetName() : Node->NodeComment;
		const bool bPureNode = IsNodePure(Node);
		// Debug comments
		if (KismetCompilerDebugOptions::EmitNodeComments && !Context.bGeneratingCpp)
		{
			FBlueprintCompiledStatement& Statement = Context.AppendStatementForNode(Node);
			Statement.Type = KCST_Comment;
			Statement.Comment = NodeComment;
		}

		// Debug opcode insertion point
		if (Context.IsDebuggingOrInstrumentationRequired())
		{
			if (!bPureNode)
			{
				bool bEmitDebuggingSite = true;

				if (Context.IsEventGraph() && (Node->IsA(UK2Node_FunctionEntry::StaticClass())))
				{
					// The entry point in the ubergraph is a non-visual construct, and will lead to some
					// other 'fake' entry point such as an event or latent action.  Therefore, don't create
					// debug data for the behind-the-scenes entry point, only for the user-visible ones.
					bEmitDebuggingSite = false;
				}
				if (bEmitDebuggingSite)
				{
					FBlueprintCompiledStatement& Statement = Context.AppendStatementForNode(Node);
					Statement.Type = Context.GetBreakpointType();
					if (Context.IsInstrumentationRequired())
					{
						for (auto Pin : Node->Pins)
						{
							if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
							{
								Statement.ExecContext = Pin;
								break;
							}
						}
					}
					Statement.Comment = NodeComment;
				}
			}
			else if (Context.IsInstrumentationRequired())
			{
				FBlueprintCompiledStatement& Statement = Context.AppendStatementForNode(Node);
				Statement.Type = KCST_InstrumentedPureNodeEntry;
			}
		}

		// Let the node handlers try to compile it
		if (FNodeHandlingFunctor* Handler = NodeHandlers.FindRef(Node->GetClass()))
		{
			Handler->Compile(Context, Node);
		}
		else
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("UnexpectedNodeTypeWhenCompilingFunc_Error", "Unexpected node type %s encountered in execution chain at @@").ToString(), *(Node->GetClass()->GetName())), Node);
		}
	}
	
	// The LinearExecutionList should be immutable at this point
	check(Context.LinearExecutionList.Num() == NumNodesAtStart);

	// Now pull out pure chains and inline their generated code into the nodes that need it
	TMap< UEdGraphNode*, TSet<UEdGraphNode*> > PureNodesNeeded;
	
	for (int32 TestIndex = 0; TestIndex < Context.LinearExecutionList.Num(); )
	{
		UEdGraphNode* Node = Context.LinearExecutionList[TestIndex];

		// List of pure nodes this node depends on.
		bool bHasAntecedentPureNodes = PureNodesNeeded.Contains(Node);

		if (IsNodePure(Node))
		{
			// For profiling purposes, find the statement that marks the function's entry point.
			FBlueprintCompiledStatement* ProfilerStatement = nullptr;
			TArray<FBlueprintCompiledStatement*>* SourceStatementList = Context.StatementsPerNode.Find(Node);
			const bool bDidNodeGenerateCode = SourceStatementList != nullptr && SourceStatementList->Num() > 0;
			if (bDidNodeGenerateCode)
			{
				for (auto Statement : *SourceStatementList)
				{
					if (Statement && Statement->Type == KCST_InstrumentedPureNodeEntry)
					{
						ProfilerStatement = Statement;
						break;
					}
				}
			}

			// Push this node to the requirements list of any other nodes using it's outputs, if this node had any real impact
			if (bDidNodeGenerateCode || bHasAntecedentPureNodes)
			{
				for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
				{
					UEdGraphPin* Pin = Node->Pins[PinIndex];
					if (Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
					{
						// Record the pure node output pin, since it's linked
						if (ProfilerStatement)
						{
							ProfilerStatement->PureOutputContextArray.AddUnique(Pin);
						}

						for (UEdGraphPin* LinkedTo : Pin->LinkedTo)
						{
							UEdGraphNode* NodeUsingOutput = LinkedTo->GetOwningNode();
							if (NodeUsingOutput != nullptr)
							{
								// Add this node, as well as other nodes this node depends on
								TSet<UEdGraphNode*>& TargetNodesRequired = PureNodesNeeded.FindOrAdd(NodeUsingOutput);
								TargetNodesRequired.Add(Node);
								if (bHasAntecedentPureNodes)
								{
									TargetNodesRequired.Append(PureNodesNeeded.FindChecked(Node));
								}
							}
						}
					}
				}
			}

			// Remove it from the linear execution list; the dependent nodes will inline the code when necessary
			Context.LinearExecutionList.RemoveAt(TestIndex);
		}
		else
		{
			if (bHasAntecedentPureNodes)
			{
				// This node requires the output of one or more pure nodes, so that pure code needs to execute at this node

				// Sort the nodes by execution order index
				TSet<UEdGraphNode*>& AntecedentPureNodes = PureNodesNeeded.FindChecked(Node);
				TArray<UEdGraphNode*> SortedPureNodes;
				for (TSet<UEdGraphNode*>::TIterator It(AntecedentPureNodes); It; ++It)
				{
					OrderedInsertIntoArray(SortedPureNodes, SortKeyMap, *It);
				}

				if (Context.IsInstrumentationRequired())
				{
					FBlueprintCompiledStatement& PopState = Context.PrependStatementForNode(Node);
					PopState.Type = KCST_InstrumentedStatePop;
				}

				// Inline their code
				for (int32 i = 0; i < SortedPureNodes.Num(); ++i)
				{
					UEdGraphNode* NodeToInline = SortedPureNodes[SortedPureNodes.Num() - 1 - i];

					Context.CopyAndPrependStatements(Node, NodeToInline);
				}

				if (Context.IsInstrumentationRequired())
				{
					FBlueprintCompiledStatement& PushState = Context.PrependStatementForNode(Node);
					PushState.Type = KCST_InstrumentedStatePush;
				}
			}

			// Proceed to the next node
			++TestIndex;
		}
	}

	if (Context.bIsUbergraph && CompileOptions.DoesRequireCppCodeGeneration())
	{
		Context.UnsortedSeparateExecutionGroups = FKismetCompilerUtilities::FindUnsortedSeparateExecutionGroups(Context.LinearExecutionList);
	}

}

/**
 * Final phase of compiling a function graph; called after all functions have had CompileFunction called
 *   - Patches up cross-references, etc..., and performs final validation
 */
void FKismetCompilerContext::PostcompileFunction(FKismetFunctionContext& Context)
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_PostcompileFunction);

	// The function links gotos, sorts statments, and merges adjacent ones. 
	Context.ResolveStatements();

	//@TODO: Code generation (should probably call backend here, not later)

	// Seal the function, it's done!
	FinishCompilingFunction(Context);
}

/**
 * Handles final post-compilation setup, flags, creates cached values that would normally be set during deserialization, etc...
 */
void FKismetCompilerContext::FinishCompilingFunction(FKismetFunctionContext& Context)
{
	UFunction* Function = Context.Function;
	Function->Bind();
	Function->StaticLink(true);

	// Set function flags and calculate cached values so the class can be used immediately
	Function->ParmsSize = 0;
	Function->NumParms = 0;
	Function->ReturnValueOffset = MAX_uint16;

	for (TFieldIterator<UProperty> PropIt(Function, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
	{
		UProperty* Property = *PropIt;
		if (Property->HasAnyPropertyFlags(CPF_Parm))
		{
			++Function->NumParms;
			Function->ParmsSize = Property->GetOffset_ForUFunction() + Property->GetSize();

			if (Property->HasAnyPropertyFlags(CPF_OutParm))
			{
				Function->FunctionFlags |= FUNC_HasOutParms;
			}

			if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				Function->ReturnValueOffset = Property->GetOffset_ForUFunction();
			}
		}
		else
		{
			if (!Property->HasAnyPropertyFlags(CPF_ZeroConstructor))
			{
			Function->FirstPropertyToInit = Property;
			Function->FunctionFlags |= FUNC_HasDefaults;
				break;
		}
		}
	}

	// Add in any extra user-defined metadata, like tooltip
	UK2Node_FunctionEntry* EntryNode = CastChecked<UK2Node_FunctionEntry>(Context.EntryPoint);
	if (!EntryNode->MetaData.ToolTip.IsEmpty())
	{
		Function->SetMetaData(FBlueprintMetadata::MD_Tooltip, *EntryNode->MetaData.ToolTip);
	}
	if (EntryNode->MetaData.bCallInEditor)
	{
		Function->SetMetaData(FBlueprintMetadata::MD_CallInEditor, TEXT( "true" ));
	}
	if (auto WorldContextPin = EntryNode->GetAutoWorldContextPin())
	{
		Function->SetMetaData(FBlueprintMetadata::MD_WorldContext, *WorldContextPin->PinName);
	}

	for (int32 EntryPinIndex = 0; EntryPinIndex < EntryNode->Pins.Num(); ++EntryPinIndex)
	{
		UEdGraphPin* EntryPin = EntryNode->Pins[EntryPinIndex];
		// No defaults for object/class pins
		if(	!Schema->IsMetaPin(*EntryPin) && 
			(EntryPin->PinType.PinCategory != Schema->PC_Object) && 
			(EntryPin->PinType.PinCategory != Schema->PC_Class) && 
			(EntryPin->PinType.PinCategory != Schema->PC_Interface) && 
			!EntryPin->DefaultValue.IsEmpty() )
		{
			Function->SetMetaData(*EntryPin->PinName, *EntryPin->DefaultValue);
		}
	}
}

/**
 * Handles adding the implemented interface information to the class
 */
void FKismetCompilerContext::AddInterfacesFromBlueprint(UClass* Class)
{
	// Make sure we actually have some interfaces to implement
	if( Blueprint->ImplementedInterfaces.Num() == 0 )
	{
		return;
	}

	// Iterate over all implemented interfaces, and add them to the class
	for(int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); i++)
	{
		UClass* Interface = Cast<UClass>(Blueprint->ImplementedInterfaces[i].Interface);
		if( Interface )
		{
			// Make sure it's a valid interface
			check(Interface->HasAnyClassFlags(CLASS_Interface));

			//propogate the inheritable ClassFlags
			Class->ClassFlags |= (Interface->ClassFlags) & CLASS_ScriptInherit;

			new (Class->Interfaces) FImplementedInterface(Interface, 0, true);
		}
	}
}

/**
 * Handles final post-compilation setup, flags, creates cached values that would normally be set during deserialization, etc...
 */
void FKismetCompilerContext::FinishCompilingClass(UClass* Class)
{
	UClass* ParentClass = Class->GetSuperClass();

	FBlueprintEditorUtils::RecreateClassMetaData(Blueprint, Class, false);

	if (ParentClass != NULL)
	{
		// Propagate the new parent's inheritable class flags
		Class->ReferenceTokenStream.Empty();
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		Class->DebugTokenMap.Empty();
#endif
		Class->ClassFlags &= ~CLASS_RecompilerClear;
		Class->ClassFlags |= (ParentClass->ClassFlags & CLASS_ScriptInherit);//@TODO: ChangeParentClass had this, but I don't think I want it: | UClass::StaticClassFlags;  // will end up with CLASS_Intrinsic
		Class->ClassCastFlags |= ParentClass->ClassCastFlags;
		Class->ClassWithin = ParentClass->ClassWithin ? ParentClass->ClassWithin : UObject::StaticClass();
		Class->ClassConfigName = ParentClass->ClassConfigName;

		// If the Blueprint was marked as deprecated, then flag the class as deprecated.
		if(Blueprint->bDeprecate)
		{
			Class->ClassFlags |= CLASS_Deprecated;
		}

		// If the flag is inherited, this will keep the bool up-to-date
		Blueprint->bDeprecate = (Class->ClassFlags & CLASS_Deprecated) == CLASS_Deprecated;
		
		// If the Blueprint was marked as abstract, then flag the class as abstract.
		if (Blueprint->bGenerateAbstractClass)
		{
			NewClass->ClassFlags |= CLASS_Abstract;
		}
		Blueprint->bGenerateAbstractClass = (Class->ClassFlags & CLASS_Abstract) == CLASS_Abstract;	

		// Add the description to the tooltip
		if (!Blueprint->BlueprintDescription.IsEmpty())
		{
			static const FName NAME_Tooltip(TEXT("Tooltip"));
			Class->SetMetaData(NAME_Tooltip, *Blueprint->BlueprintDescription);
		}

		// Copy the category info from the parent class
#if WITH_EDITORONLY_DATA
		
		// Blueprinted Components are always Blueprint Spawnable
		if (ParentClass->IsChildOf(UActorComponent::StaticClass()))
		{
			FComponentTypeRegistry::Get().InvalidateClass(Class);
		}
#endif

		// Add in additional flags implied by the blueprint
		switch (Blueprint->BlueprintType)
		{
			case BPTYPE_MacroLibrary:
				Class->ClassFlags |= CLASS_Abstract | CLASS_NotPlaceable;
				break;
			case BPTYPE_Const:
				Class->ClassFlags |= CLASS_Const;
				break;
		}

		//@TODO: Might want to be able to specify some of these here too
	}

	// Add in any other needed flags
	Class->ClassFlags |= (CLASS_Parsed | CLASS_CompiledFromBlueprint);

	// Look for OnRep 
	for( TFieldIterator<UProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		UProperty *Property = *It;
		if (Property->HasAnyPropertyFlags(CPF_Net))
		{
			// Verify rep notifies are valid, if not, clear them
			if (Property->HasAnyPropertyFlags(CPF_RepNotify))
			{
				UFunction * OnRepFunc = Class->FindFunctionByName(Property->RepNotifyFunc);
				if( OnRepFunc != NULL && OnRepFunc->NumParms == 0 && OnRepFunc->GetReturnProperty() == NULL )
				{
					// This function is good so just continue
					continue;
				}
				// Invalid function for RepNotify! clear the flag
				Property->RepNotifyFunc = NAME_None;
			}
		}
		if( Property->HasAnyPropertyFlags( CPF_Config ))
		{
			// If we have properties that are set from the config, then the class needs to also have CLASS_Config flags
			Class->ClassFlags |= CLASS_Config;
		}
	}

	// Set class metadata as needed
	if (FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint))
	{
		NewClass->ClassFlags |= CLASS_Interface;
	}

	{
		UBlueprintGeneratedClass* BPGClass = Cast<UBlueprintGeneratedClass>(Class);
		check(BPGClass);

		BPGClass->ComponentTemplates.Empty();
		BPGClass->Timelines.Empty();
		BPGClass->SimpleConstructionScript = NULL;
		BPGClass->InheritableComponentHandler = NULL;

		BPGClass->ComponentTemplates = Blueprint->ComponentTemplates;
		BPGClass->Timelines = Blueprint->Timelines;
		BPGClass->SimpleConstructionScript = Blueprint->SimpleConstructionScript;
		BPGClass->InheritableComponentHandler = Blueprint->InheritableComponentHandler;
	}

	//@TODO: Not sure if doing this again is actually necessary
	// It will be if locals get promoted to class scope during function compilation, but that should ideally happen during Precompile or similar
	Class->Bind();
	Class->StaticLink(true);

	// Create the default object for this class
	FKismetCompilerUtilities::CompileDefaultProperties(Class);

	AActor* ActorCDO = Cast<AActor>(Class->GetDefaultObject());
	if (ActorCDO)
	{
		ensureMsgf(!ActorCDO->bExchangedRoles, TEXT("Your CDO has had ExchangeNetRoles called on it (likely via RerunConstructionScripts) which should never have happened. This will cause issues replicating this actor over the network due to mutated transient data!"));
	}
}

void FKismetCompilerContext::BuildDynamicBindingObjects(UBlueprintGeneratedClass* Class)
{
	Class->DynamicBindingObjects.Empty();

	for (FKismetFunctionContext& FunctionContext : FunctionList)
	{
		for (UEdGraphNode* GraphNode : FunctionContext.SourceGraph->Nodes)
		{
			UK2Node* Node = Cast<UK2Node>(GraphNode);

			if (Node)
			{
				UClass* DynamicBindingClass = Node->GetDynamicBindingClass();

				if (DynamicBindingClass)
				{
					UDynamicBlueprintBinding* DynamicBindingObject = UBlueprintGeneratedClass::GetDynamicBindingObject(Class, DynamicBindingClass);
					if (DynamicBindingObject == NULL)
					{
						DynamicBindingObject = NewObject<UDynamicBlueprintBinding>(Class, DynamicBindingClass);
						Class->DynamicBindingObjects.Add(DynamicBindingObject);
					}
					Node->RegisterDynamicBinding(DynamicBindingObject);
				}
			}
		}
	}
}

/**
 * Helper function to create event node for a given pin on a timeline node.
 * @param TimelineNode	The timeline node to create the node event for
 * @param SourceGraph	The source graph to create the event node in
 * @param FunctionName	The function to use as the custom function for the event node
 * @param PinName		The pin name to redirect output from, into the pin of the node event
 * @param ExecFuncName	The event signature name that the event node implements
 */ 
void FKismetCompilerContext::CreatePinEventNodeForTimelineFunction(UK2Node_Timeline* TimelineNode, UEdGraph* SourceGraph, FName FunctionName, const FString& PinName, FName ExecFuncName)
{
	UEdGraphPin* SourcePin = nullptr;
	if (UK2Node_Timeline* SourceNode = Cast<UK2Node_Timeline>(MessageLog.FindSourceObject(TimelineNode)))
	{
		SourcePin = SourceNode->FindPin(PinName);
	}
	UK2Node_Event* TimelineEventNode = SpawnIntermediateEventNode<UK2Node_Event>(TimelineNode, SourcePin, SourceGraph);
	TimelineEventNode->EventReference.SetExternalMember(FunctionName, UTimelineComponent::StaticClass());
	TimelineEventNode->CustomFunctionName = FunctionName; // Make sure we name this function the thing we are expecting
	TimelineEventNode->bInternalEvent = true;
	TimelineEventNode->AllocateDefaultPins();

	// Move any links from 'update' pin to the 'update event' node
	UEdGraphPin* UpdatePin = TimelineNode->FindPin(PinName);
	check(UpdatePin);

	UEdGraphPin* UpdateOutput = Schema->FindExecutionPin(*TimelineEventNode, EGPD_Output);

	if(UpdatePin != NULL && UpdateOutput != NULL)
	{
		MovePinLinksToIntermediate(*UpdatePin, *UpdateOutput);
	}
}

UK2Node_CallFunction* FKismetCompilerContext::CreateCallTimelineFunction(UK2Node_Timeline* TimelineNode, UEdGraph* SourceGraph, FName FunctionName, UEdGraphPin* TimelineVarPin, UEdGraphPin* TimelineFunctionPin)
{
	// Create 'call play' node
	UK2Node_CallFunction* CallNode = SpawnIntermediateNode<UK2Node_CallFunction>(TimelineNode, SourceGraph);
	CallNode->FunctionReference.SetExternalMember(FunctionName, UTimelineComponent::StaticClass());
	CallNode->AllocateDefaultPins();

	// Wire 'get timeline' to 'self' pin of function call
	UEdGraphPin* CallSelfPin = CallNode->FindPinChecked(Schema->PN_Self);
	TimelineVarPin->MakeLinkTo(CallSelfPin);

	// Move any exec links from 'play' pin to the 'call play' node
	UEdGraphPin* CallExecInput = Schema->FindExecutionPin(*CallNode, EGPD_Input);
	MovePinLinksToIntermediate(*TimelineFunctionPin, *CallExecInput);
	return CallNode;
}

/** Expand timeline nodes into necessary nodes */
void FKismetCompilerContext::ExpandTimelineNodes(UEdGraph* SourceGraph)
{
	// Timeline Pair helper
	struct FTimelinePair
	{
	public:
		FTimelinePair( UK2Node_Timeline* InNode, UTimelineTemplate* InTemplate )
		: Node( InNode )
		, Template( InTemplate )
		{
		}
	public:
		UK2Node_Timeline* const Node;
		UTimelineTemplate* Template;
	};

	TArray<FName> TimelinePlayNodes;
	TArray<FTimelinePair> Timelines;
	// Extract timeline pairings and external play nodes
	for (int32 ChildIndex = 0; ChildIndex < SourceGraph->Nodes.Num(); ++ChildIndex)
	{
		if (UK2Node_Timeline* TimelineNode = Cast<UK2Node_Timeline>( SourceGraph->Nodes[ChildIndex]))
		{
			UTimelineTemplate* Timeline = Blueprint->FindTimelineTemplateByVariableName(TimelineNode->TimelineName);
			if (Timeline != NULL)
			{
				Timelines.Add(FTimelinePair( TimelineNode, Timeline ));
			}
		}
		else if( UK2Node_VariableGet* VarNode = Cast<UK2Node_VariableGet>( SourceGraph->Nodes[ChildIndex] ))
		{
			// Check for Timeline Variable Get Nodes
			UEdGraphPin* const ValuePin = VarNode->GetValuePin();

			if( ValuePin && ValuePin->LinkedTo.Num() > 0  )
			{
				UClass* ValueClass = ValuePin->PinType.PinSubCategoryObject.IsValid() ? Cast<UClass>( ValuePin->PinType.PinSubCategoryObject.Get() ) : nullptr;
				if( ValueClass == UTimelineComponent::StaticClass() )
				{
					const FName PinName( *ValuePin->PinName );
					if( UTimelineTemplate* TimelineTemplate = Blueprint->FindTimelineTemplateByVariableName( PinName ))
					{
						TimelinePlayNodes.Add( PinName );
					}
				}
			}
		}
	}
	// Expand and validate timelines
	for ( auto TimelinePair : Timelines )
	{
		UK2Node_Timeline* const TimelineNode = TimelinePair.Node;
		UTimelineTemplate* Timeline = TimelinePair.Template;

		if (bIsFullCompile)
		{
			const FString TimelineNameString = TimelineNode->TimelineName.ToString();

			UEdGraphPin* PlayPin = TimelineNode->GetPlayPin();
			bool bPlayPinConnected = (PlayPin->LinkedTo.Num() > 0);

			UEdGraphPin* PlayFromStartPin = TimelineNode->GetPlayFromStartPin();
			bool bPlayFromStartPinConnected = (PlayFromStartPin->LinkedTo.Num() > 0);

			UEdGraphPin* StopPin = TimelineNode->GetStopPin();
			bool bStopPinConnected = (StopPin->LinkedTo.Num() > 0);

			UEdGraphPin* ReversePin = TimelineNode->GetReversePin();
			bool bReversePinConnected = (ReversePin->LinkedTo.Num() > 0);

			UEdGraphPin* ReverseFromEndPin = TimelineNode->GetReverseFromEndPin();
			bool bReverseFromEndPinConnected = (ReverseFromEndPin->LinkedTo.Num() > 0);

			UEdGraphPin* SetTimePin = TimelineNode->GetSetNewTimePin();
			bool bSetNewTimePinConnected = (SetTimePin->LinkedTo.Num() > 0);

			UEdGraphPin* UpdatePin = TimelineNode->GetUpdatePin();
			bool bUpdatePinConnected = (UpdatePin->LinkedTo.Num() > 0);

			UEdGraphPin* FinishedPin = TimelineNode->GetFinishedPin();
			bool bFinishedPinConnected = (FinishedPin->LinkedTo.Num() > 0);


			// Set the timeline template as wired/not wired for component pruning later
			const bool bWiredIn = bPlayPinConnected || bPlayFromStartPinConnected || bStopPinConnected || bReversePinConnected || bReverseFromEndPinConnected || bSetNewTimePinConnected;
			const bool bWiredOut = bUpdatePinConnected || bFinishedPinConnected;
			const bool bPlayWired = Timeline->bAutoPlay;
			const bool bReferenced = TimelinePlayNodes.Find(TimelineNode->TimelineName) != INDEX_NONE;

			Timeline->bValidatedAsWired = bWiredIn || bReferenced || (bPlayWired && bWiredOut);

			// Only create nodes for play/stop if they are actually connected - otherwise we get a 'unused node being pruned' warning
			if (bWiredIn)
			{
				// First create 'get var' node to get the timeline object
				UK2Node_VariableGet* GetTimelineNode = SpawnIntermediateNode<UK2Node_VariableGet>(TimelineNode, SourceGraph);
				GetTimelineNode->VariableReference.SetSelfMember(TimelineNode->TimelineName);
				GetTimelineNode->AllocateDefaultPins();

				// Debug data: Associate the timeline node instance with the property that was created earlier
				UProperty* AssociatedTimelineInstanceProperty = TimelineToMemberVariableMap.FindChecked(Timeline);
				if (AssociatedTimelineInstanceProperty != NULL)
				{
					UObject* TrueSourceObject = MessageLog.FindSourceObject(TimelineNode);
					NewClass->GetDebugData().RegisterClassPropertyAssociation(TrueSourceObject, AssociatedTimelineInstanceProperty);
				}

				// Get the variable output pin
				UEdGraphPin* TimelineVarPin = GetTimelineNode->FindPin(TimelineNameString);

				// This might fail if this is the first compile after adding the timeline (property doesn't exist yet) - in that case, manually add the output pin
				if (TimelineVarPin == NULL)
				{
					TimelineVarPin = GetTimelineNode->CreatePin(EGPD_Output, Schema->PC_Object, TEXT(""), UTimelineComponent::StaticClass(), false, false, TimelineNode->TimelineName.ToString());
				}

				if (bPlayPinConnected)
				{
					static FName PlayName(GET_FUNCTION_NAME_CHECKED(UTimelineComponent, Play));
					CreateCallTimelineFunction(TimelineNode, SourceGraph, PlayName, TimelineVarPin, PlayPin);
				}

				if (bPlayFromStartPinConnected)
				{
					static FName PlayFromStartName(GET_FUNCTION_NAME_CHECKED(UTimelineComponent, PlayFromStart));
					CreateCallTimelineFunction(TimelineNode, SourceGraph, PlayFromStartName, TimelineVarPin, PlayFromStartPin);
				}

				if (bStopPinConnected)
				{
					static FName StopName(GET_FUNCTION_NAME_CHECKED(UTimelineComponent, Stop));
					CreateCallTimelineFunction(TimelineNode, SourceGraph, StopName, TimelineVarPin, StopPin);
				}

				if (bReversePinConnected)
				{
					static FName ReverseName(GET_FUNCTION_NAME_CHECKED(UTimelineComponent, Reverse));
					CreateCallTimelineFunction(TimelineNode, SourceGraph, ReverseName, TimelineVarPin, ReversePin);
				}

				if (bReverseFromEndPinConnected)
				{
					static FName ReverseFromEndName(GET_FUNCTION_NAME_CHECKED(UTimelineComponent, ReverseFromEnd));
					CreateCallTimelineFunction(TimelineNode, SourceGraph, ReverseFromEndName, TimelineVarPin, ReverseFromEndPin);
				}

				if (bSetNewTimePinConnected)
				{
					UEdGraphPin* NewTimePin = TimelineNode->GetNewTimePin();

					static FName SetNewTimeName(GET_FUNCTION_NAME_CHECKED(UTimelineComponent, SetNewTime));
					UK2Node_CallFunction* CallNode = CreateCallTimelineFunction(TimelineNode, SourceGraph, SetNewTimeName, TimelineVarPin, SetTimePin);

					if (CallNode && NewTimePin)
					{
						UEdGraphPin* InputPin = CallNode->FindPinChecked(TEXT("NewTime"));
						MovePinLinksToIntermediate(*NewTimePin, *InputPin);
					}
				}
			}
		}

		// Create event to call on each update
		UFunction* EventSigFunc = UTimelineComponent::GetTimelineEventSignature();

		// Create event nodes for any event tracks
		for(int32 EventTrackIdx=0; EventTrackIdx<Timeline->EventTracks.Num(); EventTrackIdx++)
		{
			FName EventTrackName =  Timeline->EventTracks[EventTrackIdx].TrackName;
			CreatePinEventNodeForTimelineFunction(TimelineNode, SourceGraph, Timeline->GetEventTrackFunctionName(EventTrackIdx), EventTrackName.ToString(), EventSigFunc->GetFName());
		}

		// Generate Update Pin Event Node
		CreatePinEventNodeForTimelineFunction(TimelineNode, SourceGraph, Timeline->GetUpdateFunctionName(), TEXT("Update"), EventSigFunc->GetFName());

		// Generate Finished Pin Event Node
		CreatePinEventNodeForTimelineFunction(TimelineNode, SourceGraph, Timeline->GetFinishedFunctionName(), TEXT("Finished"), EventSigFunc->GetFName());
	}
}

FPinConnectionResponse FKismetCompilerContext::MovePinLinksToIntermediate(UEdGraphPin& SourcePin, UEdGraphPin& IntermediatePin)
{
	 UEdGraphSchema_K2 const* K2Schema = GetSchema();
	 FPinConnectionResponse ConnectionResult = K2Schema->MovePinLinks(SourcePin, IntermediatePin, true);

	 CheckConnectionResponse(ConnectionResult, SourcePin.GetOwningNode());
	 MessageLog.NotifyIntermediatePinCreation(&IntermediatePin, &SourcePin);

	 return ConnectionResult;
}

FPinConnectionResponse FKismetCompilerContext::CopyPinLinksToIntermediate(UEdGraphPin& SourcePin, UEdGraphPin& IntermediatePin)
{
	UEdGraphSchema_K2 const* K2Schema = GetSchema();
	FPinConnectionResponse ConnectionResult = K2Schema->CopyPinLinks(SourcePin, IntermediatePin, true);

	CheckConnectionResponse(ConnectionResult, SourcePin.GetOwningNode());
	MessageLog.NotifyIntermediatePinCreation(&IntermediatePin, &SourcePin);

	return ConnectionResult;
}

UK2Node_TemporaryVariable* FKismetCompilerContext::SpawnInternalVariable(UEdGraphNode* SourceNode, FString Category, FString SubCategory, UObject* SubcategoryObject, bool bIsArray)
{
	UK2Node_TemporaryVariable* Result = SpawnIntermediateNode<UK2Node_TemporaryVariable>(SourceNode);

	Result->VariableType = FEdGraphPinType(Category, SubCategory, SubcategoryObject, bIsArray, false);
	Result->AllocateDefaultPins();

	return Result;
}

FName FKismetCompilerContext::GetEventStubFunctionName(UK2Node_Event* SrcEventNode)
{
	FName EventNodeName;

	// If we are overriding a function, we use the exact name for the event node
	if (SrcEventNode->bOverrideFunction)
	{
		EventNodeName = SrcEventNode->EventReference.GetMemberName();
	}
	else
	{
		// If not, create a new name
		if (SrcEventNode->CustomFunctionName != NAME_None)
		{
			EventNodeName = SrcEventNode->CustomFunctionName;
		}
		else
		{
			FString EventNodeString = ClassScopeNetNameMap.MakeValidName<UEdGraphNode>(SrcEventNode);
			EventNodeName = FName(*EventNodeString);
		}
	}

	return EventNodeName;
}

void FKismetCompilerContext::CreateFunctionStubForEvent(UK2Node_Event* SrcEventNode, UObject* OwnerOfTemporaries)
{
	FName EventNodeName = GetEventStubFunctionName(SrcEventNode);

	// Create the stub graph and add it to the list of functions to compile

	auto ExistingGraph = static_cast<UObject*>(FindObjectWithOuter(OwnerOfTemporaries, UEdGraph::StaticClass(), EventNodeName));
	if (ExistingGraph && !ExistingGraph->HasAnyFlags(RF_Transient))
	{
		MessageLog.Error(
			*FString::Printf(*LOCTEXT("CannotCreateStubForEvent_Error", "Graph named '%s' already exists in '%s'. Another one cannot be generated from @@").ToString()
			, *EventNodeName.ToString()
			, *GetNameSafe(OwnerOfTemporaries))
			, SrcEventNode);
		return;
	}
	UEdGraph* ChildStubGraph = NewObject<UEdGraph>(OwnerOfTemporaries, EventNodeName);
	Blueprint->EventGraphs.Add(ChildStubGraph);
	ChildStubGraph->Schema = UEdGraphSchema_K2::StaticClass();
	ChildStubGraph->SetFlags(RF_Transient);
	MessageLog.NotifyIntermediateObjectCreation(ChildStubGraph, SrcEventNode);

	FKismetFunctionContext& StubContext = *new (FunctionList) FKismetFunctionContext(MessageLog, Schema, NewClass, Blueprint, CompileOptions.DoesRequireCppCodeGeneration(), false);
	StubContext.SourceGraph = ChildStubGraph;

	// A stub graph has no visual representation and is thus not suited to be debugged via the debugger
	StubContext.bCreateDebugData = false;

	StubContext.SourceEventFromStubGraph = SrcEventNode;

	if (SrcEventNode->bOverrideFunction || SrcEventNode->bInternalEvent)
	{
		StubContext.MarkAsInternalOrCppUseOnly();
	}

	uint32 FunctionFlags = SrcEventNode->FunctionFlags;
	if(SrcEventNode->bOverrideFunction && Blueprint->ParentClass != nullptr)
	{
		const UFunction* ParentFunction = Blueprint->ParentClass->FindFunctionByName(SrcEventNode->GetFunctionName());
		if(ParentFunction != nullptr)
		{
			FunctionFlags |= ParentFunction->FunctionFlags & FUNC_NetFuncFlags;
		}
	}

	if ((FunctionFlags & FUNC_Net) > 0)
	{
		StubContext.MarkAsNetFunction(FunctionFlags);
	}

	// Create an entry point
	UK2Node_FunctionEntry* EntryNode = SpawnIntermediateNode<UK2Node_FunctionEntry>(SrcEventNode, ChildStubGraph);
	EntryNode->NodePosX = -200;
	EntryNode->SignatureClass = SrcEventNode->EventReference.GetMemberParentClass(SrcEventNode->GetBlueprintClassFromNode());
	EntryNode->SignatureName = SrcEventNode->EventReference.GetMemberName();
	EntryNode->CustomGeneratedFunctionName = EventNodeName;

	if (!SrcEventNode->bOverrideFunction && SrcEventNode->IsUsedByAuthorityOnlyDelegate())
	{
		EntryNode->AddExtraFlags(FUNC_BlueprintAuthorityOnly);
	}

	// If this is a customizable event, make sure to copy over the user defined pins
	if (UK2Node_CustomEvent const* SrcCustomEventNode = Cast<UK2Node_CustomEvent const>(SrcEventNode))
	{
		EntryNode->UserDefinedPins = SrcCustomEventNode->UserDefinedPins;
		// CustomEvents may inherit net flags (so let's use their GetNetFlags() incase this is an override)
		StubContext.MarkAsNetFunction(SrcCustomEventNode->GetNetFlags());
		// Synchronize the entry node call in editor value with the entry point
		EntryNode->MetaData.bCallInEditor = SrcCustomEventNode->bCallInEditor;
	}
	EntryNode->AllocateDefaultPins();

	// Confirm that the event node matches the latest function signature, which the newly created EntryNode should have
	if( !SrcEventNode->IsFunctionEntryCompatible(EntryNode) )
	{
		// There is no match, so the function parameters must have changed.  Throw an error, and force them to refresh
		MessageLog.Error(*FString::Printf(*LOCTEXT("EventNodeOutOfDate_Error", "Event node @@ is out-of-date.  Please refresh it.").ToString()), SrcEventNode);
		return;
	}

	// Copy each event parameter to the assignment node, if there are any inputs
	UK2Node* AssignmentNode = NULL;
	for (int32 PinIndex = 0; PinIndex < EntryNode->Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* SourcePin = EntryNode->Pins[PinIndex];
		if (!Schema->IsMetaPin(*SourcePin) && (SourcePin->Direction == EGPD_Output))
		{
			if (AssignmentNode == NULL)
			{
				// Create a variable write node to store the parameters into the ubergraph frame storage
				if (UsePersistentUberGraphFrame())
				{
					AssignmentNode = SpawnIntermediateNode<UK2Node_SetVariableOnPersistentFrame>(SrcEventNode, ChildStubGraph);
				}
				else
				{
					auto VariableSetNode = SpawnIntermediateNode<UK2Node_VariableSet>(SrcEventNode, ChildStubGraph);
					VariableSetNode->VariableReference.SetSelfMember(NAME_None);
					AssignmentNode = VariableSetNode;
				}
				AssignmentNode->AllocateDefaultPins();
			}

			// Determine what the member variable name is for this pin
			UEdGraphPin* UGSourcePin = SrcEventNode->FindPin(SourcePin->PinName);
			const FString MemberVariableName = ClassScopeNetNameMap.MakeValidName(UGSourcePin);

			UEdGraphPin* DestPin = AssignmentNode->CreatePin(EGPD_Input, SourcePin->PinType, MemberVariableName);
			MessageLog.NotifyIntermediatePinCreation(DestPin, SourcePin);
			DestPin->MakeLinkTo(SourcePin);
		}
	}

	if (AssignmentNode == NULL)
	{
		// The event took no parameters, store it as a direct-access call
		StubContext.bIsSimpleStubGraphWithNoParams = true;
	}

	// Create a call into the ubergraph
	UK2Node_CallFunction* CallIntoUbergraph = SpawnIntermediateNode<UK2Node_CallFunction>(SrcEventNode, ChildStubGraph);
	CallIntoUbergraph->NodePosX = 300;

	// Use the ExecuteUbergraph base function to generate the pins...
	CallIntoUbergraph->FunctionReference.SetExternalMember(Schema->FN_ExecuteUbergraphBase, UObject::StaticClass());
	CallIntoUbergraph->AllocateDefaultPins();
	
	// ...then swap to the generated version for this level
	CallIntoUbergraph->FunctionReference.SetSelfMember(GetUbergraphCallName());
	UEdGraphPin* CallIntoUbergraphSelf = Schema->FindSelfPin(*CallIntoUbergraph, EGPD_Input);
	CallIntoUbergraphSelf->PinType.PinSubCategory = Schema->PSC_Self;
	CallIntoUbergraphSelf->PinType.PinSubCategoryObject = *Blueprint->SkeletonGeneratedClass;

	UEdGraphPin* EntryPointPin = CallIntoUbergraph->FindPin(Schema->PN_EntryPoint);
	if (EntryPointPin)
	{
		EntryPointPin->DefaultValue = TEXT("0");
	}

	// Schedule a patchup on the event entry address
	CallsIntoUbergraph.Add(CallIntoUbergraph, SrcEventNode);

	// Wire up the node execution wires
	UEdGraphPin* ExecEntryOut = Schema->FindExecutionPin(*EntryNode, EGPD_Output);
	UEdGraphPin* ExecCallIn = Schema->FindExecutionPin(*CallIntoUbergraph, EGPD_Input);

	if (AssignmentNode != NULL)
	{
		UEdGraphPin* ExecVariablesIn = Schema->FindExecutionPin(*AssignmentNode, EGPD_Input);
		UEdGraphPin* ExecVariablesOut = Schema->FindExecutionPin(*AssignmentNode, EGPD_Output);

		ExecEntryOut->MakeLinkTo(ExecVariablesIn);
		ExecVariablesOut->MakeLinkTo(ExecCallIn);
	}
	else
	{
		ExecEntryOut->MakeLinkTo(ExecCallIn);
	}
}

void FKismetCompilerContext::MergeUbergraphPagesIn(UEdGraph* Ubergraph)
{
	for (TArray<UEdGraph*>::TIterator It(Blueprint->UbergraphPages); It; ++It)
	{
		UEdGraph* SourceGraph = *It;
		const bool bCreateTunnelBoundries = CompileOptions.IsInstrumentationActive();

		if (CompileOptions.bSaveIntermediateProducts)
		{
			TArray<UEdGraphNode*> ClonedNodeList;
			FEdGraphUtilities::CloneAndMergeGraphIn(Ubergraph, SourceGraph, MessageLog, /*bRequireSchemaMatch=*/ true, /*bIsCompiling*/ true, bCreateTunnelBoundries/*bCreateTunnelBoundries*/, &ClonedNodeList);

			// Create a comment block around the ubergrapgh contents before anything else got started
			int32 OffsetX = 0;
			int32 OffsetY = 0;
			CreateCommentBlockAroundNodes(ClonedNodeList, SourceGraph, Ubergraph, SourceGraph->GetName(), FLinearColor(1.0f, 0.7f, 0.7f), /*out*/ OffsetX, /*out*/ OffsetY);
			
			// Reposition the nodes, so nothing ever overlaps
			for (TArray<UEdGraphNode*>::TIterator NodeIt(ClonedNodeList); NodeIt; ++NodeIt)
			{
				UEdGraphNode* ClonedNode = *NodeIt;

				ClonedNode->NodePosX += OffsetX;
				ClonedNode->NodePosY += OffsetY;
			}
		}
		else
		{
			FEdGraphUtilities::CloneAndMergeGraphIn(Ubergraph, SourceGraph, MessageLog, /*bRequireSchemaMatch=*/ true, /*bIsCompiling*/ true, bCreateTunnelBoundries/*bCreateTunnelBoundries*/);
		}
	}
}

// Expands out nodes that need it
void FKismetCompilerContext::ExpansionStep(UEdGraph* Graph, bool bAllowUbergraphExpansions)
{
	// Node expansion may affect the signature of a static function
	if (bIsFullCompile)
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_Expansion);

		// Collapse any remaining tunnels or macros
		ExpandTunnelsAndMacros(Graph);

		for (int32 NodeIndex = 0; NodeIndex < Graph->Nodes.Num(); ++NodeIndex)
		{
			UK2Node* Node = Cast<UK2Node>(Graph->Nodes[NodeIndex]);
			if (Node)
			{
				Node->ExpandNode(*this, Graph);
			}
		}
	}

	if (bAllowUbergraphExpansions)
	{
		// Expand timeline nodes, in skeleton classes only the events will be generated
		ExpandTimelineNodes(Graph);
	}
}

void FKismetCompilerContext::VerifyValidOverrideEvent(const UEdGraph* Graph)
{
	check(NULL != Graph);
	check(NULL != Blueprint);

	TArray<const UK2Node_Event*> EntryPoints;
	Graph->GetNodesOfClass(EntryPoints);

	for (TFieldIterator<UFunction> FunctionIt(Blueprint->ParentClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		const UFunction* Function = *FunctionIt;
		if(!UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function))
		{
			const UClass* FuncClass = CastChecked<UClass>(Function->GetOuter());
			const FName FuncName = Function->GetFName();
			for(int32 EntryPointsIdx = 0; EntryPointsIdx < EntryPoints.Num(); EntryPointsIdx++)
			{
				const UK2Node_Event* EventNode = EntryPoints[EntryPointsIdx];
				if( EventNode && EventNode->bOverrideFunction &&
					(EventNode->EventReference.GetMemberParentClass(EventNode->GetBlueprintClassFromNode()) == FuncClass) &&
					(EventNode->EventReference.GetMemberName() == FuncName))
				{
					if (EventNode->IsDeprecated())
					{
						MessageLog.Warning(*EventNode->GetDeprecationMessage(), EventNode);
					}
					else if(!Function->HasAllFunctionFlags(FUNC_Const))	// ...allow legacy event nodes that override methods declared as 'const' to pass.
					{
						MessageLog.Error(TEXT("The function in node @@ cannot be overridden and/or placed as event"), EventNode);
					}
				}
			}
		}
	}
}

void FKismetCompilerContext::VerifyValidOverrideFunction(const UEdGraph* Graph)
{
	check(NULL != Graph);
	check(NULL != Blueprint);

	TArray<const UK2Node_FunctionEntry*> EntryPoints;
	Graph->GetNodesOfClass(EntryPoints);

	for(int32 EntryPointsIdx = 0; EntryPointsIdx < EntryPoints.Num(); EntryPointsIdx++)
	{
		const UK2Node_FunctionEntry* EventNode = EntryPoints[EntryPointsIdx];
		check(NULL != EventNode);

		const UClass* FuncClass = *EventNode->SignatureClass;
		if( FuncClass )
		{
			const UFunction* Function = FuncClass->FindFunctionByName(EventNode->SignatureName);
			if( Function )
			{
				const bool bCanBeOverridden = Function->HasAllFunctionFlags(FUNC_BlueprintEvent);
				if(!bCanBeOverridden)
				{
					MessageLog.Error(TEXT("The function in node @@ cannot be overridden"), EventNode);
				}
			}
		}
		else
		{
			// check if the function name is unique
			for (TFieldIterator<UFunction> FunctionIt(Blueprint->ParentClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
			{
				const UFunction* Function = *FunctionIt;
				if( Function && (Function->GetFName() == EventNode->SignatureName) )
				{
					MessageLog.Error(TEXT("The function name in node @@ is already used"), EventNode);
				}
			}
		}
	}
}


// Merges pages and creates function stubs, etc... from the ubergraph entry points
void FKismetCompilerContext::CreateAndProcessUbergraph()
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_ProcessUbergraph);

	ConsolidatedEventGraph = NewObject<UEdGraph>(Blueprint, GetUbergraphCallName());
	ConsolidatedEventGraph->Schema = UEdGraphSchema_K2::StaticClass();
	ConsolidatedEventGraph->SetFlags(RF_Transient);

	// Merge all of the top-level pages
	MergeUbergraphPagesIn(ConsolidatedEventGraph);

	// Loop over implemented interfaces, and add dummy event entry points for events that aren't explicitly handled by the user
	TArray<UK2Node_Event*> EntryPoints;
	ConsolidatedEventGraph->GetNodesOfClass(EntryPoints);

	for (int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); i++)
	{
		const FBPInterfaceDescription& InterfaceDesc = Blueprint->ImplementedInterfaces[i];
		for (TFieldIterator<UFunction> FunctionIt(InterfaceDesc.Interface, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
		{
			const UFunction* Function = *FunctionIt;
			const FName FunctionName = Function->GetFName();

			const bool bCanImplementAsEvent = UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function);
			bool bExistsAsGraph = false;

			// Any function that can be implemented as an event needs to check to see if there is already an interface function graph
			// If there is, we want to warn the user that this is unexpected but proceed to successfully compile the Blueprint
			if (bCanImplementAsEvent)
			{
				for (UEdGraph* InterfaceGraph : InterfaceDesc.Graphs)
				{
					if (InterfaceGraph->GetFName() == Function->GetFName())
					{
						bExistsAsGraph = true;

						// Having an event override implemented as a function won't cause issues but is something the user should be aware of.
						MessageLog.Warning(TEXT("Interface '@@' is already implemented as a function graph but is expected as an event. Remove the function graph and reimplement as an event."), InterfaceGraph);
					}
				}
			}

			// If this is an event, check the merged ubergraph to make sure that it has an event handler, and if not, add one
			if (bCanImplementAsEvent && UEdGraphSchema_K2::CanKismetOverrideFunction(Function) && !bExistsAsGraph)
			{
				bool bFoundEntry = false;
				// Search the cached entry points to see if we have a match
				for (int32 EntryIndex = 0; EntryIndex < EntryPoints.Num(); ++EntryIndex)
				{
					const UK2Node_Event* EventNode = EntryPoints[EntryIndex];
					if( EventNode && (EventNode->EventReference.GetMemberName() == FunctionName) )
					{
						bFoundEntry = true;
						break;
					}
				}

				if (!bFoundEntry)
				{
					// Create an entry node stub, so that we have a entry point for interfaces to call to
					UK2Node_Event* EventNode = SpawnIntermediateEventNode<UK2Node_Event>(nullptr, nullptr, ConsolidatedEventGraph);
					EventNode->EventReference.SetExternalMember(FunctionName, InterfaceDesc.Interface);
					EventNode->bOverrideFunction = true;
					EventNode->AllocateDefaultPins();
				}
			}
		}
	}

	// We need to stop the old EventGraphs from having the Blueprint as an outer, it impacts renaming.
	if(!Blueprint->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad))
	{
		for(UEdGraph* OldEventGraph : Blueprint->EventGraphs)
		{
			if (OldEventGraph)
			{
				OldEventGraph->Rename(NULL, GetTransientPackage(), Blueprint->bIsRegeneratingOnLoad ? REN_ForceNoResetLoaders : 0);
			}
		}
	}
	Blueprint->EventGraphs.Empty();

	if (ConsolidatedEventGraph->Nodes.Num())
	{
		// Add a dummy entry point to the uber graph, to get the function signature correct
		{
			UK2Node_FunctionEntry* EntryNode = SpawnIntermediateNode<UK2Node_FunctionEntry>(NULL, ConsolidatedEventGraph);
			EntryNode->SignatureClass = UObject::StaticClass();
			EntryNode->SignatureName = Schema->FN_ExecuteUbergraphBase;
			EntryNode->CustomGeneratedFunctionName = ConsolidatedEventGraph->GetFName();
			EntryNode->AllocateDefaultPins();
		}

		// Expand out nodes that need it
		ExpansionStep(ConsolidatedEventGraph, true);

		// If a function in the graph cannot be overridden/placed as event make sure that it is not.
		VerifyValidOverrideEvent(ConsolidatedEventGraph);

		// Do some cursory validation (pin types match, inputs to outputs, pins never point to their parent node, etc...)
		{
			UbergraphContext = new (FunctionList)FKismetFunctionContext(MessageLog, Schema, NewClass, Blueprint, CompileOptions.DoesRequireCppCodeGeneration(), CompileOptions.IsInstrumentationActive());
			UbergraphContext->SourceGraph = ConsolidatedEventGraph;
			UbergraphContext->MarkAsEventGraph();
			UbergraphContext->MarkAsInternalOrCppUseOnly();
			UbergraphContext->SetExternalNetNameMap(&ClassScopeNetNameMap);

			// Validate all the nodes in the graph
			for (int32 ChildIndex = 0; ChildIndex < ConsolidatedEventGraph->Nodes.Num(); ++ChildIndex)
			{
				const UEdGraphNode* Node = ConsolidatedEventGraph->Nodes[ChildIndex];
				const int32 SavedErrorCount = MessageLog.NumErrors;
				UK2Node_Event* SrcEventNode = Cast<UK2Node_Event>(ConsolidatedEventGraph->Nodes[ChildIndex]);
				if (bIsFullCompile || SrcEventNode)
				{
					ValidateNode(Node);
				}

				// If the node didn't generate any errors then generate function stubs for event entry nodes etc.
				if ((SavedErrorCount == MessageLog.NumErrors) && SrcEventNode)
				{
					CreateFunctionStubForEvent(SrcEventNode, Blueprint);
				}
			}
		}
	}
}

void FKismetCompilerContext::AutoAssignNodePosition(UEdGraphNode* Node)
{
	int32 Width = FMath::Max<int32>(Node->NodeWidth, AverageNodeWidth);
	int32 Height = FMath::Max<int32>(Node->NodeHeight, AverageNodeHeight);

	Node->NodePosX = MacroSpawnX;
	Node->NodePosY = MacroSpawnY;

	MacroSpawnX += Width + HorizontalNodePadding;
	MacroRowMaxHeight = FMath::Max<int32>(MacroRowMaxHeight, Height);

	// Advance the spawn position
	if (MacroSpawnX >= MaximumSpawnX)
	{
		MacroSpawnX = MinimumSpawnX;
		MacroSpawnY += MacroRowMaxHeight + VerticalSectionPadding;

		MacroRowMaxHeight = 0;
	}
}

void FKismetCompilerContext::AdvanceMacroPlacement(int32 Width, int32 Height)
{
	MacroSpawnX += Width + HorizontalSectionPadding;
	MacroRowMaxHeight = FMath::Max<int32>(MacroRowMaxHeight, Height);

	if (MacroSpawnX > MaximumSpawnX)
	{
		MacroSpawnX = MinimumSpawnX;
		MacroSpawnY += MacroRowMaxHeight + VerticalSectionPadding;

		MacroRowMaxHeight = 0;
	}
}

void FKismetCompilerContext::CreateCommentBlockAroundNodes(const TArray<UEdGraphNode*>& Nodes, UObject* SourceObject, UEdGraph* TargetGraph, FString CommentText, FLinearColor CommentColor, int32& Out_OffsetX, int32& Out_OffsetY)
{
	if (!Nodes.Num())
	{
		return;
	}

	FIntRect Bounds = FEdGraphUtilities::CalculateApproximateNodeBoundaries(Nodes);

	// Figure out how to offset the expanded nodes to fit into our tile
	Out_OffsetX = MacroSpawnX - Bounds.Min.X;
	Out_OffsetY = MacroSpawnY - Bounds.Min.Y;

	// Create a comment node around the expanded nodes, using the name
	const int32 Padding = 60;

	UEdGraphNode_Comment* CommentNode = SpawnIntermediateNode<UEdGraphNode_Comment>(Cast<UEdGraphNode>(SourceObject), TargetGraph);
	CommentNode->CommentColor = CommentColor;
	CommentNode->NodePosX = MacroSpawnX - Padding;
	CommentNode->NodePosY = MacroSpawnY - Padding;
	CommentNode->NodeWidth = Bounds.Width() + 2*Padding;
	CommentNode->NodeHeight = Bounds.Height() + 2*Padding;
	CommentNode->NodeComment = CommentText;
	CommentNode->AllocateDefaultPins();

	// Advance the macro expansion tile to the next open slot
	AdvanceMacroPlacement(Bounds.Width(), Bounds.Height());
}

void FKismetCompilerContext::ExpandTunnelsAndMacros(UEdGraph* SourceGraph)
{
	// Determine if we are regenerating a blueprint on load
	const bool bIsLoading = Blueprint ? Blueprint->bIsRegeneratingOnLoad : false;

	// Collapse any remaining tunnels
	for (TArray<UEdGraphNode*>::TIterator NodeIt(SourceGraph->Nodes); NodeIt; ++NodeIt)
	{
		UEdGraphNode* CurrentNode = *NodeIt;
		if (!CurrentNode || !CurrentNode->ShouldMergeChildGraphs())
		{
			continue;
		}

		if (UK2Node_MacroInstance* MacroInstanceNode = Cast<UK2Node_MacroInstance>(*NodeIt))
		{
			UEdGraph* MacroGraph = MacroInstanceNode->GetMacroGraph();
			// Verify that this macro can actually be expanded
			if( MacroGraph == NULL )
			{
				MessageLog.Error(TEXT("Macro node @@ is pointing at an invalid macro graph."), MacroInstanceNode);
				continue;
			}

			UBlueprint* MacroBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(MacroGraph);
			// unfortunately, you may be expanding a macro that has yet to be
			// regenerated on load (thanks cyclic dependencies!), and in certain
			// cases the nodes found within the macro may be out of date 
			// (function signatures, etc.), so let's force a reconstruct of the 
			// nodes we inject from the macro (just in case)
			const bool bForceRegenNodes = bIsLoading && MacroBlueprint && (MacroBlueprint != Blueprint) && !MacroBlueprint->bHasBeenRegenerated;

			// Clone the macro graph, then move all of its children, keeping a list of nodes from the macro
			UEdGraph* ClonedGraph = FEdGraphUtilities::CloneGraph(MacroGraph, NULL, &MessageLog, true);

			if (CompileOptions.IsInstrumentationActive())
			{
				// Create tunnel Boundary signals
				UK2Node_TunnelBoundary::CreateBoundaryNodesForTunnelInstance(MacroInstanceNode, ClonedGraph, MessageLog);
			}

			for (int32 I = 0; I < ClonedGraph->Nodes.Num(); ++I)
			{
				MacroGeneratedNodes.Add(ClonedGraph->Nodes[I], CurrentNode);
			}

			TArray<UEdGraphNode*> MacroNodes(ClonedGraph->Nodes);

			// resolve any wildcard pins in the nodes cloned from the macro
			if (!MacroInstanceNode->ResolvedWildcardType.PinCategory.IsEmpty())
			{
				for (auto ClonedNodeIt = ClonedGraph->Nodes.CreateConstIterator(); ClonedNodeIt; ++ClonedNodeIt)
				{
					UEdGraphNode* const ClonedNode = *ClonedNodeIt;
					if (ClonedNode)
					{
						for (auto ClonedPinIt = ClonedNode->Pins.CreateConstIterator(); ClonedPinIt; ++ClonedPinIt)
						{
							UEdGraphPin* const ClonedPin = *ClonedPinIt;
							if ( ClonedPin && (ClonedPin->PinType.PinCategory == Schema->PC_Wildcard) )
							{
								// copy only type info, so array or ref status is preserved
								ClonedPin->PinType.PinCategory = MacroInstanceNode->ResolvedWildcardType.PinCategory;
								ClonedPin->PinType.PinSubCategory = MacroInstanceNode->ResolvedWildcardType.PinSubCategory;
								ClonedPin->PinType.PinSubCategoryObject = MacroInstanceNode->ResolvedWildcardType.PinSubCategoryObject;
							}
							}
						}
					}
				}

			// Handle any nodes that need to inherit their macro instance's NodeGUID
			for( auto ClonedNode : MacroNodes)
			{
				UK2Node_TemporaryVariable* TempVarNode = Cast<UK2Node_TemporaryVariable>(ClonedNode);
				if(TempVarNode && TempVarNode->bIsPersistent)
				{
					TempVarNode->NodeGuid = MacroInstanceNode->NodeGuid;
				}
			}

			// We iterate the array in reverse so we can both remove the subpins safely after we've read them and
			// so we have split nested structs we combine them back together in the right order
			for (int32 PinIndex = MacroInstanceNode->Pins.Num() - 1; PinIndex >= 0; --PinIndex)
			{
				UEdGraphPin* Pin = MacroInstanceNode->Pins[PinIndex];
				if (Pin)
				{
					// Since we don't support array literals, drop a make array node on any unconnected array pins, which will allow macro expansion to succeed even if disconnected
					if (Pin->PinType.bIsArray
					&& (Pin->Direction == EGPD_Input)
					&& (Pin->LinkedTo.Num() == 0))
					{
						UK2Node_MakeArray* MakeArrayNode = SpawnIntermediateNode<UK2Node_MakeArray>(MacroInstanceNode, SourceGraph);
						MakeArrayNode->NumInputs = 0; // the generated array should be empty
						MakeArrayNode->AllocateDefaultPins();
						UEdGraphPin* MakeArrayOut = MakeArrayNode->GetOutputPin();
						check(MakeArrayOut);
						MakeArrayOut->MakeLinkTo(Pin);
						MakeArrayNode->PinConnectionListChanged(MakeArrayOut);
					}
					else if (Pin->LinkedTo.Num() == 0 &&
							Pin->Direction == EGPD_Input &&
							Pin->DefaultValue != FString() &&
							Pin->PinType.PinCategory == Schema->PC_Byte &&
							Pin->PinType.PinSubCategoryObject.IsValid() &&
							Pin->PinType.PinSubCategoryObject->IsA<UEnum>())
					{
						// Similarly, enums need a 'make enum' node because they decay to byte after instantiation:
						UK2Node_EnumLiteral* EnumLiteralNode = SpawnIntermediateNode<UK2Node_EnumLiteral>(MacroInstanceNode, SourceGraph);
						EnumLiteralNode->Enum = CastChecked<UEnum>(Pin->PinType.PinSubCategoryObject.Get());
						EnumLiteralNode->AllocateDefaultPins();
						EnumLiteralNode->FindPinChecked(Schema->PN_ReturnValue)->MakeLinkTo(Pin);

						UEdGraphPin* InPin = EnumLiteralNode->FindPinChecked(UK2Node_EnumLiteral::GetEnumInputPinName());
						check(InPin);
						InPin->DefaultValue = Pin->DefaultValue;
					}
					// Otherwise we need to handle the pin splitting
					else if (Pin->SubPins.Num() > 0)
					{
						MacroInstanceNode->ExpandSplitPin(this, SourceGraph, Pin);
					}
				}
			}

			ClonedGraph->MoveNodesToAnotherGraph(SourceGraph, IsAsyncLoading() || bIsLoading);
			FEdGraphUtilities::MergeChildrenGraphsIn(SourceGraph, ClonedGraph, /*bRequireSchemaMatch=*/ true);

			// When emitting intermediate products; make an effort to make them readable by preventing overlaps and adding informative comments
			int32 NodeOffsetX = 0;
			int32 NodeOffsetY = 0;
			if (CompileOptions.bSaveIntermediateProducts)
			{
				CreateCommentBlockAroundNodes(
					MacroNodes,
					MacroInstanceNode,
					SourceGraph, 
					FString::Printf(*LOCTEXT("ExpandedMacroComment", "Macro %s").ToString(), *(MacroGraph->GetName())),
					MacroInstanceNode->MetaData.InstanceTitleColor,
					/*out*/ NodeOffsetX,
					/*out*/ NodeOffsetY);
			}

			// Record intermediate object creation nodes, offset the nodes, and handle tunnels
			for (TArray<UEdGraphNode*>::TIterator MacroNodeIt(MacroNodes); MacroNodeIt; ++MacroNodeIt)
			{
				UEdGraphNode* DuplicatedNode = *MacroNodeIt;
				
				if( DuplicatedNode != NULL )
				{
					if (bForceRegenNodes)
					{
						DuplicatedNode->ReconstructNode();
					}

					// Record the source node mapping for the intermediate node first, as it's going to be overwritten through the MessageLog below
					UEdGraphNode* MacroSourceNode = Cast<UEdGraphNode>(MessageLog.FindSourceObject(DuplicatedNode));
					if (MacroSourceNode)
					{
						MessageLog.FinalNodeBackToMacroSourceMap.NotifyIntermediateObjectCreation(DuplicatedNode, MacroSourceNode);

						// Also record mappings from final macro source node to intermediate macro instance nodes (there may be more than one)
						UEdGraphNode* MacroInstanceSourceNode = Cast<UEdGraphNode>(MessageLog.FinalNodeBackToMacroSourceMap.FindSourceObject(MacroInstanceNode));
						if (MacroInstanceSourceNode && MacroInstanceSourceNode != MacroInstanceNode)
						{
							MessageLog.MacroSourceToMacroInstanceNodeMap.Add(MacroSourceNode, MacroInstanceSourceNode);
						}
					}

					MessageLog.NotifyIntermediateObjectCreation(DuplicatedNode, MacroInstanceNode);

					DuplicatedNode->NodePosY += NodeOffsetY;
					DuplicatedNode->NodePosX += NodeOffsetX;

					if (const UK2Node_Composite* const CompositeNode = Cast<const UK2Node_Composite>(DuplicatedNode))
					{
						// Composite nodes can be present in the MacroNodes if users have collapsed nodes in the macro.
						// No need to do anything for those:
						continue;
					}
					
					if (UK2Node_Tunnel* TunnelNode = Cast<UK2Node_Tunnel>(DuplicatedNode))
					{
						// Tunnel nodes should be connected to the MacroInstance they have been instantiated by.. Note 
						// that if there are tunnel nodes internal to the macro instance they will be incorrectly 
						// connected to the MacroInstance.
						if (TunnelNode->bCanHaveInputs)
						{
							check(!TunnelNode->bCanHaveOutputs);
							// If this check fails it indicates that we've failed to identify all uses of tunnel nodes and 
							// are erroneously connecting tunnels to the macro instance when they should be left untouched..
							check(!TunnelNode->InputSinkNode);
							TunnelNode->InputSinkNode = MacroInstanceNode;
							MacroInstanceNode->OutputSourceNode = TunnelNode;
						}
						else if (TunnelNode->bCanHaveOutputs)
						{
							check(!TunnelNode->OutputSourceNode);
							TunnelNode->OutputSourceNode = MacroInstanceNode;
							MacroInstanceNode->InputSinkNode = TunnelNode;
						}
					}
				}
			}
		}
		else if (UK2Node_Tunnel* TunnelNode = Cast<UK2Node_Tunnel>(*NodeIt))
		{
			UEdGraphNode* InputSink = TunnelNode->GetInputSink();
			for (UEdGraphPin const* TunnelPin : TunnelNode->Pins)
			{
				if ((TunnelPin->Direction != EGPD_Input) || (TunnelPin->PinType.PinCategory != Schema->PC_Exec))
				{
					continue;
				}
				check(InputSink != NULL);

				UEdGraphPin* SinkPin = InputSink->FindPin(TunnelPin->PinName);
				if (SinkPin == NULL)
				{
					continue;
				}
				check(SinkPin->Direction == EGPD_Output);

				for (UEdGraphPin* TunnelLinkedPin : TunnelPin->LinkedTo)
				{
					MessageLog.NotifyIntermediatePinCreation(TunnelLinkedPin, SinkPin);
				}
			}

			bool bSuccess = Schema->CollapseGatewayNode(TunnelNode, InputSink, TunnelNode->GetOutputSource(), this);
			if (!bSuccess)
			{
				MessageLog.Error(*LOCTEXT("CollapseTunnel_Error", "Failed to collapse tunnel @@").ToString(), TunnelNode);
			}
		}
	}
}

void FKismetCompilerContext::ResetErrorFlags(UEdGraph* Graph) const
{
	if (Graph != NULL)
	{
		for (int32 NodeIndex = 0; NodeIndex < Graph->Nodes.Num(); ++NodeIndex)
		{
			if (UEdGraphNode* GraphNode = Graph->Nodes[NodeIndex])
			{
				GraphNode->ClearCompilerMessage();
			}
		}
	}
}

/**
 * Merges macros/subgraphs into the graph and validates it, creating a function list entry if it's reasonable.
 */
void FKismetCompilerContext::ProcessOneFunctionGraph(UEdGraph* SourceGraph, bool bInternalFunction)
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_ProcessFunctionGraph);

	// Clone the source graph so we can modify it as needed; merging in the child graphs
	UEdGraph* FunctionGraph = FEdGraphUtilities::CloneGraph(SourceGraph, Blueprint, &MessageLog, true); 
	FEdGraphUtilities::MergeChildrenGraphsIn(FunctionGraph, FunctionGraph, /*bRequireSchemaMatch=*/ true);

	ExpansionStep(FunctionGraph, false);

	// If a function in the graph cannot be overridden/placed as event make sure that it is not.
	VerifyValidOverrideFunction(FunctionGraph);

	FKismetFunctionContext& Context = *new (FunctionList)FKismetFunctionContext(MessageLog, Schema, NewClass, Blueprint, CompileOptions.DoesRequireCppCodeGeneration(), CompileOptions.IsInstrumentationActive());
	Context.SourceGraph = FunctionGraph;

	if(FBlueprintEditorUtils::IsDelegateSignatureGraph(SourceGraph))
	{
		Context.SetDelegateSignatureName(SourceGraph->GetFName());
	}

	// If this is an interface blueprint, mark the function contexts as stubs
	if (FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint))
	{
		Context.MarkAsInterfaceStub();
	}

	bool bEnforceConstCorrectness = true;
	if (FBlueprintEditorUtils::IsBlueprintConst(Blueprint) || Schema->IsConstFunctionGraph(Context.SourceGraph, &bEnforceConstCorrectness))
	{
		Context.MarkAsConstFunction(bEnforceConstCorrectness);
	}

	if ( bInternalFunction )
	{
		Context.MarkAsInternalOrCppUseOnly();
	}
}

void FKismetCompilerContext::ValidateFunctionGraphNames()
{
	TSharedPtr<FKismetNameValidator> ParentBPNameValidator;
	if( Blueprint->ParentClass != NULL )
	{
		UBlueprint* ParentBP = Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy);
		if( ParentBP != NULL )
		{
			ParentBPNameValidator = MakeShareable(new FKismetNameValidator(ParentBP));
		}
	}

	if(ParentBPNameValidator.IsValid())
	{
		for (int32 FunctionIndex=0; FunctionIndex < Blueprint->FunctionGraphs.Num(); ++FunctionIndex)
		{
			UEdGraph* FunctionGraph = Blueprint->FunctionGraphs[FunctionIndex];
			if(FunctionGraph->GetFName() != Schema->FN_UserConstructionScript)
			{
				if( ParentBPNameValidator->IsValid(FunctionGraph->GetName()) != EValidatorResult::Ok )
				{
					FName NewFunctionName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, FunctionGraph->GetName());
					MessageLog.Warning(*FString::Printf(*LOCTEXT("FunctionGraphConflictWarning", "Found a function graph with a conflicting name (%s) - changed to %s.").ToString(), *FunctionGraph->GetName(), *NewFunctionName.ToString()));
					FBlueprintEditorUtils::RenameGraph(FunctionGraph, NewFunctionName.ToString());
				}
			}
		}
	}
}

// Performs initial validation that the graph is at least well formed enough to be processed further
// Merge separate pages of the ubergraph together into one ubergraph
// Creates a copy of the graph to allow further transformations to occur
void FKismetCompilerContext::CreateFunctionList()
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_CreateFunctionList);

	// Process the ubergraph if one should be present
	if (FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint))
	{
		CreateAndProcessUbergraph();
	}

	if (Blueprint->BlueprintType != BPTYPE_MacroLibrary)
	{
		// Ensure that function graph names are valid and that there are no collisions with a parent class
		//ValidateFunctionGraphNames();

		// Run thru the individual function graphs
		for (int32 i = 0; i < Blueprint->FunctionGraphs.Num(); ++i)
		{
			ProcessOneFunctionGraph(Blueprint->FunctionGraphs[i]);
		}

		for (int32 i = 0; i < Blueprint->DelegateSignatureGraphs.Num(); ++i)
		{
			// change function names to unique

			ProcessOneFunctionGraph(Blueprint->DelegateSignatureGraphs[i]);
		}

		// Run through all the implemented interface member functions
		for (int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); ++i)
		{
			for(int32 j = 0; j < Blueprint->ImplementedInterfaces[i].Graphs.Num(); ++j)
			{
				UEdGraph* SourceGraph = Blueprint->ImplementedInterfaces[i].Graphs[j];
				ProcessOneFunctionGraph(SourceGraph);
			}
		}
	}
}

FKismetFunctionContext* FKismetCompilerContext::CreateFunctionContext()
{
	return new (FunctionList)FKismetFunctionContext(MessageLog, Schema, NewClass, Blueprint, CompileOptions.DoesRequireCppCodeGeneration(), CompileOptions.IsInstrumentationActive());
}

/** Compile a blueprint into a class and a set of functions */
void FKismetCompilerContext::Compile()
{
	// Interfaces only need function signatures, so we only need to perform the first phase of compilation for them
	bIsFullCompile = CompileOptions.DoesRequireBytecodeGeneration() && (Blueprint->BlueprintType != BPTYPE_Interface);

	CallsIntoUbergraph.Empty();
	if (bIsFullCompile)
	{
		Blueprint->IntermediateGeneratedGraphs.Empty();
	}

	// This flag tries to ensure that component instances will use their template name (since that's how old->new instance mapping is done here)
	//@TODO: This approach will break if and when we multithread compiling, should be an inc-dec pair instead
	TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);

	if (Schema == NULL)
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_CreateSchema);
		Schema = CreateSchema();
		PostCreateSchema();
	}

	// Make sure the parent class exists and can be used
	check(Blueprint->ParentClass && Blueprint->ParentClass->GetPropertiesSize());

	const bool bIsSkeletonOnly = (CompileOptions.CompileType == EKismetCompileType::SkeletonOnly);
	UClass* TargetUClass = bIsSkeletonOnly ? Blueprint->SkeletonGeneratedClass : Blueprint->GeneratedClass;

	// >>> Backwards Compatibility:  Make sure this is an actual UBlueprintGeneratedClass / UAnimBlueprintGeneratedClass, as opposed to the old UClass
	EnsureProperGeneratedClass(TargetUClass);
	// <<< End Backwards Compatibility

	UBlueprintGeneratedClass* TargetClass = Cast<UBlueprintGeneratedClass>(TargetUClass);

	// >>> Backwards Compatibility: Make sure that skeleton generated classes have the proper "SKEL_" naming convention
	const FString SkeletonPrefix(TEXT("SKEL_"));
	if( bIsSkeletonOnly && TargetClass && !TargetClass->GetName().StartsWith(SkeletonPrefix) )
	{
		FString NewName = SkeletonPrefix + TargetClass->GetName();
 
		// Ensure we have a free name for this class
		UClass* AnyClassWithGoodName = (UClass*)StaticFindObject(UClass::StaticClass(), Blueprint->GetOutermost(), *NewName, false);
		if( AnyClassWithGoodName )
		{
			// Special Case:  If the CDO of the class has become dissociated from its actual CDO, attempt to find the proper named CDO, and get rid of it.
			if( AnyClassWithGoodName->ClassDefaultObject == TargetClass->ClassDefaultObject )
			{
				AnyClassWithGoodName->ClassDefaultObject = NULL;
				FString DefaultObjectName = FString(DEFAULT_OBJECT_PREFIX) + NewName;
				AnyClassWithGoodName->ClassDefaultObject = (UObject*)StaticFindObject(UObject::StaticClass(), Blueprint->GetOutermost(), *DefaultObjectName, false);
			}
 
			// Get rid of the old class to make room for renaming our class to the final SKEL name
			FKismetCompilerUtilities::ConsignToOblivion(AnyClassWithGoodName, Blueprint->bIsRegeneratingOnLoad);

			// Update the refs to the old SKC
			TMap<UObject*, UObject*> ClassReplacementMap;
			ClassReplacementMap.Add(AnyClassWithGoodName, TargetClass);
			TArray<UEdGraph*> AllGraphs;
			Blueprint->GetAllGraphs(AllGraphs);
			for (int32 i = 0; i < AllGraphs.Num(); ++i)
			{
				FArchiveReplaceObjectRef<UObject> ReplaceInBlueprintAr(AllGraphs[i], ClassReplacementMap, /*bNullPrivateRefs=*/ false, /*bIgnoreOuterRef=*/ false, /*bIgnoreArchetypeRef=*/ false);
			}
		}
 
		ERenameFlags RenameFlags = (REN_DontCreateRedirectors|REN_NonTransactional|(Blueprint->bIsRegeneratingOnLoad ? REN_ForceNoResetLoaders : 0));
		TargetClass->Rename(*NewName, NULL, RenameFlags);
	}
	// <<< End Backwards Compatibility

	// >>> Backwards compatibility:  If SkeletonGeneratedClass == GeneratedClass, we need to make a new generated class the first time we need it
	if( !bIsSkeletonOnly && (Blueprint->SkeletonGeneratedClass == Blueprint->GeneratedClass) )
	{
		Blueprint->GeneratedClass = NULL;
		TargetClass = NULL;
	}
	// <<< End Backwards Compatibility

	if( !TargetClass )
	{
		FName NewSkelClassName, NewGenClassName;
		Blueprint->GetBlueprintClassNames(NewGenClassName, NewSkelClassName);
		SpawnNewClass( bIsSkeletonOnly ? NewSkelClassName.ToString() : NewGenClassName.ToString() );
		check(NewClass);

		TargetClass = NewClass;

		// Fix up the reference in the blueprint to the new class
		if( bIsSkeletonOnly )
		{
			Blueprint->SkeletonGeneratedClass = TargetClass;
		}
		else
		{
			Blueprint->GeneratedClass = TargetClass;
		}
	}

	if (CompileOptions.DoesRequireBytecodeGeneration())
	{
		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);
		for (int32 i = 0; i < AllGraphs.Num(); i++)
		{
			//Reset error flags associated with nodes in each graph
			ResetErrorFlags(AllGraphs[i]);
		}
	}

	// Declare class wants profiling data.
	TargetClass->bHasInstrumentation = CompileOptions.IsInstrumentationActive();

	// Early validation
	if (CompileOptions.CompileType == EKismetCompileType::Full)
	{
		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);
		for(auto GraphIter = AllGraphs.CreateIterator(); GraphIter; ++GraphIter)
		{
			if(UEdGraph* Graph = *GraphIter)
			{
				TArray<UK2Node*> AllNodes;
				Graph->GetNodesOfClass(AllNodes);
				for(auto NodeIter = AllNodes.CreateIterator(); NodeIter; ++NodeIter)
				{
					if(UK2Node* Node = *NodeIter)
					{
						Node->EarlyValidation(MessageLog);
					}
				}
			}
		}
	}

	// Ensure that member variable names are valid and that there are no collisions with a parent class
	// This validation requires CDO object.
	ValidateVariableNames();

	UObject* OldCDO = NULL;
	int32 OldGenLinkerIdx = INDEX_NONE;
	FLinkerLoad* OldLinker = Blueprint->GetLinker();

	if (OldLinker)
	{
		// Cache linker addresses so we can fixup linker for old CDO
		for (int32 i = 0; i < OldLinker->ExportMap.Num(); i++)
		{
			FObjectExport& ThisExport = OldLinker->ExportMap[i];
			if (ThisExport.ObjectFlags & RF_ClassDefaultObject)
			{
				OldGenLinkerIdx = i;
				break;
			}
		}
	}

	for (int32 TimelineIndex = 0; TimelineIndex < Blueprint->Timelines.Num(); )
	{
		if (NULL == Blueprint->Timelines[TimelineIndex])
		{
			Blueprint->Timelines.RemoveAt(TimelineIndex);
			continue;
		}
		++TimelineIndex;
	}


	CleanAndSanitizeClass(TargetClass, OldCDO);

	FKismetCompilerVMBackend Backend_VM(Blueprint, Schema, *this);

	NewClass->ClassGeneratedBy = Blueprint;

	UClass* ParentClass = NewClass->ClassWithin;
	NewClass->SetSuperStruct(ParentClass);
	NewClass->ClassFlags |= (ParentClass->ClassFlags & CLASS_Inherit);
	NewClass->ClassCastFlags |= ParentClass->ClassCastFlags;
	
	if(Blueprint->bGenerateConstClass)
	{
		NewClass->ClassFlags |= CLASS_Const;
	}

	if (CompileOptions.CompileType == EKismetCompileType::Full)
	{
		auto InheritableComponentHandler = Blueprint->GetInheritableComponentHandler(false);
		if (InheritableComponentHandler)
		{
			InheritableComponentHandler->ValidateTemplates();
		}
	}

	// Make sure that this blueprint is up-to-date with regards to its parent functions
	FBlueprintEditorUtils::ConformCallsToParentFunctions(Blueprint);

	// Conform implemented events here, to ensure we generate custom events if necessary after reparenting
	FBlueprintEditorUtils::ConformImplementedEvents(Blueprint);

	// Conform implemented interfaces here, to ensure we generate all functions required by the interface as stubs
	FBlueprintEditorUtils::ConformImplementedInterfaces(Blueprint);

	// Run thru the class defined variables first, get them registered
	CreateClassVariablesFromBlueprint();

	// Add any interfaces that the blueprint implements to the class
	// (has to happen before we validate pin links in CreateFunctionList(), so that we can verify self/interface pins)
	AddInterfacesFromBlueprint(NewClass);

	// Construct a context for each function, doing validation and building the function interface
	CreateFunctionList();

	// Precompile the functions
	// Handle delegates signatures first, because they are needed by other functions
	for (int32 i = 0; i < FunctionList.Num(); ++i)
	{
		if(FunctionList[i].IsDelegateSignature())
		{
			PrecompileFunction(FunctionList[i]);
		}
	}

	for (int32 i = 0; i < FunctionList.Num(); ++i)
	{
		if(!FunctionList[i].IsDelegateSignature())
		{
			PrecompileFunction(FunctionList[i]);
		}
	}

	if (UsePersistentUberGraphFrame() && UbergraphContext)
	{
		//UBER GRAPH PERSISTENT FRAME
		FEdGraphPinType Type(TEXT("struct"), TEXT(""), FPointerToUberGraphFrame::StaticStruct(), false, false);
		auto Property = CreateVariable(UBlueprintGeneratedClass::GetUberGraphFrameName(), Type);
		Property->SetPropertyFlags(CPF_DuplicateTransient | CPF_Transient);
	}

	{ BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_BindAndLinkClass);

		// Relink the class
		NewClass->Bind();
		NewClass->StaticLink(true);
	}

	if (bIsFullCompile && !MessageLog.NumErrors)
	{
		// Generate code for each function (done in a second pass to allow functions to reference each other)
		for (int32 i = 0; i < FunctionList.Num(); ++i)
		{
			if (FunctionList[i].IsValid())
			{
				CompileFunction(FunctionList[i]);
			}
		}

		// Finalize all functions (done last to allow cross-function patchups)
		for (int32 i = 0; i < FunctionList.Num(); ++i)
		{
			if (FunctionList[i].IsValid())
			{
				PostcompileFunction(FunctionList[i]);
			}
		}

		// Create a mapping between compile time generated events and what created them.
		if (TargetClass->bHasInstrumentation)
		{
			FBlueprintDebugData& DebugData = TargetClass->GetDebugData();
			for (auto EventInfo : SourcePinToExpansionEvent)
			{
				DebugData.RegisterPinToEventName(EventInfo.Key, EventInfo.Value->GetFunctionName());
			}
		}

		// Save off intermediate build products if requested
		if (CompileOptions.bSaveIntermediateProducts && !Blueprint->bIsRegeneratingOnLoad)
		{
			// Generate code for each function (done in a second pass to allow functions to reference each other)
			for (int32 i = 0; i < FunctionList.Num(); ++i)
			{
				FKismetFunctionContext& ContextFunction = FunctionList[i];
				if (FunctionList[i].SourceGraph != NULL)
				{
					// Record this graph as an intermediate product
					ContextFunction.SourceGraph->Schema = UEdGraphSchema_K2::StaticClass();
					Blueprint->IntermediateGeneratedGraphs.Add(ContextFunction.SourceGraph);
					ContextFunction.SourceGraph->SetFlags(RF_Transient);
				}
			}
		}

		for (TFieldIterator<UMulticastDelegateProperty> PropertyIt(NewClass); PropertyIt; ++PropertyIt)
		{
			if(const UMulticastDelegateProperty* MCDelegateProp = *PropertyIt)
			{
				if(NULL == MCDelegateProp->SignatureFunction)
				{
					MessageLog.Warning(*FString::Printf(TEXT("No SignatureFunction in MulticastDelegateProperty '%s'"), *MCDelegateProp->GetName()));
				}
			}
		}
	}
	else
	{
		// Still need to set flags on the functions even for a skeleton class
		for (int32 i = 0; i < FunctionList.Num(); ++i)
		{
			FKismetFunctionContext& Function = FunctionList[i];
			if (Function.IsValid())
			{
				BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_PostcompileFunction);
				FinishCompilingFunction(Function);
			}
		}
	}

	// Late validation for Delegates.
	{
		TSet<UEdGraph*> AllGraphs;
		AllGraphs.Add(UbergraphContext ? UbergraphContext->SourceGraph : NULL);
		for(auto FunctionContextIter = FunctionList.CreateIterator(); FunctionContextIter; ++FunctionContextIter)
		{
			AllGraphs.Add((*FunctionContextIter).SourceGraph);
		}
		for(auto GraphIter = AllGraphs.CreateIterator(); GraphIter; ++GraphIter)
		{
			if(UEdGraph* Graph = *GraphIter)
			{
				TArray<UK2Node_CreateDelegate*> AllNodes;
				Graph->GetNodesOfClass(AllNodes);
				for(auto NodeIter = AllNodes.CreateIterator(); NodeIter; ++NodeIter)
				{
					if(UK2Node_CreateDelegate* Node = *NodeIter)
					{
						Node->ValidationAfterFunctionsAreCreated(MessageLog, (0 != bIsFullCompile));
					}
				}
			}
		}
	}

	// It's necessary to tell if UberGraphFunction is ready to create frame.
	if (NewClass->UberGraphFunction)
	{
		NewClass->UberGraphFunction->SetFlags(RF_LoadCompleted);
	}

	{ BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_FinalizationWork);

		// Set any final flags and seal the class, build a CDO, etc...
		FinishCompilingClass(NewClass);

		// Build delegate binding maps if we have a graph
		if (ConsolidatedEventGraph)
		{
			// Build any dynamic binding information for this class
			BuildDynamicBindingObjects(NewClass);
		}

		UObject* NewCDO = NewClass->GetDefaultObject();

		FUserDefinedStructureCompilerUtils::DefaultUserDefinedStructs(NewCDO, MessageLog);

		// Copy over the CDO properties if we're not already regenerating on load.  In that case, the copy will be done after compile on load is complete
		FBlueprintEditorUtils::PropagateParentBlueprintDefaults(NewClass);

		if (Blueprint->HasAnyFlags(RF_BeingRegenerated))
		{
			if (CompileOptions.CompileType == EKismetCompileType::Full)
			{
				check(Blueprint->PRIVATE_InnermostPreviousCDO == NULL);
				Blueprint->PRIVATE_InnermostPreviousCDO = OldCDO;
			}
		}
		else
		{
			if( NewCDO )
			{
				// Propagate the old CDO's properties to the new
				if( OldCDO )
				{
					if (ObjLoaded)
					{
						if (OldLinker && OldGenLinkerIdx != INDEX_NONE)
						{
							// If we have a list of objects that are loading, patch our export table. This also fixes up load flags
							FBlueprintEditorUtils::PatchNewCDOIntoLinker(Blueprint->GeneratedClass->GetDefaultObject(), OldLinker, OldGenLinkerIdx, *ObjLoaded);
						}
						else
						{
							UE_LOG(LogK2Compiler, Warning, TEXT("Failed to patch linker table for blueprint CDO %s"), *NewCDO->GetName());
						}
					}

					UEditorEngine::FCopyPropertiesForUnrelatedObjectsParams CopyDetails;
					CopyDetails.bCopyDeprecatedProperties = Blueprint->bIsRegeneratingOnLoad;
					UEditorEngine::CopyPropertiesForUnrelatedObjects(OldCDO, NewCDO, CopyDetails);
					FBlueprintEditorUtils::PatchCDOSubobjectsIntoExport(OldCDO, NewCDO);
				}

				// >>> Backwards Compatibility: Propagate data from the skel CDO to the gen CDO if we haven't already done so for this blueprint
				if( !bIsSkeletonOnly && !Blueprint->IsGeneratedClassAuthoritative() )
				{
					UEditorEngine::FCopyPropertiesForUnrelatedObjectsParams CopyDetails;
					CopyDetails.bAggressiveDefaultSubobjectReplacement = false;
					CopyDetails.bDoDelta = false;
					UEditorEngine::CopyPropertiesForUnrelatedObjects(Blueprint->SkeletonGeneratedClass->GetDefaultObject(), NewCDO, CopyDetails);
					Blueprint->SetLegacyGeneratedClassIsAuthoritative();
				}
				// <<< End Backwards Compatibility
			}
		}

		CopyTermDefaultsToDefaultObject(NewCDO);
		SetCanEverTick();
		SetWantsBeginPlay();

		// Note: The old->new CDO copy is deferred when regenerating, so we skip this step in that case.
		if (!Blueprint->HasAnyFlags(RF_BeingRegenerated))
		{
			// Update the custom property list used in post construction logic to include native class properties for which the Blueprint CDO differs from the native CDO.
			TargetClass->UpdateCustomPropertyListForPostConstruction();
		}
	}

	// Fill out the function bodies, either with function bodies, or simple stubs if this is skeleton generation
	{
		// Should we display debug information about the backend outputs?
		bool bDisplayCpp = false;
		bool bDisplayBytecode = false;

		if (!Blueprint->bIsRegeneratingOnLoad)
		{
			GConfig->GetBool(TEXT("Kismet"), TEXT("CompileDisplaysTextBackend"), /*out*/ bDisplayCpp, GEngineIni);
			GConfig->GetBool(TEXT("Kismet"), TEXT("CompileDisplaysBinaryBackend"), /*out*/ bDisplayBytecode, GEngineIni);
		}

		// Always run the VM backend, it's needed for more than just debug printing
		{
			BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_CodeGenerationTime);
			const bool bGenerateStubsOnly = !bIsFullCompile || (0 != MessageLog.NumErrors);
			Backend_VM.GenerateCodeFromClass(NewClass, FunctionList, bGenerateStubsOnly);
		}

		if (bDisplayBytecode && bIsFullCompile)
		{
			TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

			FKismetBytecodeDisassembler Disasm(*GLog);

			// Disassemble script code
			for (int32 i = 0; i < FunctionList.Num(); ++i)
			{
				FKismetFunctionContext& Function = FunctionList[i];
				if (Function.IsValid())
				{
					UE_LOG(LogK2Compiler, Log, TEXT("\n\n[function %s]:\n"), *(Function.Function->GetName()));
					Disasm.DisassembleStructure(Function.Function);
				}
			}
		}

		// Generate code thru the backend(s)
		if ((bDisplayCpp && bIsFullCompile) || CompileOptions.DoesRequireCppCodeGeneration())
		{
			FString CppSourceCode;
			FString HeaderSourceCode;

			{
				TUniquePtr<IBlueprintCompilerCppBackend> Backend_CPP(IBlueprintCompilerCppBackendModuleInterface::Get().Create());
				HeaderSourceCode = Backend_CPP->GenerateCodeFromClass(NewClass, FunctionList, !bIsFullCompile, CppSourceCode);
			}

			if (CompileOptions.OutHeaderSourceCode.IsValid())
			{
				*CompileOptions.OutHeaderSourceCode = HeaderSourceCode;
			}

			if (CompileOptions.OutCppSourceCode.IsValid())
			{
				*CompileOptions.OutCppSourceCode = CppSourceCode;
			}

			if (bDisplayCpp)
			{
				UE_LOG(LogK2Compiler, Log, TEXT("[header]\n\n\n%s"), *HeaderSourceCode);
				UE_LOG(LogK2Compiler, Log, TEXT("[body]\n\n\n%s"), *CppSourceCode);
			}
		}

		static const FBoolConfigValueHelper DisplayLayout(TEXT("Kismet"), TEXT("bDisplaysLayout"), GEngineIni);
		if (!Blueprint->bIsRegeneratingOnLoad && bIsFullCompile && DisplayLayout && NewClass)
		{
			UE_LOG(LogK2Compiler, Log, TEXT("\n\nLAYOUT CLASS %s:"), *GetNameSafe(NewClass));

			for (auto Prop : TFieldRange<UProperty>(NewClass, EFieldIteratorFlags::ExcludeSuper))
			{
				UE_LOG(LogK2Compiler, Log, TEXT("%5d:\t%-64s\t%s"), Prop->GetOffset_ForGC(), *GetNameSafe(Prop), *Prop->GetCPPType());
			}

			for (auto LocFunction : TFieldRange<UFunction>(NewClass, EFieldIteratorFlags::ExcludeSuper))
			{
				UE_LOG(LogK2Compiler, Log, TEXT("\n\nLAYOUT FUNCTION %s:"), *GetNameSafe(LocFunction));
				for (auto Prop : TFieldRange<UProperty>(LocFunction))
				{
					const bool bOutParam = Prop && (0 != (Prop->PropertyFlags & CPF_OutParm));
					const bool bInParam = Prop && !bOutParam && (0 != (Prop->PropertyFlags & CPF_Parm));
					UE_LOG(LogK2Compiler, Log, TEXT("%5d:\t%-64s\t%s %s%s")
						, Prop->GetOffset_ForGC(), *GetNameSafe(Prop), *Prop->GetCPPType()
						, bInParam ? TEXT("Input") : TEXT("")
						, bOutParam ? TEXT("Output") : TEXT(""));
				}
			}
		}
	}

	// If this was a skeleton compile, make sure everything is RF_Transient
	if (bIsSkeletonOnly)
	{
		TArray<UObject*> Subobjects;
		GetObjectsWithOuter(NewClass, Subobjects);

		for (auto SubObjIt = Subobjects.CreateIterator(); SubObjIt; ++SubObjIt)
		{
			UObject* CurrObj = *SubObjIt;
			CurrObj->SetFlags(RF_Transient);
		}

		NewClass->SetFlags(RF_Transient);

		check(NewClass->ClassDefaultObject != nullptr);
		NewClass->ClassDefaultObject->SetFlags(RF_Transient);
	}

	// For full compiles, find other blueprints that may need refreshing, and mark them dirty, in case they try to run
	if( bIsFullCompile && !Blueprint->bIsRegeneratingOnLoad )
	{
		TArray<UBlueprint*> DependentBlueprints;
		FBlueprintEditorUtils::GetDependentBlueprints(Blueprint, DependentBlueprints);
		for (auto CurrentBP : DependentBlueprints)
		{
			// Get the current dirty state of the package
			UPackage* const Package = Cast<UPackage>(CurrentBP->GetOutermost());
			const bool bStartedWithUnsavedChanges = Package != nullptr ? Package->IsDirty() : true;
			const EBlueprintStatus OriginalStatus = CurrentBP->Status;

			FBlueprintEditorUtils::RefreshExternalBlueprintDependencyNodes(CurrentBP, NewClass);
			
			// Dependent blueprints will be recompile anyway by reinstancer (if necessary).
			CurrentBP->Status = OriginalStatus;

			// Note: We do not send a change notification event to the dependent BP here because
			// we have not yet reinstanced any of the instances of the BP being compiled, which may
			// be referenced by instances of the dependent BP that may be reconstructed as a result.

			// Clear the package dirty state if it did not initially have any unsaved changes to begin with
			if(Package != nullptr && Package->IsDirty() && !bStartedWithUnsavedChanges)
			{
				Package->SetDirtyFlag(false);
			}
		}
	}

	// Clear out pseudo-local members that are only valid within a Compile call
	UbergraphContext = NULL;
	CallsIntoUbergraph.Empty();
	TimelineToMemberVariableMap.Empty();


	check(NewClass->PropertiesSize >= UObject::StaticClass()->PropertiesSize);
	check(NewClass->ClassDefaultObject != NULL);

	PostCompileDiagnostics();

	if (bIsFullCompile && !Blueprint->bIsRegeneratingOnLoad)
	{
		bool Result = ValidateGeneratedClass(NewClass);
		// TODO What do we do if validation fails?
	}

	if (bIsFullCompile)
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_ChecksumCDO);

		static const FBoolConfigValueHelper ChangeDefaultValueWithoutReinstancing(TEXT("Kismet"), TEXT("bChangeDefaultValueWithoutReinstancing"), GEngineIni);
		// CRC is usually calculated for all Properties. If the bChangeDefaultValueWithoutReinstancing optimization is enabled, then only specific properties are considered (in fact we should consider only . See UE-9883.
		// Some native properties (bCanEverTick) may be implicitly changed by KismetCompiler during compilation, so they always need to be compared.
		// Some properties with a custom Property Editor Widget may not propagate changes among instances. They may be also compared.

		class FSpecializedArchiveCrc32 : public FArchiveObjectCrc32
		{
		public:
			bool bAllProperties;

			FSpecializedArchiveCrc32(bool bInAllProperties)
				: FArchiveObjectCrc32()
				, bAllProperties(bInAllProperties)
			{}

			static bool PropertyCanBeImplicitlyChanged(const UProperty* InProperty)
			{
				check(InProperty);

				auto PropertyOwnerClass = InProperty->GetOwnerClass();
				const bool bOwnerIsNativeClass = PropertyOwnerClass && PropertyOwnerClass->HasAnyClassFlags(CLASS_Native);

				auto PropertyOwnerStruct = InProperty->GetOwnerStruct();
				const bool bOwnerIsNativeStruct = !PropertyOwnerClass && (!PropertyOwnerStruct || !PropertyOwnerStruct->IsA<UUserDefinedStruct>());

				return InProperty->IsA<UStructProperty>()
					|| bOwnerIsNativeClass || bOwnerIsNativeStruct;
			}

			// Begin FArchive Interface
			virtual bool ShouldSkipProperty(const UProperty* InProperty) const override
			{
				return FArchiveObjectCrc32::ShouldSkipProperty(InProperty) 
					|| (!bAllProperties && !PropertyCanBeImplicitlyChanged(InProperty));
			}
			// End FArchive Interface
		};

		UObject* NewCDO = NewClass->GetDefaultObject(false);
		FSpecializedArchiveCrc32 CrcArchive(!ChangeDefaultValueWithoutReinstancing);
		Blueprint->CrcLastCompiledCDO = NewCDO ? CrcArchive.Crc32(NewCDO) : 0;
	}

	if (bIsFullCompile)
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_ChecksumSignature);

		class FSignatureArchiveCrc32 : public FArchiveObjectCrc32
		{
		public:
			static bool IsInnerProperty(const UObject* Object)
			{
				auto Property = Cast<const UProperty>(Object);
				return Property // check arrays
					&& Cast<const UFunction>(Property->GetOwnerStruct())
					&& !Property->HasAnyPropertyFlags(CPF_Parm);
			}

			virtual FArchive& operator<<(UObject*& Object) override
			{
				FArchive& Ar = *this;

				if (Object && !IsInnerProperty(Object))
				{
					// Names of functions and properties are significant.
					auto UniqueName = GetPathNameSafe(Object);
					Ar << UniqueName;

					if (Object->IsIn(RootObject))
					{
						ObjectsToSerialize.Enqueue(Object);
					}
				}

				return Ar;
			}

			virtual bool CustomSerialize(UObject* Object) override
			{ 
				FArchive& Ar = *this;

				bool bResult = false;
				if (auto Struct = Cast<UStruct>(Object))
				{
					if (Object == RootObject) // name and location are significant for the signature
					{
						auto UniqueName = GetPathNameSafe(Object);
						Ar << UniqueName;
					}

					UObject* SuperStruct = Struct->GetSuperStruct();
					Ar << SuperStruct;
					Ar << Struct->Children;

					if (auto Function = Cast<UFunction>(Struct))
					{
						Ar << Function->FunctionFlags;
					}

					if (auto AsClass = Cast<UClass>(Struct))
					{
						Ar << AsClass->ClassFlags;
						Ar << AsClass->Interfaces;
					}

					Ar << Struct->Next;

					bResult = true;
				}

				return bResult;
			}
		};

		FSignatureArchiveCrc32 SignatureArchiveCrc32;
		auto ParentBP = UBlueprint::GetBlueprintFromClass(NewClass->GetSuperClass());
		const uint32 ParentSignatureCrc = ParentBP ? ParentBP->CrcLastCompiledSignature : 0;
		Blueprint->CrcLastCompiledSignature = SignatureArchiveCrc32.Crc32(NewClass, ParentSignatureCrc);
	}

	PostCompile();
}

bool FKismetCompilerContext::ValidateGeneratedClass(UBlueprintGeneratedClass* Class)
{
	// Our CDO should be properly constructed by this point and should always exist
	FKismetCompilerUtilities::ValidateEnumProperties(NewClass->GetDefaultObject(), MessageLog);

	return UBlueprint::ValidateGeneratedClass(Class);
}

const UK2Node_FunctionEntry* FKismetCompilerContext::FindLocalEntryPoint(const UFunction* Function) const
{
	for (int32 i = 0; i < FunctionList.Num(); ++i)
	{
		const FKismetFunctionContext& FunctionContext = FunctionList[i];
		if (FunctionContext.IsValid() && FunctionContext.Function == Function)
		{
			return FunctionContext.EntryPoint;
		}
	}
	return NULL;
}

void FKismetCompilerContext::SetCanEverTick() const
{
	FTickFunction* TickFunction = nullptr;
	FTickFunction* ParentTickFunction = nullptr;
	
	if (AActor* CDActor = Cast<AActor>(NewClass->GetDefaultObject()))
	{
		TickFunction = &CDActor->PrimaryActorTick;
		ParentTickFunction = &NewClass->GetSuperClass()->GetDefaultObject<AActor>()->PrimaryActorTick;
	}
	else if (UActorComponent* CDComponent = Cast<UActorComponent>(NewClass->GetDefaultObject()))
	{
		TickFunction = &CDComponent->PrimaryComponentTick;
		ParentTickFunction = &NewClass->GetSuperClass()->GetDefaultObject<UActorComponent>()->PrimaryComponentTick;
	}

	if (TickFunction == nullptr)
	{
		return;
	}

	const bool bOldFlag = TickFunction->bCanEverTick;
	// RESET FLAG 
	TickFunction->bCanEverTick = ParentTickFunction->bCanEverTick;
	
	// RECEIVE TICK
	if (!TickFunction->bCanEverTick)
	{
		// Make sure that both AActor and UActorComponent have the same name for their tick method
		static FName ReceiveTickName(GET_FUNCTION_NAME_CHECKED(AActor, ReceiveTick));
		static FName ComponentReceiveTickName(GET_FUNCTION_NAME_CHECKED(UActorComponent, ReceiveTick));

		if (const UFunction* ReceiveTickEvent = FKismetCompilerUtilities::FindOverriddenImplementableEvent(ReceiveTickName, NewClass))
		{
			// We have a tick node, but are we allowed to?

			const UEngine* EngineSettings = GetDefault<UEngine>();
			const bool bAllowTickingByDefault = EngineSettings->bCanBlueprintsTickByDefault;

			const UClass* FirstNativeClass = FBlueprintEditorUtils::FindFirstNativeClass(NewClass);
			const bool bHasCanTickMetadata = (FirstNativeClass != nullptr) && FirstNativeClass->HasMetaData(FBlueprintMetadata::MD_ChildCanTick);
			const bool bHasCannotTickMetadata = (FirstNativeClass != nullptr) && FirstNativeClass->HasMetaData(FBlueprintMetadata::MD_ChildCannotTick);
			const bool bHasUniversalParent = (FirstNativeClass != nullptr) && ((AActor::StaticClass() == FirstNativeClass) || (UActorComponent::StaticClass() == FirstNativeClass));

			if (bHasCanTickMetadata && bHasCannotTickMetadata)
			{
				// User error: The C++ class has conflicting metadata
				const FString ConlictingMetadataWarning = FString::Printf(
					*LOCTEXT("HasBothCanAndCannotMetadata", "Native class %s has both '%s' and '%s' metadata specified, they are mutually exclusive and '%s' will win.").ToString(),
					*FirstNativeClass->GetPathName(),
					*FBlueprintMetadata::MD_ChildCanTick.ToString(),
					*FBlueprintMetadata::MD_ChildCannotTick.ToString(),
					*FBlueprintMetadata::MD_ChildCannotTick.ToString());
				MessageLog.Warning(*ConlictingMetadataWarning);
			}

			if (bHasCannotTickMetadata)
			{
				// This could only happen if someone adds bad metadata to AActor or UActorComponent directly
				check(!bHasUniversalParent);

				// Parent class has forbidden us to tick
				const FString NativeClassSaidNo = FString::Printf(
					*LOCTEXT("NativeClassProhibitsTicking", "@@ is not allowed as the C++ parent class %s has disallowed Blueprint subclasses from ticking.  Please consider using a Timer instead of Tick.").ToString(),
					*FirstNativeClass->GetPathName(),
					*FBlueprintMetadata::MD_ChildCannotTick.ToString());
				MessageLog.Warning(*NativeClassSaidNo, FindLocalEntryPoint(ReceiveTickEvent));
			}
			else
			{
				if (bAllowTickingByDefault || bHasUniversalParent || bHasCanTickMetadata)
				{
					// We're allowed to tick for one reason or another
					TickFunction->bCanEverTick = true;
				}
				else
				{
					// Nothing allowing us to tick
					const FString ReceiveTickEventWarning = FString::Printf(
						*LOCTEXT("ReceiveTick_CanNeverTick", "@@ is not allowed for Blueprints based on the C++ parent class %s, so it will never Tick!").ToString(),
						*FirstNativeClass->GetPathName());
					MessageLog.Warning(*ReceiveTickEventWarning, FindLocalEntryPoint(ReceiveTickEvent));

					const FString ReceiveTickEventRemedies = FString::Printf(
						*LOCTEXT("RecieveTick_CanNeverTickRemedies", "You can solve this in several ways:\n  1) Consider using a Timer instead of Tick.\n  2) Add meta=(%s) to the parent C++ class\n  3) Reparent the Blueprint to AActor or UActorComponent, which can always tick.").ToString(),
						*FBlueprintMetadata::MD_ChildCanTick.ToString());
					MessageLog.Warning(*ReceiveTickEventRemedies);
				}
			}
		}
	}

	if (TickFunction->bCanEverTick != bOldFlag)
	{
		UE_LOG(LogK2Compiler, Verbose, TEXT("Overridden flag for class '%s': CanEverTick %s "), *NewClass->GetName(),
			TickFunction->bCanEverTick ? *(GTrue.ToString()) : *(GFalse.ToString()) );
	}
}

void FKismetCompilerContext::SetWantsBeginPlay() const
{
	UActorComponent* CDComponent = Cast<UActorComponent>(NewClass->GetDefaultObject());

	if (CDComponent == nullptr)
	{
		return;
	}

	const bool bOldFlag = CDComponent->bWantsBeginPlay;

	CDComponent->bWantsBeginPlay = NewClass->GetSuperClass()->GetDefaultObject<UActorComponent>()->bWantsBeginPlay;

	if (!CDComponent->bWantsBeginPlay)
	{
		static FName ReceiveBeginPlayName(GET_FUNCTION_NAME_CHECKED(UActorComponent, ReceiveBeginPlay));
		static FName ReceiveEndPlayName(GET_FUNCTION_NAME_CHECKED(UActorComponent, ReceiveEndPlay));
		const UFunction* ReciveBeginPlayEvent = FKismetCompilerUtilities::FindOverriddenImplementableEvent(ReceiveBeginPlayName, NewClass);
		const UFunction* ReciveEndPlayEvent = FKismetCompilerUtilities::FindOverriddenImplementableEvent(ReceiveEndPlayName, NewClass);
		if (ReciveBeginPlayEvent || ReciveEndPlayEvent)
		{
			CDComponent->bWantsBeginPlay = true;
		}
	}

	if(CDComponent->bWantsBeginPlay != bOldFlag)
	{
		UE_LOG(LogK2Compiler, Verbose, TEXT("Overridden flag for class '%s': bWantsBeginPlay %s "), *NewClass->GetName(),
			CDComponent->bWantsBeginPlay ? *(GTrue.ToString()) : *(GFalse.ToString()) );
	}
}

bool FKismetCompilerContext::UsePersistentUberGraphFrame() const
{
	return UBlueprintGeneratedClass::UsePersistentUberGraphFrame() && !CompileOptions.DoesRequireCppCodeGeneration();
}

FString FKismetCompilerContext::GetGuid(const UEdGraphNode* Node) const
{
	// We need a unique, deterministic name for the properties we're generating, but the chance of collision is small
	// so I think we can get away with stomping part of a guid with a hash:
	uint32 ResultCRC = FCrc::MemCrc32(&(Node->NodeGuid), sizeof(Node->NodeGuid));
	UEdGraphNode* const* SourceNode = MacroGeneratedNodes.Find(Node);
	while (SourceNode && *SourceNode)
	{
		ResultCRC = FCrc::MemCrc32(&((*SourceNode)->NodeGuid), sizeof((*SourceNode)->NodeGuid), ResultCRC);
		SourceNode = MacroGeneratedNodes.Find(*SourceNode);
	}

	FGuid Ret = Node->NodeGuid;
	Ret.D = ResultCRC;
	return Ret.ToString();
}

#undef LOCTEXT_NAMESPACE
//////////////////////////////////////////////////////////////////////////