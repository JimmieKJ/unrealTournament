// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Http.h"
#include "PlatformErrorReport.h"

/**
 * Handles uploading files to the crash report server
 */
class FCrashUpload
{
public:
	/**
	 * Constructor: pings server 
	 * @param ServerAddress Host IP of the crash report server
	 */
	explicit FCrashUpload(const FString& ServerAddress);

	/**
	 * Destructor for logging
	 */
	~FCrashUpload();

	/**
	 * Commence upload when ready
	 * @param PlatformErrorReport Error report to upload files from
	 */
	void BeginUpload(const FPlatformErrorReport& PlatformErrorReport);

	/**
	 * Provide progress or error information for the UI
	 */
	const FText& GetStatusText() const;

	/**
	 * Notification that diagnosis of the main report has finished
	 * @param DiagnosticsFile Full path to the diagnostics file to upload on success, or an empty string
	 */
	void LocalDiagnosisComplete(const FString& DiagnosticsFile);

	/**
	 * Inform uploader that no local diagnosis is happening
	 */
	void LocalDiagnosisSkipped()
	{
		LocalDiagnosisComplete("");
	}

	/**
	 * Determine whether the upload has finished (successfully or otherwise)
	 * @return Whether the upload has finished
	 */
	bool IsFinished() const;

	/**
	 * Cancel the upload
	 * @note Only valid to call this before the upload has begun
	 */
	void Cancel();

private:
	/** State enum to keep track of what the uploader is doing */
	struct EUploadState
	{
		enum Type
		{
			NotSet,

			PingingServer,
			Ready,
			CheckingReport,
			CheckingReportDetail,
			CompressAndSendData,
			WaitingToPostReportComplete,
			PostingReportComplete,
			Finished,

			ServerNotAvailable,
			UploadError,
			Cancelled,

			FirstCompletedState = Finished,
		};
	};

	/**
	 * Get a string representation of the state, for logging purposes
	 * @param State Value to stringize
	 * @return Literal string value
	 */
	static const TCHAR* ToString(EUploadState::Type InState);

	/**
	 * Set the current state, also updating the status text where necessary
	 * @param State State the uploader is now in
	 */
	void SetCurrentState(EUploadState::Type InState);

	/**
	 * Send a request to see if the server will accept this report
	 * @return Whether request was successfully sent
	 */
	bool SendCheckReportRequest();

	/**
	 * Compresses all crash report files and sends one compressed file.
	 */
	void CompressAndSendData();

	/**
	 * Convert the report name to single byte non-zero-terminated HTTP post data
	 */
	void AssignReportIdToPostDataBuffer();

	/**
	 * Send a POST request to the server indicating that all the files for the current report have been sent
	 */
	void PostReportComplete();

	/**
	 * Callback from HTTP library when a request has completed
	 * @param HttpRequest The request object
	 * @param HttpResponse The response from the server
	 * @param bSucceeded Whether a response was successfully received
	 */
	void OnProcessRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	/**
	 * Start uploading if BeginUpload has been called
	 */
	void OnPingSuccess();

	/**
	 * Callback a set amount of time after ping request was sent
	 * @note Gets fired no matter whether ping response was received
	 * @param Unused time since last call
	 * @return Always returns false, meaning one-shot
	 */
	bool PingTimeout(float DeltaTime);

	/**
	 * In case the server is not available, add the report to a file containing reports to upload next time
	 */
	void AddReportToPendingFile() const;

	/**
	 * If there a no pending files, look through pending reports for files to upload
	 */
	void CheckPendingReportsForFilesToUpload();

	/**
	 * Start uploading files, either when user presses Submit or Ping request succeeds, whichever is later
	 */
	void BeginUploadImpl();

	/**
	 * Create a request object and bind this class's response handler to it
	 */
	TSharedRef<IHttpRequest> CreateHttpRequest();

	/**
	 * Send a ping request to the server
	 */
	void SendPingRequest();

	/**
	 * Parse an XML response from the server for the success field
	 * @param Response Response to get message to parse from
	 * @return Whether it's both a valid message and the code is success
	 */
	static bool ParseServerResponse(FHttpResponsePtr Response);

	/** Host, port and common prefix of all requests to the server */
	FString UrlPrefix;

	/** What this class is currently doing */
	EUploadState::Type State;

	/** Status of upload to display */
	FText UploadStateText;

	/** State to pause at until confirmation has been received to continue */
	EUploadState::Type PauseState;

	/** Full paths of files still to be uploaded */
	TArray<FString> PendingFiles;

	/** Error report being processed */
	FPlatformErrorReport ErrorReport;

	/** Full paths of reports from previous runs still to be uploaded */
	TArray<FString> PendingReportDirectories;

	/** Buffer to keep reusing for file content and other messages */
	TArray<uint8> PostData;
};
