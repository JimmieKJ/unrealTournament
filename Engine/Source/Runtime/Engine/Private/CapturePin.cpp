// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CapturePin.cpp: CapturePin implementation
=============================================================================*/

#include "EnginePrivate.h"

#include "CapturePin.h"
#include "AVIWriter.h"

DEFINE_LOG_CATEGORY(LogMovieCapture);
#if PLATFORM_WINDOWS && !UE_BUILD_MINIMAL



FCapturePin::FCapturePin(HRESULT *phr, CSource *pFilter)
        : CSourceStream(NAME("Push Source"), phr, pFilter, L"Out"),
		FramesWritten(0),
		bZeroMemory(0),
		FrameLength(UNITS/GEngine->MatineeScreenshotOptions.MatineeCaptureFPS),
		CurrentBitDepth(32)
{
	// Get the dimensions of the window
	Screen.left   = Screen.top = 0;
	Screen.right  = FAVIWriter::GetInstance()->GetWidth();
	Screen.bottom = FAVIWriter::GetInstance()->GetHeight();

	ImageWidth  = Screen.right  - Screen.left;
	ImageHeight = Screen.bottom - Screen.top;
}

FCapturePin::~FCapturePin()
{   

}


//
// GetMediaType
//
// Prefer 5 formats - 8, 16 (*2), 24 or 32 bits per pixel
//
// Prefered types should be ordered by quality, with zero as highest quality.
// Therefore, iPosition =
//      0    Return a 32bit mediatype
//      1    Return a 24bit mediatype
//      2    Return 16bit RGB565
//      3    Return a 16bit mediatype (rgb555)
//      4    Return 8 bit palettised format
//      >4   Invalid
//
HRESULT FCapturePin::GetMediaType(int32 iPosition, CMediaType *pmt)
{
	CheckPointer(pmt,E_POINTER);
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	if(iPosition < 0)
	{
		return E_INVALIDARG;
	}

	// Have we run off the end of types?
	if(iPosition > 4)
	{
		return VFW_S_NO_MORE_ITEMS;
	}

	VIDEOINFO *pvi = (VIDEOINFO *) pmt->AllocFormatBuffer(sizeof(VIDEOINFO));
	if(NULL == pvi)
	{
		return(E_OUTOFMEMORY);
	}

	// Initialize the VideoInfo structure before configuring its members
	ZeroMemory(pvi, sizeof(VIDEOINFO));

	switch(iPosition)
	{
		case 0:
		{    
			// Return our highest quality 32bit format

			// Since we use RGB888 (the default for 32 bit), there is
			// no reason to use BI_BITFIELDS to specify the RGB
			// masks. Also, not everything supports BI_BITFIELDS
			pvi->bmiHeader.biCompression = BI_RGB;
			pvi->bmiHeader.biBitCount    = 32;
			break;
		}

		case 1:
		{   // Return our 24bit format
			pvi->bmiHeader.biCompression = BI_RGB;
			pvi->bmiHeader.biBitCount    = 24;
			break;
		}

		case 2:
		{       
			// 16 bit per pixel RGB565

			// Place the RGB masks as the first 3 doublewords in the palette area
			for(int32 i = 0; i < 3; i++)
			{
				pvi->TrueColorInfo.dwBitMasks[i] = bits565[i];
			}

			pvi->bmiHeader.biCompression = BI_BITFIELDS;
			pvi->bmiHeader.biBitCount    = 16;
			break;
		}

		case 3:
		{   // 16 bits per pixel RGB555

			// Place the RGB masks as the first 3 doublewords in the palette area
			for(int32 i = 0; i < 3; i++)
			{
				pvi->TrueColorInfo.dwBitMasks[i] = bits555[i];
			}

			pvi->bmiHeader.biCompression = BI_BITFIELDS;
			pvi->bmiHeader.biBitCount    = 16;
			break;
		}

		case 4:
		{   // 8 bit palettised

			pvi->bmiHeader.biCompression = BI_RGB;
			pvi->bmiHeader.biBitCount    = 8;
			pvi->bmiHeader.biClrUsed     = iPALETTE_COLORS;
			break;
		}
	}

	// Adjust the parameters common to all formats
	pvi->bmiHeader.biSize       = sizeof(BITMAPINFOHEADER);
	pvi->bmiHeader.biWidth      = ImageWidth;
	pvi->bmiHeader.biHeight     = ImageHeight * (-1); // negative 1 to flip the image vertically
	pvi->bmiHeader.biPlanes     = 1;
	pvi->bmiHeader.biSizeImage  = GetBitmapSize(&pvi->bmiHeader);
	pvi->bmiHeader.biClrImportant = 0;
	pvi->AvgTimePerFrame = FrameLength;

	SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
	SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

	pmt->SetType(&MEDIATYPE_Video);
	pmt->SetFormatType(&FORMAT_VideoInfo);
	pmt->SetTemporalCompression(false);

	// Work out the GUID for the subtype from the header info.
	const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
	pmt->SetSubtype(&SubTypeGUID);
	pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);

	return NOERROR;

} // GetMediaType


