// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"

#include "Animation/AnimNodeBase.h"
#include "WidgetBlueprintCompiler.h"
#include "Kismet2NameValidators.h"
#include "KismetReinstanceUtilities.h"
#include "MovieScene.h"
#include "K2Node_FunctionEntry.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "NamedSlot.h"

#define LOCTEXT_NAMESPACE "UMG"

FWidgetBlueprintCompiler::FWidgetBlueprintCompiler(UWidgetBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions, TArray<UObject*>* InObjLoaded)
	: Super(SourceSketch, InMessageLog, InCompilerOptions, InObjLoaded)
{
}

FWidgetBlueprintCompiler::~FWidgetBlueprintCompiler()
{
}

void FWidgetBlueprintCompiler::CreateFunctionList()
{
	Super::CreateFunctionList();

	for ( FDelegateEditorBinding& EditorBinding : WidgetBlueprint()->Bindings )
	{
		if ( EditorBinding.SourcePath.IsEmpty() )
		{
			const FName PropertyName = EditorBinding.SourceProperty;

			UProperty* Property = FindField<UProperty>(Blueprint->SkeletonGeneratedClass, PropertyName);
			if ( Property )
			{
				// Create the function graph.
				FString FunctionName = FString(TEXT("__Get")) + PropertyName.ToString();
				UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, FunctionName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());

				// Update the function binding to match the generated graph name
				EditorBinding.FunctionName = FunctionGraph->GetFName();

				const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(FunctionGraph->GetSchema());

				Schema->CreateDefaultNodesForGraph(*FunctionGraph);

				K2Schema->MarkFunctionEntryAsEditable(FunctionGraph, true);

				// Create a function entry node
				FGraphNodeCreator<UK2Node_FunctionEntry> FunctionEntryCreator(*FunctionGraph);
				UK2Node_FunctionEntry* EntryNode = FunctionEntryCreator.CreateNode();
				EntryNode->SignatureClass = NULL;
				EntryNode->SignatureName = FunctionGraph->GetFName();
				FunctionEntryCreator.Finalize();

				FGraphNodeCreator<UK2Node_FunctionResult> FunctionReturnCreator(*FunctionGraph);
				UK2Node_FunctionResult* ReturnNode = FunctionReturnCreator.CreateNode();
				ReturnNode->SignatureClass = NULL;
				ReturnNode->SignatureName = FunctionGraph->GetFName();
				ReturnNode->NodePosX = EntryNode->NodePosX + EntryNode->NodeWidth + 256;
				ReturnNode->NodePosY = EntryNode->NodePosY;
				FunctionReturnCreator.Finalize();

				FEdGraphPinType PinType;
				K2Schema->ConvertPropertyToPinType(Property, /*out*/ PinType);

				UEdGraphPin* ReturnPin = ReturnNode->CreateUserDefinedPin(TEXT("ReturnValue"), PinType, EGPD_Input);

				// Auto-connect the pins for entry and exit, so that by default the signature is properly generated
				UEdGraphPin* EntryNodeExec = K2Schema->FindExecutionPin(*EntryNode, EGPD_Output);
				UEdGraphPin* ResultNodeExec = K2Schema->FindExecutionPin(*ReturnNode, EGPD_Input);
				EntryNodeExec->MakeLinkTo(ResultNodeExec);

				FGraphNodeCreator<UK2Node_VariableGet> MemberGetCreator(*FunctionGraph);
				UK2Node_VariableGet* VarNode = MemberGetCreator.CreateNode();
				VarNode->VariableReference.SetSelfMember(PropertyName);
				MemberGetCreator.Finalize();

				ReturnPin->MakeLinkTo(VarNode->GetValuePin());

				// We need to flag the entry node to make sure that the compiled function is callable from Kismet2
				int32 ExtraFunctionFlags = ( FUNC_Private | FUNC_Const );
				K2Schema->AddExtraFunctionFlags(FunctionGraph, ExtraFunctionFlags);

				//Blueprint->FunctionGraphs.Add(FunctionGraph);

				ProcessOneFunctionGraph(FunctionGraph, true);
				//FEdGraphUtilities::MergeChildrenGraphsIn(Ubergraph, FunctionGraph, /*bRequireSchemaMatch=*/ true);
			}
		}
	}
}

