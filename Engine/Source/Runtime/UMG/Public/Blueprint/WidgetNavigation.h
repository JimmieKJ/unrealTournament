// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationReply.h"
#include "NavigationMetaData.h"

#include "WidgetNavigation.generated.h"

/**
 *
 */
USTRUCT()
struct UMG_API FWidgetNavigationData
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	EUINavigationRule Rule;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FName WidgetToFocus;

	TWeakObjectPtr<UWidget> Widget;
};

/**
 * 
 */
UCLASS()
class UMG_API UWidgetNavigation : public UObject
{
	GENERATED_UCLASS_BODY()
	
public:
	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FWidgetNavigationData Up;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FWidgetNavigationData Down;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FWidgetNavigationData Left;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FWidgetNavigationData Right;

public:

#if WITH_EDITOR

	/**  */
	FWidgetNavigationData& GetNavigationData(EUINavigation Nav);

	/**  */
	EUINavigationRule GetNavigationRule(EUINavigation Nav);

#endif

	void ResolveExplictRules(class UWidgetTree* WidgetTree);

	/** Updates a slate metadata object to match this configured navigation ruleset. */
	void UpdateMetaData(TSharedRef<FNavigationMetaData> MetaData);

	/** @return true if the configured navigation object is the same as an un-customized navigation rule set. */
	bool IsDefault() const;

private:

	void UpdateMetaDataEntry(TSharedRef<FNavigationMetaData> MetaData, const FWidgetNavigationData & NavData, EUINavigation Nav);
};
