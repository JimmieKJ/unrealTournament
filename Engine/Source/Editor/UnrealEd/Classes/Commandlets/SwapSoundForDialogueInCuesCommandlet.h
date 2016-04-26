#pragma once

#include "SwapSoundForDialogueInCuesCommandlet.generated.h"

UCLASS()
class USwapSoundForDialogueInCuesCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:

	virtual int32 Main(const FString& Params);
};
