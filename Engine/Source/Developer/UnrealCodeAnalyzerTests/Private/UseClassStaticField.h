// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FTestClassWithStaticField
{
public:
	static int StaticField;
};

int FTestClassWithStaticField::StaticField = 5;