//
// CheckMediaType
//
// We will accept 8, 16, 24 or 32 bit video formats, in any
// image size that gives room to bounce.
// Returns E_INVALIDARG if the mediatype is not acceptable
//
HRESULT FCapturePin::CheckMediaType(const CMediaType *pMediaType)
{
	CheckPointer(pMediaType,E_POINTER);

	if((*(pMediaType->Type()) != MEDIATYPE_Video) ||   // we only output video
		!(pMediaType->IsFixedSize()))                  // in fixed size samples
	{                                                  
		return E_INVALIDARG;
	}

	// Check for the subtypes we support
	const GUID *SubType = pMediaType->Subtype();
	if (SubType == NULL)
	{
		return E_INVALIDARG;
	}

	if(    (*SubType != MEDIASUBTYPE_RGB8)
		&& (*SubType != MEDIASUBTYPE_RGB565)
		&& (*SubType != MEDIASUBTYPE_RGB555)
		&& (*SubType != MEDIASUBTYPE_RGB24)
		&& (*SubType != MEDIASUBTYPE_RGB32))
	{
		return E_INVALIDARG;
	}

	// Get the format area of the media type
	VIDEOINFO *pvi = (VIDEOINFO *) pMediaType->Format();

	if(pvi == NULL)
	{
		return E_INVALIDARG;
	}

	// Check if the image width & height have changed
	if(    pvi->bmiHeader.biWidth   != ImageWidth || 
		abs(pvi->bmiHeader.biHeight) != ImageHeight)
	{
		// If the image width/height is changed, fail CheckMediaType() to force
		// the renderer to resize the image.
		return E_INVALIDARG;
	}

	return S_OK;  // This format is acceptable.

} // CheckMediaType


//
// DecideBufferSize
//
// This will always be called after the format has been successfully
// negotiated. So we have a look at m_mt to see what size image we agreed.
// Then we can ask for buffers of the correct size to contain them.
//
HRESULT FCapturePin::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
	CheckPointer(pAlloc,E_POINTER);
	CheckPointer(pProperties,E_POINTER);

	CAutoLock cAutoLock(m_pFilter->pStateLock());
	HRESULT hr = NOERROR;

	VIDEOINFO *pvi = (VIDEOINFO *) m_mt.Format();
	pProperties->cBuffers = 1;
	pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;

	check(pProperties->cbBuffer);

	// Ask the allocator to reserve us some sample memory. NOTE: the function
	// can succeed (return NOERROR) but still not have allocated the
	// memory that we requested, so we must check we got whatever we wanted.
	ALLOCATOR_PROPERTIES Actual;
	hr = pAlloc->SetProperties(pProperties,&Actual);
	if(FAILED(hr))
	{
		return hr;
	}

	// Is this allocator unsuitable?
	if(Actual.cbBuffer < pProperties->cbBuffer)
	{
		return E_FAIL;
	}

	check(Actual.cBuffers == 1);
	return NOERROR;

} // DecideBufferSize


//
// SetMediaType
//
// Called when a media type is agreed between filters
//
HRESULT FCapturePin::SetMediaType(const CMediaType *pMediaType)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	// Pass the call up to my base class
	HRESULT hr = CSourceStream::SetMediaType(pMediaType);

	if(SUCCEEDED(hr))
	{
		VIDEOINFO * pvi = (VIDEOINFO *) m_mt.Format();
		if (pvi == NULL)
		{
			return E_UNEXPECTED;
		}

		switch(pvi->bmiHeader.biBitCount)
		{
			case 8:     // 8-bit palettized
			case 16:    // RGB565, RGB555
			case 24:    // RGB24
			case 32:    // RGB32
				// Save the current media type and bit depth
				MediaType = *pMediaType;
				CurrentBitDepth = pvi->bmiHeader.biBitCount;
				hr = S_OK;
				break;

			default:
				// We should never agree any other media types
				check(false);
				hr = E_INVALIDARG;
				break;
		}
	} 

	return hr;

} // SetMediaType


