// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node.h"
#include "K2Node_GetBlueprintContext.generated.h"

class UBlueprintContextBase;

UCLASS()
class BLUEPRINTCONTEXTEDITOR_API UK2Node_GetBlueprintContext : public UK2Node
{
	GENERATED_BODY()
public:

	virtual void Serialize( FArchive& Ar ) override;

	void Initialize( UClass* NodeClass );
	// Begin UEdGraphNode interface.
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle( ENodeTitleType::Type TitleType ) const override;
	virtual FText GetTooltipText() const override;
	virtual void ExpandNode( class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph ) override;
	virtual bool IsCompatibleWithGraph( const UEdGraph* TargetGraph ) const override;

	// PLK - THIS GOT DEPRECATED, COPY LATEST FROM OTHER GAMES
	/*
	virtual FName GetPaletteIcon( FLinearColor& OutColor ) const override
	{
		OutColor = GetNodeTitleColor();
		return TEXT( "Kismet.AllClasses.FunctionIcon" );
	}
	*/
	// Begin UEdGraphNode interface.

	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual void ReallocatePinsDuringReconstruction( TArray< UEdGraphPin* >& OldPins ) override;
	virtual void GetNodeAttributes( TArray< TKeyValuePair< FString, FString > >& OutNodeAttributes ) const override;
	virtual void GetMenuActions( FBlueprintActionDatabaseRegistrar& ActionRegistrar ) const override;
	virtual FText GetMenuCategory() const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler( class FKismetCompilerContext& CompilerContext ) const override;

	bool IsSpawnVarPin( UEdGraphPin* Pin );

	/** Get the then output pin */
	UEdGraphPin* GetThenPin() const;
	/** Get the blueprint input pin */
	UEdGraphPin* GetClassPin( const TArray< UEdGraphPin* >* InPinsToSearch = nullptr ) const;
	/** Get the world context input pin, can return NULL */
	UEdGraphPin* GetWorldContextPin() const;
	/** Get the result output pin */
	UEdGraphPin* GetResultPin() const;


	virtual bool ShouldDrawCompact() const override { return true; }

protected:

	UPROPERTY()
	TSubclassOf<UBlueprintContextBase> CustomClass;
};