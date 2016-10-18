// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FFileLineException
{
	FString Message;
	FString Filename;
	int32   Line;

	template <typename... Types>
	FUNCTION_NO_RETURN_START
		static void VARARGS Throwf(FString&& Filename, int32 Line, const TCHAR* Fmt, Types... Args)
	FUNCTION_NO_RETURN_END
	{
		static_assert(TAnd<TIsValidVariadicFunctionArg<Types>...>::Value, "Invalid argument(s) passed to FError::Throwf");

		ThrowfImpl(MoveTemp(Filename), Line, Fmt, Args...);
	}

private:
	FUNCTION_NO_RETURN_START
	static void VARARGS ThrowfImpl(FString&& Filename, int32 Line, const TCHAR* Fmt, ...)
	FUNCTION_NO_RETURN_END;

	FFileLineException(FString&& InMessage, FString&& InFilename, int32 InLine);
};