// This is where we insert the DIB bits into the video stream.
// FillBuffer is called once for every sample in the stream.
HRESULT FCapturePin::FillBuffer(IMediaSample *pSample)
{
	uint8 *pData;
	long cbData;

	CheckPointer(pSample, E_POINTER);

	CAutoLock cAutoLockShared(&SharedState);

	// Access the sample's data buffer
	pSample->GetPointer(&pData);
	cbData = pSample->GetSize();

	// Check that we're still using video
	check(m_mt.formattype == FORMAT_VideoInfo);

	VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)m_mt.pbFormat;

	if (pData)
	{
		uint32 sizeInBytes = FAVIWriter::GetInstance()->GetWidth() * FAVIWriter::GetInstance()->GetHeight() * sizeof(FColor);
		const TArray<FColor>& Buffer = FAVIWriter::GetInstance()->GetColorBuffer();
		uint32 smallest = FMath::Min((uint32)pVih->bmiHeader.biSizeImage, (uint32)cbData);
		// Copy the DIB bits over into our filter's output buffer.
		// Since sample size may be larger than the image size, bound the copy size
		FMemory::Memcpy(pData, Buffer.GetData(), FMath::Min(smallest, sizeInBytes));
	}

	// Set the timestamps that will govern playback frame rate.
	// set the Average Time Per Frame for the AVI Header.
	// The current time is the sample's start.

	int32 FrameNumber = FAVIWriter::GetInstance()->GetFrameNumber();
	UE_LOG(LogMovieCapture, Log, TEXT(" FillBuffer: FrameNumber = %d  FramesWritten = %d"), FrameNumber, FramesWritten);

	REFERENCE_TIME Start = FramesWritten * FrameLength;
	REFERENCE_TIME Stop  = Start + FrameLength;
	FramesWritten++;

	UE_LOG(LogMovieCapture, Log, TEXT(" FillBuffer: (%d, %d)"), Start, Stop);
	UE_LOG(LogMovieCapture, Log, TEXT("-----------------END------------------"));

	pSample->SetTime(&Start, &Stop);

	// Set true on every sample for uncompressed frames
	pSample->SetSyncPoint(true);

	return S_OK;
}

// the loop executed while running
HRESULT FCapturePin::DoBufferProcessingLoop(void) 
{
	Command com;

	OnThreadStartPlay();
	int32 LastFrame = -1;

	do 
	{
		while (!CheckRequest(&com)) 
		{
			// Wait for the next frame from the game thread 
			if ( !GCaptureSyncEvent->Wait(1000) )
			{
				FPlatformProcess::Sleep( 0.01f );
				continue;	// Reevaluate request
			}

			IMediaSample *pSample;
			int32 FrameNumber = FAVIWriter::GetInstance()->GetFrameNumber();
			if (FrameNumber > LastFrame)
			{
				UE_LOG(LogMovieCapture, Log, TEXT(" FrameNumber > LastFrame = %d > %d"), FrameNumber, LastFrame);
				HRESULT hr = GetDeliveryBuffer(&pSample,NULL,NULL,0);
				if (FAILED(hr)) 
				{
					if (pSample)
					{
						pSample->Release();
					}
				}
				else
				{
					LastFrame = FrameNumber;
					hr = FillBuffer(pSample);

					if (hr == S_OK) 
					{
						hr = Deliver(pSample);
						pSample->Release();
						// downstream filter returns S_FALSE if it wants us to
						// stop or an error if it's reporting an error.
						if(hr != S_OK)
						{
							UE_LOG(LogMovieCapture, Log, TEXT("Deliver() returned %08x; stopping"), hr);
							return S_OK;
						}
					}
				}
			}
			// Allow the game thread read more data
			GCaptureSyncEvent->Trigger();
		}

		// For all commands sent to us there must be a Reply call!
		if (com == CMD_RUN || com == CMD_PAUSE) 
		{
			Reply(NOERROR);
		} 
		else if (com != CMD_STOP) 
		{
			Reply((uint32) E_UNEXPECTED);
		}
	} while (com != CMD_STOP);

	return S_FALSE;
}

#endif //#if PLATFORM_WINDOWS
