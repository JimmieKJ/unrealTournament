// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
	
#include "CrashReportClientApp.h"

#include "CrashUpload.h"
#include "UniquePtr.h"
#include "PlatformErrorReport.h"
#include "PendingReports.h"
#include "XmlFile.h"

#define LOCTEXT_NAMESPACE "CrashReportClient"

namespace
{
	const float PingTimeoutSeconds = 5.f;
	// Ignore files bigger than 100MB; mini-dumps are smaller than this, but heap dumps can be very large
	const int MaxFileSizeToUpload = 100 * 1024 * 1024;
}

FCrashUpload::FCrashUpload(const FString& ServerAddress)
	: UrlPrefix(ServerAddress / "CrashReporter")
	, State(EUploadState::NotSet)
	, PauseState(EUploadState::Ready)
{
	SendPingRequest();
}

FCrashUpload::~FCrashUpload()
{
	UE_LOG(CrashReportClientLog, Log, TEXT("Final state = %s"), ToString(State));
}

bool FCrashUpload::PingTimeout(float DeltaTime)
{
	if (EUploadState::PingingServer == State)
	{
		SetCurrentState(EUploadState::ServerNotAvailable);

		// PauseState will be Ready if user has not yet decided to send the report
		if (PauseState > EUploadState::Ready)
		{
			AddReportToPendingFile();
		}
	}

	// One-shot
	return false;
}

void FCrashUpload::BeginUpload(const FPlatformErrorReport& PlatformErrorReport)
{
	ErrorReport = PlatformErrorReport;
	PendingFiles += ErrorReport.GetFilesToUpload();
	UE_LOG(CrashReportClientLog, Log, TEXT("Got %d pending files to upload from '%s'"), PendingFiles.Num(), *ErrorReport.GetReportDirectoryLeafName());

	PauseState = EUploadState::Finished;
	if (State == EUploadState::Ready)
	{
		BeginUploadImpl();
	}
	else if (State == EUploadState::ServerNotAvailable)
	{
		AddReportToPendingFile();
	}
}

const FText& FCrashUpload::GetStatusText() const
{
	return UploadStateText;
}

void FCrashUpload::LocalDiagnosisComplete(const FString& DiagnosticsFile)
{
	if (State >= EUploadState::FirstCompletedState)
	{
		// Must be a failure/canceled state, or the report was a rejected by the server
		return;
	}

	const bool SendDiagnosticsFile = !DiagnosticsFile.IsEmpty();
	if (SendDiagnosticsFile)
	{
		PendingFiles.Push(DiagnosticsFile);
	}
}

bool FCrashUpload::IsFinished() const
{
	return State >= EUploadState::FirstCompletedState;
}

void FCrashUpload::Cancel()
{
	SetCurrentState(EUploadState::Cancelled);
}

bool FCrashUpload::SendCheckReportRequest()
{
	FString XMLString;

	UE_LOG(CrashReportClientLog, Log, TEXT("Sending HTTP request (checking report)"));
	auto Request = CreateHttpRequest();
	if (State == EUploadState::CheckingReport)
	{
		AssignReportIdToPostDataBuffer();
		Request->SetURL(UrlPrefix / TEXT("CheckReport"));
		Request->SetHeader(TEXT("Content-Type"), TEXT("text/plain; charset=us-ascii"));
	}
	else
	{
		// This part is Windows-specific on the server
		ErrorReport.LoadWindowsReportXmlFile( XMLString );

		// Convert the XMLString into the UTF-8.
		FTCHARToUTF8 Converter( (const TCHAR*)*XMLString, XMLString.Len() );
		const int32 Length = Converter.Length();
		PostData.Reset( Length );
		PostData.AddUninitialized( Length );
		CopyAssignItems( (ANSICHAR*)PostData.GetData(), Converter.Get(), Length );

		Request->SetURL(UrlPrefix / TEXT("CheckReportDetail"));
		Request->SetHeader(TEXT("Content-Type"), TEXT("text/plain; charset=utf-8"));
	}

	UE_LOG( CrashReportClientLog, Log, TEXT( "PostData Num: %i" ), PostData.Num() );
	Request->SetVerb(TEXT("POST"));
	Request->SetContent(PostData);

	return Request->ProcessRequest();
}

enum class ECompressedCrashFileHeader
{
	MAGIC = 0x7E1B83C1,
};

struct FCompressedCrashFile : FNoncopyable
{
	int32 CurrentFileIndex;
	FString Filename;
	TArray<uint8> Filedata;

