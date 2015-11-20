// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "GameFramework/Info.h"
#include "WindDirectionalSource.generated.h"

/** Actor that provides a directional wind source. Only affects SpeedTree assets. */
UCLASS(ClassGroup=Wind, showcategories=(Rendering, "Utilities|Transformation"))
class ENGINE_API AWindDirectionalSource : public AInfo
{
	GENERATED_UCLASS_BODY()

private_subobject:
	DEPRECATED_FORGAME(4.6, "Component should not be accessed directly, please use GetComponent() function instead. Component will soon be private and your code will not compile.")
	UPROPERTY(Category = WindDirectionalSource, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UWindDirectionalSourceComponent* Component;

#if WITH_EDITORONLY_DATA
	DEPRECATED_FORGAME(4.6, "ArrowComponent should not be accessed directly, please use GetArrowComponent() function instead. ArrowComponent will soon be private and your code will not compile.")
	UPROPERTY()
	class UArrowComponent* ArrowComponent;
#endif

public:
	/** Returns Component subobject **/
	class UWindDirectionalSourceComponent* GetComponent() const;

#if WITH_EDITORONLY_DATA
	/** Returns ArrowComponent subobject **/
	class UArrowComponent* GetArrowComponent() const;
#endif
};



