// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FObjectInitializer;

/* 
 * Helper class to manage thread local stack of FObjectInitializers
 * relevant to currently created objects.
 */
class COREUOBJECT_API FTlsObjectInitializers
{
public:
	/**
	* Remove top element from the stack.
	*/
	static void Pop();

	/**
	* Push new FObjectInitializer on stack.
	* @param	Initializer			Object initializer to push.
	*/
	static void Push(FObjectInitializer* Initializer);

	/**
	* Retrieve current FObjectInitializer for current thread.
	* @return Current FObjectInitializer.
	*/
	static FObjectInitializer* Top();

	/**
	 * Retrieves current FObjectInitializer for current thread. Will assert of no ObjectInitializer is currently set.
	 * @return Current FObjectInitializer reference.
	 */
	static FObjectInitializer& TopChecked();

private:
	/**
	* Retrieve thread local stack of FObjectInitializers.
	* @return The stack.
	*/
	static TArray<FObjectInitializer*>* GetTlsObjectInitializers();
};
