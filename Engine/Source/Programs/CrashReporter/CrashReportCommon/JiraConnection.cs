// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Text;
using System.Web.Script.Serialization;

/// <summary>
/// 
/// </summary>
public class JiraConnection
{
	/// <summary> Helper class for inline initialization of the list of the KeyValuePair. </summary>
	class ListKVP<TKey, TValue> : List<KeyValuePair<TKey, TValue>>
	{
		/// <summary> Overloaded Add. </summary>
		public void Add( TKey Key, TValue Value )
		{
			Add( new KeyValuePair<TKey, TValue>( Key, Value ) );
		}
	}

	/// <summary>
	/// Verbs for HTTP requests
	/// </summary>
	public enum JiraMethod
	{
		/// <summary></summary>
		POST,
		/// <summary></summary>
		GET,
		/// <summary></summary>
		DELETE,
		/// <summary></summary>
		PUT,
	}

	/// <summary>
	/// 
	/// </summary>
	Dictionary<string, string> Credentials = new Dictionary<string, string>( StringComparer.InvariantCultureIgnoreCase );

	/// <summary> Whether we can use JIRA integrations. </summary>
	bool bCanBeUsed = false;

	/// <summary> Whether we have read component and version arrays. </summary>
	bool bHasComponentAndVersionArrays = false;

	/// <summary> Maps the name to the component. </summary>
	Dictionary<string, Dictionary<string, object>> NameToComponents = new Dictionary<string, Dictionary<string, object>>();
	/// <summary> Maps the name to the component. </summary>
	public Dictionary<string, Dictionary<string, object>> GetNameToComponents()
	{
		return NameToComponents;
	}

	/// <summary> Maps the name to the version. </summary>
	Dictionary<string, Dictionary<string, object>> NameToVersions = new Dictionary<string, Dictionary<string, object>>();
	/// <summary> Maps the name to the version. </summary>
	public Dictionary<string, Dictionary<string, object>> GetNameToVersions()
	{
		return NameToVersions;
	}

	/// <summary> Maps the name to the platform. HARD-CODED </summary>
	Dictionary<string, Dictionary<string, object>> NameToPlatform = new Dictionary<string, Dictionary<string, object>>();
	/// <summary> Maps the name to the platform. HARD-CODED </summary>
	public Dictionary<string, Dictionary<string, object>> GetNameToPlatform()
	{
		return NameToPlatform;
	}

    /// <summary>
    /// Maps the name to the version.
    /// </summary>
    Dictionary<string, Dictionary<string, object>> NameToBranch = new Dictionary<string, Dictionary<string, object>>();
    /// <summary>
    /// Maps the name to the branch.
    /// </summary>
    public Dictionary<string, Dictionary<string, object>> GetNameToBranch()
    {
        return NameToBranch;
    }

	/// <summary>
	/// Singleton
	/// </summary>
	static JiraConnection Singleton = new JiraConnection();
	private static readonly object SyncObj = new object();

	/// <summary>
	/// 
	/// </summary>
	JiraConnection()
	{
		// Parse the credentials		
		string CredentialsFileName = Path.Combine( Environment.GetFolderPath( Environment.SpecialFolder.CommonApplicationData ), "ue4credentials" );
		if( File.Exists( CredentialsFileName ) )
		{
			ParseConfigFile( CredentialsFileName, Credentials );
			bCanBeUsed = true;
		}
	}

	/// <summary>
	/// Retrieves all components from https://jira.it.epicgames.net/rest/api/2/project/UE/components
	/// </summary>
	private void InitializeComponentArray()
	{
		/*
			"self": "https://jira.it.epicgames.net/rest/api/2/component/..",
			"id": "14019",
			"name": "...",
			"lead": {
			"self": "https://jira.it.epicgames.net/rest/api/2/user?username=...,
			"key": "...",
			"name": "...",
			"avatarUrls": {...},
			"displayName": "...",
			"active": true
			},
			"assigneeType": "PROJECT_DEFAULT",
			"realAssigneeType": "PROJECT_DEFAULT",
			"isAssigneeTypeValid": false
		*/
		Dictionary<string, object> Request = new Dictionary<string, object>();
		string[] FieldsToGet = new string[] { "self", "id", "name" };
		Request.Add( "fields", FieldsToGet );
		HttpWebResponse Response = JiraRequest( "/project/UE/components", JiraMethod.GET, null, HttpStatusCode.OK );

		// Parse the results
		List<Dictionary<string, object>> Results = ParseJiraResponseIntoList( Response );
		foreach( var Component in Results )
		{
			string Name = (string)Component["name"];
			NameToComponents.Add( Name, Component );
		}
	}

