// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AVIWriter.cpp: AVI creation implementation.
=============================================================================*/
#include "EnginePrivate.h"
#include "AVIWriter.h"
#include "CapturePin.h"

DEFINE_LOG_CATEGORY_STATIC(LogAVIWriter, Log, All);

#if PLATFORM_WINDOWS && !UE_BUILD_MINIMAL

#include "AllowWindowsPlatformTypes.h"
typedef TCHAR* PTCHAR;
#pragma warning(push)
#pragma warning(disable : 4263) // 'function' : member function does not override any base class virtual member function
#pragma warning(disable : 4264) // 'virtual_function' : no override available for virtual member function from base 'cla
#include <streams.h>
#pragma warning(pop)

#include <dshow.h>
#include <initguid.h>
#include "HideWindowsPlatformTypes.h"

#include "CaptureSource.h"
#include "SlateCore.h"


#define g_wszCapture    L"Capture Filter"

/** An event for synchronizing direct show encoding with the game thread */
FEvent* GCaptureSyncEvent = NULL;

// Filter setup data
const AMOVIESETUP_MEDIATYPE sudOpPinTypes =
{
	&MEDIATYPE_Video,       // Major type
	&MEDIASUBTYPE_NULL      // Minor type
};


#ifdef __clang__
	// Suppress warning about filling non-const string variable from literal
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wwritable-strings"	// warning : ISO C++11 does not allow conversion from string literal to 'LPWSTR' (aka 'wchar_t *') [-Wwritable-strings]
#endif

const AMOVIESETUP_PIN sudOutputPinDesktop = 
{
	L"Output",      // Obsolete, not used.
	false,          // Is this pin rendered?
	true,           // Is it an output pin?
	false,          // Can the filter create zero instances?
	false,          // Does the filter create multiple instances?
	&CLSID_NULL,    // Obsolete.
	NULL,           // Obsolete.
	1,              // Number of media types.
	&sudOpPinTypes  // Pointer to media types.
};

#ifdef __clang__
	#pragma clang diagnostic pop
#endif

const AMOVIESETUP_FILTER sudPushSourceDesktop =
{
	&CLSID_CaptureSource,	// Filter CLSID
	g_wszCapture,			// String name
	MERIT_DO_NOT_USE,       // Filter merit
	1,                      // Number pins
	&sudOutputPinDesktop    // Pin details
};


// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance.
// We provide a set of filters in this one DLL.

CFactoryTemplate g_Templates[1] = 
{
	{ 
		g_wszCapture,					// Name
		&CLSID_CaptureSource,			// CLSID
		FCaptureSource::CreateInstance, // Method to create an instance of MyComponent
		NULL,                           // Initialization function
		&sudPushSourceDesktop           // Set-up information (for filters)
	},
};

int32 g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);    

/**
 * Windows implementation relying on DirectShow.
 */
class FAVIWriterWin : public FAVIWriter
{
public:
	FAVIWriterWin( ) 
		: Graph(NULL),
		Control(NULL),
		Capture(NULL),
		CapturePin(NULL),
		CaptionSource(NULL)
	{

	};

public:

