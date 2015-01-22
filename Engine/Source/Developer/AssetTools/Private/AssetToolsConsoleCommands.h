// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#define LOCTEXT_NAMESPACE "AssetTools"

class FAssetToolsConsoleCommands
{
public:
	const FAssetToolsModule& Module;
	
	//FAutoConsoleCommand CreateCommand;
	
	FAssetToolsConsoleCommands(const FAssetToolsModule& InModule)
		: Module(InModule)
	//,	CreateCommand(
	//	TEXT( "CollectionManager.Create" ),
	//	*LOCTEXT("CommandText_Create", "Creates a collection of the specified name and type").ToString(),
	//	FConsoleCommandWithArgsDelegate::CreateRaw( this, &FCollectionManagerConsoleCommands::Create ) )
	{}

	//void Create(const TArray<FString>& Args)
	//{
	//}
};


#undef LOCTEXT_NAMESPACE
