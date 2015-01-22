// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

template<typename ValueType>
struct FTestLogger
{
	TArray<ValueType> ExpectedValues;
	TArray<ValueType> LoggedValues;
	FAutomationTestBase* TestRunner;

	FTestLogger() : TestRunner(nullptr)
	{

	}

	void Log(const ValueType& Value)
	{
		LoggedValues.Add(Value);

		if (ExpectedValues.Num() > 0 && TestRunner != nullptr)
		{
			if (LoggedValues.Num() <= ExpectedValues.Num())
			{
				TestRunner->TestEqual("Logged value different then expected!", LoggedValues.Top(), ExpectedValues[LoggedValues.Num() - 1]);
			}
			else
			{
				TestRunner->TestTrue("Logged more values than expected!", false);
			}
		}
	}
};
