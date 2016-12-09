// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintContextEditor.h"

#include "K2Node_GetBlueprintContext.h"

#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintEditorUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

#include "BlueprintContextBase.h"
#include "BlueprintContextLibrary.h"

#include "K2Node_CallFunction.h"

void UK2Node_GetBlueprintContext::Serialize( FArchive& Ar )
{
	if ( Ar.IsSaving() )
	{
		if ( auto ClassNode = GetClassPin() )
		{
			CustomClass = Cast<UClass>( ClassNode->DefaultObject );
		}
	}

	Super::Serialize( Ar );
}

void UK2Node_GetBlueprintContext::Initialize( UClass* NodeClass )
{
	CustomClass = NodeClass;
}

void UK2Node_GetBlueprintContext::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// If required add the world context pin
	if ( GetBlueprint()->ParentClass->HasMetaDataHierarchical( FBlueprintMetadata::MD_ShowWorldContextPin ) )
	{
		CreatePin( EGPD_Input, K2Schema->PC_Object, TEXT( "" ), UObject::StaticClass(), false, false, TEXT( "WorldContext" ) );
	}

	// Add blueprint pin
	if ( !CustomClass )
	{
		UEdGraphPin* ClassPin =
		    CreatePin( EGPD_Input, K2Schema->PC_Class, TEXT( "" ), UBlueprintContextBase::StaticClass(), false, false, TEXT( "Class" ) );
	}

	// Result pin
	UEdGraphPin* ResultPin = CreatePin( EGPD_Output, K2Schema->PC_Object, TEXT( "" ),
	                                    CustomClass ? (UClass*)CustomClass : UBlueprintContextBase::StaticClass(), false, false, K2Schema->PN_ReturnValue );

	Super::AllocateDefaultPins();
}

FLinearColor UK2Node_GetBlueprintContext::GetNodeTitleColor() const
{
	return FLinearColor( 1.f, 0.078f, 0.576f, 1.f );
}

void UK2Node_GetBlueprintContext::ExpandNode( class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph )
{
	Super::ExpandNode( CompilerContext, SourceGraph );

	static FName Get_FunctionName = GET_FUNCTION_NAME_CHECKED( UBlueprintContextLibrary, GetContext );

	static FString WorldContextObject_ParamName = FString( TEXT( "ContextObject" ) );
	static FString Class_ParamName              = FString( TEXT( "Class" ) );

	UK2Node_GetBlueprintContext* GetContextNode = this;
	UEdGraphPin* SpawnWorldContextPin           = GetContextNode->GetWorldContextPin();
	UEdGraphPin* SpawnClassPin                  = GetContextNode->GetClassPin();
	UEdGraphPin* SpawnNodeResult                = GetContextNode->GetResultPin();

	UClass* SpawnClass = ( SpawnClassPin != nullptr ) ? Cast<UClass>( SpawnClassPin->DefaultObject ) : nullptr;
	if ( SpawnClassPin && ( SpawnClassPin->LinkedTo.Num() == 0 ) && !SpawnClass )
	{
		CompilerContext.MessageLog.Error(
		    *NSLOCTEXT( "BlueprintContext", "GetBlueprintContext_Error", "Node @@ must have a class specified." ).ToString(), GetContextNode );
		// we break exec links so this is the only error we get, don't want the CreateWidget node being considered and giving 'unexpected node' type
		// warnings
		GetContextNode->BreakAllNodeLinks();
		return;
	}

	//////////////////////////////////////////////////////////////////////////
	// create 'UWidgetBlueprintLibrary::Create' call node
	UK2Node_CallFunction* CallGetNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>( GetContextNode, SourceGraph );
	CallGetNode->FunctionReference.SetExternalMember( Get_FunctionName, UBlueprintContextLibrary::StaticClass() );
	CallGetNode->AllocateDefaultPins();

	UEdGraphPin* CallCreateWorldContextPin = CallGetNode->FindPinChecked( WorldContextObject_ParamName );
	UEdGraphPin* CallCreateClassTypePin    = CallGetNode->FindPinChecked( Class_ParamName );
	UEdGraphPin* CallCreateResult          = CallGetNode->GetReturnValuePin();

	if ( SpawnClassPin && SpawnClassPin->LinkedTo.Num() > 0 )
	{
		// Copy the 'blueprint' connection from the spawn node to 'UWidgetBlueprintLibrary::Create'

		CompilerContext.MovePinLinksToIntermediate( *SpawnClassPin, *CallCreateClassTypePin );
	}
	else
	{
		// Copy blueprint literal onto 'UWidgetBlueprintLibrary::Create' call
		CallCreateClassTypePin->DefaultObject = *CustomClass;
	}

	// Copy the world context connection from the spawn node to 'UWidgetBlueprintLibrary::Create' if necessary
	if ( SpawnWorldContextPin )
	{
		CompilerContext.MovePinLinksToIntermediate( *SpawnWorldContextPin, *CallCreateWorldContextPin );
	}

	// Move result connection from spawn node to 'UWidgetBlueprintLibrary::Create'
	CallCreateResult->PinType = SpawnNodeResult->PinType; // Copy type so it uses the right actor subclass
	CompilerContext.MovePinLinksToIntermediate( *SpawnNodeResult, *CallCreateResult );

	//////////////////////////////////////////////////////////////////////////
	// create 'set var' nodes

	// Break any links to the expanded node
	GetContextNode->BreakAllNodeLinks();
}

