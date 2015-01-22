// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"

#include "ParticleDefinitions.h"
#include "CompilerResultsLog.h"
#include "CallFunctionHandler.h"
#include "Particles/ParticleSystemComponent.h"
#include "BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "K2Node_AddComponent"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_AddComponent

UK2Node_AddComponent::UK2Node_AddComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsPureFunc = false;
}

void UK2Node_AddComponent::AllocateDefaultPins()
{
	AllocateDefaultPinsWithoutExposedVariables();
	AllocatePinsForExposedVariables();

	UEdGraphPin* ManualAttachmentPin = GetManualAttachmentPin();

	UEdGraphSchema const* Schema = GetSchema();
	check(Schema != NULL);
	Schema->ConstructBasicPinTooltip(*ManualAttachmentPin, LOCTEXT("ManualAttachmentPinTooltip", "Defines whether the component should attach to the root automatically, or be left unattached for the user to manually attach later."), ManualAttachmentPin->PinToolTip);

	UEdGraphPin* TransformPin = GetRelativeTransformPin();
	Schema->ConstructBasicPinTooltip(*TransformPin, LOCTEXT("TransformPinTooltip", "Defines where to position the component (relative to its parent). If the component is left unattached, then the transform is relative to the world."), TransformPin->PinToolTip);
}

void UK2Node_AddComponent::AllocatePinsForExposedVariables()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	const UActorComponent* TemplateComponent = GetTemplateFromNode();
	const UClass* ComponentClass = TemplateComponent ? TemplateComponent->GetClass() : NULL;

	if(ComponentClass)
	{
		const UObject* ClassDefaultObject = ComponentClass ? ComponentClass->ClassDefaultObject : NULL;

		for (TFieldIterator<UProperty> PropertyIt(ComponentClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			const bool bNotDelegate = !Property->IsA(UMulticastDelegateProperty::StaticClass());
			const bool bIsExposedToSpawn = UEdGraphSchema_K2::IsPropertyExposedOnSpawn(Property);
			const bool bIsVisible = Property->HasAllPropertyFlags(CPF_BlueprintVisible);
			const bool bNotParam = !Property->HasAllPropertyFlags(CPF_Parm);
			if(bNotDelegate && bIsExposedToSpawn && bIsVisible && bNotParam)
			{
				FEdGraphPinType PinType;
				K2Schema->ConvertPropertyToPinType(Property, /*out*/ PinType);	
				const bool bIsUnique = (NULL == FindPin(Property->GetName()));
				if(K2Schema->FindSetVariableByNameFunction(PinType) && bIsUnique)
				{
					UEdGraphPin* Pin = CreatePin(EGPD_Input, TEXT(""), TEXT(""), NULL, false, false, Property->GetName());
					Pin->PinType = PinType;
					bHasExposedVariable = true;

					if (ClassDefaultObject && K2Schema->PinDefaultValueIsEditable(*Pin))
					{
						FString DefaultValueAsString;
						const bool bDefaultValueSet = FBlueprintEditorUtils::PropertyValueToString(Property, reinterpret_cast<const uint8*>(ClassDefaultObject), DefaultValueAsString);
						check(bDefaultValueSet);
						K2Schema->TrySetDefaultValue(*Pin, DefaultValueAsString);
					}

					// Copy tooltip from the property.
					K2Schema->ConstructBasicPinTooltip(*Pin, Property->GetToolTipText(), Pin->PinToolTip);
				}
			}
		}
	}
}

const UClass* UK2Node_AddComponent::GetSpawnedType() const
{
	const UActorComponent* TemplateComponent = GetTemplateFromNode();
	return TemplateComponent ? TemplateComponent->GetClass() : NULL;
}

void UK2Node_AddComponent::AllocateDefaultPinsWithoutExposedVariables()
{
	Super::AllocateDefaultPins();
	
	// set properties on template pin
	UEdGraphPin* TemplateNamePin = GetTemplateNamePinChecked();
	TemplateNamePin->bDefaultValueIsReadOnly = true;
	TemplateNamePin->bNotConnectable = true;
	TemplateNamePin->bHidden = true;

	// set properties on relative transform pin
	UEdGraphPin* RelativeTransformPin = GetRelativeTransformPin();
	RelativeTransformPin->bDefaultValueIsIgnored = true;

	// Override this as a non-ref param, because the identity transform is hooked up in the compiler under the hood
	RelativeTransformPin->PinType.bIsReference = false;
}

