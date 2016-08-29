// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAnalyticsProvider.h"

/** ET specific analytics provider instance. Exposes additional APIs to support Json-based events. */
class IAnalyticsProviderET : public IAnalyticsProvider
{
public:
	using IAnalyticsProvider::RecordEvent;
	using IAnalyticsProvider::StartSession;

	/**
	* Optimization for StartSession that avoids the array copy using rvalue references.
	*
	* @param AttributesJson	array of key/value attribute pairs
	*/
	virtual bool StartSession(TArray<FAnalyticsEventAttribute>&& Attributes) = 0;

	/**
	* Optimization for RecordEvent that avoids the array copy using rvalue references.
	*
	* @param EventName			The name of the event.
	* @param AttributesJson	array of key/value attribute pairs
	*/
	virtual void RecordEvent(FString EventName, TArray<FAnalyticsEventAttribute>&& Attributes) = 0;

	/**
	* Sends an event where each attribute value is expected to be a string-ified Json value.
	* Meaning, each attribute value can be an integer, float, bool, string,
	* arbitrarily complex Json array, or arbitrarily complex Json object.
	*
	* The main thing to remember is that if you pass a Json string as an attribute value, it is up to you to
	* quote the string, as the string you pass is expected to be able to be pasted directly into a Json value. ie:
	*
	* {
	*     "EventName": "MyStringEvent",
	*     "IntAttr": 42                 <--- You simply pass this in as "42"
	*     "StringAttr": "SomeString"    <--- You must pass SomeString as "\"SomeString\""
	* }
	*
	* @param EventName			The name of the event.
	* @param AttributesJson	array of key/value attribute pairs where each value is a Json value (pure Json strings mustbe quoted by the caller).
	*/
	virtual void RecordEventJson(FString EventName, TArray<FAnalyticsEventAttribute>&& AttributesJson) = 0;

	/**
	* Helper for RecordEventJson when the array is not an rvalue reference.
	*
	* @param EventName			The name of the event.
	* @param AttributesJson	array of key/value attribute pairs where each value is a Json value (pure Json strings mustbe quoted by the caller).
	*/
	void RecordEventJson(FString EventName, const TArray<FAnalyticsEventAttribute>& AttributesJson)
	{
		// make a copy of the array if it's not an rvalue reference
		RecordEventJson(MoveTemp(EventName), TArray<FAnalyticsEventAttribute>(AttributesJson));
	}

	/**
	* When set, all events recorded will have these attributes appended to them.
	*
	* @param Attributes array of attributes that should be appended to every event.
	*/
	virtual void SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes) = 0;
};
