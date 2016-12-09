#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "PopulateDialogueWaveFromCharacterSheetCommandlet.generated.h"

UCLASS()
class UPopulateDialogueWaveFromCharacterSheetCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:

	virtual int32 Main(const FString& Params);
};
