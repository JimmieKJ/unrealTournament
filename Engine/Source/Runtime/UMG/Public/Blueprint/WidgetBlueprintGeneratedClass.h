// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/BlueprintGeneratedClass.h"
#include "DynamicPropertyPath.h"

#include "WidgetBlueprintGeneratedClass.generated.h"

class UMovieScene;
class UStructProperty;
class UUserWidget;
class UWidgetAnimation;

UENUM()
enum class EBindingKind : uint8
{
	Function,
	Property
};

USTRUCT()
struct FDelegateRuntimeBinding
{
	GENERATED_USTRUCT_BODY()

	/** The widget that will be bound to the live data. */
	UPROPERTY()
	FString ObjectName;

	/** The property on the widget that will have a binding placed on it. */
	UPROPERTY()
	FName PropertyName;

	/** The function or property we're binding to on the source object. */
	UPROPERTY()
	FName FunctionName;

	/**  */
	UPROPERTY()
	FDynamicPropertyPath SourcePath;

	/** The kind of binding we're performing, are we binding to a property or a function. */
	UPROPERTY()
	EBindingKind Kind;
};


/**
 * The widget blueprint generated class allows us to create blueprint-able widgets for UMG at runtime.
 * All WBPGC's are of UUserWidget classes, and they perform special post initialization using this class
 * to give themselves many of the same capabilities as AActor blueprints, like dynamic delegate binding for
 * widgets.
 */
UCLASS()
class UMG_API UWidgetBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_UCLASS_BODY()

public:

	/** A tree of the widget templates to be created */
	UPROPERTY()
	class UWidgetTree* WidgetTree;

	UPROPERTY()
	TArray< FDelegateRuntimeBinding > Bindings;

	UPROPERTY()
	TArray< UWidgetAnimation* > Animations;

	UPROPERTY()
	TArray< FName > NamedSlots;

	UPROPERTY()
	uint32 bCanEverTick : 1;

	UPROPERTY()
	uint32 bCanEverPaint : 1;

public:

	virtual void PostLoad() override;

	/**
	 * This is the function that makes UMG work.  Once a user widget is constructed, it will post load
	 * call into its generated class and ask to be initialized.  The class will perform all the delegate
	 * binding and wiring necessary to have the user's widget perform as desired.
	 */
	void InitializeWidget(UUserWidget* UserWidget) const;

	static void InitializeWidgetStatic(UUserWidget* UserWidget
		, const UClass* InClass
		, bool InCanEverTick
		, bool InCanEverPaint
		, UWidgetTree* InWidgetTree
		, const TArray< UWidgetAnimation* >& InAnimations
		, const TArray< FDelegateRuntimeBinding >& InBindings);
};