void FWidgetBlueprintCompiler::ValidateWidgetNames()
{
	UWidgetBlueprint* WidgetBP = WidgetBlueprint();

	TSharedPtr<FKismetNameValidator> ParentBPNameValidator;
	if ( WidgetBP->ParentClass != nullptr )
	{
		UBlueprint* ParentBP = Cast<UBlueprint>(WidgetBP->ParentClass->ClassGeneratedBy);
		if ( ParentBP != nullptr )
		{
			ParentBPNameValidator = MakeShareable(new FKismetNameValidator(ParentBP));
		}
	}

	WidgetBP->WidgetTree->ForEachWidget([&] (UWidget* Widget) {
		if ( ParentBPNameValidator.IsValid() && ParentBPNameValidator->IsValid(Widget->GetName()) != EValidatorResult::Ok )
		{
			// TODO Support renaming items, similar to timelines.

			//// Use the viewer displayed Timeline name (without the _Template suffix) because it will be added later for appropriate checks.
			//FString TimelineName = UTimelineTemplate::TimelineTemplateNameToVariableName(TimelineTemplate->GetFName());

			//FName NewName = FBlueprintEditorUtils::FindUniqueKismetName(WidgetBP, TimelineName);
			//MessageLog.Warning(*FString::Printf(*LOCTEXT("TimelineConflictWarning", "Found a timeline with a conflicting name (%s) - changed to %s.").ToString(), *TimelineTemplate->GetName(), *NewName.ToString()));
			//FBlueprintEditorUtils::RenameTimeline(WidgetBP, FName(*TimelineName), NewName);
		}
	});
}

template<typename TOBJ>
struct FCullTemplateObjectsHelper
{
	const TArray<TOBJ*>& Templates;

	FCullTemplateObjectsHelper(const TArray<TOBJ*>& InComponentTemplates)
		: Templates(InComponentTemplates)
	{}

	bool operator()(const UObject* const RemovalCandidate) const
	{
		return ( NULL != Templates.FindByKey(RemovalCandidate) );
	}
};

void FWidgetBlueprintCompiler::CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& OldCDO)
{
	Super::CleanAndSanitizeClass(ClassToClean, OldCDO);

	// Make sure our typed pointer is set
	check(ClassToClean == NewClass);
	NewWidgetBlueprintClass = CastChecked<UWidgetBlueprintGeneratedClass>((UObject*)NewClass);

	// Don't leave behind widget trees that are outered to the same widget blueprint rename them away first.
	if ( NewWidgetBlueprintClass->WidgetTree )
	{
		NewWidgetBlueprintClass->WidgetTree->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
		NewWidgetBlueprintClass->WidgetTree = nullptr;
	}

	for ( UWidgetAnimation* Animation : NewWidgetBlueprintClass->Animations )
	{
		Animation->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
	}
	NewWidgetBlueprintClass->Animations.Empty();

	NewWidgetBlueprintClass->Bindings.Empty();
}

void FWidgetBlueprintCompiler::SaveSubObjectsFromCleanAndSanitizeClass(FSubobjectCollection& SubObjectsToSave, UBlueprintGeneratedClass* ClassToClean, UObject*& OldCDO)
{
	// Make sure our typed pointer is set
	check(ClassToClean == NewClass);
	NewWidgetBlueprintClass = CastChecked<UWidgetBlueprintGeneratedClass>((UObject*)NewClass);

	UWidgetBlueprint* WidgetBP = WidgetBlueprint();

	// We need to save the widget tree to survive the initial sub-object clean blitz, 
	// otherwise they all get renamed, and it causes early loading errors.
	SubObjectsToSave.AddObject(WidgetBP->WidgetTree);

	// We need to save all the animations to survive the initial sub-object clean blitz, 
	// otherwise they all get renamed, and it causes early loading errors.
	SubObjectsToSave.AddObject(NewWidgetBlueprintClass->WidgetTree);
	for ( UWidgetAnimation* Animation : NewWidgetBlueprintClass->Animations )
	{
		SubObjectsToSave.AddObject(Animation);
	}
}

