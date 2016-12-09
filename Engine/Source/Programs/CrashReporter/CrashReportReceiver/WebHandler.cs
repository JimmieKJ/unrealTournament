// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Web;
using System.Text;

using Tools.CrashReporter.CrashReportCommon;
using Tools.DotNETCommon.XmlHandler;
using System.ComponentModel;
using System.Threading;

namespace Tools.CrashReporter.CrashReportReceiver
{
	/// <summary>
	/// A class to handle the receiving of crash reports.
	/// </summary>
	public class WebHandler : IDisposable
	{
		/// <summary>A flag to check to ensure the service started properly.</summary>
		public bool bStartedSuccessfully = false;

		/// <summary>Base http listener to which the async listeners are associated.</summary>
		private HttpListener ServiceHttpListener = null;

		/// <summary>Intermediate path crash reports are downloaded to until complete.</summary>
		private string FileReceiptPath;

		/// <summary>Object to take care of incoming files.</summary>
		LandingZoneMonitor LandingZone;

		/// <summary>Synchronisation object to cope with erratic client behaviour.</summary>
		FUploadsInProgress UploadsInProgress = new FUploadsInProgress();

		/// <summary>Leave at least this amount of time to leave between checks for abandoned reports</summary>
		const int MinimumMinutesBetweenAbandonedReportChecks = 30;

		/// <summary>Once a report is this old without being completed, it gets deleted</summary>
		const int AgeMinutesToConsiderReportAbandoned = 4*60;

		/// <summary>Time of most recent check for abandoned incomplete reports</summary>
		DateTime LastAbandonedReportCheckTime = DateTime.Now;

		/// <summary> Last day, used to create a new log file. </summary>
		int LastDay = DateTime.UtcNow.Day;

		/// <summary>Atomic flag to indicate a thread is checking for abandoned reports</summary>
		int CheckingForAbandonedReports = 0;

		/// <summary>
		/// Implementing Dispose.
		/// </summary>
		public void Dispose()
		{
			Dispose( true );
			GC.SuppressFinalize( this );
		}

		/// <summary>
		/// Disposes the resources.
		/// </summary>
		/// <param name="Disposing">true if the Dispose call is from user code, and not system code.</param>
		protected virtual void Dispose( bool Disposing )
		{
			if (ServiceHttpListener != null)
			{
				ServiceHttpListener.Close();
			}
		}

		/// <summary>
		/// Initialise the service to listen for reports.
		/// </summary>
		public WebHandler()
		{
			try
			{
				FileReceiptPath = Properties.Settings.Default.CrashReportRepository + "-Temp";

				LandingZone = new LandingZoneMonitor(
					Properties.Settings.Default.CrashReportRepository,
					CrashReporterReceiverServicer.Log
				);

				Directory.CreateDirectory( FileReceiptPath );			

				// Fire up a listener.
				ServiceHttpListener = new HttpListener();
				ServiceHttpListener.Prefixes.Add( "http://*:57005/CrashReporter/" );
				ServiceHttpListener.Start();
				ServiceHttpListener.BeginGetContext( AsyncHandleHttpRequest, null );

				bStartedSuccessfully = true;
			}
			catch( Exception Ex )
			{
				CrashReporterReceiverServicer.WriteEvent( "Initialisation error: " + Ex.ToString() );
			}
		}

		/// <summary>
		/// Cleanup any used resources.
		/// </summary>
		public void Release()
		{
			if( bStartedSuccessfully )
			{
				if( ServiceHttpListener != null )
				{
					ServiceHttpListener.Stop();
					ServiceHttpListener.Abort();
				}
			}
		}

		/// <summary>
		/// Read in the web request payload as a string.
		/// </summary>
		/// <param name="Request">A listener request.</param>
		/// <returns>A string of the input stream in the requests encoding.</returns>
		private string GetContentStreamString( HttpListenerRequest Request )
		{
			string Result = "";

			if( Request.HasEntityBody )
			{
				using( StreamReader Reader = new StreamReader( Request.InputStream, Request.ContentEncoding ) )
				{
					Result = Reader.ReadToEnd();
				}

				Request.InputStream.Close();
			}

			return Result;
		}

