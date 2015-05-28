// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"
#include "CrashReportUtil.h"

#include "CrashDescription.h"

#include "CrashReportClient.h"
#include "UniquePtr.h"

#include "TaskGraphInterfaces.h"

#define LOCTEXT_NAMESPACE "CrashReportClient"

FCrashDescription& GetCrashDescription()
{
	static FCrashDescription Singleton;
	return Singleton;
}

#if !CRASH_REPORT_UNATTENDED_ONLY

FCrashReportClient::FCrashReportClient(const FPlatformErrorReport& InErrorReport)
	: DiagnosticText( LOCTEXT("ProcessingReport", "Processing crash report ...") )
	, DiagnoseReportTask(nullptr)
	, ErrorReport( InErrorReport )
	, Uploader( FCrashReportClientConfig::Get().GetReceiverAddress() )
	, bBeginUploadCalled(false)
	, bShouldWindowBeHidden(false)
	, bAllowToBeContacted(true)
	, bSendData(false)
{

	if (!ErrorReport.TryReadDiagnosticsFile(DiagnosticText) && !FParse::Param(FCommandLine::Get(), TEXT("no-local-diagnosis")))
	{
		DiagnoseReportTask = new FAsyncTask<FDiagnoseReportWorker>( this );
		DiagnoseReportTask->StartBackgroundTask();
	}
	else if( !DiagnosticText.IsEmpty() )
	{
		DiagnosticText = FCrashReportUtil::FormatDiagnosticText( DiagnosticText, GetCrashDescription().MachineId, GetCrashDescription().EpicAccountId, GetCrashDescription().UserName );
	}
}


FCrashReportClient::~FCrashReportClient()
{
	if( DiagnoseReportTask )
	{
		DiagnoseReportTask->EnsureCompletion();
		delete DiagnoseReportTask;
	}
}

FReply FCrashReportClient::Submit()
{
	bSendData = true;
	StoreCommentAndUpload();
	bShouldWindowBeHidden = true;
	return FReply::Handled();
}

FReply FCrashReportClient::SubmitAndRestart()
{
	Submit();

	const FString CrashedAppPath = ErrorReport.FindCrashedAppPath();
	const FCrashDescription& CrashDescription = GetCrashDescription();
	const FString CommandLineArguments = CrashDescription.CommandLine;

	FPlatformProcess::CreateProc(*CrashedAppPath, *CommandLineArguments, true, false, false, NULL, 0, NULL, NULL);

	return FReply::Handled();
}

FReply FCrashReportClient::CopyCallstack()
{
	FPlatformMisc::ClipboardCopy(*DiagnosticText.ToString());
	return FReply::Handled();
}

FText FCrashReportClient::GetDiagnosticText() const
{
	return DiagnosticText;
}

void FCrashReportClient::UserCommentChanged(const FText& Comment, ETextCommit::Type CommitType)
{
	UserComment = Comment;

	// Implement Shift+Enter to commit shortcut
	if (CommitType == ETextCommit::OnEnter && FSlateApplication::Get().GetModifierKeys().IsShiftDown())
	{
		Submit();
	}
}

void FCrashReportClient::RequestCloseWindow(const TSharedRef<SWindow>& Window)
{
	// Don't send the data.
	bSendData = false;

	// We may still processing minidump etc. so start the main ticker.
	StartTicker();
	bShouldWindowBeHidden = true;
}

bool FCrashReportClient::AreCallstackWidgetsEnabled() const
{
	return !IsProcessingCallstack();
}

EVisibility FCrashReportClient::IsThrobberVisible() const
{
	return IsProcessingCallstack() ? EVisibility::Visible : EVisibility::Hidden;
}

void FCrashReportClient::SCrashReportClient_OnCheckStateChanged( ECheckBoxState NewRadioState )
{
	bAllowToBeContacted = NewRadioState == ECheckBoxState::Checked;
}

void FCrashReportClient::StartTicker()
{
	FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FCrashReportClient::Tick), 1.f);
}

void FCrashReportClient::StoreCommentAndUpload()
{
	// Call upload even if the report is empty: pending reports will be sent if any
	ErrorReport.SetUserComment(UserComment, bAllowToBeContacted);
	StartTicker();
}

bool FCrashReportClient::Tick(float UnusedDeltaTime)
{
	// We are waiting for diagnose report task to complete.
	if( IsProcessingCallstack() )
	{
		return true;
	}
	
	if( bSendData )
	{
		if( !bBeginUploadCalled )
		{
			// Can be called only when we have all files.
			Uploader.BeginUpload( ErrorReport );
			bBeginUploadCalled = true;
		}

		// IsWorkDone will always return true here (since uploader can't finish until the diagnosis has been sent), but it
		//  has the side effect of joining the worker thread.
		if( !Uploader.IsFinished() )
		{
			// More ticks, please
			return true;
		}
	}

	FPlatformMisc::RequestExit(false);
	return false;
}

FString FCrashReportClient::GetCrashedAppName() const
{
	return GetCrashDescription().GameName;
}

void FCrashReportClient::FinalizeDiagnoseReportWorker( FText ReportText )
{
	DiagnosticText = FCrashReportUtil::FormatDiagnosticText( ReportText, GetCrashDescription().MachineId, GetCrashDescription().EpicAccountId, GetCrashDescription().UserName );

	auto DiagnosticsFilePath = ErrorReport.GetReportDirectory() / FCrashReportClientConfig::Get().GetDiagnosticsFilename();
	Uploader.LocalDiagnosisComplete(FPaths::FileExists(DiagnosticsFilePath) ? DiagnosticsFilePath : TEXT(""));
}


bool FCrashReportClient::IsProcessingCallstack() const
{
	return DiagnoseReportTask && !DiagnoseReportTask->IsWorkDone();
}

FDiagnoseReportWorker::FDiagnoseReportWorker( FCrashReportClient* InCrashReportClient ) 
	: CrashReportClient( InCrashReportClient )
{}

void FDiagnoseReportWorker::DoWork()
{
	const FText ReportText = CrashReportClient->ErrorReport.DiagnoseReport();
	// Inform the game thread that we are done.
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
	(
		FSimpleDelegateGraphTask::FDelegate::CreateRaw( CrashReportClient, &FCrashReportClient::FinalizeDiagnoseReportWorker, ReportText ),
		TStatId(), nullptr, ENamedThreads::GameThread
	);
}

#endif // !CRASH_REPORT_UNATTENDED_ONLY

FText FCrashReportUtil::FormatDiagnosticText( const FText& DiagnosticText, const FString MachineId, const FString EpicAccountId, const FString UserNameNoDot )
{
	return FText::Format( LOCTEXT( "CrashReportClientCallstackPattern", "MachineId:{0}\nEpicAccountId:{1}\n\n{2}" ), FText::FromString( MachineId ), FText::FromString( EpicAccountId ), DiagnosticText );
}

#undef LOCTEXT_NAMESPACE
