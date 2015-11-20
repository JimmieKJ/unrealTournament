// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*
 * MarkdownSharp
 * -------------
 * a C# Markdown processor
 * 
 * Markdown is a text-to-HTML conversion tool for web writers
 * Copyright (c) 2004 John Gruber
 * http://daringfireball.net/projects/markdown/
 * 
 * Markdown.NET
 * Copyright (c) 2004-2009 Milan Negovan
 * http://www.aspnetresources.com
 * http://aspnetresources.com/blog/markdown_announced.aspx
 * 
 * MarkdownSharp
 * Copyright (c) 2009-2010 Jeff Atwood
 * http://stackoverflow.com
 * http://www.codinghorror.com/blog/
 * http://code.google.com/p/markdownsharp/
 * 
 * History: Milan ported the Markdown processor to C#. He granted license to me so I can open source it
 * and let the community contribute to and improve MarkdownSharp.
 * 
 */

#region Copyright and license

/*

Copyright (c) 2009 - 2010 Jeff Atwood

http://www.opensource.org/licenses/mit-license.php
  
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

Copyright (c) 2003-2004 John Gruber
<http://daringfireball.net/>   
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name "Markdown" nor the names of its contributors may
  be used to endorse or promote products derived from this software
  without specific prior written permission.

This software is provided by the copyright holders and contributors "as
is" and any express or implied warranties, including, but not limited
to, the implied warranties of merchantability and fitness for a
particular purpose are disclaimed. In no event shall the copyright owner
or contributors be liable for any direct, indirect, incidental, special,
exemplary, or consequential damages (including, but not limited to,
procurement of substitute goods or services; loss of use, data, or
profits; or business interruption) however caused and on any theory of
liability, whether in contract, strict liability, or tort (including
negligence or otherwise) arising in any way out of the use of this
software, even if advised of the possibility of such damage.
*/

#endregion

using System;
using System.IO;
using System.Collections.Generic;
using System.Configuration;
using System.Text;
using System.Text.RegularExpressions;
using System.Linq;
using System.Drawing;
using System.Drawing.Imaging;
using DotLiquid;
using MarkdownSharp.Preprocessor;

namespace MarkdownSharp
{
    using MarkdownSharp.EpicMarkdown;
    using MarkdownSharp.EpicMarkdown.PathProviders;

    /// <summary>
    /// Options controlling functionality of the application
    /// </summary>
    public class MarkdownOptions
    {
        /// <summary>
        /// when true, (most) bare plain URLs are auto-hyperlinked  
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool AutoHyperlink { get; set; }
        /// <summary>
        /// when true, RETURN becomes a literal newline  
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool AutoNewlines { get; set; }
        /// <summary>
        /// use ">" for HTML output, or " />" for XHTML output
        /// </summary>
        public string EmptyElementSuffix { get; set; }
        /// <summary>
        /// when true, problematic URL characters like [, ], (, and so forth will be encoded 
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool EncodeProblemUrlCharacters { get; set; }
        /// <summary>
        /// when false, email addresses will never be auto-linked  
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool LinkEmails { get; set; }
        /// <summary>
        /// when true, bold and italic require non-word characters on either side  
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool StrictBoldItalic { get; set; }
    }


    /// <summary>
    /// Structure containing details of detected attachments in the files.
    /// </summary>
    public struct AttachmentConversionDetail
    {
        /// <summary>
        /// Constructor.
        /// </summary>
        public AttachmentConversionDetail(string InAttachName, string OutAttachName)
            : this()
        {
            this.InAttachName = InAttachName;
            this.OutAttachName = OutAttachName;
        }

        /// <summary>
        /// File name in markdown.text/attach folder
        /// </summary>
        public string InAttachName { get; private set; }

        /// <summary>
        /// File name in hmtl/attach folder
        /// </summary>
        public string OutAttachName { get; private set; }
    }

    /// <summary>
    /// Class of Message reported back to calling procedure
    /// </summary>
    public enum MessageClass
    {
        /// <summary>
        /// Info lowest level of error
        /// </summary>
        Info,
        /// <summary>
        /// Error is highest level of error
        /// </summary>
        Error,
        /// <summary>
        /// Warning is less severe than error
        /// </summary>
        Warning 
    }

    public struct ErrorDetail
    {
        public ErrorDetail(string message, MessageClass classOfMessage, string linkToErrorInHTML, string originalText, int line, int column, string path = null)
            : this()
        {
            Message = message;
            ClassOfMessage = classOfMessage;
            LinkToErrorInHTML = linkToErrorInHTML;
            OriginalText = originalText;
            Line = line;
            Column = column;
            Path = path;
        }

        public string Message { get; private set; }
        public MessageClass ClassOfMessage { get; private set; }
        public string LinkToErrorInHTML { get; private set; }
        public string OriginalText { get; private set; }

        // Only if different than current.
        public string Path { get; private set; }
        public int Line { get; private set; }
        public int Column { get; private set; }

    }

    /// <summary>
    /// DotLiquid access for ErrorDetail class.
    /// </summary>
    [LiquidType("Message", "LinkToErrorInHTML", "IsError", "IsWarning", "IsInfo", "IsLink")]
    public class ErrorDetailDrop
    {
        public bool IsError
        {
            get { return _errorDetail.ClassOfMessage == MessageClass.Error; }
        }

        public bool IsWarning
        {
            get { return _errorDetail.ClassOfMessage == MessageClass.Warning; }
        }

        public bool IsInfo
        {
            get { return _errorDetail.ClassOfMessage == MessageClass.Info; }
        }

        public bool IsLink
        {
            get { return !string.IsNullOrWhiteSpace(LinkToErrorInHTML); }
        }

        public string Message
        {
            get { return _errorDetail.Message; }
        }

        public string LinkToErrorInHTML
        {
            get { return _errorDetail.LinkToErrorInHTML; }
        }

        public ErrorDetailDrop(ErrorDetail errorDetail)
        {
            _errorDetail = errorDetail;
        }

        private ErrorDetail _errorDetail;
    }

    /// <summary>
    /// File and Folder Information needed to run Epic's markdown on a particular file
    /// </summary>
    public class FolderDetails
    {
        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="RelativeHTMLPath">Relative path from the target folder of the current file to the HTML folder</param>
        /// <param name="AbsoluteHTMLPath">Absolute path to the target html folder</param>
        /// <param name="AbsoluteMarkdownPath">Absolute path to the source markdown folder</param>
        /// <param name="CurrentFolderFromMarkdownAsTopLeaf">Path from the source directory to current file's folder</param>
        /// <param name="APIFolderLocationFromMarkdownAsTop">Path from the source directory to the API folder</param>
        /// <param name="DocumentTitle">Document title generated from the file name and language, this will be supersecceded by Title: meta data</param>
        /// <param name="Language">The language of the file, given by the extension .LANG.udn </param>
        public FolderDetails(string RelativeHTMLPath, string AbsoluteHTMLPath, string AbsoluteMarkdownPath, string CurrentFolderFromMarkdownAsTopLeaf, string APIFolderLocationFromMarkdownAsTop, string DocumentTitle, string Language)
        {
            this.RelativeHTMLPath = RelativeHTMLPath;
            this.AbsoluteHTMLPath = AbsoluteHTMLPath;
            this.AbsoluteMarkdownPath = AbsoluteMarkdownPath;
            this.CurrentFolderFromMarkdownAsTopLeaf = CurrentFolderFromMarkdownAsTopLeaf;
            this.APIFolderLocationFromMarkdownAsTop = APIFolderLocationFromMarkdownAsTop;
            this.DocumentTitle = DocumentTitle;
            this.Language = Language;
        }

        /// <summary>
        /// Constructor
        /// </summary>
        public FolderDetails()
        {
        }

        public FolderDetails(FolderDetails folderDetails)
            : this(folderDetails.RelativeHTMLPath, folderDetails.AbsoluteHTMLPath, folderDetails.AbsoluteMarkdownPath,
                folderDetails.CurrentFolderFromMarkdownAsTopLeaf, folderDetails.APIFolderLocationFromMarkdownAsTop,
                folderDetails.DocumentTitle, folderDetails.Language)
        {

        }

        /// <summary>
        /// Relative path from the target folder of the current file to the HTML folder
        /// </summary>
        public string RelativeHTMLPath { get; set; }

        /// <summary>
        /// Absolute path to the target html folder
        /// </summary>
        public string AbsoluteHTMLPath { get; set; }

        /// <summary>
        /// Absolute path to the source markdown folder
        /// </summary>
        public string AbsoluteMarkdownPath { get; set; }

        /// <summary>
        /// Path from the source directory to current file's folder
        /// </summary>
        public string CurrentFolderFromMarkdownAsTopLeaf { get; set; }

        /// <summary>
        /// Path from the source directory to the API folder
        /// </summary>
        public string APIFolderLocationFromMarkdownAsTop { get; set; }

        /// <summary>
        /// Document title generated from the file name and language, this will be superseded by Title: meta data
        /// </summary>
        public string DocumentTitle { get; set; }

        /// <summary>
        /// The language of the file, given by the extension .LANG.udn
        /// </summary>
        public string Language { get; set; }

        /// <summary>
        /// Return the name of the file used for generation of this markdown.
        /// </summary>
        public string GetThisFileName()
        {
            FileInfo[] ThisFileInfo = new DirectoryInfo(Path.Combine(AbsoluteMarkdownPath, CurrentFolderFromMarkdownAsTopLeaf)).GetFiles("*." + Language + ".udn");
            return ThisFileInfo[0].FullName;
        }

    }




