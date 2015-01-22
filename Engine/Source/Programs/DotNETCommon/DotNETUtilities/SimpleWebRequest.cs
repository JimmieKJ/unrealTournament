// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Text;

namespace Tools.DotNETCommon.SimpleWebRequest
{
	/// <summary>
	/// A class to make simple Xml web requests to return a string.
	/// </summary>
	public class SimpleWebRequest
	{
		/// <summary>
		/// Default constructor.
		/// </summary>
		public SimpleWebRequest()
		{
		}

		/// <summary>
		/// Create an HttpWebRequest with an optional Xml payload.
		/// </summary>
		/// <param name="WebRequestString">The URL and parameters of the web request.</param>
		/// <param name="Payload">The Xml data to attached to request, or null.</param>
		/// <returns>A valid HttpWebRequest, or null for any failure.</returns>
		/// <remarks>This uses the POST method with default credentials and a content type of 'text/xml'.</remarks>
		static public HttpWebRequest CreateWebRequest( string WebRequestString, string Payload )
		{
			HttpWebRequest Request = null;
			try
			{
				Request = ( HttpWebRequest )WebRequest.Create( WebRequestString );
				Request.Method = "POST";
				Request.UseDefaultCredentials = true;
				Request.SendChunked = false;

				if( Payload != null )
				{
					byte[] Buffer = Encoding.UTF8.GetBytes( Payload );
					Request.ContentType = "text/xml";
					Request.ContentLength = Buffer.Length;
					Request.GetRequestStream().Write( Buffer, 0, Buffer.Length );
				}
				else
				{
					Request.ContentLength = 0;
				}
			}
			catch( Exception Ex )
			{
				Debug.WriteLine( "Exception in CreateRequest: " + Ex.Message );
				Request = null;
			}

			return Request;
		}

		/// <summary>
		/// Get a string response from an HttpWebRequest.
		/// </summary>
		/// <param name="Request">A valid HttpWebRequest.</param>
		/// <returns>The string returned from the web request, or an empty string if there was an error.</returns>
		/// <remarks>The response status code needs to be 'OK' for results to be processed. Also, this is designed for simple queries, 
		/// so there is an arbitrary 1024 byte limit to the response size.</remarks>
		static public string GetWebResponse( HttpWebRequest Request )
		{
			string ResponseString = "";

			try
			{
				HttpWebResponse WebResponse = ( HttpWebResponse )Request.GetResponse();
				// Simple size check to prevent exploits
				if( WebResponse.StatusCode == HttpStatusCode.OK && WebResponse.ContentLength < 1024 )
				{
					// Process the response
					Stream ResponseStream = WebResponse.GetResponseStream();
					byte[] RawResponse = new byte[WebResponse.ContentLength];
					ResponseStream.Read( RawResponse, 0, ( int )WebResponse.ContentLength );
					ResponseStream.Close();

					ResponseString = Encoding.UTF8.GetString( RawResponse );
				}
			}
			catch( Exception Ex )
			{
				Debug.WriteLine( "Exception in GetWebResponse: " + Ex.Message );
				ResponseString = "";
			}

			return ResponseString;
		}

		/// <summary>
		/// Send a web request to a web service and return a response string.
		/// </summary>
		/// <param name="WebRequestString">The URL of the request with parameters.</param>
		/// <param name="Payload">The Xml payload of the request (can be null).</param>
		/// <returns>A string returned from the web request, or an empty string if there was any error.</returns>
		/// <remarks>This calls CreateWebRequest and GetWebResponse internally.</remarks>
		static public string GetWebServiceResponse( string WebRequestString, string Payload )
		{
			string ResponseString = "";

			if( WebRequestString != null )
			{
				HttpWebRequest Request = CreateWebRequest( WebRequestString, Payload );
				if( Request != null )
				{
					ResponseString = GetWebResponse( Request );
				}
			}

			return ResponseString;
		}
	}
}