	void StartCapture(FViewport* Viewport , const FString& OutputPath /*=FString()*/)
	{
		GCaptureSyncEvent = FPlatformProcess::GetSynchEventFromPool();
		if (!bCapturing)
		{
			if (!Viewport)
			{
				Viewport = GEngine->GameViewport != NULL ? GEngine->GameViewport->Viewport : NULL;
				if (!Viewport)
				{
					UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Could not get a valid viewport for capture!" ));
					return;
				}
			}

			if ( Viewport->GetSizeXY().GetMin() <= 0)
			{
				UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Attempting to record a 0 sized viewport!" ));
				return;
			}

			CaptureViewport = Viewport;

			ViewportColorBuffer.AddUninitialized( Viewport->GetSizeXY().X * Viewport->GetSizeXY().Y );
			

			// Initialize the COM library.
			if (!FWindowsPlatformMisc::CoInitialize()) 
			{
				UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Could not initialize COM library!" ));
				return;
			}

			// Create the filter graph manager and query for interfaces.
			HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&Graph);
			if (FAILED(hr)) 
			{
				UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Could not create the Filter Graph Manager!" ));
				FWindowsPlatformMisc::CoUninitialize();
				return;
			}

			// Create the capture graph builder
			hr = CoCreateInstance (CLSID_CaptureGraphBuilder2 , NULL, CLSCTX_INPROC, IID_ICaptureGraphBuilder2, (void **) &Capture);
			if (FAILED(hr)) 
			{
				UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Could not create the Capture Graph Manager!" ));
				FWindowsPlatformMisc::CoUninitialize();
				return;
			}

			// Specify a filter graph for the capture graph builder to use
			hr = Capture->SetFiltergraph(Graph);
			if (FAILED(hr))
			{
				UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Failed to set capture filter graph!" ));
				FWindowsPlatformMisc::CoUninitialize();
				return;
			}

			CaptionSource = new FCaptureSource(NULL, &hr);
			if (FAILED(hr)) 
			{
				UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Could not create CaptureSource filter!" ));
				Graph->Release();
				FWindowsPlatformMisc::CoUninitialize(); 
				return;
			}
			CaptionSource->AddRef();

			hr = Graph->AddFilter(CaptionSource, L"PushSource"); 
			if (FAILED(hr)) 
			{
				UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Could not add CaptureSource filter!" ));
				CaptionSource->Release();
				Graph->Release();
				FWindowsPlatformMisc::CoUninitialize(); 
				return;
			}

			IAMVideoCompression* VideoCompression = NULL;
			if (GEngine->MatineeScreenshotOptions.bCompressMatineeCapture)
			{
				// Create the filter graph manager and query for interfaces.
				hr = CoCreateInstance (CLSID_MJPGEnc, NULL, CLSCTX_INPROC, IID_IBaseFilter, ( void ** ) &VideoCompression);
				if (FAILED(hr)) 
				{
					UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Could not create the video compression!" ));
					FWindowsPlatformMisc::CoUninitialize();
					return;
				}

				hr = Graph->AddFilter( (IBaseFilter *)VideoCompression, TEXT("Compressor") );

				if (FAILED(hr)) 
				{
					UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Could not add filter!" ));
					CaptionSource->Release();
					Graph->Release();
					FWindowsPlatformMisc::CoUninitialize(); 
					return;
				}	
			}

			IPin* temp = NULL;
			hr = CaptionSource->FindPin(L"1", &temp);
			if (FAILED(hr)) 
			{
				UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Could not find pin of filter CaptureSource!" ));
				CaptionSource->Release();
				Graph->Release();
				FWindowsPlatformMisc::CoUninitialize(); 
				return;
			}
			CapturePin = (FCapturePin*)temp;

			// Attempt to make the dir if it doesn't exist.
			TCHAR File[MAX_SPRINTF] = TEXT("");
			if (OutputPath.IsEmpty())
			{
				FString OutputDir = FPaths::VideoCaptureDir();
				IFileManager::Get().MakeDirectory(*OutputDir, true);

				if (GEngine->MatineeScreenshotOptions.bCompressMatineeCapture)
				{
					bool bFoundUnusedName = false;
					while (!bFoundUnusedName)
					{
						const FString FileNameInfo = FString::Printf( TEXT("_%dfps_%dx%d"), 
							GEngine->MatineeScreenshotOptions.MatineeCaptureFPS, CaptureViewport->GetSizeXY().X, CaptureViewport->GetSizeXY().Y );
						if (GEngine->MatineeScreenshotOptions.bCompressMatineeCapture)
						{
							FCString::Sprintf( File, TEXT("%s_%d.avi"), *(OutputDir + TEXT("/") + GEngine->MatineeScreenshotOptions.MatineePackageCaptureName + "_" + GEngine->MatineeScreenshotOptions.MatineeCaptureName + FileNameInfo + "_compressed"), MovieCaptureIndex );
						}
						else
						{
							FCString::Sprintf( File, TEXT("%s_%d.avi"), *(OutputDir + TEXT("/") + GEngine->MatineeScreenshotOptions.MatineePackageCaptureName + "_" + GEngine->MatineeScreenshotOptions.MatineeCaptureName + FileNameInfo), MovieCaptureIndex );
						}
						if (IFileManager::Get().FileSize(File) != -1)
						{
							MovieCaptureIndex++;
						}
						else
						{
							bFoundUnusedName = true;
						}
					}
				}
				else
				{
					bool bFoundUnusedName = false;
					while (!bFoundUnusedName)
					{
						FString FileName = FString::Printf( TEXT("Movie%i.avi"), MovieCaptureIndex );
						FCString::Sprintf( File, TEXT("%s"), *(OutputDir / FileName) );
						if (IFileManager::Get().FileSize(File) != -1)
						{
							MovieCaptureIndex++;
						}
						else
						{
							bFoundUnusedName = true;
						}
					}
				}
			}
			else
			{
				FCString::Sprintf( File, TEXT("%s"), *OutputPath );
			}

			IBaseFilter *pMux;
			hr = Capture->SetOutputFileName(
				&MEDIASUBTYPE_Avi,  // Specifies AVI for the target file.
				File,				// File name.
				&pMux,              // Receives a pointer to the mux.
				NULL);              // (Optional) Receives a pointer to the file sink.

			if (FAILED(hr)) 
			{
				UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Failed to set capture filter graph output name!" ));
				CaptionSource->Release();
				Graph->Release();
				FWindowsPlatformMisc::CoUninitialize(); 
				return;
			}

			hr = Capture->RenderStream(
				NULL,								// Pin category.
				&MEDIATYPE_Video,					// Media type.
				CaptionSource,					// Capture filter.
				(IBaseFilter *)VideoCompression,	// Intermediate filter (optional).
				pMux);								// Mux or file sink filter.


			if (FAILED(hr)) 
			{
				UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Could not start capture!" ));
				CaptionSource->Release();
				Graph->Release();
				FWindowsPlatformMisc::CoUninitialize(); 
				return;
			}
			// Release the mux filter.
			pMux->Release();

			SAFE_RELEASE(VideoCompression);
			
			Graph->QueryInterface(IID_IMediaControl, (void **)&Control);

			MovieCaptureIndex++;
			bReadyForCapture = true;
		}
	}

	void StopCapture()
	{
		if (bCapturing)
		{
			Control->Stop();

			// Trigger the event to force the capture thread to unblock and quit.
			GCaptureSyncEvent->Trigger();
			GCaptureSyncEvent->Wait();

			MovieCaptureIndex = 0;
			bCapturing = false;
			bReadyForCapture = false;
			bMatineeFinished = false;
			CaptureViewport = NULL;
			CaptureSlateRenderer = NULL;
			FrameNumber = 0;
			CurrentAccumSeconds = 0.f;
			SAFE_RELEASE(CapturePin);
			SAFE_RELEASE(CaptionSource);
			SAFE_RELEASE(Capture);
			SAFE_RELEASE(Control);
			SAFE_RELEASE(Graph);
			FWindowsPlatformMisc::CoUninitialize();
			FPlatformProcess::ReturnSynchEventToPool(GCaptureSyncEvent);
			GCaptureSyncEvent = nullptr;

			if(CaptureViewport)
			{
				ViewportColorBuffer.Empty();
			}
		}
	}

	void Close()
	{
		StopCapture();
	}

	void Update(float DeltaSeconds)
	{
		bool bShouldUpdate = true;
		const float CaptureFrequency = 1.0f/(float)GEngine->MatineeScreenshotOptions.MatineeCaptureFPS;
		if (CaptureFrequency > 0.f)
		{
			CurrentAccumSeconds += DeltaSeconds;

			if (CurrentAccumSeconds > CaptureFrequency)
			{
				while (CurrentAccumSeconds > CaptureFrequency)
				{
					CurrentAccumSeconds -= CaptureFrequency;
				}
			}
			else
			{
				bShouldUpdate = false;
			}
		}

		if (GIsRequestingExit || !bShouldUpdate || CaptureSlateRenderer)
		{
			return;
		}

		if (bMatineeFinished)
		{
			if (GEngine->MatineeScreenshotOptions.MatineeCaptureType == 0)
			{
				if (CaptureViewport)
				{
					CaptureViewport->GetClient()->CloseRequested(CaptureViewport);
				}
			}
			else
			{
				FViewport* ViewportUsed = GEngine->GameViewport != NULL ? GEngine->GameViewport->Viewport : NULL;
				if (ViewportUsed)
				{
					ViewportUsed->GetClient()->CloseRequested(ViewportUsed);
				}
			}
			StopCapture();
		}
		else if (bCapturing)
		{
			if ( GCaptureSyncEvent )
			{
				// Wait for the directshow thread to finish encoding the last data
				GCaptureSyncEvent->Wait();
			}

			CaptureViewport->ReadPixels(ViewportColorBuffer, FReadSurfaceDataFlags());

			// Allow the directshow thread to encode more data if the event is still available
			if ( GCaptureSyncEvent )
			{
				GCaptureSyncEvent->Trigger();

				UE_LOG(LogMovieCapture, Log, TEXT("-----------------START------------------"));
				UE_LOG(LogMovieCapture, Log, TEXT(" INCREASE FrameNumber from %d "), FrameNumber);
				FrameNumber++;
			}
		}
		else if (bReadyForCapture)
		{
			CaptureViewport->ReadPixels(ViewportColorBuffer, FReadSurfaceDataFlags());

			if ( GCaptureSyncEvent )
			{
				// Allow the directshow thread to process the pixels we just read
				GCaptureSyncEvent->Trigger();
				Control->Run();
				bReadyForCapture = false;
				bCapturing = true;
				UE_LOG(LogMovieCapture, Log, TEXT("-----------------START------------------"));
				UE_LOG(LogMovieCapture, Log, TEXT(" INCREASE FrameNumber from %d "), FrameNumber);
				FrameNumber++;
			}
		}
	}

