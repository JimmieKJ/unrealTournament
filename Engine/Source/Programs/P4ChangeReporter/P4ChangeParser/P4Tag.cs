/*
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;
using System.Reflection;
using System.Xml;
using System.Xml.Serialization;
using System.Web;
using System.Web.UI;

namespace P4ChangeParser
{
	/// <summary>
	/// Abstract class to serve as the basis for all P4Tags
	/// </summary>
	public abstract class P4Tag
	{
		#region Static Constants
		/// <summary>
		/// Start of the tag regex pattern
		/// </summary>
		protected static readonly String TagPatternStart = @"\s*#(";

		/// <summary>
		/// End of the tag regex pattern
		/// </summary>
		protected static readonly String TagPatternEnd = @"):?\s+";

		/// <summary>
		/// Generic tag pattern for any tag, whether specifying a tag represented here or not
		/// </summary>
		public static readonly String GenericTagPattern = @"\s*#(\S+):?\s+";
		#endregion

		/// <summary>
		/// Determines if the provided string is in a tag format; not necessarily one of the supported tags
		/// </summary>
		/// <param name="InString">String to check if it is a tag</param>
		/// <returns>true if the string is in tag format; false if it is not</returns>
		public static bool IsStringInTagFormat(String InString)
		{
			return Regex.IsMatch(InString, GenericTagPattern);
		}
	}

	/// <summary>
	/// Class representing a P4 tag which starts a subchange
	/// </summary>
	public class P4StartTag : P4Tag
	{
		#region Static Constants
		/// <summary>
		/// The regex pattern for start tags
		/// </summary>
		public static readonly String StartTagPattern;

		/// <summary>
		/// The names of the start tags; as each requires the same data, they don't require their own
		/// unique classes
		/// </summary>
		private static readonly HashSet<String> StartTagNames;
		#endregion

		#region Member Variables
		/// <summary>
		/// Name/Type of the start tag
		/// </summary>
		private String mStartTagName;

		/// <summary>
		/// Brief description associated with the start tag/subchange
		/// </summary>
		private String mBriefDescription;
		#endregion

		#region Properties
		/// <summary>
		/// Name/Type of the start tag
		/// </summary>
		public String StartTagName
		{
			get { return mStartTagName; }
		}

		/// <summary>
		/// Brief description associated with the start tag/subchange
		/// </summary>
		public String BriefDescription
		{
			get { return mBriefDescription; }
		}
		#endregion

		#region Constructors
		/// <summary>
		/// Static constructor; Set up the various regex patterns
		/// </summary>
		static P4StartTag()
		{
			// Add the names of the valid start tags
			StartTagNames = new HashSet<String>();
			StartTagNames.Add("new");
			StartTagNames.Add("change");
			StartTagNames.Add("fix");

			// Create both the start tag regex pattern and the start tag regex pattern including tags to ignore
			// via use of a string builder and the patterns initialized in P4Tag
			StringBuilder StartTagStringBuilder = new StringBuilder();
			int TagNameIndex = 0;
			int NumTagNames = StartTagNames.Count;
			foreach (String TagName in StartTagNames)
			{
				StartTagStringBuilder.Append(TagName);
				if (TagNameIndex < NumTagNames - 1)
				{
					StartTagStringBuilder.Append("|");
				}
				++TagNameIndex;
			}
			StartTagPattern = P4Tag.TagPatternStart + StartTagStringBuilder.ToString() + P4Tag.TagPatternEnd;
		}

		/// <summary>
		/// Construct a P4StartTag object
		/// </summary>
		/// <param name="InStartTag">Name/type of the start tag</param>
		/// <param name="InDescription">Brief description of the start tag/associated subchange</param>
		public P4StartTag(String InStartTag, String InDescription)
		{
			mStartTagName = InStartTag;
			mBriefDescription = InDescription;
		}
		#endregion

		#region Public Methods
		/// <summary>
		/// Identifies if the provided string represents the name of a valid start tag (case-insensitive)
		/// </summary>
		/// <param name="InString">String that should be checked to see if it is a start tag</param>
		/// <returns>true if the provided string is a start tag; false otherwise</returns>
		public static bool IsStringAStartTag(String InString)
		{
			return StartTagNames.Contains(InString.ToLower());
		}

		/// <summary>
		/// Write the contents of the tag to HTML
		/// </summary>
		/// <param name="HtmlWriter">HTML writer to use</param>
		public void WriteHtml(HtmlTextWriter HtmlWriter)
		{
			HtmlWriter.Write(mStartTagName + " - " + mBriefDescription);
		}
		#endregion
	}

	/// <summary>
	/// Abstract base class representing a tag that occurs logically within a subchange
	/// </summary>
	public abstract class P4AuxiliaryTag : P4Tag
	{
		#region Static Constants
		/// <summary>
		/// The tag pattern for auxiliary tags; formed via reflection and contains all concrete subclasses of P4AuxiliaryTag
		/// </summary>
		public static readonly String AuxiliaryTagPattern;

		/// <summary>
		/// Dictionary of strings of auxiliary tag names to object types; Allows for factory-pattern type behavior, where a type can
		/// be constructed from a string
		/// </summary>
		private static readonly Dictionary<String, Type> TagNameToTypeMap;
		#endregion

		#region Properties
		/// <summary>
		/// Abstract property that must be implemented by derived classes; represents the tag type name; used in conjunction with the
		/// TagNameToTypeMap
		/// </summary>
		public abstract String TagName { get; }
		#endregion

		#region Constructors
		/// <summary>
		/// Static constructor; Sets up the tag to type dictionary, as well as the auxiliary tag regex pattern
		/// </summary>
		static P4AuxiliaryTag()
		{
			TagNameToTypeMap = new Dictionary<String, Type>();

			// Find all of the public types located in the executing assembly
			Type[] AssemblyTypes = Assembly.GetExecutingAssembly().GetExportedTypes();
			foreach (Type CurType in AssemblyTypes)
			{
				// If one of the found types is a subclass of P4AuxiliaryTag and is not abstract, it should be added to
				// the regex pattern and the tag to type dictionary for future use
				if (CurType.IsSubclassOf(typeof(P4AuxiliaryTag)) && !CurType.IsAbstract)
				{
					try
					{
						// Instantiate a temporary instance of the relevant type and add it to the dictionary
						P4AuxiliaryTag CurTag = (P4AuxiliaryTag)Activator.CreateInstance(CurType);
						if (!TagNameToTypeMap.ContainsKey(CurTag.TagName))
						{
							TagNameToTypeMap.Add(CurTag.TagName.ToLower(), CurType);
						}
						// Throw an exception if two different tags share the same tag name
						else
						{
							throw new Exception("Error: Auxiliary tag class " + CurType.Name + " has a tag name in common with another tag!");
						}
					}
					// A default constructor is required for Activator.CreateInstance() to work as is, so throw a specific exception to warn
					// the user if it's not implemented
					catch (MissingMethodException E)
					{
						Console.WriteLine("The auxiliary tag class {0} lacks a required default constructor!\n{1}", CurType.Name, E.Message);
					}
					catch (Exception E)
					{
						Console.WriteLine("Exception raised while trying to instantiate potential auxiliary tags!\n{0}", E.Message);
					}
				}
			}

			// Construct the regex pattern for all valid auxiliary types by appending the name of every auxiliary type
			// from the dictionary into the pattern
			StringBuilder AuxStringBuilder = new StringBuilder();

			// Append the tag pattern start
			AuxStringBuilder.Append(P4Tag.TagPatternStart);
			int TagNameIndex = 0;
			int NumTagNames = TagNameToTypeMap.Keys.Count;

			// Append each valid auxiliary type name
			foreach (String TagName in TagNameToTypeMap.Keys)
			{
				AuxStringBuilder.Append(TagName);

				// If not the last type in the dictionary, append an "|" character to the pattern
				if (TagNameIndex < NumTagNames - 1)
				{
					AuxStringBuilder.Append("|");
				}
				++TagNameIndex;
			}

			// Append the tag pattern end and set the auxiliary tag pattern to the result of the string builder
			AuxStringBuilder.Append(P4Tag.TagPatternEnd);
			AuxiliaryTagPattern = AuxStringBuilder.ToString();
		}
		#endregion

		#region Abstract Methods
		/// <summary>
		/// Populate the tag from a provided string, if possible
		/// </summary>
		/// <param name="InStringToParse">String to populate from</param>
		/// <returns>true if the tag was successfully populated from the provided string; false otherwise</returns>
		public abstract bool PopulateFromString(String InStringToParse);

		/// <summary>
		/// Write out the contents of the tag in HTML format
		/// </summary>
		/// <param name="HtmlWriter">Html text writer to write to</param>
		public abstract void WriteHtml(HtmlTextWriter HtmlWriter);
	
		#endregion

		#region Static Public Methods
		/// <summary>
		/// Checks if the provided string is an auxiliary tag
		/// </summary>
		/// <param name="InString">String to check if it is an auxiliary tag</param>
		/// <returns>true if the provided string is an auxiliary tag; false otherwise</returns>
		public static bool IsStringAnAuxiliaryTag(String InString)
		{
			return TagNameToTypeMap.ContainsKey(InString.ToLower());
		}

		/// <summary>
		/// Returns a type associated with a given string
		/// </summary>
		/// <param name="InString">String whose type should be retrieved</param>
		/// <returns>The type associated with the provided string; null if no type is associated with the provided string</returns>
		public static Type GetTagTypeFromString(String InString)
		{
			Type FoundType = null;
			TagNameToTypeMap.TryGetValue(InString.ToLower(), out FoundType);
			return FoundType;
		}
		#endregion
	}

	/// <summary>
	/// Concrete implementation of P4AuxiliaryTag, representing the "Extra" tag in the tagging system, which allows the user
	/// to specify additional information about a change
	/// </summary>
	[XmlType("ExtraInformation")]
	public class P4ExtraTag : P4AuxiliaryTag
	{
		#region Member Variables
		/// <summary>
		/// Description string
		/// </summary>
		private String mDescription = String.Empty;
		#endregion

		#region Properties
		/// <summary>
		/// Name of the tag/how it appears in a changelist; Used with the tag to type dictionary
		/// </summary>
		public override String TagName { get { return "extra"; } }

		/// <summary>
		/// Description string
		/// </summary>
		public String Description
		{
			get { return mDescription; }
			set { mDescription = value; }
		}
		#endregion

		#region Constructors
		/// <summary>
		/// Construct a P4ExtraTag; Default constructor required for Activation.CreateInstance, as well as Xml serialization
		/// </summary>
		public P4ExtraTag() { }

		/// <summary>
		/// Construct a P4ExtraTag, populating it with the data from the provided string
		/// </summary>
		/// <param name="InStringToParse">String to use to populate the tag with</param>
		public P4ExtraTag(String InStringToParse)
		{
			PopulateFromString(InStringToParse);
		}
		#endregion

		#region Public Methods
		/// <summary>
		/// Populate the tag from a provided string, if possible
		/// </summary>
		/// <param name="InStringToParse">String to populate from</param>
		/// <returns>true if the tag was successfully populated from the provided string; false otherwise</returns>
		public override bool PopulateFromString(String InStringToParse)
		{
			mDescription = InStringToParse;
			return true;
		}

		/// <summary>
		/// Write out the contents of the tag in HTML format
		/// </summary>
		/// <param name="HtmlWriter">Html text writer to write to</param>
		public override void WriteHtml(HtmlTextWriter HtmlWriter)
		{
			HtmlWriter.Write(TagName);

			// Write the description as a list element
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Ul);
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Li);
			HtmlWriter.Write(mDescription);
			HtmlWriter.RenderEndTag();
			HtmlWriter.RenderEndTag();
		}
		#endregion
	}

	/// <summary>
	/// Concrete implementation of P4AuxiliaryTag, representing the "ttp" tag in the tagging system, which allows the user
	/// to specify a TestTrack Pro defect that is associated with this change
	/// </summary>
	[XmlType("TTP")]
	public class P4TTPTag : P4AuxiliaryTag
	{
		#region Member Variables
		/// <summary>
		/// TestTrack Pro defect number associated with the change
		/// </summary>
		private int mTTPNumber = 0;
		#endregion

		#region Properties
		/// <summary>
		/// Name of the tag/how it appears in a changelist; Used with the tag to type dictionary
		/// </summary>
		public override String TagName { get { return "ttp"; } }

		/// <summary>
		/// TestTrack Pro defect number associated with the change
		/// </summary>
		[XmlAttribute("Number")]
		public int TTPNumber
		{
			get { return mTTPNumber; }
			set { mTTPNumber = value; }
		}
		#endregion

		#region Constructors

		/// <summary>
		/// Construct a P4TTPTag; Default constructor required for Activation.CreateInstance, as well as Xml serialization
		/// </summary>
		public P4TTPTag() {}

		/// <summary>
		/// Construct a P4TTPTag, populating it with data from the provided string
		/// </summary>
		/// <param name="InStringToParse">String to use to populate the tag</param>
		public P4TTPTag(String InStringToParse)
		{
			PopulateFromString(InStringToParse);
		}
		#endregion

		/// <summary>
		/// Populate the tag from a provided string, if possible
		/// </summary>
		/// <param name="InStringToParse">String to populate from</param>
		/// <returns>true if the tag was successfully populated from the provided string; false otherwise</returns>
		public override bool PopulateFromString(String InStringToParse)
		{
			// Split on whitespace to remove potential junk after the TTP number
			String[] StringArgs = InStringToParse.Split(new String[] { "\t", " " }, StringSplitOptions.RemoveEmptyEntries);
			return int.TryParse(StringArgs[0], out mTTPNumber);
		}

		/// <summary>
		/// Write out the contents of the tag in HTML format
		/// </summary>
		/// <param name="HtmlWriter">Html text writer to write to</param>
		public override void WriteHtml(HtmlTextWriter HtmlWriter)
		{
			HtmlWriter.Write(TagName + " " + mTTPNumber);
		}
	}

	/// <summary>
	/// Concrete implementation of P4AuxiliaryTag, representing the "fixttp" tag in the tagging system, which allows the user
	/// to specify a TestTrack Pro defect fixed by the particular change, how much time was spent on the task, and to provide an
	/// optional comment to submit to TestTrack Pro as a result of fixing the issue
	/// </summary>
	[XmlType("FixTTP")]
	public class P4FixTTPTag : P4AuxiliaryTag
	{
		#region Member Variables
		/// <summary>
		/// TestTrack Pro defect number associated with the fix
		/// </summary>
		private int mTTPNumber = 0;

		/// <summary>
		/// Number of hours spent on the fix
		/// </summary>
		private int mTimeSpent = 0;

		/// <summary>
		/// Additional comment to submit to the TestTrack Pro fix dialog
		/// </summary>
		private String mComment = String.Empty;
		#endregion

		#region Properties
		/// <summary>
		/// Name of the tag/how it appears in a changelist; Used with the tag to type dictionary
		/// </summary>
		public override String TagName { get { return "fixttp"; } }

		/// <summary>
		/// TestTrack Pro defect number associated with the fix
		/// </summary>
		[XmlAttribute("Number")]
		public int TTPNumber
		{
			get { return mTTPNumber; }
			set { mTTPNumber = value; }
		}

		/// <summary>
		/// Number of hours spent on the fix
		/// </summary>
		[XmlElement("TimeSpent")]
		public int TimeSpent
		{
			get { return mTimeSpent; }
			set { mTimeSpent = value; }
		}

		/// <summary>
		/// Additional comment to submit to the TestTrack Pro fix dialog
		/// </summary>
		[XmlElement("Comment", IsNullable = false)]
		public String Comment
		{
			get { return mComment; }
			set { mComment = value; }
		}
		#endregion

		#region Constructors
		/// <summary>
		/// Construct a P4FixTTPTag; Default constructor required for Activation.CreateInstance, as well as Xml serialization
		/// </summary>
		public P4FixTTPTag() {}

		/// <summary>
		/// Construct a P4FixTTPTag, populating the tag from the provided string
		/// </summary>
		/// <param name="InStringToParse">String to populate the tag with</param>
		public P4FixTTPTag(String InStringToParse)
		{
			PopulateFromString(InStringToParse);
		}
		#endregion

		/// <summary>
		/// Populate the tag from a provided string, if possible
		/// </summary>
		/// <param name="InStringToParse">String to populate from</param>
		/// <returns>true if the tag was successfully populated from the provided string; false otherwise</returns>
		public override bool PopulateFromString(String InStringToParse)
		{
			// Split the string on white space
			String[] Delimiters = { " ", "\t" };
			String[] StringArgs = InStringToParse.Split(Delimiters, 3, StringSplitOptions.RemoveEmptyEntries);

			bool bValidPopulationFromString = true;

			// If there aren't two or three arguments after the split, this tag can't be properly populated
			if (StringArgs.Length >= 2 && StringArgs.Length <= 3)
			{
				// Try to parse the TTP number from the string
				if (!int.TryParse(StringArgs[0], out mTTPNumber))
				{
					bValidPopulationFromString = false;
				}

				// Try to parse the time spent from the string
				if (!int.TryParse(StringArgs[1], out mTimeSpent))
				{
					bValidPopulationFromString = false;
				}

				// If there are three sub-strings, the last one is the comment
				if (StringArgs.Length == 3)
				{
					mComment = StringArgs[2];
				}
			}
			else
			{
				bValidPopulationFromString = false;
			}
			return bValidPopulationFromString;
		}

		/// <summary>
		/// Write out the contents of the tag in HTML format
		/// </summary>
		/// <param name="HtmlWriter">Html text writer to write to</param>
		public override void WriteHtml(HtmlTextWriter HtmlWriter)
		{
			HtmlWriter.Write(TagName);

			// Write the ttp number, hours spent, and optional comment
			// as list elements
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Ul);
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Li);
			HtmlWriter.Write("Number: " + mTTPNumber);
			HtmlWriter.RenderEndTag();
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Li);
			HtmlWriter.Write("Hours Spent: " + mTimeSpent);
			HtmlWriter.RenderEndTag();

			// Only write out the comment if it's not the empty string
			if (!String.IsNullOrEmpty(mComment))
			{
				HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Li);
				HtmlWriter.Write("Comment: " + mComment);
				HtmlWriter.RenderEndTag();
			}
			HtmlWriter.RenderEndTag();
		}
	}

	/// <summary>
	/// Concrete implementation of P4AuxiliaryTag, representing the "proj" tag in the tagging system, which allows the user
	/// to specify which projects are affected by the change
	/// </summary>
	[XmlType("Proj")]
	public class P4ProjTag : P4AuxiliaryTag
	{
		#region Member Variables
		/// <summary>
		/// Project affected by the change
		/// </summary>
		private String mProject;
		#endregion

		#region Properties
		/// <summary>
		/// Name of the tag/how it appears in a changelist; Used with the tag to type dictionary
		/// </summary>
		public override String TagName { get { return "proj"; } }

		/// <summary>
		/// Project affected by the change
		/// </summary>
		[XmlAttribute("Name")]
		public String Project
		{
			get { return mProject; }
			set { mProject = value; }
		}
		#endregion

		#region Constructors
		/// <summary>
		/// Construct a P4ProjTag; Default constructor required for Activator.CreateInstance, as well as Xml serialization
		/// </summary>
		public P4ProjTag() {}

		/// <summary>
		/// Construct a P4ProjTag, populating it from the provided string
		/// </summary>
		/// <param name="InStringToParse">String to populate the tag from</param>
		public P4ProjTag(String InStringToParse)
		{
			PopulateFromString(InStringToParse);
		}
		#endregion

		/// <summary>
		/// Populate the tag from a provided string, if possible
		/// </summary>
		/// <param name="InStringToParse">String to populate from</param>
		/// <returns>true if the tag was successfully populated from the provided string; false otherwise</returns>
		public override bool PopulateFromString(String InStringToParse)
		{
			mProject = InStringToParse;
			return true;
		}

		/// <summary>
		/// Write out the contents of the tag in HTML format
		/// </summary>
		/// <param name="HtmlWriter">Html text writer to write to</param>
		public override void WriteHtml(HtmlTextWriter HtmlWriter)
		{
			HtmlWriter.Write(TagName + " " + mProject);
		}
	}

	/// <summary>
	/// Concrete implementation of P4AuxiliaryTag, representing the "qanote" tag in the tagging system, which allows the user
	/// to specify a comment/useful information to QA testers
	/// </summary>
	[XmlType("QANote")]
	public class P4QANoteTag : P4AuxiliaryTag
	{
		#region Member Variables
		/// <summary>
		/// Note to QA testers
		/// </summary>
		private String mQADescription;
		#endregion

		#region Properties
		/// <summary>
		/// Name of the tag/how it appears in a changelist; Used with the tag to type dictionary
		/// </summary>
		public override String TagName { get { return "qanote"; } }

		/// <summary>
		/// Note to QA testers
		/// </summary>
		public String Description
		{
			get { return mQADescription; }
			set { mQADescription = value; }
		}
		#endregion

		#region Constructors
		/// <summary>
		/// Construct a P4QANoteTag; Default constructor required for Activator.CreateInstance and Xml serialization
		/// </summary>
		public P4QANoteTag() {}

		/// <summary>
		/// Construct a P4QANoteTag, populating it from the provided string
		/// </summary>
		/// <param name="InStringToParse">String to populate the tag from</param>
		public P4QANoteTag(String InStringToParse)
		{
			PopulateFromString(InStringToParse);
		}
		#endregion

		/// <summary>
		/// Populate the tag from a provided string, if possible
		/// </summary>
		/// <param name="InStringToParse">String to populate from</param>
		/// <returns>true if the tag was successfully populated from the provided string; false otherwise</returns>
		public override bool PopulateFromString(String InStringToParse)
		{
			mQADescription = InStringToParse;
			return true;
		}

		/// <summary>
		/// Write out the contents of the tag in HTML format
		/// </summary>
		/// <param name="HtmlWriter">Html text writer to write to</param>
		public override void WriteHtml(HtmlTextWriter HtmlWriter)
		{
			HtmlWriter.Write(TagName);
			
			// Write the description as a list element
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Ul);
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Li);
			HtmlWriter.Write(mQADescription);
			HtmlWriter.RenderEndTag();
			HtmlWriter.RenderEndTag();
		}
	}

	/// <summary>
	/// Concrete implementation of P4AuxiliaryTag, representing the "branch" tag in the tagging system, which allows the user
	/// to specify the branch name the submission is occurring in
	/// </summary>
	[XmlType("Branch")]
	public class P4BranchTag : P4AuxiliaryTag
	{
		#region Properties
		/// <summary>
		/// Name of the branch the submission is occurring in
		/// </summary>
		[XmlAttribute("Name")]
		public String BranchName { get; set; }

		/// <summary>
		/// Name of the tag/how it appears in a changelist; Used with the tag to type dictionary
		/// </summary>
		public override String TagName
		{
			get { return "branch"; }
		}
		#endregion

		#region Constructors
		/// <summary>
		/// Construct a P4BranchTag; Default constructor required for Activator.CreateInstance and Xml serialization
		/// </summary>
		public P4BranchTag() {}

		/// <summary>
		/// Construct a P4BranchTag, populating it from the provided string
		/// </summary>
		/// <param name="InStringToParse">String to populate the tag from</param>
		public P4BranchTag(String InStringToParse)
		{
			PopulateFromString(InStringToParse);
		}
		#endregion

		/// <summary>
		/// Populate the tag from a provided string, if possible
		/// </summary>
		/// <param name="InStringToParse">String to populate from</param>
		/// <returns>true if the tag was successfully populated from the provided string; false otherwise</returns>
		public override bool PopulateFromString(String InStringToParse)
		{
			BranchName = InStringToParse;
			return true;
		}

		/// <summary>
		/// Write out the contents of the tag in HTML format
		/// </summary>
		/// <param name="HtmlWriter">Html text writer to write to</param>
		public override void WriteHtml(HtmlTextWriter HtmlWriter)
		{
			HtmlWriter.Write(TagName + " " + BranchName);
		}
	}

	///// <summary>
	///// Concrete implementation of P4AuxiliaryTag, representing the "integrate" tag in the tagging system, which allows the user
	///// to specify a changelist number being integrated, as well as an optional branch name to specify where the integration is coming from
	///// </summary>
	//public class P4IntegrateTag : P4AuxiliaryTag
	//{
	//    #region Member Variables
	//    /// <summary>
	//    /// Changelist number being integrated
	//    /// </summary>
	//    private int mChangelistNumber;
	//    #endregion

	//    #region Properties
	//    /// <summary>
	//    /// Changelist number being integrated
	//    /// </summary>
	//    [XmlAttribute("Changelist")]
	//    public int ChangelistNumber 
	//    {
	//        get { return mChangelistNumber; } 
	//        set { mChangelistNumber = value; }
	//    }

	//    /// <summary>
	//    /// Name of the branch the integration is occurring from
	//    /// </summary>
	//    [XmlAttribute("FromBranch")]
	//    public String BranchName { get; set; }
	//    #endregion

	//    /// <summary>
	//    /// Name of the tag/how it appears in a changelist; Used with the tag to type dictionary
	//    /// </summary>
	//    public override String TagName
	//    {
	//        get { return "integrate"; }
	//    }

	//    #region Constructors
	//    /// <summary>
	//    /// Construct a P4IntegrateTag; Default constructor required for Activator.CreateInstance and Xml serialization
	//    /// </summary>
	//    public P4IntegrateTag() { }

	//    /// <summary>
	//    /// Construct a P4IntegrateTag, populating it from the provided string
	//    /// </summary>
	//    /// <param name="InStringToParse">String to populate the tag from</param>
	//    public P4IntegrateTag(String InStringToParse)
	//    {
	//        PopulateFromString(InStringToParse);
	//    }
	//    #endregion

	//    /// <summary>
	//    /// Populate the tag from a provided string, if possible
	//    /// </summary>
	//    /// <param name="InStringToParse">String to populate from</param>
	//    /// <returns>true if the tag was successfully populated from the provided string; false otherwise</returns>
	//    public override bool PopulateFromString(String InStringToParse)
	//    {
	//        // Split the string on white space
	//        String[] Delimiters = { " ", "\t" };
	//        String[] StringArgs = InStringToParse.Split(Delimiters, 2, StringSplitOptions.RemoveEmptyEntries);

	//        bool bValidPopulationFromString = true;
	//        if (StringArgs.Length > 0 && StringArgs.Length < 3)
	//        {
	//            // Attempt to parse the changelist number from the string
	//            if (!int.TryParse(StringArgs[0], out mChangelistNumber))
	//            {
	//                bValidPopulationFromString = false;
	//            }

	//            // Parse the branch name from the string, if it was provided
	//            if (StringArgs.Length > 1)
	//            {
	//                BranchName = StringArgs[1];
	//            }
	//        }
	//        else
	//        {
	//            bValidPopulationFromString = false;
	//        }

	//        return bValidPopulationFromString;
	//    }

	//    /// <summary>
	//    /// Write out the contents of the tag in HTML format
	//    /// </summary>
	//    /// <param name="HtmlWriter">Html text writer to write to</param>
	//    public override void WriteHtml(HtmlTextWriter HtmlWriter)
	//    {
	//        HtmlWriter.Write(TagName + " " + ChangelistNumber);
	//        if (!String.IsNullOrEmpty(BranchName))
	//        {
	//            HtmlWriter.Write(" from " + BranchName + " branch");
	//        }
	//    }
	//}
}
