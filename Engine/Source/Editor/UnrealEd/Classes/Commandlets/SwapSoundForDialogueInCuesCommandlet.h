#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "SwapSoundForDialogueInCuesCommandlet.generated.h"

UCLASS()
class USwapSoundForDialogueInCuesCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:

	virtual int32 Main(const FString& Params);
};
