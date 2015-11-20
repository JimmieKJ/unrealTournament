// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaPrivatePCH.h"
#include "AllowWindowsPlatformTypes.h"


#define LOCTEXT_NAMESPACE "FWmfMediaTrack"


/* FWmfMediaTrack structors
 *****************************************************************************/

FWmfMediaTrack::FWmfMediaTrack( IMFPresentationDescriptor* InPresentationDescriptor, FWmfMediaSampler* InSampler, IMFStreamDescriptor* InStreamDescriptor, DWORD InStreamIndex )
	: PresentationDescriptor(InPresentationDescriptor)
	, Sampler(InSampler)
	, StreamDescriptor(InStreamDescriptor)
	, StreamIndex(InStreamIndex)
{
	PWSTR LanguageString = NULL;

	if (SUCCEEDED(StreamDescriptor->GetAllocatedString(MF_SD_LANGUAGE, &LanguageString, NULL)))
	{
		Language = LanguageString;
		CoTaskMemFree(LanguageString);
	}

	PWSTR NameString = NULL;

	if (SUCCEEDED(StreamDescriptor->GetAllocatedString(MF_SD_STREAM_NAME, &NameString, NULL)))
	{
		Name = NameString;
		CoTaskMemFree(NameString);
	}

	Protected = ::MFGetAttributeUINT32(StreamDescriptor, MF_SD_PROTECTED, FALSE) != 0;
}


/* IMediaStream interface
 *****************************************************************************/

void FWmfMediaTrack::AddSink( const IMediaSinkRef& Sink )
{
	Sampler->RegisterSink(Sink);
}


bool FWmfMediaTrack::Disable()
{
	return ((PresentationDescriptor != NULL) && SUCCEEDED(PresentationDescriptor->DeselectStream(StreamIndex)));
}


bool FWmfMediaTrack::Enable()
{
	return ((PresentationDescriptor != NULL) && SUCCEEDED(PresentationDescriptor->SelectStream(StreamIndex)));
}


FText FWmfMediaTrack::GetDisplayName() const
{
	FText DisplayName;

	if (Name.IsEmpty())
	{
		DisplayName = FText::Format(LOCTEXT("UnnamedStreamFormat", "Unnamed Stream {0}"), FText::AsNumber((uint32)StreamIndex));
	}
	else
	{
		DisplayName = FText::FromString(Name);
	}

	if (Language.IsEmpty())
	{
		return DisplayName;
	}

	return FText::Format(LOCTEXT("LocalizedTrackFormat", "{0} ({1})"), DisplayName, FText::FromString(Language));
}


FString FWmfMediaTrack::GetLanguage() const
{
	return Language;
}


FString FWmfMediaTrack::GetName() const
{
	return Name;
}


bool FWmfMediaTrack::IsEnabled() const
{
	BOOL Selected = FALSE;
	PresentationDescriptor->GetStreamDescriptorByIndex(StreamIndex, &Selected, NULL);

	return (Selected == TRUE);
}


bool FWmfMediaTrack::IsMutuallyExclusive( const IMediaStreamRef& Other ) const
{
	return false;
}


bool FWmfMediaTrack::IsProtected() const
{
	return Protected;
}


void FWmfMediaTrack::RemoveSink( const IMediaSinkRef& Sink )
{
	Sampler->UnregisterSink(Sink);
}


#undef LOCTEXT_NAMESPACE


#include "HideWindowsPlatformTypes.h"
