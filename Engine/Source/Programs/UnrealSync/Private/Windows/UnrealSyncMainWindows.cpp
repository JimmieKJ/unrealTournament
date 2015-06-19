// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../UnrealSync.h"

/**
 * WinMain, called when the application is started
 */
int WINAPI WinMain(_In_ HINSTANCE hInInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR, _In_ int nCmdShow)
{
	hInstance = hInInstance;

	FUnrealSync::RunUnrealSync(GetCommandLineW());

	return 0;
}