    /// <summary>
    /// Markdown is a text-to-HTML conversion tool for web writers. 
    /// Markdown allows you to write using an easy-to-read, easy-to-write plain text format, 
    /// then convert it to structurally valid XHTML (or HTML).
    /// </summary>
    public class Markdown
    {
        private const string _version = "Epic.1.13";

        #region Constructors and Options

        /// <summary>
        /// Create a new Markdown instance using default options
        /// </summary>
        public Markdown()
            : this(false)
        {
        }

        /// <summary>
        /// Create a new Markdown instance and optionally load options from a configuration
        /// file. There they should be stored in the appSettings section, available options are:
        /// 
        ///     Markdown.StrictBoldItalic (true/false)
        ///     Markdown.EmptyElementSuffix (">" or " />" without the quotes)
        ///     Markdown.LinkEmails (true/false)
        ///     Markdown.AutoNewLines (true/false)
        ///     Markdown.AutoHyperlink (true/false)
        ///     Markdown.EncodeProblemUrlCharacters (true/false) 
        ///     
        /// </summary>
        public Markdown(bool loadOptionsFromConfigFile)
        {
            SupportedLanguageMap = new Dictionary<string, string>();
            JustReadVariableDefinitionsRun = false;

            Preprocessor = new Preprocessor.Preprocessor(this);

            if (!loadOptionsFromConfigFile) return;

            var settings = ConfigurationManager.AppSettings;
            foreach (string key in settings.Keys)
            {
                switch (key)
                {
                    case "Markdown.AutoHyperlink":
                        _autoHyperlink = Convert.ToBoolean(settings[key]);
                        break;
                    case "Markdown.AutoNewlines":
                        _autoNewlines = Convert.ToBoolean(settings[key]);
                        break;
                    case "Markdown.EmptyElementSuffix":
                        _emptyElementSuffix = settings[key];
                        break;
                    case "Markdown.EncodeProblemUrlCharacters":
                        _encodeProblemUrlCharacters = Convert.ToBoolean(settings[key]);
                        break;
                    case "Markdown.LinkEmails":
                        _linkEmails = Convert.ToBoolean(settings[key]);
                        break;
                    case "Markdown.StrictBoldItalic":
                        _strictBoldItalic = Convert.ToBoolean(settings[key]);
                        break;
                }
            }
        }

        /// <summary>
        /// Create a new Markdown instance and set the options from the MarkdownOptions object.
        /// </summary>
        public Markdown(MarkdownOptions options)
        {
            SupportedLanguageMap = new Dictionary<string, string>();
            _autoHyperlink = options.AutoHyperlink;
            _autoNewlines = options.AutoNewlines;
            _emptyElementSuffix = options.EmptyElementSuffix;
            _encodeProblemUrlCharacters = options.EncodeProblemUrlCharacters;
            _linkEmails = options.LinkEmails;
            _strictBoldItalic = options.StrictBoldItalic;

            Preprocessor = new Preprocessor.Preprocessor(this);
        }


        /// <summary>
        /// use ">" for HTML output, or " />" for XHTML output
        /// </summary>
        public string EmptyElementSuffix
        {
            get { return _emptyElementSuffix; }
            set { _emptyElementSuffix = value; }
        }
        private string _emptyElementSuffix = " />";

        /// <summary>
        /// when false, email addresses will never be auto-linked  
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool LinkEmails
        {
            get { return _linkEmails; }
            set { _linkEmails = value; }
        }
        private bool _linkEmails = true;

        /// <summary>
        /// when true, bold and italic require non-word characters on either side  
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool StrictBoldItalic
        {
            get { return _strictBoldItalic; }
            set { _strictBoldItalic = value; }
        }
        //@UE3 - StrictBoldItalic is true to stop filenames with _ in being treated as <em>
        private bool _strictBoldItalic = true;

        /// <summary>
        /// when true, RETURN becomes a literal newline  
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool AutoNewLines
        {
            get { return _autoNewlines; }
            set { _autoNewlines = value; }
        }
        private bool _autoNewlines = false;

        /// <summary>
        /// when true, (most) bare plain URLs are auto-hyperlinked  
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool AutoHyperlink
        {
            get { return _autoHyperlink; }
            set { _autoHyperlink = value; }
        }
        private bool _autoHyperlink = false;

        /// <summary>
        /// when true, problematic URL characters like [, ], (, and so forth will be encoded 
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool EncodeProblemUrlCharacters
        {
            get { return _encodeProblemUrlCharacters; }
            set { _encodeProblemUrlCharacters = value; }
        }
        private bool _encodeProblemUrlCharacters = false;
 
        #endregion

        private enum TokenType { Text, Tag }

        private struct Token
        {
            public Token(TokenType type, string value)
                :this()
            {
                this.Type = type;
                this.Value = value;
            }
            public TokenType Type {get; set;}
            public string Value {get; set;}
        }

        /// <summary>
        /// maximum nested depth of [] and () supported by the transform; implementation detail
        /// </summary>
        private const int _nestDepth = 6;

        /// <summary>
        /// Tabs are automatically converted to spaces as part of the transform  
        /// this constant determines how "wide" those tabs become in spaces  
        /// </summary>
        public static readonly int TabWidth = 4;

        // Unordered list pattern matching
        public const string _markerUL = @"[-*+]";

        // Ordered list pattern matching
        public const string _markerOL = @"\d+[.]";
        
        // @UE3 definition list
        public const string _markerDL = @"[\$]";

        private static readonly Dictionary<string, string> _escapeTable;
        private static readonly Dictionary<string, string> _invertedEscapeTable;
        public static readonly Dictionary<string, string> _backslashEscapeTable;

        //Added dictionary to hold the API manifest information for looking up document names
        public static readonly Dictionary<string, string> APIFileLocation = new Dictionary<string, string>();

        private readonly Dictionary<string, string> _quoteBlock = new Dictionary<string, string>();

        //@UE3 Original Text split into lines
        private static string[] OriginalText { get; set; }

        // Internal counter incremented when stepping to a new list level and decremented on the way out.
        private int ListLevel { get; set; }

        // What languages are supported
        public Dictionary<string, string> SupportedLanguageMap { get; set; }

        // What languages are supported
        public string[] SupportedLanguages { get; set; }

        // What languages are supported
        public string[] SupportedLanguageLabels { get; set; }

        // Error if this metadata value is missing from a file
        public static string[] MetadataErrorIfMissing { get; set; }

        // Warn if this metadata value is missing from a file
        public static string[] MetadataInfoIfMissing { get; set; }

        // What are the publish settings
        public List<string> PublishFlags { get; set; }

        // Do not publish flag
        public string DoNotPublishAvailabilityFlag { get; set; }

        // What are the allowed Availability settings
        public List<string> AllSupportedAvailability { get; set; }

        // Name of html document to use as the template
        public string DefaultTemplate { get; set; }

        // Skipping variable replacement trigger
        public bool JustReadVariableDefinitionsRun { get; set; }
        
        // Is this running in preview mode?
        public bool ThisIsPreview { get; set; }

        // Should images be compressed by default?
        public bool DefaultImageDoCompress { get; set; }

        // On converting to jpg what color should be used to replace transparent
        public Color DefaultImageFillColor { get; set; }
        
        // On compression what should be the quality of the original - e.g. 80(%)
        public int DefaultImageQuality { get; set; }

        // What is the extension for the converted image format
        public string DefaultImageFormatExtension { get; set; }

        // What are we converting our images too by default
        public ImageFormat DefaultImageFormat { get; set; }
        
        public static ErrorDetail GenerateError(string FullErrorMessage, MessageClass ClassOfError, string OriginalText, int CountOfPreceedingErrors, TransformationData translationVariablesForThisRun)
        {
            //Check the OrignalText, it must not be blank
            if (String.IsNullOrWhiteSpace(OriginalText))
            {
                //Unable to determine line of error with blank OriginalText
                FullErrorMessage = Language.Message("UnableToDetectLineOfError", FullErrorMessage);
            }
            else
            {
                //Get the line numbers with the string that caused this error
                List<ErrorLineNumbersAndColumns> ErrorLineNumbers = GetErrorLineNumbers(OriginalText);

                // 1 detection, raise
                if (ErrorLineNumbers.Count() == 1)
                {
                    return (new ErrorDetail(FullErrorMessage, ClassOfError, "#Error" + CountOfPreceedingErrors, OriginalText, ErrorLineNumbers[0].LineNumber, ErrorLineNumbers[0].Column));
                }
                //more than check for the error line number and column not already raised
                else
                {
                    foreach (ErrorLineNumbersAndColumns LineNumberAndColumn in ErrorLineNumbers)
                    {

                        bool FoundThisErrorHasBeenRaised = false;
                        foreach (ErrorDetail Error in translationVariablesForThisRun.ErrorList)
                        {
                            if (Error.Line == LineNumberAndColumn.LineNumber && Error.Column == LineNumberAndColumn.Column && Error.OriginalText.Equals(OriginalText))
                            {
                                FoundThisErrorHasBeenRaised = true;
                            }
                        }
                        if (!FoundThisErrorHasBeenRaised)
                        {
                            return (new ErrorDetail(FullErrorMessage, ClassOfError, "#Error" + CountOfPreceedingErrors, OriginalText, LineNumberAndColumn.LineNumber, LineNumberAndColumn.Column));
                        }
                    }
                }
            }

            //if here problem have to return 0 line number
            return (new ErrorDetail(FullErrorMessage, ClassOfError, "#Error" + CountOfPreceedingErrors, OriginalText, 0, 0));
        }

        private struct ErrorLineNumbersAndColumns
        {
            public ErrorLineNumbersAndColumns(int LineNumber, int Column)
            {
                this.lineNumber = LineNumber;
                this.column = Column;
            }
            private int lineNumber;
            private int column;

