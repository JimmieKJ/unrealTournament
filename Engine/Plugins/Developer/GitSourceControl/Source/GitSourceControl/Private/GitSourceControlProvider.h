// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlProvider.h"
#include "IGitSourceControlWorker.h"
#include "GitSourceControlState.h"

DECLARE_DELEGATE_RetVal(FGitSourceControlWorkerRef, FGetGitSourceControlWorker)

// We disable the deprecation warnings here because otherwise it'll complain about us
// implementing RegisterSourceControlStateChanged and UnregisterSourceControlStateChanged.  We know
// that, but we only want it to complain if *others* implement or call these functions.
//
// These macros should be removed when those functions are removed.
PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FGitSourceControlProvider : public ISourceControlProvider
{
public:
	/** Constructor */
	FGitSourceControlProvider() 
		: bGitAvailable(false)
		, bGitRepositoryFound(false)
	{
	}

	/* ISourceControlProvider implementation */
	virtual void Init(bool bForceConnection = true) override;
	virtual void Close() override;
	virtual FText GetStatusText() const override;
	virtual bool IsEnabled() const override;
	virtual bool IsAvailable() const override;
	virtual const FName& GetName(void) const override;
	virtual ECommandResult::Type GetState( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage ) override;
	virtual TArray<FSourceControlStateRef> GetCachedStateByPredicate(const TFunctionRef<bool(const FSourceControlStateRef&)>& Predicate) const override;
	virtual void RegisterSourceControlStateChanged(const FSourceControlStateChanged::FDelegate& SourceControlStateChanged) override;
	virtual void UnregisterSourceControlStateChanged( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged ) override;
	virtual FDelegateHandle RegisterSourceControlStateChanged_Handle(const FSourceControlStateChanged::FDelegate& SourceControlStateChanged) override;
	virtual void UnregisterSourceControlStateChanged_Handle(FDelegateHandle Handle) override;
	virtual ECommandResult::Type Execute(const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete()) override;
	virtual bool CanCancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) const override;
	virtual void CancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) override;
	virtual bool UsesLocalReadOnlyState() const override;
	virtual bool UsesChangelists() const override;
	virtual void Tick() override;
	virtual TArray< TSharedRef<class ISourceControlLabel> > GetLabels( const FString& InMatchingSpec ) const override;
#if SOURCE_CONTROL_WITH_SLATE
	virtual TSharedRef<class SWidget> MakeSettingsWidget() const override;
#endif

	/**
	 * Run a Git "version" command to check the availability of the binary.
	 */
	void CheckGitAvailability();

	/** Is git binary found and working. */
	inline bool IsGitAvailable() const
	{
		return bGitAvailable;
	}

	/** Get the path to the root of the Git repository: can be the GameDir itself, or any parent directory */
	inline const FString& GetPathToRepositoryRoot() const
	{
		return PathToRepositoryRoot;
	}

	/** Git config user.name */
	inline const FString& GetUserName() const
	{
		return UserName;
	}

	/** Git config user.email */
	inline const FString& GetUserEmail() const
	{
		return UserEmail;
	}

	/** Helper function used to update state cache */
	TSharedRef<FGitSourceControlState, ESPMode::ThreadSafe> GetStateInternal(const FString& Filename);

	/**
	 * Register a worker with the provider.
	 * This is used internally so the provider can maintain a map of all available operations.
	 */
	void RegisterWorker( const FName& InName, const FGetGitSourceControlWorker& InDelegate );

private:

	/** Is git binary found and working. */
	bool bGitAvailable;

	/** Is git repository found. */
	bool bGitRepositoryFound;

	/** Helper function for Execute() */
	TSharedPtr<class IGitSourceControlWorker, ESPMode::ThreadSafe> CreateWorker(const FName& InOperationName) const;

	/** Helper function for running command synchronously. */
	ECommandResult::Type ExecuteSynchronousCommand(class FGitSourceControlCommand& InCommand, const FText& Task);
	/** Issue a command asynchronously if possible. */
	ECommandResult::Type IssueCommand(class FGitSourceControlCommand& InCommand);

	/** Output any messages this command holds */
	void OutputCommandMessages(const class FGitSourceControlCommand& InCommand) const;

	/** Path to the root of the Git repository: can be the GameDir itself, or any parent directory (found by the "Connect" operation) */
	FString PathToRepositoryRoot;

	/** Git config user.name (from local repository, else globally) */
	FString UserName;

	/** Git config user.email (from local repository, else globally) */
	FString UserEmail;

	/** Name of the current branch */
	FString BranchName;

	/** State cache */
	TMap<FString, TSharedRef<class FGitSourceControlState, ESPMode::ThreadSafe> > StateCache;

	/** The currently registered source control operations */
	TMap<FName, FGetGitSourceControlWorker> WorkersMap;

	/** Queue for commands given by the main thread */
	TArray < FGitSourceControlCommand* > CommandQueue;

	/** For notifying when the source control states in the cache have changed */
	FSourceControlStateChanged OnSourceControlStateChanged;
};