	FCompressedCrashFile( int32 InCurrentFileIndex, const FString& InFilename, const TArray<uint8>& InFiledata )
		: CurrentFileIndex(InCurrentFileIndex)
		, Filename(InFilename)
		, Filedata(InFiledata)
	{
	}

	/** Serialization operator. */
	friend FArchive& operator << (FArchive& Ar, FCompressedCrashFile& Data)
	{
		Ar << Data.CurrentFileIndex;
		Data.Filename.SerializeAsANSICharArray( Ar, 260 );
		Ar << Data.Filedata;
		return Ar;
	}
};

void FCrashUpload::CompressAndSendData()
{
	UE_LOG(CrashReportClientLog, Log, TEXT("CompressAndSendData have %d pending files"), PendingFiles.Num());

	// Compress all files into one archive.
	const int32 BufferSize = 16*1024*1024;

	TArray<uint8> UncompressedData;
	UncompressedData.Reserve( BufferSize );
	FMemoryWriter MemoryWriter( UncompressedData, false, true );

	int32 CurrentFileIndex = 0;

	// Loop to keep trying files until a send succeeds or we run out of files
	while (PendingFiles.Num() != 0)
	{
		FString PathOfFileToUpload = PendingFiles.Pop();
		
		if (FPlatformFileManager::Get().GetPlatformFile().FileSize(*PathOfFileToUpload) > MaxFileSizeToUpload)
		{
			UE_LOG(CrashReportClientLog, Warning, TEXT("Skipping large crash report file"));
			continue;
		}

		if (!FFileHelper::LoadFileToArray(PostData, *PathOfFileToUpload))
		{
			UE_LOG(CrashReportClientLog, Warning, TEXT("Failed to load crash report file"));
			continue;
		}

		UE_LOG(CrashReportClientLog, Log, TEXT("CompressAndSendData compressing %d bytes ('%s')"), PostData.Num(), *PathOfFileToUpload);
		FString Filename = FPaths::GetCleanFilename(PathOfFileToUpload);
		if (Filename == "diagnostics.txt")
		{
			// Ensure diagnostics file is capitalized for server
			Filename[0] = 'D';
		}

		FCompressedCrashFile FileToCompress( CurrentFileIndex, Filename, PostData );
		CurrentFileIndex++;

		MemoryWriter << FileToCompress;
	}

	
	uint8* CompressedDataRaw = new uint8[BufferSize];

	int32 CompressedSize = BufferSize;
	int32 UncompressedSize = UncompressedData.Num();
	const bool bResult = FCompression::CompressMemory( COMPRESS_ZLIB, CompressedDataRaw, CompressedSize, UncompressedData.GetData(), UncompressedSize );
	if( !bResult )
	{
		UE_LOG(CrashReportClientLog, Warning, TEXT("Couldn't compress the crash report files"));
		SetCurrentState(EUploadState::Cancelled);
		return;
	}
	
	const FString Filename = ErrorReport.GetReportDirectoryLeafName() + TEXT(".ue4crash");

	// Copy compressed data into the array.
	TArray<uint8> CompressedData;
	CompressedData.Append( CompressedDataRaw, CompressedSize );
	delete [] CompressedDataRaw;
	CompressedDataRaw = nullptr;

	// Set up request for upload
	UE_LOG(CrashReportClientLog, Log, TEXT("Sending HTTP request (posting file)"));
	auto Request = CreateHttpRequest();
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
	Request->SetURL(UrlPrefix / TEXT("UploadReportFile"));
	Request->SetContent(CompressedData);
	Request->SetHeader(TEXT("DirectoryName"), *ErrorReport.GetReportDirectoryLeafName());
	Request->SetHeader(TEXT("FileName"), Filename);
	Request->SetHeader(TEXT("FileLength"), TTypeToString<int32>::ToString(CompressedData.Num()) );
	Request->SetHeader(TEXT("CompressedSize"), TTypeToString<int32>::ToString(CompressedSize) );
	Request->SetHeader(TEXT("UncompressedSize"), TTypeToString<int32>::ToString(UncompressedSize) );
	Request->SetHeader(TEXT("NumberOfFiles"), TTypeToString<int32>::ToString(CurrentFileIndex) );

	if (Request->ProcessRequest())
	{
		return;
	}
	else
	{
		UE_LOG(CrashReportClientLog, Warning, TEXT("Failed to send file upload request"));
		SetCurrentState(EUploadState::Cancelled);
	}	
}

