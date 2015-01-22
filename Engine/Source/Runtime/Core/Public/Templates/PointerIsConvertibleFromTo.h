// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Tests if a From* is convertible to a To*
 **/
template <typename From, typename To>
struct TPointerIsConvertibleFromTo
{
private:
	static uint8  Test(...);
	static uint16 Test(To*);

public:
	enum { Value  = sizeof(Test((From*)nullptr)) - 1 };
};
