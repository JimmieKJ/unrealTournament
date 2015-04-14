// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectile.h"
#include "UTProj_LinkPlasma.generated.h"

UCLASS(Abstract)
class UNREALTOURNAMENT_API AUTProj_LinkPlasma : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = LinksUpdated, Category = LinkBolt)
	int32 Links;

public:
	UFUNCTION(BlueprintCallable, Category = LinkBolt)
	virtual void SetLinks(int32 NewLinks);

	UFUNCTION(BlueprintNativeEvent, Category = LinkBolt)
	void LinksUpdated();

	/** extra speed per link */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkBolt)
	float MaxSpeedPerLink;

	/** added to scale (visuals and collision) per link */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkBolt)
	float ExtraScalePerLink;
};
