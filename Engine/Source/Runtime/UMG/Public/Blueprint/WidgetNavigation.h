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
	TEnumAsByte<EUINavigationRule> Rule;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
	FName WidgetToFocus;
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

	EUINavigationRule GetNavigationRule(EUINavigation Nav)
	{
		switch (Nav)
		{
		case EUINavigation::Up:
			return Up.Rule;
		case EUINavigation::Down:
			return Down.Rule;
		case EUINavigation::Left:
			return Left.Rule;
		case EUINavigation::Right:
			return Right.Rule;
		}
		return EUINavigationRule::Escape;
	}

	void UpdateMetaData(TSharedRef<FNavigationMetaData> MetaData)
	{
		UpdateMetaDataEntry(MetaData, Up, EUINavigation::Up);
		UpdateMetaDataEntry(MetaData, Down, EUINavigation::Down);
		UpdateMetaDataEntry(MetaData, Left, EUINavigation::Left);
		UpdateMetaDataEntry(MetaData, Right, EUINavigation::Right);
	}

private:

	void UpdateMetaDataEntry(TSharedRef<FNavigationMetaData> MetaData, const FWidgetNavigationData & NavData, EUINavigation Nav)
	{
		switch (NavData.Rule)
		{
			case EUINavigationRule::Escape:
				MetaData->SetNavigationEscape(Nav);
				break;
			case EUINavigationRule::Stop:
				MetaData->SetNavigationStop(Nav);
				break;
			case EUINavigationRule::Wrap:
				MetaData->SetNavigationWrap(Nav);
				break;
		}
	}
};
