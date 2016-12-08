// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTeamInterface.h"
#include "UTTeamDeco.generated.h"

UCLASS(Blueprintable)
class UNREALTOURNAMENT_API AUTTeamDeco : public AActor, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TeamDeco)
		bool bBlockTeamProjectiles;

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "GetTeamNum"))
		uint8 ScriptGetTeamNum();

	virtual uint8 GetTeamNum() const
	{
		static FName NAME_ScriptGetTeamNum(TEXT("ScriptGetTeamNum"));
		UFunction* GetTeamNumFunc = GetClass()->FindFunctionByName(NAME_ScriptGetTeamNum);
		if (GetTeamNumFunc != NULL && GetTeamNumFunc->Script.Num() > 0)
		{
			return IUTTeamInterface::Execute_ScriptGetTeamNum(const_cast<AUTTeamDeco*>(this)); 
		}
		return 0;
	}

	void SetTeamForSideSwap_Implementation(uint8 NewTeamNum)
	{}
};

