// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FRpcMessage;
class UScriptStruct;


/**
 * Interface for RPC return values.
 */
class IMessageRpcReturn
{
public:

	/** Cancel the computation of the return value. */
	virtual void Cancel() = 0;

	/**
	 */
	virtual FRpcMessage* CreateResponseMessage() const = 0;

	/**
	 * Gets the type information for the corresponding RPC response message.
	 *
	 * @return The message type information.
	 */
	virtual UScriptStruct* GetResponseTypeInfo() const = 0;

	/**
	 * Check whether the return value is ready.
	 *
	 * @return true if the return value is ready, false otherwise.
	 */
	virtual bool IsReady() const = 0;

public:

	/** Virtual destructor. */
	virtual ~IMessageRpcReturn() { }
};
