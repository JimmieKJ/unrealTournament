// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ProjectsPrivatePCH.h"
#include "PluginDescriptor.h"

#define LOCTEXT_NAMESPACE "PluginDescriptor"


FPluginDescriptor::FPluginDescriptor()
	: FileVersion(EPluginDescriptorVersion::Latest)
	, Version(0)
	, bEnabledByDefault(false)
	, bCanContainContent(false)
	, bIsBetaVersion(false)
	, bInstalled(false)
	, bRequiresBuildPlatform(true)
{ 
}


bool FPluginDescriptor::Load( const FString& FileName, FText& OutFailReason )
{
	// Read the file to a string
	FString FileContents;

	if (!FFileHelper::LoadFileToString(FileContents, *FileName))
	{
		OutFailReason = FText::Format(LOCTEXT("FailedToLoadDescriptorFile", "Failed to open descriptor file '{0}'"), FText::FromString(FileName));
		return false;
	}

	// Parse it as a plug-in descriptor
	return Read(FileContents, OutFailReason);
}


bool FPluginDescriptor::Read(const FString& Text, FText& OutFailReason)
{
	// Deserialize a JSON object from the string
	TSharedPtr< FJsonObject > ObjectPtr;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, ObjectPtr) || !ObjectPtr.IsValid() )
	{
		OutFailReason = FText::Format(LOCTEXT("FailedToReadDescriptorFile", "Failed to read file. {0}"), FText::FromString(Reader->GetErrorMessage()));
		return false;
	}
	FJsonObject& Object = *ObjectPtr.Get();

	// Read the file version
	int32 FileVersionInt32;

	if(!Object.TryGetNumberField(TEXT("FileVersion"), FileVersionInt32))
	{
		if(!Object.TryGetNumberField(TEXT("PluginFileVersion"), FileVersionInt32))
		{
			OutFailReason = LOCTEXT("InvalidProjectFileVersion", "File does not have a valid 'FileVersion' number.");
			return false;
		}
	}

	// Check that it's within range
	EPluginDescriptorVersion::Type PluginFileVersion = (EPluginDescriptorVersion::Type)FileVersionInt32;
	if ((PluginFileVersion <= EPluginDescriptorVersion::Invalid) || (PluginFileVersion > EPluginDescriptorVersion::Latest))
	{
		FText ReadVersionText = FText::FromString(FString::Printf(TEXT("%d"), (int32)PluginFileVersion));
		FText LatestVersionText = FText::FromString(FString::Printf(TEXT("%d"), (int32)EPluginDescriptorVersion::Latest));
		OutFailReason = FText::Format( LOCTEXT("ProjectFileVersionTooLarge", "File appears to be in a newer version ({0}) of the file format that we can load (max version: {1})."), ReadVersionText, LatestVersionText);
		return false;
	}

	// Read the other fields
	Object.TryGetNumberField(TEXT("Version"), Version);
	Object.TryGetStringField(TEXT("VersionName"), VersionName);
	Object.TryGetStringField(TEXT("FriendlyName"), FriendlyName);
	Object.TryGetStringField(TEXT("Description"), Description);

	if (!Object.TryGetStringField(TEXT("Category"), Category))
	{
		// Category used to be called CategoryPath in .uplugin files
		Object.TryGetStringField(TEXT("CategoryPath"), Category);
	}
        
	// Due to a difference in command line parsing between Windows and Mac, we shipped a few Mac samples containing
	// a category name with escaped quotes. Remove them here to make sure we can list them in the right category.
	if (Category.Len() >= 2 && Category.StartsWith(TEXT("\"")) && Category.EndsWith(TEXT("\"")))
	{
		Category = Category.Mid(1, Category.Len() - 2);
	}

	Object.TryGetStringField(TEXT("CreatedBy"), CreatedBy);
	Object.TryGetStringField(TEXT("CreatedByURL"), CreatedByURL);
	Object.TryGetStringField(TEXT("DocsURL"), DocsURL);
	Object.TryGetStringField(TEXT("MarketplaceURL"), MarketplaceURL);
	Object.TryGetStringField(TEXT("SupportURL"), SupportURL);

	if (!FModuleDescriptor::ReadArray(Object, TEXT("Modules"), Modules, OutFailReason))
	{
		return false;
	}

	if (!FLocalizationTargetDescriptor::ReadArray(Object, TEXT("LocalizationTargets"), LocalizationTargets, OutFailReason))
	{
		return false;
	}

	Object.TryGetBoolField(TEXT("EnabledByDefault"), bEnabledByDefault);
	Object.TryGetBoolField(TEXT("CanContainContent"), bCanContainContent);
	Object.TryGetBoolField(TEXT("IsBetaVersion"), bIsBetaVersion);
	Object.TryGetBoolField(TEXT("Installed"), bInstalled);

	if(!Object.TryGetBoolField(TEXT("RequiresBuildPlatform"), bRequiresBuildPlatform))
	{
		bRequiresBuildPlatform = true;
	}

	PreBuildSteps.Read(Object, TEXT("PreBuildSteps"));
	PostBuildSteps.Read(Object, TEXT("PostBuildSteps"));

	return true;
}

