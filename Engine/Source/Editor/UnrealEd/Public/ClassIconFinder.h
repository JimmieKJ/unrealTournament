// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAssetData;

class FClassIconFinder
{
public:
	/** Registers a new style set to use to try to find icons. */
	UNREALED_API static void RegisterIconSource(const ISlateStyle* StyleSet);

	/** Unregisters an style set. */
	UNREALED_API static void UnregisterIconSource(const ISlateStyle* StyleSet);

	/** Find the best fitting small icon to use for the supplied actor array */
	UNREALED_API static const FSlateBrush* FindIconForActors(const TArray< TWeakObjectPtr<AActor> >& InActors, UClass*& CommonBaseClass);

	/** Find the small icon to use for the supplied actor */
	UNREALED_API static const FSlateBrush* FindIconForActor( const TWeakObjectPtr<AActor>& InActor );
	
	/** Find the small icon name to use for the supplied actor */
	UNREALED_API static FName FindIconNameForActor( const TWeakObjectPtr<AActor>& InActor );

	/** Find the small icon to use for the supplied class */
	UNREALED_API static const FSlateBrush* FindIconForClass(const UClass* InClass, const FName& InDefaultName = FName() );

	/** Find the small icon name to use for the supplied class */
	UNREALED_API static FName FindIconNameForClass(const UClass* InClass, const FName& InDefaultName = FName() );

	/** Find the large thumbnail name to use for the supplied class */
	UNREALED_API static const FSlateBrush* FindThumbnailForClass(const UClass* InClass, const FName& InDefaultName = FName() );

	/** Find the large thumbnail name to use for the supplied class */
	UNREALED_API static FName FindThumbnailNameForClass(const UClass* InClass, const FName& InDefaultName = FName() );

	/** Utility function to convert a Blueprint into the most suitable class possible for use by the icon finder */
	UNREALED_API static const UClass* GetIconClassForBlueprint(const UBlueprint* InBlueprint);

	/** Utility function to convert an asset into the most suitable class possible for use by the icon finder */
	UNREALED_API static const UClass* GetIconClassForAssetData(const FAssetData& InAssetData, bool* bOutIsClassType = nullptr);

private:
	/** Find a thumbnail/icon name to use for the supplied class */
	UNREALED_API static FName FindIconNameImpl(const UClass* InClass, const FName& InDefaultName = FName(), const TCHAR* StyleRoot = TEXT("ClassIcon") );

private:
	UNREALED_API static const FSlateBrush* LookupBrush(FName IconName);

	/** The available style sets to source icons from */
	static TArray < const ISlateStyle* > Styles;
};
