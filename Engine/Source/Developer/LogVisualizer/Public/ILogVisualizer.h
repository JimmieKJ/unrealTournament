// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class ILogVisualizer
{
public:
	/** Virtual destructor */
	virtual ~ILogVisualizer() {}

	/** spawns LogVisualizer's UI */
	virtual void SummonUI(UWorld* InWorld) = 0;

	/** closes LogVisualizer's UI */
	virtual void CloseUI(UWorld* InWorld) = 0;

	/** checks if we have LogVisualizer UI opened for given World */
	virtual bool IsOpenUI(UWorld* InWorld) = 0;

	/** Helper actor used to render data in given world */
	virtual class AActor* GetHelperActor(class UWorld* InWorld) = 0;
};