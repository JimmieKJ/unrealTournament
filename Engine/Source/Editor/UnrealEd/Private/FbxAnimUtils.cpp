// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "FbxAnimUtils.h"
#include "FbxExporter.h"

namespace FbxAnimUtils
{
	void ExportAnimFbx( const FString& ExportFilename, UAnimSequence* AnimSequence, USkeletalMesh* Mesh, bool bSaveSkeletalMesh )
	{
		if( !ExportFilename.IsEmpty() && AnimSequence && Mesh )
		{
			FString FileName = ExportFilename;
			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::FBX_ANIM, FPaths::GetPath(FileName)); // Save path as default for next time.

			UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();

			Exporter->CreateDocument();

			Exporter->ExportAnimSequence(AnimSequence, Mesh, bSaveSkeletalMesh);

			// Save to disk
			Exporter->WriteToFile( *ExportFilename );
		}
	}
}