void FWidgetBlueprintCompiler::CreateClassVariablesFromBlueprint()
{
	Super::CreateClassVariablesFromBlueprint();

	UWidgetBlueprint* WidgetBP = WidgetBlueprint();
	UClass* ParentClass = WidgetBP->ParentClass;

	ValidateWidgetNames();

	// Build the set of variables based on the variable widgets in the widget tree.
	TArray<UWidget*> Widgets;
	WidgetBP->WidgetTree->GetAllWidgets(Widgets);

	// Sort the widgets alphabetically
	Widgets.Sort( []( const UWidget& Lhs, const UWidget& Rhs ) { return Rhs.GetFName() < Lhs.GetFName(); } );

	// Add widget variables
	for ( UWidget* Widget : Widgets )
	{
		// Skip non-variable widgets
		if ( !Widget->bIsVariable )
		{
			continue;
		}

		// This code was added to fix the problem of recompiling dependent widgets, not using the newest
		// class thus resulting in REINST failures in dependent blueprints.
		UClass* WidgetClass = Widget->GetClass();
		if ( UBlueprintGeneratedClass* BPWidgetClass = Cast<UBlueprintGeneratedClass>(WidgetClass) )
		{
			WidgetClass = BPWidgetClass->GetAuthoritativeClass();
		}

		UProperty* ExistingProperty = ParentClass->FindPropertyByName( Widget->GetFName() );
		if ( ExistingProperty && ExistingProperty->HasMetaData( "BindWidget" ) )
		{
			WidgetToMemberVariableMap.Add( Widget, ExistingProperty );
			continue;
		}

		FEdGraphPinType WidgetPinType(Schema->PC_Object, TEXT(""), WidgetClass, false, false);
		
		// Always name the variable according to the underlying FName of the widget object
		UProperty* WidgetProperty = CreateVariable(Widget->GetFName(), WidgetPinType);
		if ( WidgetProperty != nullptr )
		{
			const FString VariableName = Widget->IsGeneratedName() ? Widget->GetName() : Widget->GetLabelText().ToString();
			WidgetProperty->SetMetaData(TEXT("DisplayName"), *VariableName);
			WidgetProperty->SetMetaData(TEXT("Category"), *WidgetBP->GetName());
			
			WidgetProperty->SetPropertyFlags(CPF_BlueprintVisible);
			WidgetProperty->SetPropertyFlags(CPF_Transient);
			WidgetProperty->SetPropertyFlags(CPF_RepSkip);

			WidgetToMemberVariableMap.Add(Widget, WidgetProperty);
		}
	}

	// Add movie scenes variables here
	for(UWidgetAnimation* Animation : WidgetBP->Animations)
	{
		FEdGraphPinType WidgetPinType(Schema->PC_Object, TEXT(""), Animation->GetClass(), false, true);
		UProperty* AnimationProperty = CreateVariable(Animation->GetFName(), WidgetPinType);

		if ( AnimationProperty != nullptr )
		{
			AnimationProperty->SetMetaData(TEXT("Category"), TEXT("Animations"));

			AnimationProperty->SetPropertyFlags(CPF_BlueprintVisible);
			AnimationProperty->SetPropertyFlags(CPF_Transient);
			AnimationProperty->SetPropertyFlags(CPF_RepSkip);
		}
	}
}