	/// <summary>
	/// Retrieves all build versions from https://jira.it.epicgames.net/rest/api/2/project/UE/versions
	/// </summary>
	private void InitializeVersionArray()
	{
		/*
			"self": "https://jira.it.epicgames.net/rest/api/2/version/...",
			"id": "11700",
			"description": "Scheduled engine release",
			"name": "4.8",
			"archived": false,
			"released": false,
			"startDate": "2015-01-12",
			"releaseDate": "2015-03-03",
			"overdue": false,
			"userStartDate": "12/Jan/15",
			"userReleaseDate": "03/Mar/15",
			"projectId": 11205
		*/
		Dictionary<string, object> Request = new Dictionary<string, object>();
		string[] FieldsToGet = new string[] { "self", "id", "name" };
		Request.Add( "fields", FieldsToGet );
		HttpWebResponse Response = JiraRequest( "/project/UE/versions", JiraMethod.GET, null, HttpStatusCode.OK );

		// Parse the results
		List<Dictionary<string, object>> Results = ParseJiraResponseIntoList( Response );
		foreach( var Version in Results )
		{
			string Name = (string)Version["name"];
			NameToVersions.Add( Name, Version );
		}
	}


	/// <summary> Initializes hard-coded array of field Platforms. </summary>
	private void InitializePlatforms()
	{
		var JiraNameToPlatform = new ListKVP<string,string>
		{ 
			{"All","/customFieldOption/11103"},
			{"Android","/customFieldOption/11104"},
			{"IOS","/customFieldOption/11105"},
			{"Cooker","/customFieldOption/11706"},
			{"HTML5","/customFieldOption/11106"},
			{"Linux","/customFieldOption/11107"},
			{"Mac","/customFieldOption/11108"},
			{"PS4","/customFieldOption/11111"},
			{"Windows","/customFieldOption/11112"},
			{"Win32","/customFieldOption/11112"},
			{"Win64","/customFieldOption/11112"},
			{"XBoxOne","/customFieldOption/11113"},
			{"Oculus","/customFieldOption/11353"},		
		};

		foreach( var Mapping in JiraNameToPlatform )
		{
			HttpWebResponse Response = JiraRequest( Mapping.Value, JiraMethod.GET, null, HttpStatusCode.OK );
			Dictionary<string, object> JSON = ParseJiraResponse( Response );

			NameToPlatform.Add( Mapping.Key, JSON );
		}
	}

	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	public static JiraConnection Get()
	{
		if( !Singleton.bHasComponentAndVersionArrays )
		{
			lock( SyncObj )
			{
				if( !Singleton.bHasComponentAndVersionArrays )
				{
					Singleton.InitializeComponentArray();
					Singleton.InitializeVersionArray();
					Singleton.InitializePlatforms();
					Singleton.bHasComponentAndVersionArrays = true;
				}
			}
		}

		return Singleton;
	}

	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	public bool CanBeUsed()
	{
		return bCanBeUsed;
	}

	/// <summary>
	/// Parses a config file on disk. 
	/// </summary>
	/// <param name="FileName">The config file to read.</param>
	/// <param name="Settings">Dictionary to receive all the key/value pairs read from the config file.</param>
	void ParseConfigFile( string FileName, Dictionary<string, string> Settings )
	{
		foreach( string Line in File.ReadAllLines( FileName ) )
		{
			string TrimLine = Line.Trim();
			if( !TrimLine.StartsWith( ";" ) )
			{
				int EqualsIdx = Line.IndexOf( '=' );
				if( EqualsIdx != -1 )
				{
					Settings.Add( Line.Substring( 0, EqualsIdx ).TrimEnd(), Line.Substring( EqualsIdx + 1 ).TrimStart() );
				}
			}
		}
	}

	/// <summary>
	/// Creates a Jira ticket with the given fields
	/// </summary>
	public string AddJiraTicket( Dictionary<string, object> fields )
	{
		// Create the request object
		var request = new Dictionary<string, object> { { "fields", fields } };
		var response = JiraRequest( "/issue", JiraMethod.POST, request, HttpStatusCode.Created );

		// Return the new issue id
		var responseObject = ParseJiraResponse( response );
		return (string)responseObject["key"];
	}

	/// <summary>
	/// Updates a Jira ticket with the given fields
	/// </summary>
	public void UpdateJiraTicket( string Key, Dictionary<string, object> Fields )
	{
		// Create the request object
		Dictionary<string, object> Request = new Dictionary<string, object> { { "fields", Fields } };
		HttpWebResponse Response = JiraRequest( String.Format( "/issue/{0}", Key ), JiraMethod.PUT, Request, HttpStatusCode.OK );
	}

