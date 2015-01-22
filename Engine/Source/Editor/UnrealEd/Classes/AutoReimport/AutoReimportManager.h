// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AutoReimportManager.generated.h"

/* Deals with auto reimporting of objects when the objects file on disk is modified*/
UCLASS(config=Editor, transient)
class UNREALED_API UAutoReimportManager : public UObject, public FTickableEditorObject
{
	GENERATED_UCLASS_BODY()

public:

	/* Initialize during engine startup */
	void Initialize();

	// FTickableEditorObject interface
	/* Tick will reimport files if any are modified and enough delta time has elapsed*/
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override;
	// End of FTickableEditorObject interface

	// Begin UObject Interface
	virtual void BeginDestroy(void) override;
	// End UObject Interface

	/** Delegate to process a auto reimport*/
	DECLARE_DELEGATE_RetVal_TwoParams( bool, FAutoReimportDelegate, UObject*, const FString&);

	/** Add a handler for reimporting custom file types, ex AddReimportDelegate("xml", FAutoReimportDelegate::CreateStatic(&MyImporter::AutoReimportStuff))  */
	void AddReimportDelegate(const FString& FileType, const FAutoReimportDelegate& Delegate);

	/** Remove a handler for reimporting a custom file type. */
	void RemoveReimportDelegate( const FString& FileType, const FAutoReimportDelegate& Delegate );

	/** Reimport the file */
	static bool ReimportFile(UObject* Obj, const FString File);

	/* Determine if the filename matches the name of the object stored in the full path. Ignores the paths. */
	static bool JustFileNameMatches(const FString& ObjectPath, const FString& FileName);

	/* Determine if both full paths match*/
	static bool IsSamePath(const FString& ObjectPath, const FString& FileName);

private:

	/* The auto reimporters that handle the details for specific Object types */
	typedef TArray<FAutoReimportDelegate>	ReimportDelegates;
	
	/** Called when a file in a content directory changes on disk */
	void OnDirectoryChanged (const TArray<struct FFileChangeData>& Files);

	/* Called when a file in one of the watched directories, with a extension we care about, is modified */
	bool OnFileModified(ReimportDelegates& Delegate, const FString& File );

	/** Callback for when an editor user setting has changed */
	void HandleLoadingSavingSettingChanged(FName PropertyName);

	/*Takes a path entered by user, and fixes path separators, adds missing separator at end etc.*/
	static FString FixUserPath( FString File );

	/* Activate auto reimport for the given directory */
	void ActivateAutoReimport( const FString& Directory);

	/* Deactivate auto reimport for the given directory */
	void DeactivateAutoReimport( const FString& Directory );

	//the directories currently being watched
	struct FDirectories
	{
		/*Add a directory to watch */
		void Add(UAutoReimportManager* AutoReimportManager, const FString& Directory);
		/*Remove a watched directory */
		void Remove(UAutoReimportManager* AutoReimportManager,const FString& Directory);
	private:
		struct FDirectory
		{
			FDirectory(){Count = 0;}
			FDirectory(const FString& InPath):Path(InPath), Count(1){}
			FString		Path;
			int			Count;
		};

		TArray<FDirectory>		Paths;
	};

	/* Maps file extension to the auto reimporter */
	TMap<FString, ReimportDelegates> ExtensionToReimporter;

	/** Accumulated time that the reimport manager has been running*/
	double ReimportTime;

	/* Paths user has specified to watch in preferences */
	TArray<FString> AutoReimportDirectories;

	/** The files that have been modified recently, stores filename and the last time it was modified*/
	TMap<FString, double> ModifiedFiles;

	/*The Directories we are currently watching */
	FDirectories WatchedDirectories;

	//////////////////////////////////////////////////////////////////////////
	// Auto Reimport handlers

	/*Reimport a Curve*/
	static bool AutoReimportCurve(UObject* Object, const FString& FileName);

	/*Reimport a Curve Table*/
	static bool AutoReimportCurveTable(UObject* Object, const FString& FileName);

	/*Reimport a Data Table*/
	static bool AutoReimportDataTable(UObject* Object, const FString& FileName);

	/*Reimport a Texture*/
	static bool AutoReimportTexture(UObject* Object, const FString& FileName);
};
