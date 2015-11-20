// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IDetailKeyframeHandler
{
public:
	virtual ~IDetailKeyframeHandler(){}

	virtual bool IsPropertyKeyable(UClass* InObjectClass, const class IPropertyHandle& PropertyHandle) const = 0;

	virtual bool IsPropertyKeyingEnabled() const = 0;

	virtual void OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle) = 0;

};