	/// <summary>
	/// Searches for JIRA tickets
	/// </summary>
	/// <param name="SearchQuery">Search query in JQL format</param>
	/// <param name="FieldsToGet">Fields that will be grabbed from the results</param>
	public Dictionary<string,Dictionary<string, object>> SearchJiraTickets( string SearchQuery, string[] FieldsToGet )
	{
		var result = new Dictionary<string, Dictionary<string, object>>();

		// Do the query in multiple steps, to avoid timeouts when receiving large amounts of data
		const int maxResultsPerQuery = 100;
		for( var startAt = 0; ; startAt += maxResultsPerQuery )
		{
			// Create the query
			var request = new Dictionary<string, object>();
			request.Add( "jql", SearchQuery );
			request.Add( "startAt", startAt.ToString() );
			request.Add( "maxResults", maxResultsPerQuery.ToString() );
			request.Add( "fields", FieldsToGet );
			var response = JiraRequest( "/search", JiraMethod.POST, request, HttpStatusCode.OK );

			// Parse the results
			var results = ParseJiraResponse( response );
			var issues = (System.Collections.ArrayList)results["issues"];

			// Process this chunk of results
			foreach( Dictionary<string, object> Issue in issues )
			{
				var key = (string)Issue["key"];
				var fields = (Dictionary<string, object>)Issue["fields"];
				result.Add( key, fields );
			}

			// Quit once we got less than the max results
			if( issues.Count < maxResultsPerQuery )
			{
				break;
			}
		}

		return result;
	}

	/// <summary>
	/// Issues a Jira web request
	/// </summary>
	HttpWebResponse JiraRequest( string RequestUrl, JiraMethod Method, object Input )
	{
	    try
	    {

	        // Get the username and password
	        string UserName, Password;
	        if (Credentials.TryGetValue("jira.username", out UserName) &&
	            Credentials.TryGetValue("jira.password", out Password))
	        {
	            // Send the request
	            var request = (HttpWebRequest) WebRequest.Create("https://jira.it.epicgames.net/rest/api/2" + RequestUrl);
	            request.Method = Enum.GetName(typeof (JiraMethod), Method);
	            request.Headers.Add("Authorization",
	                "Basic " + Convert.ToBase64String(UTF8Encoding.UTF8.GetBytes(UserName + ":" + Password)));
	            request.ContentType = "application/json";
	            if (Input != null)
	            {
	                using (var requestWriter = new StreamWriter(request.GetRequestStream(), Encoding.ASCII))
	                {
	                    var serializer = new JavaScriptSerializer();
	                    requestWriter.Write(serializer.Serialize(Input));
	                }
	            }
	            // Check the response was ok
	            return (HttpWebResponse) request.GetResponse();
	        }
	        else
	        {
	            return null;
	        }
	    }
	    catch (WebException ex)
	    {
            using( StreamReader ResponseReader = new StreamReader( ex.Response.GetResponseStream() ) )
		    {
			    string ResponseText = ResponseReader.ReadToEnd();
		    }
            return null;
	    }
	    catch (Exception)
	    {
            return null;
	    }
	}

	/// <summary>
	/// Issues a Jira web request
	/// </summary>
	public HttpWebResponse JiraRequest( string RequestUrl, JiraMethod Method, object Input, HttpStatusCode ExpectedStatus )
	{
		HttpWebResponse Response = JiraRequest( RequestUrl, Method, Input );
		if( Response.StatusCode != ExpectedStatus )
		{
			return null;
		}
		return Response;
	}

    /// <summary>
	/// Parses a Json object from a web response
	/// </summary>
	public static Dictionary<string, object> ParseJiraResponse( HttpWebResponse Response )
	{
		using( StreamReader ResponseReader = new StreamReader( Response.GetResponseStream() ) )
		{
			string ResponseText = ResponseReader.ReadToEnd();
			JavaScriptSerializer Serializer = new JavaScriptSerializer();
			return Serializer.Deserialize<Dictionary<string, object>>( ResponseText );
		}
	}

	/// <summary>
	/// Parses a Json object from a web response
	/// </summary>
	public static List<Dictionary<string, object>> ParseJiraResponseIntoList( HttpWebResponse Response )
	{
		using( StreamReader ResponseReader = new StreamReader( Response.GetResponseStream() ) )
		{
			string ResponseText = ResponseReader.ReadToEnd();
			JavaScriptSerializer Serializer = new JavaScriptSerializer();
			return Serializer.Deserialize<List<Dictionary<string, object>>>( ResponseText );
		}
	}
}