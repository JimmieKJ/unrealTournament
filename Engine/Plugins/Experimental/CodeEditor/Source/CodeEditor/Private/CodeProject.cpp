// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CodeProject.h"
#include "Misc/Paths.h"


UCodeProject::UCodeProject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Path = FPaths::GameSourceDir();
}
