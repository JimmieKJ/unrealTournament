// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "Layers/Layer.h"

ULayer::ULayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LayerName( NAME_None )
	, bIsVisible( true )
{
}
