// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Diagnostics;
using System.Security.AccessControl;
using System.Xml;
using System.Text;
using Ionic.Zip;
using Ionic.Zlib;

namespace UnrealBuildTool
{
	class TVOSToolChain : IOSToolChain
	{
		public TVOSToolChain(FileReference InProjectFile, IOSPlatformContext InPlatformContext)
			: base(CPPTargetPlatform.TVOS, InProjectFile, InPlatformContext)
		{
		}
	};
}