		/// <summary>
		/// Check to see if a report has already been uploaded.
		/// </summary>
		/// <param name="Request">A request containing either the Report Id as a string or an XML representation of a CheckReportRequest class instance.</param>
		/// <returns>Result object, indicating whether the report has already been uploaded.</returns>
		private CrashReporterResult CheckReport(HttpListenerRequest Request)
		{
			CrashReporterResult ReportResult = new CrashReporterResult();
#if DISABLED_CRR
			ReportResult.bSuccess = false;
			CrashReporterReceiverServicer.WriteEvent("CheckReport() Report rejected by disabled CRR");
#else
			var RequestClass = new CheckReportRequest();
			RequestClass.ReportId = GetReportIdFromPostData(GetContentStreamString(Request));

			ReportResult.bSuccess = !LandingZone.HasReportAlreadyBeenReceived(RequestClass.ReportId);

			if( !ReportResult.bSuccess )
			{
				CrashReporterReceiverServicer.WriteEvent( string.Format( "Report \"{0}\" has already been received", RequestClass.ReportId ) );
			}
#endif
			return ReportResult;
		}

		/// <summary>
		/// Check to see if we wish to reject a report based on the WER meta data.
		/// </summary>
		/// <param name="Request">A request containing the XML representation of a WERReportMetadata class instance.</param>
		/// <returns>true if we do not reject.</returns>
		private CrashReporterResult CheckReportDetail( HttpListenerRequest Request )
		{
			CrashReporterResult ReportResult = new CrashReporterResult();
#if DISABLED_CRR	
			ReportResult.bSuccess = false;
			ReportResult.Message = "CRR disabled";
			CrashReporterReceiverServicer.WriteEvent("CheckReportDetail() Report rejected by disabled CRR");
#else
			string WERReportMetadataString = GetContentStreamString( Request );
			WERReportMetadata WERData = null;

			if( WERReportMetadataString.Length > 0 )
			{
				try
				{
					WERData = XmlHandler.FromXmlString<WERReportMetadata>( WERReportMetadataString );
				}
				catch( System.Exception Ex )
				{
					CrashReporterReceiverServicer.WriteEvent( "Error during XmlHandler.FromXmlString, probably incorrect encoding, trying to fix: " + Ex.Message );

					byte[] StringBytes = System.Text.Encoding.Unicode.GetBytes( WERReportMetadataString );
					string ConvertedXML = System.Text.Encoding.UTF8.GetString( StringBytes );
					WERData = XmlHandler.FromXmlString<WERReportMetadata>( ConvertedXML );
				}
			}

			if( WERData != null )
			{
				// Ignore crashes in the minidump parser itself
				ReportResult.bSuccess = true;
				if( WERData.ProblemSignatures.Parameter0.ToLower() == "MinidumpDiagnostics".ToLower() )
				{
					ReportResult.bSuccess = false;
					ReportResult.Message = "Rejecting MinidumpDiagnostics crash";
				}

				// Ignore Debug and DebugGame crashes
				string CrashingModule = WERData.ProblemSignatures.Parameter3.ToLower();
				if( CrashingModule.Contains( "-debug" ) )
				{
					ReportResult.bSuccess = false;
					ReportResult.Message = "Rejecting Debug or DebugGame crash";
				}
			}
#endif
			return ReportResult;
		}

