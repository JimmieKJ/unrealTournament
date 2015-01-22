// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;

using Tools.DotNETCommon.ThreadSafeQueue;

namespace Tools.Distill
{
	public partial class Distill
	{

		/// <summary>Find all languages based on a file system search</summary>
		/// <returns>An array of languages found</returns>
		public static string[] FindKnownLanguages()
		{
			DetailLog( "Finding known languages ...", ConsoleColor.Blue );
			List<string> KnownLanguages = new List<string>();

			DirectoryInfo DirInfo = new DirectoryInfo( Path.Combine( Options.Source, "Engine/Content/Localization" ) );
			if( DirInfo.Exists )
			{
				foreach( DirectoryInfo SubDirInfo in DirInfo.GetDirectories( "???" ) )
				{
					if( SubDirInfo.GetFiles( Path.ChangeExtension( "Core", SubDirInfo.Name ) ).Length == 0 )
					{
						continue;
					}

					string KnownLanguage = SubDirInfo.Name.ToUpper();
					KnownLanguages.Add( KnownLanguage );
					DetailLog( " ... adding known language: " + KnownLanguage, ConsoleColor.Blue );
				}
			}

			if( !KnownLanguages.Contains( "INT" ) )
			{
				KnownLanguages.Add( "INT" );
				DetailLog( " ... INT not found; adding INT to known languages ", ConsoleColor.Blue );
			}

			return KnownLanguages.ToArray();
		}

		private static bool CopyRequired( FileInfo SourceFile, FileInfo DestFile )
		{
			if( Options.bNoCopy )
			{
				return false;
			}

			if( Options.bForce )
			{
				return true;
			}

			if( !DestFile.Exists )
			{
				return true;
			}

			if( SourceFile.Length != DestFile.Length )
			{
				return true;
			}

			// Allow for the slop in OSX file time stamps
			if( SourceFile.LastWriteTimeUtc > DestFile.LastWriteTimeUtc.AddSeconds( 2 ) )
			{
				return true;
			}

			return false;
		}

		private class CopyFilePacket
		{
			public FileInfo SourceFile = null;
			public FileInfo DestinationFile = null;
			public string Hash = "";

			public CopyFilePacket( FileInfo InSourceFile, FileInfo InDestinationFile, string InHash )
			{
				SourceFile = InSourceFile;
				DestinationFile = InDestinationFile;
				Hash = InHash;
			}
		}

		private static bool bCopyingFiles = true;
		private static bool bVerifyingFiles = true;
		private static ThreadSafeQueue<CopyFilePacket> CopyFilePackets = new ThreadSafeQueue<CopyFilePacket>();
		private static ThreadSafeQueue<CopyFilePacket> VerifyFilePackets = new ThreadSafeQueue<CopyFilePacket>();

		private static void EnsureFolderExists( DirectoryInfo DirInfo )
		{
			if( DirInfo.Parent != null )
			{
				EnsureFolderExists( DirInfo.Parent );
			}

			if( !DirInfo.Exists )
			{
				DirInfo.Create();
			}
		}

		private static void CopyFilesThreadProc()
		{
			while( bCopyingFiles )
			{
				while( CopyFilePackets.Count > 0 )
				{
					CopyFilePacket Packet = null;
					try
					{
						Packet = CopyFilePackets.Dequeue();

						DetailLog( " ... copying: " + Packet.DestinationFile.FullName, ConsoleColor.DarkMagenta );
						EnsureFolderExists( Packet.DestinationFile.Directory );
						if( Packet.DestinationFile.Exists )
						{
							Packet.DestinationFile.IsReadOnly = false;
						}
						Packet.SourceFile.CopyTo( Packet.DestinationFile.FullName, true );

						if( Packet.Hash.Length > 0 )
						{
							VerifyFilePackets.Enqueue( Packet );
						}
					}
					catch( Exception Ex )
					{
						Warning( "Failed to copy: " + Packet.DestinationFile.FullName + " (" + Ex.Message + ") -- trying again!" );
						Packet.SourceFile.Refresh();
						Packet.DestinationFile.Refresh();

						CopyFilePackets.Enqueue( Packet );
					}
				}
			}
		}