private:
	IGraphBuilder* Graph;
	IMediaControl* Control;
	ICaptureGraphBuilder2* Capture;
	FCapturePin* CapturePin;
	IBaseFilter* CaptionSource;
};

#elif PLATFORM_MAC && !UE_BUILD_MINIMAL

#import <AVFoundation/AVFoundation.h>

/**
 * Mac implementation relying on AVFoundation.
 */
class FAVIWriterMac : public FAVIWriter
{
public:
	FAVIWriterMac( )
	: AVFWriterRef(nil)
	, AVFWriterInputRef(nil)
	, AVFPixelBufferAdaptorRef(nil)
	{
		
	};
	
public:
	
	void StartCapture(FViewport* Viewport , const FString& OutputPath /*=FString()*/)
	{
		SCOPED_AUTORELEASE_POOL;
		if (!bCapturing)
		{
			if (!Viewport)
			{
				Viewport = GEngine->GameViewport != NULL ? GEngine->GameViewport->Viewport : NULL;
				if (!Viewport)
				{
					UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Could not get a valid viewport for capture!" ));
					return;
				}
			}
			
			if ( Viewport->GetSizeXY().GetMin() <= 0)
			{
				UE_LOG(LogAVIWriter, Error, TEXT( "ERROR - Attempting to record a 0 sized viewport!" ));
				return;
			}
			
			CaptureViewport = Viewport;
			
			ViewportColorBuffer.AddUninitialized( Viewport->GetSizeXY().X * Viewport->GetSizeXY().Y );
			
			// Attempt to make the dir if it doesn't exist.
			TCHAR File[MAX_SPRINTF] = TEXT("");
			if (OutputPath.IsEmpty())
			{
				FString OutputDir = FPaths::VideoCaptureDir();
				IFileManager::Get().MakeDirectory(*OutputDir, true);
				
				if (GEngine->MatineeScreenshotOptions.bStartWithMatineeCapture)
				{
					bool bFoundUnusedName = false;
					while (!bFoundUnusedName)
					{
						const FString FileNameInfo = FString::Printf( TEXT("_%dfps_%dx%d"),
																	 GEngine->MatineeScreenshotOptions.MatineeCaptureFPS, CaptureViewport->GetSizeXY().X, CaptureViewport->GetSizeXY().Y );
						if (GEngine->MatineeScreenshotOptions.bCompressMatineeCapture)
						{
							FCString::Sprintf( File, TEXT("%s_%d.mov"), *(OutputDir + TEXT("/") + GEngine->MatineeScreenshotOptions.MatineePackageCaptureName + "_" + GEngine->MatineeScreenshotOptions.MatineeCaptureName + FileNameInfo + "_compressed"), MovieCaptureIndex );
						}
						else
						{
							FCString::Sprintf( File, TEXT("%s_%d.mov"), *(OutputDir + TEXT("/") + GEngine->MatineeScreenshotOptions.MatineePackageCaptureName + "_" + GEngine->MatineeScreenshotOptions.MatineeCaptureName + FileNameInfo), MovieCaptureIndex );
						}
						if (IFileManager::Get().FileSize(File) != -1)
						{
							MovieCaptureIndex++;
						}
						else
						{
							bFoundUnusedName = true;
						}
					}
				}
				else
				{
					bool bFoundUnusedName = false;
					while (!bFoundUnusedName)
					{
						FString FileName = FString::Printf( TEXT("Movie%i.mov"), MovieCaptureIndex );
						FCString::Sprintf( File, TEXT("%s"), *(OutputDir / FileName) );
						if (IFileManager::Get().FileSize(File) != -1)
						{
							MovieCaptureIndex++;
						}
						else
						{
							bFoundUnusedName = true;
						}
					}
				}
			}
			else
			{
				FCString::Sprintf( File, TEXT("%s"), *OutputPath );
			}
			
			NSError *Error = nil;
			
			CFStringRef FilePath = FPlatformString::TCHARToCFString(File);
			CFURLRef FileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, FilePath, kCFURLPOSIXPathStyle, false);
			
			// allocate the writer object with our output file URL
			AVFWriterRef = [[AVAssetWriter alloc] initWithURL:(NSURL*)FileURL fileType:AVFileTypeQuickTimeMovie error:&Error];
			
			CFRelease(FilePath);
			CFRelease(FileURL);
			if (Error) {
				UE_LOG(LogMovieCapture, Error, TEXT(" AVAssetWriter initWithURL failed "));
				return;
			}
			
			NSDictionary* VideoSettings = nil;
			if (GEngine->MatineeScreenshotOptions.bCompressMatineeCapture && GEngine->MatineeScreenshotOptions.bStartWithMatineeCapture)
			{
				VideoSettings = [NSDictionary dictionaryWithObjectsAndKeys:
								 AVVideoCodecH264, AVVideoCodecKey,
								 [NSNumber numberWithInt:Viewport->GetSizeXY().X], AVVideoWidthKey,
								 [NSNumber numberWithInt:Viewport->GetSizeXY().Y], AVVideoHeightKey,
								 nil];
			}
			else
			{
				VideoSettings = [NSDictionary dictionaryWithObjectsAndKeys:
								 AVVideoCodecJPEG, AVVideoCodecKey,
								 [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithInt:1], AVVideoQualityKey, nil], AVVideoCompressionPropertiesKey,
								 [NSNumber numberWithInt:Viewport->GetSizeXY().X], AVVideoWidthKey,
								 [NSNumber numberWithInt:Viewport->GetSizeXY().Y], AVVideoHeightKey,
								 nil];
			}
			AVFWriterInputRef = [[AVAssetWriterInput
								  assetWriterInputWithMediaType:AVMediaTypeVideo
								  outputSettings:VideoSettings] retain];
			NSDictionary* BufferAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
											  [NSNumber numberWithInt:kCVPixelFormatType_32BGRA], kCVPixelBufferPixelFormatTypeKey, nil];
			
			AVFPixelBufferAdaptorRef = [[AVAssetWriterInputPixelBufferAdaptor
										 assetWriterInputPixelBufferAdaptorWithAssetWriterInput:AVFWriterInputRef
										 sourcePixelBufferAttributes:BufferAttributes] retain];
			check(AVFWriterInputRef);
			check([AVFWriterRef canAddInput:AVFWriterInputRef]);
			[AVFWriterRef addInput:AVFWriterInputRef];
			
			//Start a session:
			[AVFWriterInputRef setExpectsMediaDataInRealTime:YES];
			[AVFWriterRef startWriting];
			[AVFWriterRef startSessionAtSourceTime:kCMTimeZero];
			
			MovieCaptureIndex++;
			bReadyForCapture = true;
			bCapturing = true;
		}
	}
	
	void StopCapture()
	{
		if (bCapturing)
		{
			SCOPED_AUTORELEASE_POOL;
			[AVFWriterInputRef markAsFinished];
			// This will finish asynchronously and then destroy the relevant objects.
			// We must wait for this to complete.
			FEvent* Event = FPlatformProcess::GetSynchEventFromPool(true);
			[AVFWriterRef finishWritingWithCompletionHandler:^{
				check(AVFWriterRef.status == AVAssetWriterStatusCompleted);
				Event->Trigger();
			}];
			Event->Wait(~0u);
			FPlatformProcess::ReturnSynchEventToPool(Event);
			Event = nullptr;
			[AVFWriterInputRef release];
			[AVFWriterRef release];
			[AVFPixelBufferAdaptorRef release];
			AVFWriterInputRef = nil;
			AVFWriterRef = nil;
			AVFPixelBufferAdaptorRef = nil;
			
			if (CaptureViewport)
			{
				ViewportColorBuffer.Empty();
			}
			MovieCaptureIndex = 0;
			bCapturing = false;
			bReadyForCapture = false;
			bMatineeFinished = false;
			CaptureViewport = NULL;
			CaptureSlateRenderer = NULL;
			FrameNumber = 0;
			CurrentAccumSeconds = 0.f;
		}
	}
	
	void Close()
	{
		StopCapture();
	}
	
	void Update(float DeltaSeconds)
	{
		bool bShouldUpdate = true;
		const float CaptureFrequency = 1.0f/(float)GEngine->MatineeScreenshotOptions.MatineeCaptureFPS;
		if (CaptureFrequency > 0.f)
		{
			CurrentAccumSeconds += DeltaSeconds;
			
			if (CurrentAccumSeconds > CaptureFrequency)
			{
				while (CurrentAccumSeconds > CaptureFrequency)
				{
					CurrentAccumSeconds -= CaptureFrequency;
				}
			}
			else
			{
				bShouldUpdate = false;
			}
		}
		
		if (GIsRequestingExit || !bShouldUpdate || CaptureSlateRenderer)
		{
			return;
		}
		
		if (bMatineeFinished)
		{
			if (GEngine->MatineeScreenshotOptions.MatineeCaptureType == 0)
			{
				if (CaptureViewport)
				{
					CaptureViewport->GetClient()->CloseRequested(CaptureViewport);
				}
			}
			else
			{
				FViewport* ViewportUsed = GEngine->GameViewport != NULL ? GEngine->GameViewport->Viewport : NULL;
				if (ViewportUsed)
				{
					ViewportUsed->GetClient()->CloseRequested(ViewportUsed);
				}
			}
			StopCapture();
		}
		else if (bCapturing && bReadyForCapture && [AVFWriterInputRef isReadyForMoreMediaData])
		{
			SCOPED_AUTORELEASE_POOL;
			CaptureViewport->ReadPixels(ViewportColorBuffer, FReadSurfaceDataFlags());
			
			CVPixelBufferRef PixeBuffer = NULL;
			CVPixelBufferPoolCreatePixelBuffer (NULL, AVFPixelBufferAdaptorRef.pixelBufferPool, &PixeBuffer);
			if(!PixeBuffer)
			{
				CVPixelBufferCreate(kCFAllocatorDefault, CaptureViewport->GetSizeXY().X, CaptureViewport->GetSizeXY().Y, kCVPixelFormatType_32BGRA, NULL, &PixeBuffer);
			}
			check(PixeBuffer);
			
			CVPixelBufferLockBaseAddress(PixeBuffer, 0);
			void* Data = CVPixelBufferGetBaseAddress(PixeBuffer);
			FMemory::Memcpy(Data, ViewportColorBuffer.GetData(), ViewportColorBuffer.Num()*sizeof(FColor));
			CVPixelBufferUnlockBaseAddress(PixeBuffer, 0);
			
            CMTime PresentTime = FrameNumber > 0 ? CMTimeMake(FrameNumber, GEngine->MatineeScreenshotOptions.MatineeCaptureFPS) : kCMTimeZero;
			BOOL OK = [AVFPixelBufferAdaptorRef appendPixelBuffer:PixeBuffer withPresentationTime:PresentTime];
			check(OK);
			
			CVPixelBufferRelease(PixeBuffer);
			
			UE_LOG(LogMovieCapture, Log, TEXT("-----------------START------------------"));
			UE_LOG(LogMovieCapture, Log, TEXT(" INCREASE FrameNumber from %d "), FrameNumber);
			FrameNumber++;
		}
	}
	
private:
	AVAssetWriter* AVFWriterRef;
	AVAssetWriterInput* AVFWriterInputRef;
	AVAssetWriterInputPixelBufferAdaptor* AVFPixelBufferAdaptorRef;
};
#endif

FAVIWriter* FAVIWriter::GetInstance()
{
#if PLATFORM_WINDOWS && !UE_BUILD_MINIMAL
	static FAVIWriterWin Instance;
	return &Instance;
#elif PLATFORM_MAC && !UE_BUILD_MINIMAL
	static FAVIWriterMac Instance;
	return &Instance;
#else
	return NULL;
#endif
}