		/// <summary>
		/// Receive a file and write it to a temporary folder.
		/// </summary>
		/// <param name="Request">A request containing the file details in the headers (DirectoryName/FileName/FileLength).</param>
		/// <returns>true if the file is received successfully.</returns>
		/// <remarks>There is an arbitrary file size limit of CrashReporterConstants.MaxFileSizeToUpload as a simple exploit prevention method.</remarks>
		private CrashReporterResult ReceiveFile( HttpListenerRequest Request )
		{
			CrashReporterResult ReportResult = new CrashReporterResult();

			if( !Request.HasEntityBody )
			{
				return ReportResult;
			}

			// Take this opportunity to clean out folders for reports that were never completed
			CheckForAbandonedReports();

			// Make sure we have a sensible file size
			long BytesToReceive = 0;
			if (long.TryParse(Request.Headers["FileLength"], out BytesToReceive))
			{
				if (BytesToReceive >= CrashReporterConstants.MaxFileSizeToUpload)
				{
					return ReportResult;
				}
			}

			string DirectoryName = Request.Headers["DirectoryName"];
			string FileName = Request.Headers["FileName"];
			var T = Request.ContentLength64;
			bool bIsOverloaded = false;
			if( !UploadsInProgress.TryReceiveFile( DirectoryName, FileName, BytesToReceive, ref ReportResult.Message, ref bIsOverloaded ) )
			{
				CrashReporterReceiverServicer.WriteEvent(ReportResult.Message);
				ReportResult.bSuccess = false;
				return ReportResult;
			}

			string PathName = Path.Combine( FileReceiptPath, DirectoryName, FileName );

			// Recreate the file receipt directory, just in case.
			Directory.CreateDirectory( FileReceiptPath );

			// Create the folder to save files to
			DirectoryInfo DirInfo = new DirectoryInfo( Path.GetDirectoryName( PathName ) );
			DirInfo.Create();

			// Make sure the file doesn't already exist. If it does, delete it.
			if (File.Exists(PathName))
			{
				File.Delete(PathName);
			}

			FileInfo Info = new FileInfo( PathName );
			FileStream FileWriter = Info.OpenWrite();

			// Read in the input stream from the request, and write to a file
			long OriginalBytesToReceive = BytesToReceive;
			try
			{
				using (BinaryReader Reader = new BinaryReader(Request.InputStream))
				{
					byte[] Buffer = new byte[CrashReporterConstants.StreamChunkSize];

					while (BytesToReceive > 0)
					{
						int BytesToRead = Math.Min((int)BytesToReceive, CrashReporterConstants.StreamChunkSize);
						Interlocked.Add( ref UploadsInProgress.CurrentReceivedData, BytesToRead );

						int ReceivedChunkSize = Reader.Read(Buffer, 0, BytesToRead);

						if (ReceivedChunkSize == 0)
						{
							ReportResult.Message = string.Format("Partial file \"{0}\" received", FileName);
							ReportResult.bSuccess = false;
							CrashReporterReceiverServicer.WriteEvent(ReportResult.Message);

							return ReportResult;
						}

						BytesToReceive -= ReceivedChunkSize;
						FileWriter.Write(Buffer, 0, ReceivedChunkSize);
					}
				}
			}
			finally
			{
				FileWriter.Close();
				Request.InputStream.Close();

				UploadsInProgress.FileUploadAttemptDone( OriginalBytesToReceive, bIsOverloaded );

				bool bWriteMetadata = Path.GetExtension( FileName ) == ".ue4crash";
				if( bWriteMetadata )
				{
					string CompressedSize = Request.Headers["CompressedSize"];
					string UncompressedSize = Request.Headers["UncompressedSize"];
					string NumberOfFiles = Request.Headers["NumberOfFiles"];

					string MetadataPath = Path.Combine( FileReceiptPath, DirectoryName, Path.GetFileNameWithoutExtension( FileName ) + ".xml" );
					XmlHandler.WriteXml<FCompressedCrashInformation>( new FCompressedCrashInformation( CompressedSize, UncompressedSize, NumberOfFiles ), MetadataPath );
				}
			}

			ReportResult.bSuccess = true;
			return ReportResult;
		}

		/// <summary>
		/// Rename to the temporary landing zone directory to the final location.
		/// </summary>
		/// <param name="Request">A request containing either the Report Id as a string or an XML representation of a CheckReportRequest class instance.</param>
		/// <returns>true if everything is renamed correctly.</returns>
		private CrashReporterResult UploadComplete(HttpListenerRequest Request)
		{
			var ReportResult = new CrashReporterResult();

			var RequestClass = new CheckReportRequest();
			RequestClass.ReportId = GetReportIdFromPostData(GetContentStreamString(Request));

			string IntermediatePathName = Path.Combine(FileReceiptPath, RequestClass.ReportId);

			if (!UploadsInProgress.TrySetReportComplete(RequestClass.ReportId))
			{
				ReportResult.Message = string.Format("Report \"{0}\" has already been completed", RequestClass.ReportId);
				ReportResult.bSuccess = false;
				return ReportResult;
			}

			DirectoryInfo DirInfo = new DirectoryInfo(IntermediatePathName);
			if (!DirInfo.Exists)
			{
				return ReportResult;
			}

			LandingZone.ReceiveReport(DirInfo, RequestClass.ReportId);
			ReportResult.bSuccess = true;

			int CurrentDay = DateTime.UtcNow.Day;
			if( CurrentDay > LastDay )
			{
				// Check the log and create a new one for a new day.
				CrashReporterReceiverServicer.Log.CreateNewLogFile();
				LastDay = CurrentDay;
			}

			return ReportResult;
		}

