// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Base class for all SceneCapture actors
 *
 */

#pragma once
#include "SceneCapture.generated.h"

UCLASS(abstract, hidecategories=(Collision, Attachment, Actor), MinimalAPI)
class ASceneCapture : public AActor
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** To display the 3d camera in the editor. */
	DEPRECATED_FORGAME(4.6, "MeshComp should not be accessed directly, please use GetMeshComp() function instead. MeshComp will soon be private and your code will not compile.")
	UPROPERTY()
	class UStaticMeshComponent* MeshComp;

public:
	/** Returns MeshComp subobject **/
	ENGINE_API class UStaticMeshComponent* GetMeshComp() const;
};



