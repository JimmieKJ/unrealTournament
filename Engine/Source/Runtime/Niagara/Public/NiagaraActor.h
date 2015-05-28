// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraActor.generated.h"

UCLASS(MinimalAPI)
class ANiagaraActor : public AActor
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** Pointer to effect component */
	DEPRECATED_FORGAME(4.6, "NiagaraComponent should not be accessed directly, please use GetNiagaraComponent() function instead. NiagaraComponent will soon be private and your code will not compile.")
	UPROPERTY(VisibleAnywhere, Category=NiagaraActor)
	class UNiagaraComponent* NiagaraComponent;

#if WITH_EDITORONLY_DATA
	// Reference to sprite visualization component
	DEPRECATED_FORGAME(4.6, "SpriteComponent should not be accessed directly, please use GetSpriteComponent() function instead. SpriteComponent will soon be private and your code will not compile.")
	UPROPERTY()
	class UBillboardComponent* SpriteComponent;

	// Reference to arrow visualization component
	DEPRECATED_FORGAME(4.6, "ArrowComponent should not be accessed directly, please use GetArrowComponent() function instead. ArrowComponent will soon be private and your code will not compile.")
	UPROPERTY()
	class UArrowComponent* ArrowComponent;

#endif

public:
	/** Returns NiagaraComponent subobject **/
	NIAGARA_API class UNiagaraComponent* GetNiagaraComponent() const;
#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	class UBillboardComponent* GetSpriteComponent() const;
	/** Returns ArrowComponent subobject **/
	class UArrowComponent* GetArrowComponent() const;
#endif

#if WITH_EDITOR
	// AActor interface
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
	// End of AActor interface
#endif // WITH_EDITOR

};