void FCrashUpload::AssignReportIdToPostDataBuffer()
{
	FString ReportDirectoryName = *ErrorReport.GetReportDirectoryLeafName();
	const int32 DirectoryNameLength = ReportDirectoryName.Len();
	PostData.SetNum(DirectoryNameLength);
	for (int32 Index = 0; Index != DirectoryNameLength; ++Index)
	{
		PostData[Index] = ReportDirectoryName[Index];
	}
}

void FCrashUpload::PostReportComplete()
{
	if (PauseState == EUploadState::PostingReportComplete)
	{
		// Wait for confirmation
		SetCurrentState(EUploadState::WaitingToPostReportComplete);
		return;
	}

	AssignReportIdToPostDataBuffer();

	UE_LOG(CrashReportClientLog, Log, TEXT("Sending HTTP request (posting \"Upload complete\")"));
	auto Request = CreateHttpRequest();
	Request->SetVerb( TEXT( "POST" ) );
	Request->SetURL(UrlPrefix / TEXT("UploadComplete"));
	Request->SetHeader( TEXT( "Content-Type" ), TEXT( "text/plain; charset=us-ascii" ) );
	Request->SetContent(PostData);

	if (Request->ProcessRequest())
	{
		SetCurrentState(EUploadState::PostingReportComplete);
	}
	else
	{
		CheckPendingReportsForFilesToUpload();
	}
}

void FCrashUpload::OnProcessRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	UE_LOG(CrashReportClientLog, Log, TEXT("OnProcessRequestComplete(), State=%s bSucceeded=%i"), ToString(State), (int32)bSucceeded );
	switch (State)
	{
	default:
		// May get here if response is received after time-out has passed
		break;

	case EUploadState::PingingServer:
		if (bSucceeded)
		{
			OnPingSuccess();
		}
		else
		{
			PingTimeout(0);
		}
		break;

	case EUploadState::CheckingReport:
	case EUploadState::CheckingReportDetail:
		if (!bSucceeded || !ParseServerResponse(HttpResponse))
		{
			if (!bSucceeded)
			{
				UE_LOG(CrashReportClientLog, Warning, TEXT("Request to server failed"));
			}
			else
			{
				UE_LOG(CrashReportClientLog, Warning, TEXT("Did not get a valid server response."));
			}

			// Skip this report
			CheckPendingReportsForFilesToUpload();
			break;
		}

		if (State == EUploadState::CheckingReport)
		{
			SetCurrentState(EUploadState::CheckingReportDetail);
			if (!SendCheckReportRequest())
			{
				// Skip this report
				CheckPendingReportsForFilesToUpload();
			}
		}
		else
		{
			SetCurrentState(EUploadState::CompressAndSendData);
			CompressAndSendData();
		}
		break;

	case EUploadState::CompressAndSendData:
		if (!bSucceeded)
		{
			UE_LOG(CrashReportClientLog, Warning, TEXT("File upload failed"));
			SetCurrentState(EUploadState::Cancelled);
		}
		else
		{
			PostReportComplete();
		}
		break;

	case EUploadState::PostingReportComplete:
		CheckPendingReportsForFilesToUpload();
		break;
	}
}

void FCrashUpload::OnPingSuccess()
{
	if (PauseState > EUploadState::Ready)
	{
		BeginUploadImpl();
	}
	else
	{
		// Await instructions
		SetCurrentState(EUploadState::Ready);
	}
}

void FCrashUpload::AddReportToPendingFile() const
{
	if (PendingFiles.Num() == 0)
	{
		// No files so don't bother
		return;
	}

	FPendingReports PendingReports;
	// Extract full path of report from the first file
	PendingReports.Add(FPaths::GetPath(*PendingFiles[0]));
	PendingReports.Save();
}

void FCrashUpload::CheckPendingReportsForFilesToUpload()
{
	SetCurrentState(EUploadState::CheckingReport);
	do
	{
		if (PendingReportDirectories.Num() == 0)
		{
			// Nothing to upload
			UE_LOG(CrashReportClientLog, Log, TEXT("All uploads done"));
			SetCurrentState(EUploadState::Finished);
			return;
		}

		ErrorReport = FPlatformErrorReport(PendingReportDirectories.Pop());
		PendingFiles = ErrorReport.GetFilesToUpload();
	}
	while (PendingFiles.Num() == 0 || !SendCheckReportRequest());
}

