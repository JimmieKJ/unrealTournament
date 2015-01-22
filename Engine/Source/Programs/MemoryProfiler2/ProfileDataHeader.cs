/**
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;

namespace MemoryProfiler2
{
	/**
	 * Header written by capture tool
	 */
	public class FProfileDataHeader
	{
		/** Magic number at beginning of data file.					*/
		public const UInt32 ExpectedMagic = 0xDA15F7D8;

		/** Magic to ensure we're opening the right file.			*/
		public UInt32 Magic;
		/** Version number to detect version mismatches.			*/
		public UInt32 Version;
		/** Platform that was captured.								*/
		public EPlatformType Platform;
		/** Platform that was captured.								*/
		public string PlatformName;
		/** Whether symbol information was serialized.				*/
		public bool	bShouldSerializeSymbolInfo;
		/** Name of executable this information was gathered with.	*/
		public string ExecutableName;

		/** Offset in file for name table.							*/
		public UInt64 NameTableOffset;
		/** Number of name table entries.							*/
		public UInt64 NameTableEntries;

		/** Offset in file for callstack address table.				*/
		public UInt64 CallStackAddressTableOffset;
		/** Number of callstack address entries.					*/
		public UInt64 CallStackAddressTableEntries;

		/** Offset in file for callstack table.						*/
		public UInt64 CallStackTableOffset;
		/** Number of callstack entries.							*/
		public UInt64 CallStackTableEntries;
		/** The file offset for module information.					*/
		public UInt64 ModulesOffset;
		/** The number of module entries.							*/
		public UInt64 ModuleEntries;
		/** Number of data files the stream spans.					*/
		public UInt64 NumDataFiles;

		// New in version 3 files

		/** Offset in file for stript callstack table. Requires version 3 or later. */
		public UInt32 ScriptCallstackTableOffset = UInt32.MaxValue;

		/** Offset in file for script name table. Requires version 3 or later. */
		public UInt32 ScriptNameTableOffset = UInt32.MaxValue;

		/** Can/should script callstacks be converted into readable names? */
		public bool bDecodeScriptCallstacks = false;

		/**
		 * Constructor, serializing header from passed in stream.
		 * 
		 * @param	BinaryStream	Stream to serialize header from.
		 */
		public FProfileDataHeader(BinaryReader BinaryStream)
		{
			// Serialize the file format magic first.
			Magic = BinaryStream.ReadUInt32();

			// Stop serializing data if magic number doesn't match. Most likely endian issue.
			if( Magic == ExpectedMagic )
			{
				// Version info for backward compatible serialization.
				Version = BinaryStream.ReadUInt32();
				
				if( Version >= 4 )
				{
					// Read platform name.
					PlatformName = FStreamParser.ReadString( BinaryStream );
					PlatformName = FixFixedSizeString( PlatformName );
				}
				else
				{
					// Read platform type.
					Platform = (EPlatformType)BinaryStream.ReadUInt32();
				}

				// Whether symbol information was serialized.
				bShouldSerializeSymbolInfo = BinaryStream.ReadUInt32() == 0 ? false : true;

				if( Version >= 5 )
				{
					// Name table offset in file and number of entries.
					NameTableOffset = BinaryStream.ReadUInt64();
					NameTableEntries = BinaryStream.ReadUInt64();

					// CallStack address table offset and number of entries.
					CallStackAddressTableOffset = BinaryStream.ReadUInt64();
					CallStackAddressTableEntries = BinaryStream.ReadUInt64();

					// CallStack table offset and number of entries.
					CallStackTableOffset = BinaryStream.ReadUInt64();
					CallStackTableEntries = BinaryStream.ReadUInt64();

					ModulesOffset = BinaryStream.ReadUInt64();
					ModuleEntries = BinaryStream.ReadUInt64();

					NumDataFiles = 1;
				}
				else
				{
					// Name table offset in file and number of entries.
					NameTableOffset = BinaryStream.ReadUInt32();
					NameTableEntries = BinaryStream.ReadUInt32();

					// CallStack address table offset and number of entries.
					CallStackAddressTableOffset = BinaryStream.ReadUInt32();
					CallStackAddressTableEntries = BinaryStream.ReadUInt32();

					// CallStack table offset and number of entries.
					CallStackTableOffset = BinaryStream.ReadUInt32();
					CallStackTableEntries = BinaryStream.ReadUInt32();

					ModulesOffset = BinaryStream.ReadUInt32();
					ModuleEntries = BinaryStream.ReadUInt32();

					// Number of data files the stream spans.
					NumDataFiles = BinaryStream.ReadUInt32();
				}
	

				FStreamToken.Version = Version;
				if( Version >= 4 )
				{
					// Just ignore
				}
				else if (Version > 2)
				{
					ScriptCallstackTableOffset = BinaryStream.ReadUInt32();
					ScriptNameTableOffset = BinaryStream.ReadUInt32();
				
					bDecodeScriptCallstacks = ScriptCallstackTableOffset != UInt32.MaxValue;
				}

				// Name of executable.
				ExecutableName = FStreamParser.ReadString( BinaryStream );
				ExecutableName = FixFixedSizeString( ExecutableName );			
			}
		}

		/// <summary> Trims the null characters that make it in by converting char[] to string. </summary>
		static String FixFixedSizeString( string String )
		{
			int RealLength = 0;
			while( String[RealLength++] != '\0' )
			{
			}
			string Result = String.Remove( RealLength - 1 );
			return Result;
		}
	}

	// Mirrored in UnFile.h
	[Flags]
	public enum EPlatformType
	{
		Unknown = 0x00000000,
		Windows = 0x00000001,
		WindowsServer = 0x00000002,		// Windows platform dedicated server mode ("lean and mean" cooked as console without editor support)
		Xbox360 = 0x00000004,
		PS3 = 0x00000008,
		Linux = 0x00000010,
		MacOSX = 0x00000020,
		WindowsConsole = 0x00000040,     // Windows platform cooked as console without editor support
		IPhone = 0x00000080,
		Android = 0x00000200,

		// Combination Masks
		/** PC platform types */
		PC = Windows | WindowsServer | WindowsConsole | Linux | MacOSX,

		/** Windows platform types */
		AnyWindows = Windows | WindowsServer | WindowsConsole,

		/** Console platform types */
		Console = Xbox360 | PS3 | IPhone | Android,

		/** Mobile platform types */
		Mobile = IPhone | Android,

		/** Platforms with data that has been stripped during cooking */
		Stripped = Console | WindowsServer | WindowsConsole,

		/** Platforms who's vertex data can't be packed into 16-bit floats */
		OpenGLES2 = IPhone | Android,
	}
}