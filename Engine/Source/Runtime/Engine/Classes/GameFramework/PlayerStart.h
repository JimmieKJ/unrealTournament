// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/NavigationObjectBase.h"
#include "PlayerStart.generated.h"

/** 
 *	This class indicates a location where a player can spawn when the game begins
 *	
 *	@see https://docs.unrealengine.com/latest/INT/Engine/Actors/PlayerStart/
 */
UCLASS(Blueprintable, ClassGroup=Common, hidecategories=Collision)
class ENGINE_API APlayerStart : public ANavigationObjectBase
{
	/** To take more control over PlayerStart selection, you can override the virtual AGameMode::FindPlayerStart and AGameMode::ChoosePlayerStart function */

	GENERATED_UCLASS_BODY()

	/** Used when searching for which playerstart to use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Object)
	FName PlayerStartTag;

	/** Arrow component to indicate forward direction of start */
#if WITH_EDITORONLY_DATA
private_subobject:
	DEPRECATED_FORGAME(4.6, "ArrowComponent should not be accessed directly, please use GetArrowComponent() function instead. ArrowComponent will soon be private and your code will not compile.")
	UPROPERTY()
	class UArrowComponent* ArrowComponent;
public:
#endif

#if WITH_EDITORONLY_DATA
	/** Returns ArrowComponent subobject **/
	class UArrowComponent* GetArrowComponent() const;
#endif
};



