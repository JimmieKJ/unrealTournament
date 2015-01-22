// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading;

using Tools.DotNETCommon.ThreadSafeQueue;

namespace Tools.Distill
{
	public partial class Distill
	{
		private class FTPFilePacket
		{
			public FileInfo SourceFile = null;
			public string DestinationFile = null;

			public FTPFilePacket( FileInfo InSourceFile, string InDestinationFile )
			{
				SourceFile = InSourceFile;
				DestinationFile = InDestinationFile;
			}
		}

		private static bool bFTPingFiles = true;
		private static long TotalBytesUploaded = 0;
		private static Hashtable Folders = new Hashtable();
		private static ThreadSafeQueue<FTPFilePacket> FTPFilePackets = new ThreadSafeQueue<FTPFilePacket>();

		private static void CreateFolder( string DirectoryName )
		{
			Uri DestURI = new Uri( "ftp://" + Options.FTPSite + "/" + DirectoryName );
			FtpWebRequest Request = ( FtpWebRequest )FtpWebRequest.Create( DestURI );

			// Set the credentials required to connect to the FTP server
			Request.Credentials = new NetworkCredential( Options.FTPUser, Options.FTPPass );

			// Automatically close the connection on completion
			Request.KeepAlive = true;

			// Set to upload a file
			Request.Method = WebRequestMethods.Ftp.MakeDirectory;
			WebResponse Response = Request.GetResponse();
		}

		private static void EnsureRemoteFolderExists( string DirectoryName )
		{
			if( DirectoryName.Contains( "/" ) )
			{
				EnsureRemoteFolderExists( Path.GetDirectoryName( DirectoryName ).Replace( "\\", "/" ) );
			}

			if( !Folders.Contains( DirectoryName ) )
			{
				try
				{
					Folders[DirectoryName] = true;

					DetailLog( "Creating folder: " + DirectoryName, ConsoleColor.DarkMagenta );
					CreateFolder( DirectoryName );
				}
				catch
				{
				}
			}
		}

		private static Stream GetWebRequestStream( FileInfo SourceFile, string DestinationName )
		{
			Uri DestURI = new Uri( "ftp://" + Options.FTPSite + "/" + DestinationName );
			FtpWebRequest Request = ( FtpWebRequest )FtpWebRequest.Create( DestURI );

			// Set the credentials required to connect to the FTP server
			Request.Credentials = new NetworkCredential( Options.FTPUser, Options.FTPPass );

			// Automatically close the connection on completion
			Request.KeepAlive = true;

			// Set to upload a file
			Request.Method = WebRequestMethods.Ftp.UploadFile;

			// Send a binary file
			Request.UseBinary = true;

			// Amount of data to upload
			Request.ContentLength = SourceFile.Length;

			return Request.GetRequestStream();
		}

		private static long UploadFile( FileInfo SourceFile, string DestinationName )
		{
			FileStream Source = SourceFile.OpenRead();
			Stream Destination = GetWebRequestStream( SourceFile, DestinationName );

			int MaxBufferLength = 65536;
			byte[] Buffer = new byte[MaxBufferLength];

			int BufferLength = Source.Read( Buffer, 0, MaxBufferLength );
			while( BufferLength > 0 )
			{
				Destination.Write( Buffer, 0, BufferLength );
				TotalBytesUploaded += BufferLength;

				BufferLength = Source.Read( Buffer, 0, MaxBufferLength );
			}

			Destination.Close();
			Source.Close();

			return SourceFile.Length;
		}

		private static void FTPFilesThreadProc()
		{
			while( bFTPingFiles )
			{
				while( FTPFilePackets.Count > 0 )
				{
					FTPFilePacket Packet = null;
					try
					{
						Packet = FTPFilePackets.Dequeue();

						DetailLog( " ... FTPing: " + Packet.SourceFile.FullName, ConsoleColor.DarkYellow );
						EnsureRemoteFolderExists( Path.GetDirectoryName( Packet.DestinationFile ).Replace( "\\", "/" ) );
						UploadFile( Packet.SourceFile, Packet.DestinationFile );
					}
					catch( Exception Ex )
					{
						Warning( "Failed to FTP: " + Packet.SourceFile.FullName + " (" + Ex.Message + ") -- trying again!" );
						Packet.SourceFile.Refresh();

						FTPFilePackets.Enqueue( Packet );
					}
				}
			}
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="TableOfContents"></param>
		public static void FTPFiles( TOC TableOfContents )
		{
			if( Options.FTPSite.Length > 0 && Options.FTPUser.Length > 0 )
			{
				int TotalFilesUploaded = TableOfContents.Entries.Count;
				TotalBytesUploaded = 0;

				bCopyingFiles = true;
				Thread FTPFilesThread = new Thread( FTPFilesThreadProc );
				FTPFilesThread.Start();

				foreach( TOCEntry Entry in TableOfContents.Entries )
				{
					// Handle file copies
					FileInfo SourceFile = Entry.Info;
					string DestFile = Path.Combine( Options.FTPFolder, Entry.Name ).Replace( "\\", "/" );

					// Create copy packet and add to queue
					FTPFilePacket Packet = new FTPFilePacket( SourceFile, DestFile );
					FTPFilePackets.Enqueue( Packet );
				}

				while( FTPFilePackets.Count > 0 )
				{
					if( !Options.bLog )
					{
						ProgressLog( " ... waiting for " + FTPFilePackets.Count + " FTPs (" + TotalBytesUploaded + " uploaded)   ", ConsoleColor.Cyan );
					}
					Thread.Sleep( 100 );
				}

				if( !Options.bLog )
				{
					ProgressLog( " ... waiting for " + FTPFilePackets.Count + " FTPs (" + TotalBytesUploaded + " bytes uploaded)   ", ConsoleColor.Cyan );
				}

				bFTPingFiles = false;

				Log( "", ConsoleColor.Green );
				Log( "Completed FTPing " + TotalFilesUploaded + " files, totaling " + TotalBytesUploaded + " bytes", ConsoleColor.Green );
			}
		}
	}
}