void FWidgetBlueprintCompiler::FinishCompilingClass(UClass* Class)
{
	UWidgetBlueprint* WidgetBP = WidgetBlueprint();

	// Don't do a bunch of extra work on the skeleton generated class
	if ( WidgetBP->SkeletonGeneratedClass != Class )
	{
		UWidgetBlueprintGeneratedClass* BPGClass = CastChecked<UWidgetBlueprintGeneratedClass>(Class);
		if( !WidgetBP->bHasBeenRegenerated )
		{
			FBlueprintEditorUtils::ForceLoadMembers(WidgetBP->WidgetTree);
		}
		BPGClass->WidgetTree = DuplicateObject<UWidgetTree>(WidgetBP->WidgetTree, BPGClass);

		for ( const UWidgetAnimation* Animation : WidgetBP->Animations )
		{
			UWidgetAnimation* ClonedAnimation = DuplicateObject<UWidgetAnimation>(Animation, BPGClass, *( Animation->GetName() + TEXT("_INST") ));

			BPGClass->Animations.Add(ClonedAnimation);
		}

		// Only check bindings on a full compile.  Also don't check them if we're regenerating on load,
		// that has a nasty tendency to fail because the other dependent classes that may also be blueprints
		// might not be loaded yet.
		const bool bIsLoading = WidgetBP->bIsRegeneratingOnLoad;
		if ( bIsFullCompile )
		{
			// Convert all editor time property bindings into a list of bindings
			// that will be applied at runtime.  Ensure all bindings are still valid.
			for ( const FDelegateEditorBinding& EditorBinding : WidgetBP->Bindings )
			{
				if ( bIsLoading || EditorBinding.IsBindingValid(Class, WidgetBP, MessageLog) )
				{
					BPGClass->Bindings.Add(EditorBinding.ToRuntimeBinding(WidgetBP));
				}
			}
		}

		// Add all the names of the named slot widgets to the slot names structure.
		BPGClass->NamedSlots.Reset();
		BPGClass->WidgetTree->ForEachWidget([&] (UWidget* Widget) {
			if ( Widget && Widget->IsA<UNamedSlot>() )
			{
				BPGClass->NamedSlots.Add(Widget->GetFName());
			}
		});
	}

	UClass* ParentClass = WidgetBP->ParentClass;
	for ( TUObjectPropertyBase<UWidget*>* WidgetProperty : TFieldRange<TUObjectPropertyBase<UWidget*>>( ParentClass ) )
	{
		if ( WidgetProperty->HasMetaData( "BindWidget" ) && ( !WidgetProperty->HasMetaData( "OptionalWidget" ) || !WidgetProperty->GetBoolMetaData( "OptionalWidget" ) ) )
		{
			UWidget* const* Widget = WidgetToMemberVariableMap.FindKey( WidgetProperty );
			if ( !Widget )
			{
				if (Blueprint->bIsNewlyCreated)
				{
					MessageLog.Warning(*LOCTEXT("RequiredWidget_NotBound", "Non-optional widget binding @@ not found.").ToString(),
						WidgetProperty);
				}
				else
				{
					MessageLog.Error(*LOCTEXT("RequiredWidget_NotBound", "Non-optional widget binding @@ not found.").ToString(),
						WidgetProperty);
				}
			}
			else if ( !( *Widget )->IsA( WidgetProperty->PropertyClass ) )
			{
				if (Blueprint->bIsNewlyCreated)
				{
					MessageLog.Warning(*LOCTEXT("IncorrectWidgetTypes", "@@ is of type @@ property is of type @@.").ToString(), *Widget,
						(*Widget)->GetClass(), WidgetProperty->PropertyClass);
				}
				else
				{
					MessageLog.Error(*LOCTEXT("IncorrectWidgetTypes", "@@ is of type @@ property is of type @@.").ToString(), *Widget,
						(*Widget)->GetClass(), WidgetProperty->PropertyClass);
				}
			}
		}
	}

	Super::FinishCompilingClass(Class);
}

