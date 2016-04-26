#pragma once

#include "PopulateDialogueWaveFromCharacterSheetCommandlet.generated.h"

UCLASS()
class UPopulateDialogueWaveFromCharacterSheetCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:

	virtual int32 Main(const FString& Params);
};