		/// <summary>
		/// The main listener callback to handle client requests.
		/// </summary>
		/// <param name="ClientRequest">The request from the client.</param>
		private void AsyncHandleHttpRequest( IAsyncResult ClientRequest )
		{
			try
			{
				HttpListenerContext Context = ServiceHttpListener.EndGetContext( ClientRequest );
				ServiceHttpListener.BeginGetContext( AsyncHandleHttpRequest, null );
				HttpListenerRequest Request = Context.Request;
				bool bIgnorePerfData = false;

				using( HttpListenerResponse Response = Context.Response )
				{
					// Extract the URL parameters
					string[] UrlElements = Request.RawUrl.Split( "/".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );

					// http://*:57005/CrashReporter/CheckReport
					// http://*:57005/CrashReporter/CheckReportDetail
					CrashReporterResult ReportResult = new CrashReporterResult();
					if( UrlElements[0].ToLower() == "crashreporter" )
					{
						switch( UrlElements[1].ToLower() )
						{
						case "ping":
							ReportResult.bSuccess = true;
							break;
						case "checkreport":
							ReportResult = CheckReport( Context.Request );
							break;
						case "checkreportdetail":
							ReportResult = CheckReportDetail( Context.Request );
							break;
						case "uploadreportfile":
							ReportResult = ReceiveFile( Context.Request );
							bIgnorePerfData = true;
							break;
						case "uploadcomplete":
							ReportResult = UploadComplete( Context.Request );
							break;
						default:
							ReportResult.bSuccess = false;
							ReportResult.Message = "Invalid command: " + UrlElements[1];
							break;
						}
					}
					else
					{
						ReportResult.bSuccess = false;
						ReportResult.Message = "Invalid application: " + UrlElements[0] + " (expecting CrashReporter)";
					}

					string ResponseString = XmlHandler.ToXmlString<CrashReporterResult>( ReportResult );

					Response.SendChunked = true;
					Response.ContentType = "text/xml";

					byte[] Buffer = Encoding.UTF8.GetBytes( ResponseString );
					Response.ContentLength64 = Buffer.Length;
					Response.OutputStream.Write( Buffer, 0, Buffer.Length );

					Response.StatusCode = ( int )HttpStatusCode.OK;

					if( !bIgnorePerfData )
					{
						// Update the overhead data.
						Int64 ContentLenght = Response.ContentLength64 + Request.ContentLength64;
						Interlocked.Add( ref UploadsInProgress.CurrentReceivedData, ContentLenght );
					}
				}
			}
			catch( Exception Ex )
			{
				CrashReporterReceiverServicer.WriteEvent( "Error during async listen: " + Ex.Message );
			}
		}

		/// <summary>
		/// Translate post data, which may be XML or a raw string, to a report ID.
		/// </summary>
		/// <param name="ReportIdPostData">A string that's either the Report Id or an XML representation of a CheckReportRequest class instance.</param>
		/// <returns>The report ID string</returns>
		private string GetReportIdFromPostData(string ReportIdPostData)
		{
			if (ReportIdPostData.Length > 0)
			{
				// XML snippet will always start with <
				if (ReportIdPostData[0] != '<')
				{
					return ReportIdPostData;
				}
				else
				{
					// Report id is embedded in a serialised request object (sent by old C# uploader)
					var RequestObject = XmlHandler.FromXmlString<CheckReportRequest>(ReportIdPostData);
					if ( RequestObject.ReportId != null && RequestObject.ReportId.Length > 0 )
					{
						return RequestObject.ReportId;
					}
					// LEGACY SUPPORT. ReportId was once named DirectoryName
					else if ( RequestObject.DirectoryName != null && RequestObject.DirectoryName.Length > 0 )
					{
						return RequestObject.DirectoryName;
					}
				}
			}

			return "";
		}

		/// <summary>
		/// Periodically delete folders of abandoned reports
		/// </summary>
		void CheckForAbandonedReports()
		{
			if (Interlocked.Exchange(ref CheckingForAbandonedReports, 1) == 1)
			{
				// Being checked by another thread, so no need
				return;
			}

			try
			{
				var Now = DateTime.Now;

				// No need to do this very often
				if ((Now - LastAbandonedReportCheckTime).Minutes < MinimumMinutesBetweenAbandonedReportChecks)
				{
					return;
				}
				LastAbandonedReportCheckTime = Now;

				try
				{
					foreach (string Dir in Directory.EnumerateDirectories(FileReceiptPath))
					{
						var LastAccessTime = new DateTime();
						string ReportId = new DirectoryInfo(Dir).Name;
						bool bGotAccessTime = UploadsInProgress.TryGetLastAccessTime(ReportId, ref LastAccessTime);
						if (!bGotAccessTime || (Now - LastAccessTime).Minutes > AgeMinutesToConsiderReportAbandoned)
						{
							try
							{
								Directory.Delete(Dir, true /* delete all contents */);
							}
							catch (Exception Ex)
							{
								CrashReporterReceiverServicer.WriteEvent(string.Format("Failed to delete directory {0}: {1}", ReportId, Ex.Message));
							}
						}
					}
				}
				catch (Exception Ex)
				{
					// If we get in here, the reports won't get cleaned up. May happen if
					// permissions are incorrect or the directory is locked
					CrashReporterReceiverServicer.WriteEvent( "Error during folder tidy: " + Ex.Message );
				}
			}
			finally
			{
				// Allow other threads to check again
				Interlocked.Exchange(ref CheckingForAbandonedReports, 0);
			}
		}
	}
}
