// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IStructBoxModule.h"

class FStructBoxModule : public IStructBoxModuleInterface
{

};

IMPLEMENT_MODULE(FStructBoxModule, StructBox);
