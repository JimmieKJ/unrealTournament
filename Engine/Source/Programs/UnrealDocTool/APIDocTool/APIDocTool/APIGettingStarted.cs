// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	class APIGettingStarted : APIPage
	{
		public APIGettingStarted(APIPage InParent)
			: base(InParent, "QuickStart")
		{
		}

		public void WriteSitemapContents(string OutputPath)
		{
		}

		public override void GatherReferencedPages(List<APIPage> ReferencedPages)
		{
		}

		public override void AddToManifest(UdnManifest Manifest)
		{
		}

		public override void WritePage(UdnManifest Manifest, string OutputPath)
		{
			using (UdnWriter Writer = new UdnWriter(OutputPath))
			{
				Writer.WritePageHeader("Getting started with the Unreal Engine API", PageCrumbs, "Getting started with the Unreal Engine API");

				Writer.WriteHeading(2, "Orientation");

				Writer.WriteLine("Games, programs and the Unreal Editor are all *targets* built by UnrealBuildTool. Each target is compiled from C++ *modules*, each implementing a particular area of functionality. Your game is a target, and your game code is implemented in one or more modules.");
				Writer.WriteLine();
				Writer.WriteLine("Code in each module can use other modules by referencing them in their *build rules*. Build rules for each module are given by C# scripts with the .build.cs extension. *Target rules* are given by C# scripts with the .target.cs extension.");
				Writer.WriteLine();
				Writer.WriteLine(Manifest, "Modules supplied with the Unreal Engine are divided into three categories; the {Runtime|ModuleIndex:Runtime Modules}, functionality for the {Editor|ModuleIndex:Editor Modules}, and {Developer|ModuleIndex:Developer Modules} utilities.");
				Writer.WriteLine(Manifest, "Most gameplay programming just uses runtime modules, and the three most commonly encountered are {Core|Filter:Core}, {CoreUObject|Filter:CoreUObject} and {Engine|Filter:Engine}.");
				Writer.WriteLine();

				Writer.WriteHeading(2, "Core");

				Writer.WriteLine(Manifest, "The **{Core|Filter:Core}** module provides a common framework for Unreal modules to communicate; a standard set of types, a {math library|Filter:Core.Math}, a {container|Filter:Core.Containers} library, and a lot of the hardware abstraction that allows Unreal to run on so many platforms.");
				Writer.WriteLine(Manifest, "Some common topics are listed below; full documentation is available {here|Filter:Core}.");
				Writer.WriteLine();
				List<UdnListItem> CoreItems = new List<UdnListItem>();
				CoreItems.Add(new UdnListItem("Basic types:", Manifest.FormatString("bool &middot; float/double &middot; {int8}/{int16}/{int32}/{int64} &middot; {uint8}/{uint16}/{uint32}/{uint64} &middot; {ANSICHAR} &middot; {TCHAR} &middot; {FString}"), null));
				CoreItems.Add(new UdnListItem("Math:", Manifest.FormatString("{FMath} &middot; {FVector} &middot; {FRotator} &middot; {FTransform} &middot; {FMatrix} &middot; {More...|Filter:Core.Math}"), null));
				CoreItems.Add(new UdnListItem("Containers:", Manifest.FormatString("{TArray} &middot; {TList} &middot; {TMap} &middot; {More...|Filter:Core.Containers}"), null));
				CoreItems.Add(new UdnListItem("Other:", Manifest.FormatString("{FName} &middot; {FArchive} &middot; {FOutputDevice}"), null));
				Writer.WriteList(CoreItems);
				
				Writer.WriteHeading(2, "CoreUObject");

				Writer.WriteLine(Manifest, "The **{CoreUObject|Filter:CoreUObject}** module defines UObject, the base class for all managed objects in Unreal. Managed objects are key to integrating with the editor, for serialization, network replication, and runtime type information. UObject derived classes are garbage collected.");
				Writer.WriteLine(Manifest, "Some common topics are listed below; full documentation is available {here|Filter:CoreUObject}.");
				Writer.WriteLine();
				List<UdnListItem> CoreUObjectItems = new List<UdnListItem>();
				CoreUObjectItems.Add(new UdnListItem("Classes:", Manifest.FormatString("{UObject} &middot; {UClass} &middot; {UProperty} &middot; {UPackage}"), null));
				CoreUObjectItems.Add(new UdnListItem("Functions:", Manifest.FormatString("{ConstructObject} &middot; {FindObject} &middot; {Cast} &middot; {CastChecked}"), null));
				Writer.WriteList(CoreUObjectItems);

				Writer.WriteHeading(2, "Engine");

				Writer.WriteLine(Manifest, "The **{Engine|Filter:Engine}** module contains functionality you’d associate with a game. The game world, actors, characters, physics and special effects are all defined here.");
				Writer.WriteLine(Manifest, "Some common topics are listed below; full documentation is available {here|Filter:Engine}.");
				Writer.WriteLine();
				List<UdnListItem> EngineItems = new List<UdnListItem>();
				EngineItems.Add(new UdnListItem("Actors:", Manifest.FormatString("{AActor} &middot; {AVolume} &middot; {AGameMode} &middot; {AHUD} &middot; {More...|Hierarchy:AActor}"), null));
				EngineItems.Add(new UdnListItem("Pawns:", Manifest.FormatString("{APawn} &middot; {ACharacter} &middot; {AWheeledVehicle}"), null));
				EngineItems.Add(new UdnListItem("Controllers:", Manifest.FormatString("{AController} &middot; {AAIController} &middot; {APlayerController}"), null));
				EngineItems.Add(new UdnListItem("Components:", Manifest.FormatString("{UActorComponent} &middot; {UBrainComponent} &middot; {UInputComponent} &middot; {USkeletalMeshComponent} &middot; {UParticleSystemComponent} &middot; {More...|Hierarchy:UActorComponent}"), null));
				EngineItems.Add(new UdnListItem("Gameplay:", Manifest.FormatString("{UPlayer} &middot; {ULocalPlayer} &middot; {UWorld} &middot; {ULevel}"), null));
				EngineItems.Add(new UdnListItem("Assets:", Manifest.FormatString("{UTexture} &middot; {UMaterial} &middot; {UStaticMesh} &middot; {USkeletalMesh} &middot; {UParticleSystem}"), null));
				Writer.WriteList(EngineItems);
			}
		}
	}
}
