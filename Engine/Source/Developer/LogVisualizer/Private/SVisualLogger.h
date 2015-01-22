// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "VisualLogger/VisualLogger.h"

class SVisualLoggerFilters;
class SVisualLoggerView;
class SVisualLoggerLogsList;
class SVisualLoggerStatusView;

class SVisualLogger : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SVisualLogger) { }
	SLATE_END_ARGS()

public:

	/**
	* Default constructor.
	*/
	SVisualLogger();
	virtual ~SVisualLogger();

public:

	/**
	* Constructs the application.
	*
	* @param InArgs The Slate argument list.
	* @param ConstructUnderMajorTab The major tab which will contain the session front-end.
	* @param ConstructUnderWindow The window in which this widget is being constructed.
	* @param InStyleSet The style set to use.
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow);

public:
	class FVisualLoggerDevice : public FVisualLogDevice
	{
	public:
		FVisualLoggerDevice(SVisualLogger* Owner);
		virtual ~FVisualLoggerDevice();
		virtual void Serialize(const class UObject* LogOwner, FName OwnerName, const FVisualLogEntry& LogEntry) override;

	protected:
		SVisualLogger* Owner;
	};
	friend class FVisualLogDevice;

	TSharedRef<SDockTab> HandleTabManagerSpawnTab(const FSpawnTabArgs& Args, FName TabIdentifier) const;
	static void FillWindowMenu(FMenuBuilder& MenuBuilder, const TSharedPtr<FTabManager> TabManager);

	/** Callback for for when the owner tab's visual state is being persisted. */
	void HandleMajorTabPersistVisualState();
	void OnTabLosed();

	void OnNewLogEntry(const FVisualLogDevice::FVisualLogEntryItem& Entry);
	void CollectNewCategories(const FVisualLogDevice::FVisualLogEntryItem& Entry);
	void OnItemSelectionChanged(const FVisualLogDevice::FVisualLogEntryItem& EntryItem);
	void OnFiltersChanged();
	void OnObjectSelectionChanged(TSharedPtr<class STimeline> TimeLine);
	void OnFiltersSearchChanged(const FText& Filter);

	bool HandleStartRecordingCommandCanExecute() const;
	void HandleStartRecordingCommandExecute();
	bool HandleStartRecordingCommandIsVisible() const;

	bool HandleStopRecordingCommandCanExecute() const;
	void HandleStopRecordingCommandExecute();
	bool HandleStopRecordingCommandIsVisible() const;

	bool HandlePauseCommandCanExecute() const;
	void HandlePauseCommandExecute();
	bool HandlePauseCommandIsVisible() const;

	bool HandleResumeCommandCanExecute() const;
	void HandleResumeCommandExecute();
	bool HandleResumeCommandIsVisible() const;

	bool HandleLoadCommandCanExecute() const;
	void HandleLoadCommandExecute();

	bool HandleSaveCommandCanExecute() const;
	void HandleSaveCommandExecute();

	bool HandleCameraCommandCanExecute() const;
	void HandleCameraCommandExecute();
	bool HandleCameraCommandIsChecked() const;

	TSharedPtr<SVisualLoggerFilters> GetVisualLoggerFilters() { return VisualLoggerFilters; }
	void HandleTabManagerPersistLayout(const TSharedRef<FTabManager::FLayout>& LayoutToSave);
protected:
	// Holds the list of UI commands.
	TSharedRef<FUICommandList> CommandList;

	// Holds the tab manager that manages the front-end's tabs.
	TSharedPtr<FTabManager> TabManager;

	// Visual Logger device to get and collect logs.
	TSharedPtr<FVisualLoggerDevice> InternalDevice;

	TWeakObjectPtr<class AVisualLoggerCameraController> CameraController;
	TSharedPtr<struct FVisualLoggerCanvasRenderer> VisualLoggerCanvasRenderer;
	TSharedPtr<IVisualLoggerInterface> VisualLoggerInterface;
	mutable TSharedPtr<SVisualLoggerFilters> VisualLoggerFilters;
	mutable TSharedPtr<SVisualLoggerView> MainView;
	mutable TSharedPtr<SVisualLoggerLogsList> LogsList;
	mutable TSharedPtr<SVisualLoggerStatusView> StatusView;
};