void UK2Node_AddComponent::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);

	UEdGraphPin* TemplateNamePin = GetTemplateNamePinChecked();
	//UEdGraphPin* ReturnValuePin = GetReturnValuePin();
	for(int32 OldPinIdx = 0; TemplateNamePin && OldPinIdx < OldPins.Num(); ++OldPinIdx)
	{
		if(TemplateNamePin && (TemplateNamePin->PinName == OldPins[OldPinIdx]->PinName))
		{
			TemplateNamePin->DefaultValue = OldPins[OldPinIdx]->DefaultValue;
		}
	}

	AllocatePinsForExposedVariables();
}

void UK2Node_AddComponent::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	UActorComponent* Template = GetTemplateFromNode();
	if (Template)
	{
		UClass* TemplateClass = Template->GetClass();
		if (!TemplateClass->IsChildOf(UActorComponent::StaticClass()) || TemplateClass->HasAnyClassFlags(CLASS_Abstract) || !TemplateClass->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent) )
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("TemplateClass"), FText::FromString(TemplateClass->GetName()));
			Args.Add(TEXT("NodeTitle"), GetNodeTitle(ENodeTitleType::FullTitle));
			MessageLog.Error(*FText::Format(NSLOCTEXT("KismetCompiler", "InvalidComponentTemplate_Error", "Invalid class '{TemplateClass}' used as template by '{NodeTitle}' for @@"), Args).ToString(), this);
		}

		if (UChildActorComponent const* ChildActorComponent = Cast<UChildActorComponent const>(Template))
		{
			UBlueprint const* Blueprint = GetBlueprint();

			UClass const* ChildActorClass = ChildActorComponent->ChildActorClass;
			if (ChildActorClass == Blueprint->GeneratedClass)
			{
				UEdGraph const* ParentGraph = GetGraph();
				UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();

				if (K2Schema->IsConstructionScript(ParentGraph))
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("ChildActorClass"), FText::FromString(ChildActorClass->GetName()));
					MessageLog.Error(*FText::Format(NSLOCTEXT("KismetCompiler", "AddSelfComponent_Error", "@@ cannot add a '{ChildActorClass}' component in the construction script (could cause infinite recursion)."), Args).ToString(), this);
				}
			}
			else if (ChildActorClass != nullptr)
			{
				AActor const* ChildActor = Cast<AActor>(ChildActorClass->ClassDefaultObject);
				check(ChildActor != nullptr);
				USceneComponent* RootComponent = ChildActor->GetRootComponent();

				if ((RootComponent != nullptr) && (RootComponent->Mobility == EComponentMobility::Static) && (ChildActorComponent->Mobility != EComponentMobility::Static))
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("ChildActorClass"), FText::FromString(ChildActorClass->GetName()));
					MessageLog.Error(*FText::Format(NSLOCTEXT("KismetCompiler", "AddStaticChildActorComponent_Error", "@@ cannot add a '{ChildActorClass}' component as it has static mobility, and the ChildActorComponent does not."), Args).ToString(), this);
				}
			}
		}
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeTitle"), GetNodeTitle(ENodeTitleType::FullTitle));
		MessageLog.Error(*FText::Format(NSLOCTEXT("KismetCompiler", "MissingComponentTemplate_Error", "Unknown template referenced by '{NodeTitle}' for @@"), Args).ToString(), this);
	}
}

UActorComponent* UK2Node_AddComponent::GetTemplateFromNode() const
{
	UBlueprint* BlueprintObj = GetBlueprint();

	// Find the template name input pin, to get the name from
	UEdGraphPin* TemplateNamePin = GetTemplateNamePin();
	if (TemplateNamePin)
	{
		const FString& TemplateName = TemplateNamePin->DefaultValue;
		return BlueprintObj->FindTemplateByName(FName(*TemplateName));
	}

	return NULL;
}

void UK2Node_AddComponent::DestroyNode()
{
	// See if this node has a template
	UActorComponent* Template = GetTemplateFromNode();
	if (Template != NULL)
	{
		// Get the blueprint so we can remove it from it
		UBlueprint* BlueprintObj = GetBlueprint();

		// remove it
		BlueprintObj->Modify();
		BlueprintObj->ComponentTemplates.Remove(Template);
	}

	Super::DestroyNode();
}

void UK2Node_AddComponent::PrepareForCopying()
{
	TemplateBlueprint = GetBlueprint()->GetPathName();
}

