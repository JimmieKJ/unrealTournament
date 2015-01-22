// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////

/** A terminal in the graph (literal or variable reference) */
struct FBPTerminal
{
	FString Name;
	FEdGraphPinType Type;
	bool bIsLiteral;
	bool bIsLocal; // @TODO: Should be an enumeration or driven from the var property
	bool bIsConst;
	bool bIsSavePersistent;

	bool bIsStructContext;
	bool bPassedByReference;

	// Source node (e.g., pin)
	UObject* Source;

	// Context->
	FBPTerminal* Context;

	// For non-literal terms, this is the UProperty being referenced (in the stack if bIsLocal set, or on the context otherwise)
	UProperty* AssociatedVarProperty;

	/** Pointer to an object literal */
	UObject* ObjectLiteral;

	/** The FText literal */
	FText TextLiteral;

	/** String representation of the default value of the property associated with this term (or path to object) */
	FString PropertyDefault;

	FBPTerminal()
		: bIsLiteral(false)
		, bIsLocal(false)
		, bIsConst(false)
		, bIsSavePersistent(false)
		, bIsStructContext(false)
		, bPassedByReference(false)
		, Source(NULL)
		, Context(NULL)
		, AssociatedVarProperty(NULL)
		, ObjectLiteral(NULL)
	{
	}

	KISMETCOMPILER_API void CopyFromPin(UEdGraphPin* Net, const FString& NewName);

	bool IsTermWritable() const
	{
		return !bIsLiteral && !bIsConst;
	}
};

//////////////////////////////////////////////////////////////////////////
