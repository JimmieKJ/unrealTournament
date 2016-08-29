// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "EdGraphToken.h"
#include "Kismet2/CompilerResultsLog.h"

TSharedRef<IMessageToken> FEdGraphToken::Create(const UObject* InObject, const FCompilerResultsLog* Log, UEdGraphNode*& OutSourceNode)
{
	const UObject* SourceNode = Log->FindSourceObject(InObject);
	if (OutSourceNode == nullptr)
	{
		OutSourceNode = const_cast<UEdGraphNode*>(Cast<UEdGraphNode>(SourceNode));
	}
	return MakeShareable(new FEdGraphToken(Log->FindSourceObject(InObject), nullptr));
}

TSharedRef<IMessageToken> FEdGraphToken::Create(const UEdGraphPin* InPin, const FCompilerResultsLog* Log, UEdGraphNode*& OutSourceNode)
{
	const UObject* SourceNode = InPin ? Log->FindSourceObject(InPin->GetOwningNode()) : nullptr;
	if (OutSourceNode == nullptr)
	{
		OutSourceNode = const_cast<UEdGraphNode*>(Cast<UEdGraphNode>(SourceNode));
	}
	return MakeShareable(new FEdGraphToken(SourceNode, Log->FindSourcePin(InPin)));
}

const UEdGraphPin* FEdGraphToken::GetPin() const
{
	return PinBeingReferenced.Get();
}

const UObject* FEdGraphToken::GetGraphObject() const
{
	return ObjectBeingReferenced.Get();
}

FEdGraphToken::FEdGraphToken(const UObject* InObject, const UEdGraphPin* InPin)
	: ObjectBeingReferenced(InObject)
	, PinBeingReferenced(InPin)
{
	if (InPin)
	{
		CachedText = FText::FromString(InPin->GetName());
	}
	else if (InObject)
	{
		CachedText = FText::FromString(InObject->GetName());
	}
	else
	{
		CachedText = NSLOCTEXT("MessageLog", "NoneObjectToken", "<None>");
	}
}
