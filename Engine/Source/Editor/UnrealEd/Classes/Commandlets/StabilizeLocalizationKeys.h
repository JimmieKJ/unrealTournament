#pragma once

#include "StabilizeLocalizationKeys.generated.h"

UCLASS()
class UStabilizeLocalizationKeysCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:

	virtual int32 Main(const FString& Params);
};