void UK2Node_AddComponent::PostPasteNode()
{
	Super::PostPasteNode();

	// There is a template associated with this node that should be unique, but after a node is pasted, it either points to a
	// template shared by the copied node, or to nothing (when pasting into a different blueprint)
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UBlueprint* Blueprint = GetBlueprint();

	// Find the template name and return type pins
	UEdGraphPin* TemplateNamePin = GetTemplateNamePin();
	UEdGraphPin* ReturnPin = GetReturnValuePin();
	if ((TemplateNamePin != NULL) && (ReturnPin != NULL))
	{
		// Find the current template if it exists
		FString TemplateName = TemplateNamePin->DefaultValue;
		UActorComponent* SourceTemplate = Blueprint->FindTemplateByName(FName(*TemplateName));

		// Determine the type of the component needed
		UClass* ComponentClass = Cast<UClass>(ReturnPin->PinType.PinSubCategoryObject.Get());
			
		if (ComponentClass)
		{
			ensure(NULL != Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass));
			// Create a new template object and update the template pin to point to it
			UActorComponent* NewTemplate = ConstructObject<UActorComponent>(ComponentClass, Blueprint->GeneratedClass);
			NewTemplate->SetFlags(RF_ArchetypeObject);
			Blueprint->ComponentTemplates.Add(NewTemplate);

			TemplateNamePin->DefaultValue = NewTemplate->GetName();

			// Copy the old template data over to the new template if it's compatible
			if ((SourceTemplate != NULL) && (SourceTemplate->GetClass()->IsChildOf(ComponentClass)))
			{
				TArray<uint8> SavedProperties;
				FObjectWriter Writer(SourceTemplate, SavedProperties);
				FObjectReader(NewTemplate, SavedProperties);
			}
			else if(TemplateBlueprint.Len() > 0)
			{
				// try to find/load our blueprint to copy the template
				UBlueprint* SourceBlueprint = FindObject<UBlueprint>(NULL, *TemplateBlueprint);
				if(SourceBlueprint != NULL)
				{
					SourceTemplate = SourceBlueprint->FindTemplateByName(FName(*TemplateName));
					if ((SourceTemplate != NULL) && (SourceTemplate->GetClass()->IsChildOf(ComponentClass)))
					{
						TArray<uint8> SavedProperties;
						FObjectWriter Writer(SourceTemplate, SavedProperties);
						FObjectReader(NewTemplate, SavedProperties);
					}
				}

				TemplateBlueprint.Empty();
			}
		}
		else
		{
			// Clear the template connection; can't resolve the type of the component to create
			ensure(false);
			TemplateNamePin->DefaultValue = TEXT("");
		}
	}
}

FText UK2Node_AddComponent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UEdGraphPin* TemplateNamePin = GetTemplateNamePin();
	if (CachedNodeTitle.IsOutOfDate() && (TemplateNamePin != NULL))
	{
		FString TemplateName = TemplateNamePin->DefaultValue;
		UBlueprint* Blueprint = GetBlueprint();
		UActorComponent* SourceTemplate = Blueprint->FindTemplateByName(FName(*TemplateName));
		if(SourceTemplate)
		{
			UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(SourceTemplate);
			USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(SourceTemplate);
			UParticleSystemComponent* PSysComp = Cast<UParticleSystemComponent>(SourceTemplate);
			UChildActorComponent* SubActorComp = Cast<UChildActorComponent>(SourceTemplate);

			if(StaticMeshComp != NULL && StaticMeshComp->StaticMesh != NULL)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("StaticMeshName"), FText::FromString(StaticMeshComp->StaticMesh->GetName()));
				CachedNodeTitle = FText::Format(LOCTEXT("AddStaticMesh", "Add StaticMesh {StaticMeshName}"), Args);
			}
			else if(SkelMeshComp != NULL && SkelMeshComp->SkeletalMesh != NULL)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("SkeletalMeshName"), FText::FromString(SkelMeshComp->SkeletalMesh->GetName()));
				CachedNodeTitle = FText::Format(LOCTEXT("AddSkeletalMesh", "Add SkeletalMesh {SkeletalMeshName}"), Args);
			}
			else if(PSysComp != NULL && PSysComp->Template != NULL)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("ParticleSystemName"), FText::FromString(PSysComp->Template->GetName()));
				CachedNodeTitle = FText::Format(LOCTEXT("AddParticleSystem", "Add ParticleSystem {ParticleSystemName}"), Args);
			}
			else if (SubActorComp && SubActorComp->ChildActorClass)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("ComponentClassName"), FText::FromString(SubActorComp->ChildActorClass->GetName()));
				CachedNodeTitle = FText::Format(LOCTEXT("AddChildActorComponent", "Add ChildActorComponent {ComponentClassName}"), Args);
			}
			else
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("ClassName"), FText::FromString(SourceTemplate->GetClass()->GetName()));
				CachedNodeTitle = FText::Format(LOCTEXT("AddClass", "Add {ClassName}"), Args);
			}
		}
	}

	// FText::Format() is slow, so we cache the title to save on performance
	if (!CachedNodeTitle.IsOutOfDate())
	{
		return CachedNodeTitle;
	}
	return Super::GetNodeTitle(TitleType);
}