bool FPluginDescriptor::Save(const FString& FileName, FText& OutFailReason) const
{
	// Write the contents of the descriptor to a string. Make sure the writer is destroyed so that the contents are flushed to the string.
	FString Text = ToString();
	if ( FFileHelper::SaveStringToFile(Text, *FileName) )
	{
		return true;
	}
	else
	{
		OutFailReason = FText::Format( LOCTEXT("FailedToWriteOutputFile", "Failed to write output file '{0}'. Perhaps the file is Read-Only?"), FText::FromString(FileName) );
		return false;
	}
}

FString FPluginDescriptor::ToString() const
{
	FString Text;
	TSharedRef< TJsonWriter<> > WriterRef = TJsonWriterFactory<>::Create(&Text);
	TJsonWriter<>& Writer = WriterRef.Get();

	Writer.WriteObjectStart();

	Writer.WriteValue(TEXT("FileVersion"), EProjectDescriptorVersion::Latest);
	Writer.WriteValue(TEXT("Version"), Version);
	Writer.WriteValue(TEXT("VersionName"), VersionName);
	Writer.WriteValue(TEXT("FriendlyName"), FriendlyName);
	Writer.WriteValue(TEXT("Description"), Description);
	Writer.WriteValue(TEXT("Category"), Category);
	Writer.WriteValue(TEXT("CreatedBy"), CreatedBy);
	Writer.WriteValue(TEXT("CreatedByURL"), CreatedByURL);
	Writer.WriteValue(TEXT("DocsURL"), DocsURL);
	Writer.WriteValue(TEXT("MarketplaceURL"), MarketplaceURL);
	Writer.WriteValue(TEXT("SupportURL"), SupportURL);
	Writer.WriteValue(TEXT("EnabledByDefault"), bEnabledByDefault);
	Writer.WriteValue(TEXT("CanContainContent"), bCanContainContent);
	Writer.WriteValue(TEXT("IsBetaVersion"), bIsBetaVersion);
	Writer.WriteValue(TEXT("Installed"), bInstalled);

	FModuleDescriptor::WriteArray(Writer, TEXT("Modules"), Modules);

	FLocalizationTargetDescriptor::WriteArray(Writer, TEXT("LocalizationTargets"), LocalizationTargets);

	if(!bRequiresBuildPlatform)
	{
		Writer.WriteValue(TEXT("RequiresBuildPlatform"), bRequiresBuildPlatform);
	}

	if(!PreBuildSteps.IsEmpty())
	{
		PreBuildSteps.Write(Writer, TEXT("PreBuildSteps"));
	}

	if(!PostBuildSteps.IsEmpty())
	{
		PostBuildSteps.Write(Writer, TEXT("PostBuildSteps"));
	}

	Writer.WriteObjectEnd();
	Writer.Close();

	return Text;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FPluginReferenceDescriptor::FPluginReferenceDescriptor( const FString& InName, bool bInEnabled, const FString& InMarketplaceURL )
	: Name(InName)
	, bEnabled(bInEnabled)
	, bOptional(false)
	, MarketplaceURL(InMarketplaceURL)
{ }


bool FPluginReferenceDescriptor::IsEnabledForPlatform( const FString& Platform ) const
{
	// If it's not enabled at all, return false
	if(!bEnabled)
	{
		return false;
	}

	// If there is a list of whitelisted platforms, and this isn't one of them, return false
	if(WhitelistPlatforms.Num() > 0 && !WhitelistPlatforms.Contains(Platform))
	{
		return false;
	}

	// If this platform is blacklisted, also return false
	if(BlacklistPlatforms.Contains(Platform))
	{
		return false;
	}

	return true;
}

bool FPluginReferenceDescriptor::IsEnabledForTarget(const FString& Target) const
{
    // If it's not enabled at all, return false
    if (!bEnabled)
    {
        return false;
    }

    // If there is a list of whitelisted platforms, and this isn't one of them, return false
    if (WhitelistTargets.Num() > 0 && !WhitelistTargets.Contains(Target))
    {
        return false;
    }

    // If this platform is blacklisted, also return false
    if (BlacklistTargets.Contains(Target))
    {
        return false;
    }

    return true;
}

bool FPluginReferenceDescriptor::Read( const FJsonObject& Object, FText& OutFailReason )
{
	// Get the name
	if(!Object.TryGetStringField(TEXT("Name"), Name))
	{
		OutFailReason = LOCTEXT("PluginReferenceWithoutName", "Plugin references must have a 'Name' field");
		return false;
	}

	// Get the enabled field
	if(!Object.TryGetBoolField(TEXT("Enabled"), bEnabled))
	{
		OutFailReason = LOCTEXT("PluginReferenceWithoutEnabled", "Plugin references must have an 'Enabled' field");
		return false;
	}

	// Read the optional field
	Object.TryGetBoolField(TEXT("Optional"), bOptional);

	// Read the metadata for users that don't have the plugin installed
	Object.TryGetStringField(TEXT("Description"), Description);
	Object.TryGetStringField(TEXT("MarketplaceURL"), MarketplaceURL);

	// Get the platform lists
	Object.TryGetStringArrayField(TEXT("WhitelistPlatforms"), WhitelistPlatforms);
	Object.TryGetStringArrayField(TEXT("BlacklistPlatforms"), BlacklistPlatforms);

	// Get the target lists
	Object.TryGetStringArrayField(TEXT("WhitelistTargets"), WhitelistTargets);
	Object.TryGetStringArrayField(TEXT("BlacklistTargets"), BlacklistTargets);

	return true;
}


bool FPluginReferenceDescriptor::ReadArray( const FJsonObject& Object, const TCHAR* Name, TArray<FPluginReferenceDescriptor>& OutPlugins, FText& OutFailReason )
{
	const TArray< TSharedPtr<FJsonValue> > *Array;

	if (Object.TryGetArrayField(Name, Array))
	{
		for (const TSharedPtr<FJsonValue> &Item : *Array)
		{
			const TSharedPtr<FJsonObject> *ObjectPtr;

			if (Item.IsValid() && Item->TryGetObject(ObjectPtr))
			{
				FPluginReferenceDescriptor Plugin;

				if (!Plugin.Read(*ObjectPtr->Get(), OutFailReason))
				{
					return false;
				}

				OutPlugins.Add(Plugin);
			}
		}
	}

	return true;
}


void FPluginReferenceDescriptor::Write( TJsonWriter<>& Writer ) const
{
	Writer.WriteObjectStart();
	Writer.WriteValue(TEXT("Name"), Name);
	Writer.WriteValue(TEXT("Enabled"), bEnabled);

	if (bEnabled && bOptional)
	{
		Writer.WriteValue(TEXT("Optional"), bOptional);
	}

	if (Description.Len() > 0)
	{
		Writer.WriteValue(TEXT("Description"), Description);
	}

	if (MarketplaceURL.Len() > 0)
	{
		Writer.WriteValue(TEXT("MarketplaceURL"), MarketplaceURL);
	}

	if (WhitelistPlatforms.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("WhitelistPlatforms"));

		for (int Idx = 0; Idx < WhitelistPlatforms.Num(); Idx++)
		{
			Writer.WriteValue(WhitelistPlatforms[Idx]);
		}

		Writer.WriteArrayEnd();
	}

	if (BlacklistPlatforms.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("BlacklistPlatforms"));

		for (int Idx = 0; Idx < BlacklistPlatforms.Num(); Idx++)
		{
			Writer.WriteValue(BlacklistPlatforms[Idx]);
		}

		Writer.WriteArrayEnd();
	}

	if (WhitelistTargets.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("WhitelistTargets"));

		for (int Idx = 0; Idx < WhitelistTargets.Num(); Idx++)
		{
			Writer.WriteValue(WhitelistTargets[Idx]);
		}

		Writer.WriteArrayEnd();
	}

	if (BlacklistTargets.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("BlacklistTargets"));

		for (int Idx = 0; Idx < BlacklistTargets.Num(); Idx++)
		{
			Writer.WriteValue(BlacklistTargets[Idx]);
		}

		Writer.WriteArrayEnd();
	}

	Writer.WriteObjectEnd();
}


void FPluginReferenceDescriptor::WriteArray( TJsonWriter<>& Writer, const TCHAR* Name, const TArray<FPluginReferenceDescriptor>& Plugins )
{
	if( Plugins.Num() > 0)
	{
		Writer.WriteArrayStart(Name);

		for (int Idx = 0; Idx < Plugins.Num(); Idx++)
		{
			Plugins[Idx].Write(Writer);
		}

		Writer.WriteArrayEnd();
	}
}


#undef LOCTEXT_NAMESPACE