void FWidgetBlueprintCompiler::Compile()
{
	Super::Compile();

	//TODO Once we handle multiple derived blueprint classes, we need to check parent versions of the class.
	if ( const UFunction* ReceiveTickEvent = FKismetCompilerUtilities::FindOverriddenImplementableEvent(GET_FUNCTION_NAME_CHECKED(UUserWidget, Tick), NewWidgetBlueprintClass) )
	{
		NewWidgetBlueprintClass->bCanEverTick = true;
	}
	else
	{
		NewWidgetBlueprintClass->bCanEverTick = false;
	}
	
	//TODO Once we handle multiple derived blueprint classes, we need to check parent versions of the class.
	if ( const UFunction* ReceivePaintEvent = FKismetCompilerUtilities::FindOverriddenImplementableEvent(GET_FUNCTION_NAME_CHECKED(UUserWidget, OnPaint), NewWidgetBlueprintClass) )
	{
		NewWidgetBlueprintClass->bCanEverPaint = true;
	}
	else
	{
		NewWidgetBlueprintClass->bCanEverPaint = false;
	}

	WidgetToMemberVariableMap.Empty();
}

void FWidgetBlueprintCompiler::EnsureProperGeneratedClass(UClass*& TargetUClass)
{
	if ( TargetUClass && !( (UObject*)TargetUClass )->IsA(UWidgetBlueprintGeneratedClass::StaticClass()) )
	{
		FKismetCompilerUtilities::ConsignToOblivion(TargetUClass, Blueprint->bIsRegeneratingOnLoad);
		TargetUClass = nullptr;
	}
}

void FWidgetBlueprintCompiler::SpawnNewClass(const FString& NewClassName)
{
	NewWidgetBlueprintClass = FindObject<UWidgetBlueprintGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);

	if ( NewWidgetBlueprintClass == nullptr )
	{
		NewWidgetBlueprintClass = NewObject<UWidgetBlueprintGeneratedClass>(Blueprint->GetOutermost(), FName(*NewClassName), RF_Public | RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked in the Blueprint yet due to load ordering issues
		FBlueprintCompileReinstancer::Create(NewWidgetBlueprintClass);
	}
	NewClass = NewWidgetBlueprintClass;
}

void FWidgetBlueprintCompiler::PrecompileFunction(FKismetFunctionContext& Context)
{
	Super::PrecompileFunction(Context);

	VerifyEventReplysAreNotEmpty(Context);
}

void FWidgetBlueprintCompiler::VerifyEventReplysAreNotEmpty(FKismetFunctionContext& Context)
{
	TArray<UK2Node_FunctionResult*> FunctionResults;
	Context.SourceGraph->GetNodesOfClass<UK2Node_FunctionResult>(FunctionResults);

	UScriptStruct* EventReplyStruct = FEventReply::StaticStruct();
	FEdGraphPinType EventReplyPinType(Schema->PC_Struct, TEXT(""), EventReplyStruct, /*bIsArray =*/false, /*bIsReference =*/false);

	for ( UK2Node_FunctionResult* FunctionResult : FunctionResults )
	{
		for ( UEdGraphPin* ReturnPin : FunctionResult->Pins )
		{
			if ( ReturnPin->PinType == EventReplyPinType )
			{
				const bool IsUnconnectedEventReply = ReturnPin->Direction == EEdGraphPinDirection::EGPD_Input && ReturnPin->LinkedTo.Num() == 0;
				if ( IsUnconnectedEventReply )
				{
					MessageLog.Warning(*LOCTEXT("MissingEventReply_Warning", "Event Reply @@ should not be empty.  Return a reply such as Handled or Unhandled.").ToString(), ReturnPin);
				}
			}
		}
	}
}

bool FWidgetBlueprintCompiler::ValidateGeneratedClass(UBlueprintGeneratedClass* Class)
{
	bool SuperResult = Super::ValidateGeneratedClass(Class);
	bool Result = UWidgetBlueprint::ValidateGeneratedClass(Class);

	return SuperResult && Result;
}

#undef LOCTEXT_NAMESPACE