FString UK2Node_AddComponent::GetDocumentationLink() const
{
	return TEXT("Shared/GraphNodes/Blueprint/UK2Node_AddComponent");
}

FString UK2Node_AddComponent::GetDocumentationExcerptName() const
{
	UEdGraphPin* TemplateNamePin = GetTemplateNamePin();
	UBlueprint* Blueprint = GetBlueprint();

	if ((TemplateNamePin != NULL) && (Blueprint != NULL))
	{
		FString TemplateName = TemplateNamePin->DefaultValue;
		UActorComponent* SourceTemplate = Blueprint->FindTemplateByName(FName(*TemplateName));

		if (SourceTemplate != NULL)
		{
			return SourceTemplate->GetClass()->GetName();
		}
	}

	return Super::GetDocumentationExcerptName();
}

bool UK2Node_AddComponent::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	return (Blueprint != nullptr) && FBlueprintEditorUtils::IsActorBased(Blueprint) && Super::IsCompatibleWithGraph(Graph);
}

void UK2Node_AddComponent::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	auto TransformPin = GetRelativeTransformPin();
	if (TransformPin && !TransformPin->LinkedTo.Num())
	{
		FString DefaultValue;

		// Try and find the template and get relative transform from it
		UEdGraphPin* TemplateNamePin = GetTemplateNamePinChecked();
		const FString TemplateName = TemplateNamePin->DefaultValue;
		check(CompilerContext.Blueprint);
		USceneComponent* SceneCompTemplate = Cast<USceneComponent>(CompilerContext.Blueprint->FindTemplateByName(FName(*TemplateName)));
		if (SceneCompTemplate)
		{
			FTransform TemplateTransform = FTransform(SceneCompTemplate->RelativeRotation, SceneCompTemplate->RelativeLocation, SceneCompTemplate->RelativeScale3D);
			DefaultValue = TemplateTransform.ToString();
		}

		auto ValuePin = InnerHandleAutoCreateRef(this, TransformPin, CompilerContext, SourceGraph, !DefaultValue.IsEmpty());
		if (ValuePin)
		{
			ValuePin->DefaultValue = DefaultValue;
		}
	}

	if (bHasExposedVariable)
	{
		static FString ObjectParamName = FString(TEXT("Object"));
		static FString ValueParamName = FString(TEXT("Value"));
		static FString PropertyNameParamName = FString(TEXT("PropertyName"));

		UK2Node_AddComponent* NewNode = CompilerContext.SpawnIntermediateNode<UK2Node_AddComponent>(this, SourceGraph); 
		NewNode->SetFromFunction(GetTargetFunction());
		NewNode->AllocateDefaultPinsWithoutExposedVariables();

		// function parameters
		CompilerContext.MovePinLinksToIntermediate(*GetTemplateNamePinChecked(), *NewNode->GetTemplateNamePinChecked());
		CompilerContext.MovePinLinksToIntermediate(*GetRelativeTransformPin(), *NewNode->GetRelativeTransformPin());
		CompilerContext.MovePinLinksToIntermediate(*GetManualAttachmentPin(), *NewNode->GetManualAttachmentPin());

		UEdGraphPin* ReturnPin = NewNode->GetReturnValuePin();
		UEdGraphPin* OriginalReturnPin = GetReturnValuePin();
		check((NULL != ReturnPin) && (NULL != OriginalReturnPin));
		ReturnPin->PinType = OriginalReturnPin->PinType;
		CompilerContext.MovePinLinksToIntermediate(*OriginalReturnPin, *ReturnPin);
		// exec in
		CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *NewNode->GetExecPin());

		UEdGraphPin* LastThen = FKismetCompilerUtilities::GenerateAssignmentNodes( CompilerContext, SourceGraph, NewNode, this, ReturnPin, GetSpawnedType() );

		CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *LastThen);
		BreakAllNodeLinks();
	}
}

#undef LOCTEXT_NAMESPACE