		private static void VerifyFilesThreadProc()
		{
			while( bVerifyingFiles )
			{
				while( VerifyFilePackets.Count > 0 )
				{
					CopyFilePacket Packet = null;
					try
					{
						bool bVerificationSuccessful = true;
						Packet = VerifyFilePackets.Dequeue();

						DetailLog( " ... verifying: " + Packet.DestinationFile.FullName, ConsoleColor.DarkGreen );
						Packet.DestinationFile.Refresh();
						if( !Packet.DestinationFile.Exists )
						{
							Warning( "Failed to locate for verification: " + Packet.DestinationFile.FullName );
							bVerificationSuccessful = false;
						}
						else
						{
							byte[] HashData = Crypto.CreateChecksum( Packet.DestinationFile );
							string NewHash = Convert.ToBase64String( HashData );
							if( NewHash != Packet.Hash )
							{
								Warning( "Verification checksum failed: " + Packet.DestinationFile.FullName + " (" + NewHash + " != " + Packet.Hash + ")" );
								bVerificationSuccessful = false;
							}
						}

						if( !bVerificationSuccessful )
						{
							Packet.SourceFile.Refresh();
							Packet.DestinationFile.Refresh();

							CopyFilePackets.Enqueue( Packet );
						}
					}
					catch( Exception Ex )
					{
						Warning( "Failed to verify: " + Packet.DestinationFile.FullName + " (" + Ex.Message + ") -- trying again!" );
						VerifyFilePackets.Enqueue( Packet );
					}
				}
			}
		}

		/// <summary>Copy all the files in the TOC to the destination folder(s)</summary>
		/// <param name="TableOfContents">Container for all the files to copy</param>
		public static void CopyFiles( TOC TableOfContents )
		{
			if( Options.Destination.Length > 0 )
			{
				long TotalFileCount = 0;
				long TotalBytesCopied = 0;

				bCopyingFiles = true;
				Thread CopyFilesThread = new Thread( CopyFilesThreadProc );
				CopyFilesThread.Start();

				bVerifyingFiles = true;
				Thread VerifyFilesThread = new Thread( VerifyFilesThreadProc );
				VerifyFilesThread.Start();

				foreach( TOCEntry Entry in TableOfContents.Entries )
				{
					// Handle file copies
					FileInfo SourceFile = Entry.Info;
					FileInfo DestFile = new FileInfo( Path.Combine( Options.Destination, Entry.Name ) );

					if( CopyRequired( SourceFile, DestFile ) )
					{
						// Create copy packet and add to queue
						CopyFilePacket Packet = new CopyFilePacket( SourceFile, DestFile, Entry.Hash );
						CopyFilePackets.Enqueue( Packet );

						TotalFileCount++;
						TotalBytesCopied += SourceFile.Length;
					}
					else
					{
						DetailLog( " ... not copying: " + DestFile.FullName, ConsoleColor.DarkYellow );
					}
				}

				while( CopyFilePackets.Count > 0 || VerifyFilePackets.Count > 0 )
				{
					if( !Options.bLog )
					{
						ProgressLog( " ... waiting for " + CopyFilePackets.Count + " copies, and " + VerifyFilePackets.Count + " verifies    ", ConsoleColor.Cyan );
					}
					Thread.Sleep( 100 );
				}

				if( !Options.bLog )
				{
					ProgressLog( " ... waiting for " + CopyFilePackets.Count + " copies, and " + VerifyFilePackets.Count + " verifies    ", ConsoleColor.Cyan );
				}

				bCopyingFiles = false;
				bVerifyingFiles = false;

				Log( "", ConsoleColor.Green );
				Log( "Completed copying " + TotalFileCount + " files totaling " + TotalBytesCopied + " bytes", ConsoleColor.Green );
			}
		}
	}
}
