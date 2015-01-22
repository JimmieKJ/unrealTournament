// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Specialized;
using System.Configuration;
using System.Runtime.CompilerServices;
using System.Web.Configuration;

namespace MarkdownSharp
{
    /// <summary>
    /// Container for Markdown options. This class is immutable, create a new instance
    /// if you want to change the options after construction.
    /// </summary>
    public class MarkdownOptions
    {
        /// <summary>
        /// Default constructor. First loads default values, then
        /// overrides them with any values specified in the configuration file.
        /// Configuration values are specified in the &lt;appSettings&gt;
        /// section of the config file and take the form Markdown.PropertyName
        /// where PropertyName is any of the properties in this class.
        /// </summary>
        public MarkdownOptions() : this(true) { }

        /// <summary>
        /// Sets all values to their defaults, and if loadFromConfigFile
        /// is true it overrides them with configuration file values.
        /// Configuration values are specified in the &lt;appSettings&gt;
        /// section of the config file and take the form Markdown.PropertyName
        /// where PropertyName is any of the properties in this class.
        /// </summary>
        /// <param name="loadFromConfigFile">True to override defaults with values from config file.</param>
        public MarkdownOptions(bool loadFromConfigFile) 
            : this(loadFromConfigFile, new[] { ConfigurationManager.AppSettings, WebConfigurationManager.AppSettings })
        {
        }


        /// <summary>
        /// Constructor for internal use and unit testing.
        /// </summary>
        /// <param name="loadFromConfigFile"></param>
        /// <param name="configProviders"></param>
        internal MarkdownOptions(bool loadFromConfigFile, NameValueCollection[] configProviders)
        {
            Defaults();
            if (!loadFromConfigFile) return;


            foreach (var appSettings in configProviders)
            {
                foreach (string key in appSettings.Keys)
                {
                    switch (key)
                    {
                        case "Markdown.AutoHyperlink":
                            AutoHyperlink = Convert.ToBoolean(appSettings[key]);
                            break;
                        case "Markdown.AutoNewlines":
                            AutoNewlines = Convert.ToBoolean(appSettings[key]);
                            break;
                        case "Markdown.EmptyElementSuffix":
                            EmptyElementSuffix = appSettings[key];
                            break;
                        case "Markdown.EncodeProblemUrlCharacters":
                            EncodeProblemUrlCharacters = Convert.ToBoolean(appSettings[key]);
                            break;
                        case "Markdown.LinkEmails":
                            LinkEmails = Convert.ToBoolean(appSettings[key]);
                            break;
                        case "Markdown.NestDepth":
                            NestDepth = Convert.ToInt32(appSettings[key]);
                            break;
                        case "Markdown.StrictBoldItalic":
                            StrictBoldItalic = Convert.ToBoolean(appSettings[key]);
                            break;
                        case "Markdown.TabWidth":
                            TabWidth = Convert.ToInt32(appSettings[key]);
                            break;
                    }
                }
            }    
        }


        /// <summary>
        /// Sets all options explicitly and does not attempt to override them with values
        /// from configuration file.
        /// </summary>
        /// <param name="autoHyperlink"></param>
        /// <param name="autoNewlines"></param>
        /// <param name="emptyElementsSuffix"></param>
        /// <param name="encodeProblemUrlCharacters"></param>
        /// <param name="linkEmails"></param>
        /// <param name="nestDepth"></param>
        /// <param name="strictBoldItalic"></param>
        /// <param name="tabWidth"></param>
        public MarkdownOptions(bool autoHyperlink, bool autoNewlines, string emptyElementsSuffix, 
            bool encodeProblemUrlCharacters, bool linkEmails, int nestDepth, bool strictBoldItalic, 
            int tabWidth)
        {
            AutoHyperlink = autoHyperlink;
            AutoNewlines = autoNewlines;
            EmptyElementSuffix = emptyElementsSuffix;
            EncodeProblemUrlCharacters = encodeProblemUrlCharacters;
            LinkEmails = linkEmails;
            NestDepth = nestDepth;
            StrictBoldItalic = strictBoldItalic;
            TabWidth = tabWidth;
        }

        /// <summary>
        /// Sets all fields to their default values
        /// </summary>
        private void Defaults()
        {
            AutoHyperlink = false;
            AutoNewlines = false;
            EmptyElementSuffix = " />";
            EncodeProblemUrlCharacters = false;
            LinkEmails = true;
            NestDepth = 6;
            StrictBoldItalic = false;
            TabWidth = 4;
        }

        /// <summary>
        /// when true, (most) bare plain URLs are auto-hyperlinked  
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool AutoHyperlink { get; private set; }

        /// <summary>
        /// when true, RETURN becomes a literal newline  
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool AutoNewlines { get; private set; }

        /// <summary>
        /// use ">" for HTML output, or " />" for XHTML output
        /// </summary>
        public string EmptyElementSuffix { get; private set; }

        /// <summary>
        /// when true, problematic URL characters like [, ], (, and so forth will be encoded 
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool EncodeProblemUrlCharacters { get; private set; }

        /// <summary>
        /// when false, email addresses will never be auto-linked  
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool LinkEmails { get; private set; }

        /// <summary>
        /// maximum nested depth of [] and () supported by the transform
        /// </summary>
        public int NestDepth { get; private set; }

        /// <summary>
        /// when true, bold and italic require non-word characters on either side  
        /// WARNING: this is a significant deviation from the markdown spec
        /// </summary>
        public bool StrictBoldItalic { get; private set; }

        /// <summary>
        /// Tabs are automatically converted to spaces as part of the transform  
        /// this variable determines how "wide" those tabs become in spaces
        /// </summary>
        public int TabWidth { get; private set; }
    }
}