void FCrashUpload::BeginUploadImpl()
{
	FPendingReports PendingReports;
	// Make sure we don't try to upload the current report twice
	PendingReports.Forget(*ErrorReport.GetReportDirectoryLeafName());
	PendingReportDirectories = PendingReports.GetReportDirectories();

	UE_LOG(CrashReportClientLog, Log, TEXT("Proceeding to upload %d pending files from '%s'"), PendingFiles.Num(), *ErrorReport.GetReportDirectoryLeafName());

	// Not sure about this: probably want to removes files one by one as they're successfully uploaded
	PendingReports.Clear();
	PendingReports.Save();

	SetCurrentState(EUploadState::CheckingReport);
	if (PendingFiles.Num() == 0 || !SendCheckReportRequest())
	{
		CheckPendingReportsForFilesToUpload();
	}
}

TSharedRef<IHttpRequest> FCrashUpload::CreateHttpRequest()
{
	auto Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindRaw(this, &FCrashUpload::OnProcessRequestComplete);
	return Request;
}

void FCrashUpload::SendPingRequest()
{
	SetCurrentState(EUploadState::PingingServer);

	auto Request = CreateHttpRequest();
	Request->SetVerb(TEXT("GET"));
	Request->SetURL(UrlPrefix / TEXT("Ping"));
	UE_LOG(CrashReportClientLog, Log, TEXT("Sending HTTP request (pinging server)"));

	if (Request->ProcessRequest())
	{
		FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FCrashUpload::PingTimeout), PingTimeoutSeconds);
	}
	else
	{
		PingTimeout(0);
	}
}

bool FCrashUpload::ParseServerResponse(FHttpResponsePtr Response)
{
	// Turn the snippet into a complete XML document, to keep the XML parser happy
	FXmlFile ParsedResponse(FString(TEXT("<Root>")) + Response->GetContentAsString() + TEXT("</Root>"), EConstructMethod::ConstructFromBuffer);
	UE_LOG(CrashReportClientLog, Log, TEXT("Response->GetContentAsString(): '%s'"), *Response->GetContentAsString());	
	if (!ParsedResponse.IsValid())
	{
		UE_LOG(CrashReportClientLog, Log, TEXT("Invalid response!"));	
		return false;
	}
	if (auto ResultNode = ParsedResponse.GetRootNode()->FindChildNode(TEXT("CrashReporterResult")))
	{
		UE_LOG(CrashReportClientLog, Log, TEXT("ResultNode->GetAttribute(TEXT(\"bSuccess\")) = %s"), *ResultNode->GetAttribute(TEXT("bSuccess")));	
		return ResultNode->GetAttribute(TEXT("bSuccess")) == TEXT("true");
	}
	UE_LOG(CrashReportClientLog, Log, TEXT("Could not find CrashReporterResult"));	
	return false;
}

void FCrashUpload::SetCurrentState(EUploadState::Type InState)
{
	if (State == EUploadState::NotSet)
	{
		UE_LOG(CrashReportClientLog, Log, TEXT("Initial state = %s"), ToString(State));
	}
	else
	{
		UE_LOG(CrashReportClientLog, Log, TEXT("State change from %s to %s"), ToString(State), ToString(InState));
	}

	State = InState;

	switch (State)
	{
	default:
		break;

	case EUploadState::PingingServer:
		UploadStateText = LOCTEXT("PingingServer", "Pinging server");
		break;

	case EUploadState::Ready:
		UploadStateText = LOCTEXT("UploaderReady", "Ready to send to server");
		break;

	case EUploadState::ServerNotAvailable:
		UploadStateText = LOCTEXT("ServerNotAvailable", "Server not available - report will be stored for later upload");
		break;
	}
}

const TCHAR* FCrashUpload::ToString(EUploadState::Type State)
{
	switch(State)
	{
		case EUploadState::PingingServer: 
			return TEXT("PingingServer");

		case EUploadState::Ready:
			return TEXT("Ready");

		case EUploadState::CheckingReport:
			return TEXT("CheckingReport");

		case EUploadState::CheckingReportDetail:
			return TEXT("CheckingReportDetail");

		case EUploadState::CompressAndSendData:
			return TEXT("SendingFiles");

		case EUploadState::WaitingToPostReportComplete:
			return TEXT("WaitingToPostReportComplete");

		case EUploadState::PostingReportComplete:
			return TEXT("PostingReportComplete");

		case EUploadState::Finished:
			return TEXT("Finished");

		case EUploadState::ServerNotAvailable:
			return TEXT("ServerNotAvailable");

		case EUploadState::UploadError:
			return TEXT("UploadError");

		case EUploadState::Cancelled:
			return TEXT("Cancelled");

		default:
			break;
	}

	return TEXT("Unknown UploadState value");
}

#undef LOCTEXT_NAMESPACE