            public int LineNumber
            {
                get { return lineNumber; }
            }
            public int Column
            {
                get { return column; }
            }
        }


        private static List<ErrorLineNumbersAndColumns> GetErrorLineNumbers(string Text)
        {
            List<ErrorLineNumbersAndColumns> IndexesOfErrorString = new List<ErrorLineNumbersAndColumns>();

            int LineNumber = 0;

            foreach (string LineOfText in OriginalText)
            {
                ++LineNumber;
                if (LineOfText.Contains(Text))
                {
                    //check for columns on this row.
                    if (LineOfText.Equals(Text))
                    {
                        IndexesOfErrorString.Add(new ErrorLineNumbersAndColumns(LineNumber,0));
                    }
                    else
                    {
                        int i = 0;
                        while ((i = LineOfText.IndexOf(Text,i))!=-1)
                        {
                            IndexesOfErrorString.Add(new ErrorLineNumbersAndColumns(LineNumber,i));
                            i++;
                        }
                    }
                }
            }

            return IndexesOfErrorString;
        }


        /// <summary>
        /// In the static constuctor we'll initialize what stays the same across all transforms.
        /// </summary>
        static Markdown()
        {
            // Table of hash values for escaped characters:
            _escapeTable = new Dictionary<string, string>();
            _invertedEscapeTable = new Dictionary<string, string>();
            // Table of hash value for backslash escaped characters:
            _backslashEscapeTable = new Dictionary<string, string>();

            string backslashPattern = "";

            foreach (char EachCharacter in @"\`*_{}[]()>#+-.!|%")
            {
                string key = EachCharacter.ToString();
                string hash = GetHashKey(key);
                _escapeTable.Add(key, hash);
                _invertedEscapeTable.Add(hash, key);
                _backslashEscapeTable.Add(@"\" + key, hash);
                //@UE3 stop escaping . followed by dot, which will happen in relative paths
                if (key == ".")
                {
                    backslashPattern += Regex.Escape(@"\" + key) + "(?!\\.)|";
                }
                else
                {
                    backslashPattern += Regex.Escape(@"\" + key) + "|";
                }

            }

            _backslashEscapes = new Regex(backslashPattern.Substring(0, backslashPattern.Length - 1), RegexOptions.Compiled);
        }

        /// <summary>
        /// current version of MarkdownSharp;  
        /// see http://code.google.com/p/markdownsharp/ for the latest code or to contribute
        /// </summary>
        public string Version
        {
            get { return _version; }
        }

        public string Transform(string text, List<ErrorDetail> ErrorList, List<ImageConversion> ImageDetails, List<AttachmentConversionDetail> AttachNames, FolderDetails CurrentFolderDetails, IEnumerable<string> LanguagesLinksToGenerate = null, bool nonDynamicHTMLOutput = false)
        {
            Setup();

            using (
                var td = new TransformationData(
                    this, ErrorList, ImageDetails, AttachNames, CurrentFolderDetails, LanguagesLinksToGenerate, nonDynamicHTMLOutput))
            {
                return Transform(text, td);
            }
        }

        public Preprocessor.Preprocessor Preprocessor { get; private set; }

        public string Transform(string text, TransformationData transformationData)
        {
            if (String.IsNullOrEmpty(text))
            {
                return null;
            }

            var document = ParseDocument(text, transformationData);

            document.FlattenInternalLinks(transformationData);

            return document.Render();
        }

        public EMDocument ParseDocument(
            string text,
            List<ErrorDetail> errorList,
            List<ImageConversion> imageDetails,
            List<AttachmentConversionDetail> attachNames,
            FolderDetails currentFolderDetails,
            IEnumerable<string> languagesLinksToGenerate = null)
        {
            return ParseDocument(
                text, new TransformationData(
                    this, errorList, imageDetails, attachNames, currentFolderDetails, languagesLinksToGenerate));
        }

        private EMDocument ParseDocument(string text, TransformationData transformationData)
        {
            Templates.Init(transformationData, ThisIsPreview);
            OffensiveWordFilterHelper.Init(transformationData, ThisIsPreview);

            OriginalText = text.Split('\n');

            LoadAPIManifest(transformationData);

            transformationData.Document.Parse(text);

            return transformationData.Document;
        }

        // @UE3 Added this section to work with OBJECT and PARAM sections
        public static readonly Regex ObjectMatching = new Regex(@"^([ ]{0,3})\[OBJECT:(?<ObjectName>[^\]|\n]*?)\][^\n]*?\n(?<Contents>[\s|\S]*?)^\1\[/OBJECT(:\2)?\]", RegexOptions.Compiled | RegexOptions.IgnoreCase | RegexOptions.Multiline);

        // @UE3 Added this section to work with PLATFORM sections
        public static readonly Regex PlatformMatching = new Regex(@"\[PLATFORM:(?<PlatformType>[^\#]*?)\#(?<TemplateName>[^\]]*?)\]", RegexOptions.Compiled | RegexOptions.IgnoreCase | RegexOptions.Multiline);

        public static bool CheckAvailability(TransformationData data)
        {
            //Check Availability metadata values
            if (data.ProcessedDocumentCache.CurrentFileDocument.Metadata.Contains("availability"))
            {
                foreach (var metaDataValue in data.ProcessedDocumentCache.Metadata.Get("availability"))
                {
                    var availabilityPlatforms = metaDataValue.Replace(" ", "").Split(',');

                    foreach (var availabilityPlatform in availabilityPlatforms)
                    {
                        foreach (var publishFlag in data.Markdown.PublishFlags)
                        {
                            if (publishFlag.ToLower().Equals(data.Markdown.DoNotPublishAvailabilityFlag.ToLower()))
                            {
                                return false;
                            }

                            if (!data.Markdown.AllSupportedAvailability.Contains(publishFlag, StringComparer.OrdinalIgnoreCase))
                            {
                                // Not recognized
                                data.ErrorList.Add(new ErrorDetail(Language.Message("AvailabilityValueNotRecognized", publishFlag), MessageClass.Error, "", "", 0, 0));
                                break;
                            }

                            if (publishFlag.Equals(availabilityPlatform, StringComparison.OrdinalIgnoreCase) || publishFlag.Equals("all", StringComparison.OrdinalIgnoreCase))
                            {
                                return true;
                            }
                        }
                    }
                }
            }

            // if preview mode then allow to continue
            return data.Markdown.ThisIsPreview;
        }

        private static readonly Regex RegionMatchingSingleLine = new Regex(@"\[REGION:([^\]]*?)\](.*?)\[/REGION(:\1)?\]", RegexOptions.Compiled | RegexOptions.IgnoreCase | RegexOptions.Multiline);

        private string ParseRegionsToDivsSingleLine(Match match, TransformationData translationVariablesForThisRun)
        {
            var regionParameters = match.Groups[1].Value;
            var regionContent =
                RegionMatchingSingleLine.Replace(RunSpanGamut(match.Groups[2].Value, translationVariablesForThisRun),
                                                 EveryMatch =>
                                                 ParseRegionsToDivsSingleLine(EveryMatch, translationVariablesForThisRun));

            return Templates.Region.Render(
                       Hash.FromAnonymousObject(new
                       {
                           regionParameters = regionParameters,
                           regionContent = regionContent,
                           singleLine = true
                       }));
        }

        private string ReplaceRegionsWithDivsSpan(string text, TransformationData translationVariablesForThisRun)
        {
            return RegionMatchingSingleLine.Replace(text, EveryMatch => ParseRegionsToDivsSingleLine(EveryMatch, translationVariablesForThisRun));
        }

        /// @UE3 Added includes functionality
        ///

        public static readonly Regex IncludeFileSpan = new Regex(@"\[Include:(?<IncludeFileRegion>[^\]\n]*?)[ ]*(?:\]|(?:\(offset:[ ]*(?<OffsetValue>[^\)\n]*?)[ ]*\)[ ]*\]))", RegexOptions.Compiled | RegexOptions.IgnoreCase | RegexOptions.Multiline);
        private static readonly Regex IncludeIncludeRelativeFile = new Regex(string.Format(@"([ ]{{0,{0}}}\[Include:[ ]*)(#[^\]]*?])", TabWidth - 1), RegexOptions.Compiled | RegexOptions.IgnoreCase | RegexOptions.Multiline);

        public static string GetSubRegionOfFileMatcher(string IncludeRegion)
        {
            return @"\[EXCERPT:" + Regex.Escape(IncludeRegion) + @"\]((\s|\S)*)\[/EXCERPT:" + Regex.Escape(IncludeRegion) + @"\]";
        }

        public static string ProcessRelativePaths(string text, string includeFileFolderPath)
        {
            // Change location of attachments and images. These should be processed as we do with
            // external images by the page including this section.
            text = ProcessIncludedAttachmentsImagesThumbnails(text);
            text = ProcessIncludedAttachmentsImages(text);
            text = ProcessIncludedReferences(text);

            text = text.Replace("%PATHTOINCLUDE%/#", includeFileFolderPath + "#");
            text = text.Replace("%PATHTOINCLUDE%", includeFileFolderPath);

            // Replace any include section that are relative with this includeFileFolderPath in
            // the text.
            text = ProcessIncludedRelativeFile(text, includeFileFolderPath);

            return text;
        }

        private static string ProcessIncludedRelativeFile(string text, string folderName)
        {
            return IncludeIncludeRelativeFile.Replace(text, "$1" + folderName + "$2");
        }

        private static string ProcessIncludedReferences(string text)
        {
            return IncludedReferences.Replace(text, IncludedReferencesProcessing);
        }

        private static string ProcessIncludedAttachmentsImages(string text)
        {
            return IncludedAttachmentsImages.Replace(text, IncludedImageAttachmentProcessing);
        }

        private static string ProcessIncludedAttachmentsImagesThumbnails(string text)
        {
            return IncludedAttachmentsImagesThumbnails.Replace(text, IncludedThumbnailsProcessing);
        }

        private static readonly Regex IncludedAttachmentsImagesThumbnails = new Regex(string.Format(@"(!?\[[^\]]*?\][ ]*\()([^)]*?)(\)[^\]|\n]*?\][ ]*\()\2(\))"), RegexOptions.Multiline | RegexOptions.Compiled);
        private static readonly Regex IncludedAttachmentsImages = new Regex(string.Format(@"(!?\[[^\]]*?\][ ]*\()([^)|\n]*?\.[^)|\n]*?)([\)| ])"), RegexOptions.Multiline | RegexOptions.Compiled);
        private static readonly Regex IncludedReferences = new Regex(string.Format(@"(^\[[^\]]*?\]:[ ]*)([^ |\n]*)"), RegexOptions.Multiline | RegexOptions.Compiled);

        private static string IncludedImageAttachmentProcessing(Match match)
        {
            string PathFileName = match.Groups[2].Value;
            
            Match matches = Regex.Match(PathFileName, @"(([^\\|/]*?[\\|/])*)(.*)");

            if (!String.IsNullOrWhiteSpace(matches.Groups[1].Value) || match.Groups[2].Value.ToUpper().Contains("ROOT"))
            {
                //already a relative path, will be same for this file we are including in so can be ignored
                //handle root in normal document processing code.
                return match.Groups[0].Value;
            }
            else
            {
                return match.Groups[1].Value + "%PATHTOINCLUDE%/" + match.Groups[2].Value + match.Groups[3].Value;
            }

        }

        private static string IncludedThumbnailsProcessing(Match match)
        {
            string ReturnString;
            
            string PathFileName = match.Groups[2].Value;

            Match matches = Regex.Match(PathFileName, @"(([^\\|/]*?[\\|/])*)(.*)");

            if (!String.IsNullOrWhiteSpace(matches.Groups[1].Value) || match.Groups[2].Value.ToUpper().Contains("ROOT"))
            {
                //already a relative path, will be same for this file we are including in so can be ignored
                //handle root in normal document processing code.
                ReturnString = match.Groups[1].Value + match.Groups[2].Value + match.Groups[3].Value;
            }
            else
            {
                ReturnString = match.Groups[1].Value + "%PATHTOINCLUDE%/" + match.Groups[2].Value + match.Groups[3].Value;
            }

            PathFileName = match.Groups[4].Value;

            matches = Regex.Match(PathFileName, @"(([^\\|/]*?[\\|/])*)(.*)");

            if (!String.IsNullOrWhiteSpace(matches.Groups[1].Value) || match.Groups[4].Value.ToUpper().Contains("ROOT"))
            {
                //already a relative path, will be same for this file we are including in so can be ignored
                //handle root in normal document processing code.
                ReturnString += match.Groups[4].Value + match.Groups[5].Value;
            }
            else
            {
                ReturnString += "%PATHTOINCLUDE%/" + match.Groups[4].Value + match.Groups[5].Value;
            }

            return ReturnString;

        }


        private static string IncludedReferencesProcessing(Match match)
        {
            string PathFileName = match.Groups[2].Value;

            Match matches = Regex.Match(PathFileName, @"(([^\\|/]*?[\\|/])*)(.*)");

            if (String.IsNullOrWhiteSpace(matches.Groups[1].Value))
            {
                return match.Groups[1].Value + "%PATHTOINCLUDE%/" + match.Groups[2].Value;
            }
            else
            {
                //already a relative path, will be same for this file we are including in so can be ignored
                return match.Groups[0].Value;
            }
        }

        /// @UE3 ParseMetaData functionality added
        /// 
        private static string meta_row = @"(?<MetaDataKey>[a-zA-Z0-9][0-9a-zA-Z _-]*)[ ]*:[ ]*(?<MetaDataValue>.*)";

        private static string meta_rows = meta_row + @"\n?";

        public static readonly Regex MetaData = new Regex(@"
                        \A                                  #Starts on first line
                        ^                                   #Start of line
                        (" + meta_rows + @")+                 # meta Data = $1
                        (?:\n+|\Z)
                        ", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        public static readonly Regex MetaDataRow = new Regex(@"
                        ^                                   #Start of line
                        " + meta_row + @"                   # meta Data = $1
                        ", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        private static string GetTitleFromDocument(string FileLocation, TransformationData translationVariablesForThisRun)
        {
            //Check for this languages markdown file if not found use INT

            //Full local path to markdown directory
            DirectoryInfo AbsoluteDir = new DirectoryInfo(Path.Combine(translationVariablesForThisRun.CurrentFolderDetails.AbsoluteMarkdownPath, FileLocation));

            //string AbsolutePath = Path.Combine(InstanceVariablesForThisRun.CurrentFolderDetails.AbsoluteMarkdownPath, FileLocation);

            if (AbsoluteDir.Exists)
            {
                FileInfo[] AbsoluteMarkdownFilePath = AbsoluteDir.GetFiles(string.Format("*.{0}.udn", translationVariablesForThisRun.CurrentFolderDetails.Language));

                if (AbsoluteMarkdownFilePath.Length == 0)
                {
                    AbsoluteMarkdownFilePath = AbsoluteDir.GetFiles("*.INT.udn");
                }

                if (AbsoluteMarkdownFilePath.Length == 0)
                {
                    //Neither language file or INT file exists in the location specified, use the path as the title
                    return Regex.Replace(FileLocation, "([^\\/]*?)\\|/", "$1");
                }
                else
                {
                    var title = "";

                    return
                        translationVariablesForThisRun.ProcessedDocumentCache.TryGetLinkedFileVariable(AbsoluteMarkdownFilePath[0], "title", out title)
                            ? title
                            : Regex.Replace(FileLocation, "([^\\/]*?)\\|/", "$1");
                }
            }
            else
            {
                //Directory does not exist
                return Regex.Replace(FileLocation, "([^\\/]*?)\\|/", "$1");
            }
        }

        private static char[] _problemFileChars = @"""'*()[]$:<>|?".ToCharArray();

        public string DoAddHTMLWrapping(string text, TransformationData data)
        {
            // Add a meta data item for each row in the MetaData Dictionary,
            // which was populated at the start of all document processing.

            var metadata = "";
            var metadataTemplate = "";

            foreach (var metadataRow in data.ProcessedDocumentCache.Metadata.MetadataMap)
            {
                switch (metadataRow.Key)
                {
                    case "template":
                        metadataTemplate = metadataRow.Value[0];
                        break;
                    case "forcepublishfiles":
                        foreach (var reference in metadataRow.Value[0].Split(','))
                        {
                            RunSpanGamut(reference, data);
                        }
                        break;
                }

                if (metadataRow.Key == "description" && data.ProcessedDocumentCache.Metadata.MetadataMap.ContainsKey("seo-description"))
                {
                    continue;
                }
                
                if (metadataRow.Key == "seo-description")
                {
                    metadata = metadataRow.Value.Aggregate(metadata,
                        (current, metadataValue) =>
                        current + Templates.Metadata.Render(Hash.FromAnonymousObject(new
                        {
                            key = "description",
                            value = metadataValue
                        })) + Environment.NewLine);
                }
                else
                {
                    metadata = metadataRow.Value.Aggregate(metadata,
                        (current, metadataValue) =>
                        current + Templates.Metadata.Render(Hash.FromAnonymousObject(new
                            {
                                key = metadataRow.Key,
                                value = metadataValue
                            })) + Environment.NewLine);
                }
            }

            var translatedPageLinks = "";
            var translatedPageSelect = "";

            //Generate links to other languages for this page
            foreach (var translatedLanguage in data.LanguagesLinksToGenerate)
            {
                var linkParams =
                    Hash.FromAnonymousObject(
                        new
                            {
                                pathToPage =
                                    String.IsNullOrWhiteSpace(
                                        data.CurrentFolderDetails.CurrentFolderFromMarkdownAsTopLeaf)
                                        ? ""
                                        : data.CurrentFolderDetails.CurrentFolderFromMarkdownAsTopLeaf.Replace("\\", "/"),
                                otherLanguage = translatedLanguage,
                                otherLanguageLabel = SupportedLanguageMap[translatedLanguage],
                                relativeHTMLPath = data.CurrentFolderDetails.RelativeHTMLPath
                            });

                if (translatedLanguage.Equals(data.CurrentFolderDetails.Language))
                {
                    linkParams.Add("selected", " selected");
                }

                if(Directory.GetFiles(Path.Combine(
                        Path.Combine(data.CurrentFolderDetails.AbsoluteMarkdownPath,
                        data.CurrentFolderDetails.CurrentFolderFromMarkdownAsTopLeaf)),
                        string.Format("*.{0}.udn", translatedLanguage)).Length == 0)
                {
                    linkParams.Add("disabled", " disabled");
                }

                // Cope with top level folders having blank current folder.
                translatedPageLinks +=
                    Templates.TranslatedPageLink.Render(linkParams);
                var linkPageParams =
                    Hash.FromAnonymousObject(
                        new
                        {
                            languageLinks = translatedPageLinks
                        });
                translatedPageSelect = Templates.TranslatedPages.Render(linkPageParams);
            }

            //add a warning if crumbs for the document have not been updated to new format
            if ((data.ProcessedDocumentCache.Metadata.CrumbsLinks == null || data.ProcessedDocumentCache.Metadata.CrumbsLinks.Count == 0) && text.Contains("<div class=\"crumbs\">"))
            {
                data.ErrorList.Add(new ErrorDetail(Language.Message("DocumentNeedsToBeUpdatedToUserCrumbLinkMetadata"), MessageClass.Info, "", "", 0, 0));
            }

            // Use language template files if available.
            var defaultLanguagePageLocation = Path.Combine(Path.Combine(Path.Combine(data.CurrentFolderDetails.AbsoluteMarkdownPath, "Include"), "Templates"), data.CurrentFolderDetails.Language);
            var defaultIntPageLocation = Path.Combine(Path.Combine(data.CurrentFolderDetails.AbsoluteMarkdownPath, "Include"), "Templates");

            var webPage = Templates.GetCached(!string.IsNullOrWhiteSpace(metadataTemplate) ? metadataTemplate : DefaultTemplate);

            // If crumbs is empty remove div crumbs from the template.

            var crumbs = data.Markdown.ProcessCrumbs(data.ProcessedDocumentCache.Metadata.CrumbsLinks, data);

            return webPage.Render(Hash.FromAnonymousObject(
                new
                    {
                        crumbsLink = String.IsNullOrWhiteSpace(crumbs)
                                         ? ""
                                         : Templates.GetCached("crumbsDiv.html").Render(Hash.FromAnonymousObject(new { crumbs })),
                        title = data.ProcessedDocumentCache.CurrentFileDocument.Metadata.DocumentTitle,
                        seotitle = (data.ProcessedDocumentCache.CurrentFileDocument.Metadata.SEOTitle != null) ? data.ProcessedDocumentCache.CurrentFileDocument.Metadata.SEOTitle : data.ProcessedDocumentCache.CurrentFileDocument.Metadata.DocumentTitle,
                        metadata = metadata,
                        translatedPages = translatedPageSelect,
                        relatedPagesMenu = Templates.RelatedPages.Render(Hash.FromAnonymousObject(
                            new {
                                relatedPages = data.ProcessedDocumentCache.Metadata.RelatedLinks,
                                relatedPagesCount = data.ProcessedDocumentCache.Metadata.RelatedLinks.Count,
                                quickjump = ""
                            })),
                        relatedPages = Templates.RelatedPages.Render(Hash.FromAnonymousObject(
                            new
                            {
                                relatedPages = data.ProcessedDocumentCache.Metadata.RelatedLinks,
                                relatedPagesCount = data.ProcessedDocumentCache.Metadata.RelatedLinks.Count,
                                relativeHtmlPath = data.CurrentFolderDetails.RelativeHTMLPath
                            })),
                        prereqPages = Templates.PrereqPages.Render(Hash.FromAnonymousObject(
                            new
                            {
                                prereqPages = data.ProcessedDocumentCache.Metadata.PrereqLinks,
                                prereqPagesCount = data.ProcessedDocumentCache.Metadata.PrereqLinks.Count,
                                relativeHtmlPath = data.CurrentFolderDetails.RelativeHTMLPath
                            })),
                        versions = Templates.Versions.Render(Hash.FromAnonymousObject(
                            new
                            {
                                versions = data.ProcessedDocumentCache.Metadata.EngineVersions,
                                versionCount = data.ProcessedDocumentCache.Metadata.EngineVersions.Count,
                                language = data.CurrentFolderDetails.Language,
                                relativeHtmlPath = data.CurrentFolderDetails.RelativeHTMLPath
                            })),
                        skilllevels = Templates.SkillLevels.Render(Hash.FromAnonymousObject(
                            new
                            {
                                skilllevels = data.ProcessedDocumentCache.Metadata.SkillLevels,
                                skilllevelCount = data.ProcessedDocumentCache.Metadata.SkillLevels.Count,
                                language = data.CurrentFolderDetails.Language,
                                relativeHtmlPath = data.CurrentFolderDetails.RelativeHTMLPath
                            })),
                        tags = Templates.Tags.Render(Hash.FromAnonymousObject(
                            new
                            {
                                tags = data.ProcessedDocumentCache.Metadata.Tags,
                                tagsCount = data.ProcessedDocumentCache.Metadata.Tags.Count,
                                language = data.CurrentFolderDetails.Language,
                                relativeHtmlPath = data.CurrentFolderDetails.RelativeHTMLPath
                            })),
                        errors = ThisIsPreview
                                     ? Templates.ErrorDetails.Render(Hash.FromAnonymousObject(
                                         new
                                             {
                                                 errorDetails = data.ErrorList.ConvertAll(errorDetail => new ErrorDetailDrop(errorDetail)).ToList(),
                                                 outputAtAll = data.ErrorList.Count > 0,
                                                 outputErrors = data.ErrorList.Any(errorDetail => errorDetail.ClassOfMessage == MessageClass.Error),
                                                 outputWarnings = data.ErrorList.Any(errorDetail => errorDetail.ClassOfMessage == MessageClass.Warning),
                                                 outputInfo = data.ErrorList.Any(errorDetail => errorDetail.ClassOfMessage == MessageClass.Info)
                                             }))
                                     : null,
                        markdownContent = text,
                        language = data.CurrentFolderDetails.Language,
                        relativeHtmlPath = data.CurrentFolderDetails.RelativeHTMLPath
                    }));
        }

        /// <summary>
        /// @UE3 search for (#name) and replace with a bookmark
        /// </summary>
        public static readonly Regex _Bookmark = new Regex(@"^\(\#(?<BookmarkName>[^\)]+)\)", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        private static readonly Regex _outDentDiv = new Regex(
            @"  \A
                \s*
                ^(?<indent>[ ]{1," + TabWidth + @"}).*\S.*$
                \s*
                (^\k<indent>.*\S.*$\s*)*
                \z", RegexOptions.Multiline | RegexOptions.Compiled | RegexOptions.IgnorePatternWhitespace);

        public static string OutdentDiv(string block)
        {
            var match = _outDentDiv.Match(block);

            if (match.Groups["indent"].Length == 0)
            {
                return block;
            }

            var pattern = new Regex(
                @"^" + Regex.Escape(match.Groups["indent"].Value), RegexOptions.Compiled | RegexOptions.Multiline);

            return pattern.Replace(block, "");
        }

        private static Dictionary<string, string> CachedLinkText = new Dictionary<string,string>();

        private string GetCrumbsURL(string FileLocation, TransformationData InstanceVariablesForThisRun)
        {
            string result = "";
            bool IsURLProblem = false;
            var errorId = 0;
            FileLocation = FileLocation.Replace("%ROOT%", "");


            //Check for invalid characters
            foreach (char EachCharacter in FileLocation)
            {
                if (Array.IndexOf(_problemFileChars, EachCharacter) != -1)
                {
                    errorId = InstanceVariablesForThisRun.ErrorList.Count;

                    result = FileLocation.Replace("<", "&lt;");
                    InstanceVariablesForThisRun.ErrorList.Add(GenerateError(Language.Message("InvalidCharactersDetected", result), MessageClass.Error, Unescape(FileLocation), errorId, InstanceVariablesForThisRun));

                    IsURLProblem = true;
                    break;
                }
            }

            if (!IsURLProblem)
            {
                string linkText;
                var cacheKey = Path.Combine(FileLocation, InstanceVariablesForThisRun.CurrentFolderDetails.Language);
                if (!CachedLinkText.TryGetValue(cacheKey, out linkText))
                {
                    linkText = GetTitleFromDocument(FileLocation, InstanceVariablesForThisRun);
                    CachedLinkText.Add(cacheKey, linkText);
                }

                bool bChangedLanguage = false;

                //This should be a link to a local web page, test that the folder and file exists for the current language in the destination of the link folder in markdown

                string LanguageForFile = InstanceVariablesForThisRun.CurrentFolderDetails.Language;

                DirectoryInfo DirLocationOfLinkedFile =
                    new DirectoryInfo(Path.Combine(Path.Combine(InstanceVariablesForThisRun.CurrentFolderDetails.AbsoluteMarkdownPath, FileLocation)));

                if (!DirLocationOfLinkedFile.Exists ||
                    DirLocationOfLinkedFile.GetFiles(string.Format("*.{0}.udn", InstanceVariablesForThisRun.CurrentFolderDetails.Language)).Length ==
                    0)
                {
                    // if this is not an INT file check for the INT version.
                    if (InstanceVariablesForThisRun.CurrentFolderDetails.Language != "INT")
                    {
                        if (!DirLocationOfLinkedFile.Exists || DirLocationOfLinkedFile.GetFiles("*.INT.udn").Length == 0)
                        {
                            errorId = InstanceVariablesForThisRun.ErrorList.Count;
                            InstanceVariablesForThisRun.ErrorList.Add(
                                GenerateError(
                                    Language.Message("BadLinkOrMissingMarkdownFileForLinkAndNoINTFile", FileLocation),
                                    MessageClass.Error, Unescape(FileLocation), errorId,
                                    InstanceVariablesForThisRun));

                            IsURLProblem = true;
                        }
                        else
                        {
                            // Found int file
                            LanguageForFile = "INT";

                            //Raise info so that now we are allowing missing linked files to still allow processing of the file if INT file is there
                            errorId = InstanceVariablesForThisRun.ErrorList.Count;
                            InstanceVariablesForThisRun.ErrorList.Add(
                                GenerateError(
                                    Language.Message("BadLinkOrMissingMarkdownFileForLinkINTUsed", FileLocation),
                                    MessageClass.Info, Unescape(FileLocation),
                                    errorId, InstanceVariablesForThisRun));

                            IsURLProblem = true;

                            //If we had to replace the language with int then update the linkText to include link to image of flag
                            linkText += Templates.ImageFrame.Render(
                                Hash.FromAnonymousObject(
                                    new
                                    {
                                        imageClass = "languageinline",
                                        imagePath = Path.Combine(
                                            InstanceVariablesForThisRun.CurrentFolderDetails.RelativeHTMLPath,
                                            "Include", "Images", "INT_flag.jpg")
                                    }));

                            bChangedLanguage = true;

                        }
                    }
                    else
                    {
                        errorId = InstanceVariablesForThisRun.ErrorList.Count;
                        InstanceVariablesForThisRun.ErrorList.Add(GenerateError(
                            Language.Message("BadLinkOrMissingMarkdownFileForLink", FileLocation), MessageClass.Error,
                            Unescape(FileLocation), errorId, InstanceVariablesForThisRun));

                        IsURLProblem = true;
                    }
                }

                //add relative htmlpath to local linked page
                FileLocation =
                    Path.Combine(Path.Combine(Path.Combine(InstanceVariablesForThisRun.CurrentFolderDetails.RelativeHTMLPath, LanguageForFile),
                                              FileLocation.Replace("%ROOT%", ""), "index.html"));

                FileLocation = SimplifyHtmlLinkPath(FileLocation, InstanceVariablesForThisRun);

                result = Templates.Link.Render(
                    Hash.FromAnonymousObject(
                        new
                        {
                            linkUrl = FileLocation,
                            linkText = linkText
                        }));

                if (bChangedLanguage)
                {
                    result = Templates.NonlocalizedFrame.Render(
                        Hash.FromAnonymousObject(new { content = result }));
                }
            }

            if (ThisIsPreview && IsURLProblem)
            {
                return Templates.ErrorHighlight.Render(
                    Hash.FromAnonymousObject(
                        new
                        {
                            errorId = errorId,
                            errorText = result
                        }));
            }
            else
            {
                return result;
            }

        }

        public string ProcessCrumbs(List<string> crumbsLists, TransformationData data)
        {
            var output = new StringBuilder();

            foreach (var crumbList in crumbsLists)
            {
                output.Append(
                    Templates.Crumbs.Render(
                        Hash.FromAnonymousObject(
                            new
                                {
                                    crumbs = crumbList.Split(',').Select((crumb) => GetCrumbsURL(MarkdownSharp.Preprocessor.Preprocessor.UnescapeChars(crumb.Trim(), true), data)),
                                    title = data.ProcessedDocumentCache.CurrentFileDocument.Metadata.DocumentTitle
                                })));
            }

            return output.ToString();
        }

        public string SimplifyHtmlLinkPath(string path, TransformationData data)
        {
            // Get the full path to the source and target files
            var sourceDirPath = Path.Combine(
                data.CurrentFolderDetails.AbsoluteHTMLPath,
                data.CurrentFolderDetails.Language,
                data.CurrentFolderDetails.CurrentFolderFromMarkdownAsTopLeaf);

            if (sourceDirPath[sourceDirPath.Length - 1] != Path.AltDirectorySeparatorChar
                && sourceDirPath[sourceDirPath.Length - 1] != Path.DirectorySeparatorChar)
            {
                sourceDirPath += "/";
            }

            Uri source = new Uri(sourceDirPath);
            Uri target = new Uri(source, path);

            // Create the relative URI between them
            Uri relative = source.MakeRelativeUri(target);
            return relative.ToString();
        }

        public Hash ProcessRelated(string path, TransformationData data)
        {
            ClosestFileStatus status;
            var linkedDoc = data.ProcessedDocumentCache.GetClosest(path, out status);

            if (status == ClosestFileStatus.FileMissing)
            {
                var errorId = data.ErrorList.Count;
                data.ErrorList.Add(
                    GenerateError(
                        Language.Message("BadRelatedPageLink", path), MessageClass.Error, path, errorId, data));

                return new Hash();
            }

            var parameters = Hash.FromAnonymousObject(
                new {
                    relativeLink = linkedDoc.GetRelativeTargetPath(data.Document)
                });

            foreach(var metadata in linkedDoc.PreprocessedData.Metadata.MetadataMap)
            {
                parameters.Add(metadata.Key, string.Join(", ", metadata.Value));
            }

            return parameters;
        }

        public Hash ProcessPrereqs(string path, TransformationData data)
        {
            ClosestFileStatus status;
            var linkedDoc = data.ProcessedDocumentCache.GetClosest(path, out status);

            if (status == ClosestFileStatus.FileMissing)
            {
                var errorId = data.ErrorList.Count;
                data.ErrorList.Add(
                    GenerateError(
                        Language.Message("BadPrereqPageLink", path), MessageClass.Error, path, errorId, data));

                return new Hash();
            }

            var parameters = Hash.FromAnonymousObject(
                new
                {
                    relativeLink = linkedDoc.GetRelativeTargetPath(data.Document)
                });

            foreach (var metadata in linkedDoc.PreprocessedData.Metadata.MetadataMap)
            {
                parameters.Add(metadata.Key, string.Join(", ", metadata.Value));
            }

            return parameters;
        }

        /// <summary>
        /// Perform transformations that occur *within* block-level tags like paragraphs, headers, and list items.
        /// </summary>
        public string RunSpanGamut(string text, TransformationData translationVariablesForThisRun)
        {
            text = DoColours(text);
            text = DoGraphicIcons(text, translationVariablesForThisRun);
            text = EscapeSpecialCharsWithinTagAttributes(text);
            text = EscapeBackslashes(text);

            text = ReplaceRegionsWithDivsSpan(text, translationVariablesForThisRun);

            text = DoGraphicIcons(text, translationVariablesForThisRun);

            // Must come after DoAnchors(), because you can use < and >
            // delimiters in inline links like [this](<url>).
            text = DoAutoLinks(text);

            text = EncodeAmpsAndAngles(text);

            text = DoHardBreaks(text);

            return text;
        }

        //@UE3 handle %X% twiki style graphic icons
        private static readonly Regex _graphicIcons = new Regex(@"%([A-Z]){1}%", RegexOptions.Compiled);

        private static string DoGraphicIcons(string text, TransformationData translationVariablesForThisRun)
        {
            return _graphicIcons.Replace(text, everyMatch => IconEvaluator(everyMatch, translationVariablesForThisRun));
        }

        private static string IconEvaluator(Match match, TransformationData translationVariablesForThisRun)
        {
            // These keywords are skipped previously by the variables engine. To add another icon, update also VariableManager.IsKeywordToSkip accordingly.

            string text = match.Groups[1].Value;
            string imagesPath = Path.Combine(Path.Combine(
                translationVariablesForThisRun.CurrentFolderDetails.RelativeHTMLPath,
                "include"), "images");
            
            switch (text)
            {
                //These are all shared images at the top level.
                case "H":
                    text = Templates.ImageFrame.Render(Hash.FromAnonymousObject(new { imageAlt = "HELP", imagePath = Path.Combine(imagesPath, "help.gif") }));
                    break;
                case "I":
                    text = Templates.ImageFrame.Render(Hash.FromAnonymousObject(new { imageAlt = "IDEA!", imagePath = Path.Combine(imagesPath, "idea.gif") }));
                    break;
                case "M":
                    text = Templates.ImageFrame.Render(Hash.FromAnonymousObject(new { imageAlt = "MOVED TO...", imagePath = Path.Combine(imagesPath, "arrowright.gif") }));
                    break;
                case "N":
                    text = Templates.ImageFrame.Render(Hash.FromAnonymousObject(new { imageAlt = "NEW", imagePath = Path.Combine(imagesPath, "new.gif") }));
                    break;
                case "P":
                    text = Templates.ImageFrame.Render(Hash.FromAnonymousObject(new { imageAlt = "REFACTOR", imagePath = Path.Combine(imagesPath, "pencil.gif") }));
                    break;
                case "Q":
                    text = Templates.ImageFrame.Render(Hash.FromAnonymousObject(new { imageAlt = "QUESTION?", imagePath = Path.Combine(imagesPath, "question.gif") }));
                    break;
                case "S":
                    text = Templates.ImageFrame.Render(Hash.FromAnonymousObject(new { imageAlt = "PICK", imagePath = Path.Combine(imagesPath, "starred.gif") }));
                    break;
                case "T":
                    text = Templates.ImageFrame.Render(Hash.FromAnonymousObject(new { imageAlt = "TIP", imagePath = Path.Combine(imagesPath, "tip.gif") }));
                    break;
                case "U":
                    text = Templates.ImageFrame.Render(Hash.FromAnonymousObject(new { imageAlt = "UPDATED", imagePath = Path.Combine(imagesPath, "updated.gif") }));
                    break;
                case "X":
                    text = Templates.ImageFrame.Render(Hash.FromAnonymousObject(new { imageAlt = "ALERT!", imagePath = Path.Combine(imagesPath, "warning.gif") }));
                    break;
                case "Y":
                    text = Templates.ImageFrame.Render(Hash.FromAnonymousObject(new { imageAlt = "DONE", imagePath = Path.Combine(imagesPath, "choice-yes.gif") }));
                    break;
                default:
                    //set back to how it was
                    text = "%" + text + "%";
                    break;
            }
            return text;
        }

        
        //@UE3 handle colours
        private static readonly Regex _colours = new Regex(@"%([^%]*?)%([^%]*?)%ENDCOLOR%", RegexOptions.Compiled);

        private static string DoColours(string text)
        {
            return _colours.Replace(text, ColourEvaluator);
        }

        private static string ColourEvaluator(Match match)
        {
            // These keywords (%COLOR_NAME% and %ENDCOLOR%) are skipped previously by the variables engine.
            // To add another color, update also VariableManager.IsKeywordToSkip accordingly.

            return Templates.ColorFrame.Render(Hash.FromAnonymousObject(new
                {
                    colorId = match.Groups[1].Value,
                    text = match.Groups[2].Value
                }));
        }

        private void Setup()
        {
            ListLevel = 0;
        }

        private static readonly Regex MatchLastCommaOnALine = new Regex(@"[ ]*(?<Key>.+)[ ]*,[ ]*(?<Value>.*)[ ]*", RegexOptions.Compiled | RegexOptions.Singleline);

        private static void LoadAPIManifest(TransformationData translationVariablesForThisRun)
        {
            //We only need to load this file once and share across all files in this run
            if (APIFileLocation.Count() == 0)
            {
                FileInfo APIManifestFile = new FileInfo(Path.Combine(Path.Combine(translationVariablesForThisRun.CurrentFolderDetails.AbsoluteMarkdownPath, translationVariablesForThisRun.CurrentFolderDetails.APIFolderLocationFromMarkdownAsTop), "api.manifest"));
                if (!APIManifestFile.Exists)
                {
                    //Raise info on not being able to locate the api manifest file, error on specific API: matches in the document
                    translationVariablesForThisRun.ErrorList.Add(new ErrorDetail(Language.Message("UnableToLocateAPIManifestInTheSourceDir"), MessageClass.Info, "", "", 0, 0));
                }
                else
                {
                    StreamReader APIManifestFileSR = APIManifestFile.OpenText();
                    string APIManifestFileLine = "";
                    while((APIManifestFileLine = APIManifestFileSR.ReadLine()) != null)
                    {
                        Match match = MatchLastCommaOnALine.Match(APIManifestFileLine);
                    
                        //Check that no duplicates are in the file if they are raise an info and do not add the subsequent.
                        if (APIFileLocation.ContainsKey(match.Groups["Key"].Value))
                        {
                            translationVariablesForThisRun.ErrorList.Add(new ErrorDetail(Language.Message("DuplicateAPIKeyInAPIManifestFile", match.Groups["Key"].Value), MessageClass.Info, "", "", 0, 0));
                        }
                        else
                        {
                            APIFileLocation.Add(match.Groups["Key"].Value, match.Groups["Value"].Value);
                        }
                    }
                }
            }
        }

        private static string _nestedBracketsPattern;

        /// <summary>
        /// Reusable pattern to match balanced [brackets]. See Friedl's 
        /// "Mastering Regular Expressions", 2nd Ed., pp. 328-331.
        /// </summary>
        public static string GetNestedBracketsPattern()
        {
            // in other words [this] and [this[also]] and [this[also[too]]]
            // up to _nestDepth
            if (_nestedBracketsPattern == null)
                _nestedBracketsPattern =
                    RepeatString(@"
                    (?>              # Atomic matching
                       [^\[\]]+      # Anything other than brackets
                     |
                       \[
                           ", _nestDepth) + RepeatString(
                    @" \]
                    )*"
                    , _nestDepth);
            return _nestedBracketsPattern;
        }

        private static string _nestedParensPattern;

        /// <summary>
        /// Reusable pattern to match balanced (parens). See Friedl's 
        /// "Mastering Regular Expressions", 2nd Ed., pp. 328-331.
        /// </summary>
        public static string GetNestedParensPattern()
        {
            // in other words (this) and (this(also)) and (this(also(too)))
            // up to _nestDepth
            if (_nestedParensPattern == null)
                _nestedParensPattern =
                    RepeatString(@"
                    (?>              # Atomic matching
                       [^()\s]+      # Anything other than parens or whitespace
                     |
                       \(
                           ", _nestDepth) + RepeatString(
                    @" \)
                    )*"
                    , _nestDepth);
            return _nestedParensPattern;
        }

        private static string GetHashKey(string s)
        {
            return "\x1A" + Math.Abs(s.GetHashCode()).ToString() + "\x1A";
        }

        public static readonly Regex _htmlTokens = new Regex(@"
            (<!(?:--.*?--\s*)+>)|        # match <!-- foo -->
            (<\?.*?\?>)|                 # match <?foo?> " +
            RepeatString(@" 
            (<[A-Za-z\/!$](?:[^<>]|", _nestDepth) + RepeatString(@")*>)", _nestDepth) +
                                       " # match <tag> and </tag>",
            RegexOptions.Multiline | RegexOptions.Singleline | RegexOptions.ExplicitCapture | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        /// <summary>
        /// returns an array of HTML tokens comprising the input string. Each token is 
        /// either a tag (possibly with nested, tags contained therein, such 
        /// as &lt;a href="&lt;MTFoo&gt;"&gt;, or a run of text between tags. Each element of the 
        /// array is a two-element array; the first is either 'tag' or 'text'; the second is 
        /// the actual value.
        /// </summary>
        private static List<Token> TokenizeHTML(string text)
        {
            int pos = 0;
            int tagStart = 0;
            var tokens = new List<Token>();

            // this regex is derived from the _tokenize() subroutine in Brad Choate's MTRegex plugin.
            // http://www.bradchoate.com/past/mtregex.php
            foreach (Match m in _htmlTokens.Matches(text))
            {
                tagStart = m.Index;

                if (pos < tagStart)
                    tokens.Add(new Token(TokenType.Text, text.Substring(pos, tagStart - pos)));

                tokens.Add(new Token(TokenType.Tag, m.Value));
                pos = tagStart + m.Length;
            }

            if (pos < text.Length)
                tokens.Add(new Token(TokenType.Text, text.Substring(pos, text.Length - pos)));

            return tokens;
        }

        // @UE3 - added optional tag for TOC
        public static readonly Regex _headerSetext = new Regex(@"
                ^(!{2}[ ]*)? # $1 Optional header starts with two ! marks and should not be in TOC. 
                ([\S ]+?)        # $2 The Header
                [ ]*
                \r?\n
                (=+|-+)      # $3 = string of ='s or -'s
                [ ]*
                (?=\r|\n)",
            RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        // @UE3 - added optional tag for TOC
        public static readonly Regex _headerAtx = new Regex(@"
                ^(\#{1,6})       # $1 = string of #'s
                (!{2}[ ]*)?      # $2 Optional header starts with two ! marks and should not be in TOC.
                [ ]*
                (.+?)           # $3 = Header text
                [ ]*
                \#*             # optional closing #'s (not counted)
                (?=\r|\n)",
            RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        public static readonly Regex _horizontalRules = new Regex(@"
            ^[ ]{0,3}         # Leading space
                ([-*_])       # $1: First marker
                (?>           # Repeated marker group
                    [ ]{0,2}  # Zero, one, or two spaces.
                    \1        # Marker character
                ){2,}         # Group repeated at least twice
                [ ]*          # Trailing spaces
                $             # End of line.
            ", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        //@UE3 redefined lists to be able to match on definition lists
        public static string _wholeList = string.Format(@"
            (                               # $1 = whole list
              (                             # $2
                [ ]{{0,{1}}}
                ({0})                       # $3 = first list item marker
                [ |\t]+
              )
              (?s:
                (
                  [^\n]+
                  (
                    \n[ ]*\n
                    (?![ ]*\n)
                    |
                    \n
                  )
                )+
              )
            )", string.Format("(?:{0}|{1}|{2})", _markerUL, _markerOL, _markerDL), TabWidth - 1);

        private const string UnorderedListHeaderPattern = @"[ ]{0,3}[-+*][ ]+";
        private const string OrderedListHeaderPattern = @"[ ]{0,3}\d+[.][ ]+";
        private const string DefinitionListHeaderPattern = @"[ ]{0,3}[$][ ]+";
        private const string ListPatternTemplate = @"
                (?<{0}>{1}).+\n
                ([ \t]*\n(?![ \t]*\n)|([ ]{{4,}}|({1})).+(\n|$)|(?<=.+\n).+(\n|$))*";

        public static readonly Regex ListPattern = new Regex(string.Format(@"
                (?<=\n|^)
                (
                    (
                        {0}
                    )
                    |
                    (
                        {1}
                    )
                    |
                    (
                        {2}
                    )
                )",
                    string.Format(ListPatternTemplate, "unorderedListHeader", UnorderedListHeaderPattern),
                    string.Format(ListPatternTemplate, "orderedListHeader", OrderedListHeaderPattern),
                    string.Format(ListPatternTemplate, "definitionListHeader", DefinitionListHeaderPattern)
                    ), RegexOptions.Compiled | RegexOptions.IgnorePatternWhitespace); 

        public static readonly Regex _codeSpan = new Regex(@"
                    (?<!\\)   # Character before opening ` can't be a backslash
                    (`+)      # $1 = Opening run of `
                    (.+?)     # $2 = The code block
                    (?<!`)
                    \1
                    (?!`)", RegexOptions.IgnorePatternWhitespace | RegexOptions.Singleline | RegexOptions.Compiled);

        /// <summary>
        /// Turn markdown line breaks (two space at end of line) into HTML break tags
        /// </summary>
        private string DoHardBreaks(string text)
        {
            if (_autoNewlines)
                text = Regex.Replace(text, @"\n", Templates.HardBreak.Render());
            else
                text = Regex.Replace(text, @" {2,}\n", Templates.HardBreak.Render());
            return text;
        }

        public static readonly Regex _blockquote = new Regex(@"
            (                           # Wrap whole match in $1
                (
                ^[ ]*>[ ]?              # '>' at the start of a line
                    .+\n                # rest of the first line
                (.+\n)*                 # subsequent consecutive lines
                \n*                     # blanks
                )+
            )", RegexOptions.IgnorePatternWhitespace | RegexOptions.Multiline | RegexOptions.Compiled);

        private static readonly Regex _autolinkBare = new Regex(@"(^|\s)(https?|ftp)(://[-A-Z0-9+&@#/%?=~_|\[\]\(\)!:,\.;]*[-A-Z0-9+&@#/%=~_|\[\]])($|\W)",
            RegexOptions.IgnoreCase | RegexOptions.Compiled);

        /// <summary>
        /// Turn angle-delimited URLs into HTML anchor tags
        /// </summary>
        /// <remarks>
        /// &lt;http://www.example.com&gt;
        /// </remarks>
        private string DoAutoLinks(string text)
        {

            if (_autoHyperlink)
            {
                // fixup arbitrary URLs by adding Markdown < > so they get linked as well
                // note that at this point, all other URL in the text are already hyperlinked as <a href=""></a>
                // *except* for the <http://www.foo.com> case
                text = _autolinkBare.Replace(text, @"$1<$2$3>$4");
            }

            // Hyperlinks: <http://foo.com>
            text = Regex.Replace(text, "<((https?|ftp):[^'\">\\s]+)>", HyperlinkEvaluator);

            if (_linkEmails)
            {
                // Email addresses: <address@domain.foo>
                string pattern =
                    @"<
                      (?:mailto:)?
                      (
                        [-.\w]+
                        \@
                        [-a-z0-9]+(\.[-a-z0-9]+)*\.[a-z]+
                      )
                      >";
                text = Regex.Replace(text, pattern, EmailEvaluator, RegexOptions.IgnoreCase | RegexOptions.IgnorePatternWhitespace);
            }

            return text;
        }

        private static string HyperlinkEvaluator(Match match)
        {
            var link = match.Groups[1].Value;
            return Templates.Link.Render(Hash.FromAnonymousObject(new {linkUrl = link, linkText = link}));
        }

        private static string EmailEvaluator(Match match)
        {
            string email = Unescape(match.Groups[1].Value);

            //
            //    Input: an email address, e.g. "foo@example.com"
            //
            //    Output: the email address as a mailto link, with each character
            //            of the address encoded as either a decimal or hex entity, in
            //            the hopes of foiling most address harvesting spam bots. E.g.:
            //
            //      <a href="&#x6D;&#97;&#105;&#108;&#x74;&#111;:&#102;&#111;&#111;&#64;&#101;
            //        x&#x61;&#109;&#x70;&#108;&#x65;&#x2E;&#99;&#111;&#109;">&#102;&#111;&#111;
            //        &#64;&#101;x&#x61;&#109;&#x70;&#108;&#x65;&#x2E;&#99;&#111;&#109;</a>
            //
            //    Based by a filter by Matthew Wickline, posted to the BBEdit-Talk
            //    mailing list: <http://tinyurl.com/yu7ue>
            //
            email = "mailto:" + email;

            // leave ':' alone (to spot mailto: later) 
            email = EncodeEmailAddress(email);

            email = Templates.Link.Render(Hash.FromAnonymousObject(new {linkUrl = email, linkText = email}));

            // strip the mailto: from the visible part
            email = Regex.Replace(email, "\">.+?:", "\">");
            return email;
        }

        private static readonly Regex _outDent = new Regex(@"^[ ]{1," + TabWidth + @"}", RegexOptions.Multiline | RegexOptions.Compiled);

        /// <summary>
        /// Remove one level of line-leading spaces
        /// </summary>
        public static string Outdent(string block, PreprocessedTextLocationMap textMap = null)
        {
            return (textMap == null) ? _outDent.Replace(block, "") : MarkdownSharp.Preprocessor.Preprocessor.Replace(_outDent, block, "", textMap);
        }

        private static readonly Regex IndentedPattern = new Regex(@"\A(\s*)+[ ]{4}", RegexOptions.Compiled);

        public static string OutdentIfPossible(string block, PreprocessedTextLocationMap textMap = null)
        {
            if (IndentedPattern.Match(block).Success)
            {
                block = Outdent(block, textMap);
            }

            return block;
        }

        #region Encoding and Normalization


        /// <summary>
        /// encodes email address randomly  
        /// roughly 10% raw, 45% hex, 45% dec 
        /// note that @ is always encoded and : never is
        /// </summary>
        private static string EncodeEmailAddress(string addr)
        {
            var sb = new StringBuilder(addr.Length * 5);
            var rand = new Random();
            int r;
            foreach (char EachCharacter in addr)
            {
                r = rand.Next(1, 100);
                if ((r > 90 || EachCharacter == ':') && EachCharacter != '@')
                    sb.Append(EachCharacter);                         // m
                else if (r < 45)
                    sb.AppendFormat("&#x{0:x};", (int)EachCharacter); // &#x6D
                else
                    sb.AppendFormat("&#{0};", (int)EachCharacter);    // &#109
            }
            return sb.ToString();
        }

        private static readonly Regex _codeEncoder = new Regex(@"&|<|>|\\|\*|_|\{|\}|\[|\]|%|\|", RegexOptions.Compiled);

        /// <summary>
        /// Encode/escape certain Markdown characters inside code blocks and spans where they are literals
        /// </summary>
        public static string EncodeCode(string code)
        {
            return _codeEncoder.Replace(code, EncodeCodeEvaluator);
        }

        private static string EncodeCodeEvaluator(Match match)
        {
            switch (match.Value)
            {
                // Encode all ampersands; HTML entities are not
                // entities within a Markdown code span.
                case "&":
                    return "&amp;";
                // Do the angle bracket song and dance
                case "<":
                    return "&lt;";
                case ">":
                    return "&gt;";
                // escape characters that are magic in Markdown
                default:
                    return _escapeTable[match.Value];
            }
        }

        private static readonly Regex _amps = new Regex(@"&(?!(#[0-9]+)|(#[xX][a-fA-F0-9])|([a-zA-Z][a-zA-Z0-9]*);)", RegexOptions.ExplicitCapture | RegexOptions.Compiled);
        private static readonly Regex _angles = new Regex(@"<(?![A-Za-z/?\$!])", RegexOptions.ExplicitCapture | RegexOptions.Compiled);

        /// <summary>
        /// Encode any ampersands (that aren't part of an HTML entity) and left or right angle brackets
        /// </summary>
        public static string EncodeAmpsAndAngles(string s)
        {
            s = _amps.Replace(s, "&amp;");
            s = _angles.Replace(s, "&lt;");
            return s;
        }

        private static Regex _backslashEscapes; 

        /// <summary>
        /// Encodes any escaped characters such as \`, \*, \[ etc
        /// </summary>
        private static string EscapeBackslashes(string s)
        {
            return _backslashEscapes.Replace(s, new MatchEvaluator(EscapeBackslashesEvaluator));
        }
        private static string EscapeBackslashesEvaluator(Match match)
        {
            return _backslashEscapeTable[match.Value];
        }

        private static readonly Regex _unescapes = new Regex("\x1A\\d+\x1A", RegexOptions.Compiled);

        /// <summary>
        /// swap back in all the special characters we've hidden
        /// </summary>
        public static string Unescape(string s)
        {
            return _unescapes.Replace(s, UnescapeEvaluator);
        }
        private static string UnescapeEvaluator(Match match)
        {
            return _invertedEscapeTable[match.Value];
        }


        /// <summary>
        /// escapes Bold [ * ] and Italic [ _ ] characters
        /// </summary>
        public static string EscapeBoldItalic(string s)
        {
            s = s.Replace("*", _escapeTable["*"]);
            s = s.Replace("_", _escapeTable["_"]);
            return s;
        }

        private static char[] _problemUrlChars = @"""'*()[]$:".ToCharArray();

        /// <summary>
        /// hex-encodes some unusual "problem" chars in URLs to avoid URL detection problems 
        /// </summary>
        public string EncodeProblemUrlChars(string url)
        {
            if (!_encodeProblemUrlCharacters) return url;

            var sb = new StringBuilder(url.Length);
            bool encode;
            char c;

            for (int i = 0; i < url.Length; i++)
            {
                c = url[i];
                encode = Array.IndexOf(_problemUrlChars, c) != -1;
                if (encode && c == ':' && i < url.Length - 1)
                    encode = !(url[i + 1] == '/') && !(url[i + 1] >= '0' && url[i + 1] <= '9');

                if (encode)
                    sb.Append("%" + String.Format("{0:x}", (byte)c));
                else
                    sb.Append(c);                
            }

            return sb.ToString();
        }


        /// <summary>
        /// Within tags -- meaning between &lt; and &gt; -- encode [\ ` * _] so they 
        /// don't conflict with their use in Markdown for code, italics and strong. 
        /// We're replacing each such character with its corresponding hash 
        /// value; this is likely overkill, but it should prevent us from colliding 
        /// with the escape values by accident.
        /// </summary>
        private static string EscapeSpecialCharsWithinTagAttributes(string text)
        {
            var tokens = TokenizeHTML(text);

            // now, rebuild text from the tokens
            var sb = new StringBuilder(text.Length);

            foreach (var token in tokens)
            {
                if (token.Value.StartsWith("<!--"))
                {
                    // ignore comments
                    continue;
                }

                string value = token.Value;

                if (token.Type == TokenType.Tag)
                {
                    value = value.Replace(@"\", _escapeTable[@"\"]);
                    value = Regex.Replace(value, "(?<=.)</?code>(?=.)", _escapeTable[@"`"]);
                    value = EscapeBoldItalic(value);
                }

                sb.Append(value);
            }

            return sb.ToString();
        }

        #endregion

        /// <summary>
        /// this is to emulate what's evailable in PHP
        /// </summary>
        private static string RepeatString(string text, int count)
        {
            var sb = new StringBuilder(text.Length * count);
            for (int i = 0; i < count; i++)
                sb.Append(text);
            return sb.ToString();
        }
    }
}