bool UK2Node_GetBlueprintContext::IsCompatibleWithGraph( const UEdGraph* TargetGraph ) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph( TargetGraph );
	return Super::IsCompatibleWithGraph( TargetGraph ) &&
	       ( !Blueprint || FBlueprintEditorUtils::FindUserConstructionScript( Blueprint ) != TargetGraph );
}

void UK2Node_GetBlueprintContext::ReallocatePinsDuringReconstruction( TArray<UEdGraphPin*>& OldPins )
{
	AllocateDefaultPins();

	if ( CustomClass )
	{
		UEdGraphPin* ResultPin                  = GetResultPin();
		ResultPin->PinType.PinSubCategoryObject = *CustomClass;
	}
}

void UK2Node_GetBlueprintContext::GetNodeAttributes( TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes ) const
{
	const FString ClassToSpawnStr = CustomClass ? CustomClass->GetName() : TEXT( "InvalidClass" );
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Type" ), TEXT( "GetBlueprintContext" ) ) );
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Class" ), GetClass()->GetName() ) );
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Name" ), GetName() ) );
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "ObjectClass" ), ClassToSpawnStr ) );
}

void UK2Node_GetBlueprintContext::GetMenuActions( FBlueprintActionDatabaseRegistrar& ActionRegistrar ) const
{
	static TArray<UClass*> Subclasses;
	Subclasses.Reset();
	GetDerivedClasses( UBlueprintContextBase::StaticClass(), Subclasses );

	auto CustomizeCallback = []( UEdGraphNode* Node, bool bIsTemplateNode, UClass* Subclass )
	{
		auto TypedNode = CastChecked<UK2Node_GetBlueprintContext>( Node );
		TypedNode->Initialize( Subclass );
	};

	UClass* ActionKey = GetClass();

	if ( ActionRegistrar.IsOpenForRegistration( ActionKey ) )
	{
		for ( auto& Iter : Subclasses )
		{
			UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create( GetClass() );
			check( Spawner );

			Spawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic( CustomizeCallback, Iter );
			ActionRegistrar.AddBlueprintAction( ActionKey, Spawner );
		}
	}
}

FText UK2Node_GetBlueprintContext::GetMenuCategory() const
{
	return FText::FromString( TEXT( "Blueprint Context" ) );
}

class FNodeHandlingFunctor* UK2Node_GetBlueprintContext::CreateNodeHandler( class FKismetCompilerContext& CompilerContext ) const
{
	return new FNodeHandlingFunctor( CompilerContext );
}

bool UK2Node_GetBlueprintContext::IsSpawnVarPin( UEdGraphPin* Pin )
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	return ( Pin->PinName != K2Schema->PN_Execute && Pin->PinName != K2Schema->PN_Then && Pin->PinName != K2Schema->PN_ReturnValue &&
	         Pin->PinName != TEXT( "Class" ) && Pin->PinName != TEXT( "WorldContext" ) );
}

UEdGraphPin* UK2Node_GetBlueprintContext::GetThenPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPinChecked( K2Schema->PN_Then );
	check( Pin->Direction == EGPD_Output );
	return Pin;
}

UEdGraphPin* UK2Node_GetBlueprintContext::GetWorldContextPin() const
{
	UEdGraphPin* Pin = FindPin( TEXT( "WorldContext" ) );
	check( Pin == NULL || Pin->Direction == EGPD_Input );
	return Pin;
}

UEdGraphPin* UK2Node_GetBlueprintContext::GetResultPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPinChecked( K2Schema->PN_ReturnValue );
	check( Pin->Direction == EGPD_Output );
	return Pin;
}

FText UK2Node_GetBlueprintContext::GetNodeTitle( ENodeTitleType::Type TitleType ) const
{
	switch ( TitleType )
	{
		case ENodeTitleType::FullTitle:
			if ( CustomClass )
			{
				const FString& ClassName = CustomClass->GetName();
				const int32 Position = ClassName.Find( TEXT( "Context" ) );
				return FText::FromString( ClassName.Left( Position ) );
			}
			else
			{
				return NSLOCTEXT( "BlueprintContext", "Context", "Context" );
			}
		default: return GetTooltipText();
	}
}

FText UK2Node_GetBlueprintContext::GetTooltipText() const
{
	static const auto DefaultText = NSLOCTEXT( "BlueprintContext", "Get Blueprint Context", "Get Blueprint Context" );
	static const auto TypedText   = NSLOCTEXT( "BlueprintContext", "Get {ClassName}", "Get {ClassName}" );

	if ( CustomClass )
	{
		return FText::FormatNamed( TypedText, TEXT( "ClassName" ), FText::FromString( CustomClass->GetName() ) );
	}

	return DefaultText;
}

UEdGraphPin* UK2Node_GetBlueprintContext::GetClassPin( const TArray<UEdGraphPin*>* InPinsToSearch /*= NULL */ ) const
{
	const TArray<UEdGraphPin*>* PinsToSearch = InPinsToSearch ? InPinsToSearch : &Pins;

	UEdGraphPin* Pin = NULL;
	for ( auto PinIt = PinsToSearch->CreateConstIterator(); PinIt; ++PinIt )
	{
		UEdGraphPin* TestPin = *PinIt;
		if ( TestPin && TestPin->PinName == TEXT( "Class" ) )
		{
			Pin = TestPin;
			break;
		}
	}
	
	return Pin;
}
