// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text.RegularExpressions;
using DotLiquid;
using System.IO;
using DotLiquid.Exceptions;
using DotLiquid.NamingConventions;

namespace MarkdownSharp
{
    public class UdnInclude : Tag
    {
        public override void Initialize(string tagName, string markup, List<string> tokens)
        {
            var incParams = _incMarkupPattern.Match(markup);

            _templatePath = incParams.Groups["templateName"].Value.Trim();

            foreach (var assignment in incParams.Groups["assignmentList"].Value.Split(','))
            {
                if (string.IsNullOrWhiteSpace(assignment))
                {
                    continue;
                }

                var assignmentPair = assignment.Split('=');

                if (assignmentPair.Length != 2)
                {
                    throw new SyntaxException(
                        "Assignment list should be in format <dst_var1> = <src_var1>, <dst_var2> = <src_var2>, ..., <dst_varn> = <src_varn>",
                        assignmentPair);
                }

                _assignmentMap[assignmentPair[0].Trim()] = assignmentPair[1].Trim();
            }

            base.Initialize(tagName, markup, tokens);
        }

        public override void Render(Context context, TextWriter result)
        {
            var template = Templates.GetCached(_templatePath);

            var parameters = new Hash();

            foreach (var assignment in _assignmentMap)
            {
                parameters.Add(assignment.Key, context[assignment.Value]);
            }

            result.Write(template.Render(parameters));
        }

        private string _templatePath;
        private readonly Dictionary<string, string> _assignmentMap = new Dictionary<string, string>();
        private readonly Regex _incMarkupPattern = new Regex(@"^\s*(?<templateName>[^ ]+)\s*(\s+assign\s+(?<assignmentList>.+))?\s*$",
                                                   RegexOptions.Compiled);
    }

    public delegate void TemplateChangedHandler();

    public class TemplateFile
    {
        private readonly bool nonDynamicHTMLOutput;
        private readonly List<string> publishFlags = new List<string>();

        public TemplateFile(string templatePath, TransformationData data)
        {
            this.nonDynamicHTMLOutput = data.NonDynamicHTMLOutput;
            this.publishFlags = data.Markdown.PublishFlags;
            ParsedTemplate = null;

            if (string.IsNullOrWhiteSpace(templatePath))
            {
                throw new System.ArgumentException("Template path should be valid path.");
            }

            ReparseTemplate(templatePath);

            var templatePathDir = Path.GetDirectoryName(templatePath);
            var templatePathFileName = Path.GetFileName(templatePath);

            if (templatePathDir != null && templatePathFileName != null)
            {
                fileWatcher = new FileSystemWatcher(templatePathDir, templatePathFileName)
                    {
                        EnableRaisingEvents = true
                    };

                fileWatcher.Changed += (sender, args) =>
                    {
                        ReparseTemplate(templatePath);
                        if (TemplateChanged != null)
                        {
                            TemplateChanged();
                        }
                    };
            }
        }

        public TemplateFile()
        {
            ParsedTemplate = Template.Parse("Missing template.");
        }
        
        public string Render()
        {
            // Empty parameters.
            return Render(Hash.FromAnonymousObject(new {}));
        }

        public string Render(Hash hash)
        {
            var renderParams = new RenderParameters();
            TextWriter textWriter = new StringWriter();

            hash.Add("Context", CreateContextHash());

            renderParams.LocalVariables = hash;
            renderParams.RethrowErrors = true;

            ParsedTemplate.Render(textWriter, renderParams);

            textWriter.Flush();

            return textWriter.ToString().Trim();
        }

        private Hash CreateContextHash()
        {
            var context = new Hash();

            context.Add("IsNonDynamicHTMLOutput", nonDynamicHTMLOutput);
            AddPublishingParameters(context);

            return context;
        }

        private void AddPublishingParameters(Hash hash)
        {
            var publishingHash = new Hash();

            foreach(var publishFlag in publishFlags)
            {
                publishingHash.Add(publishFlag.Substring(0, 1).ToUpper() + publishFlag.Substring(1).ToLower(), true);
            }

            hash.Add("PublishingFor", publishingHash);
        }

        private void ReparseTemplate(string path)
        {
            string templateContent = null;

            // to avoid an editor saving process and FileSystemWatcher clash
            // if IOException is thrown, recheck opening in 0.5 second 5 times
            for (int i = 0; i < 5 && templateContent == null; i++)
            {
                try
                {
                    templateContent = File.ReadAllText(path);
                }
                catch (IOException)
                {
                    // wait until other app stops using the file
                    System.Threading.Thread.Sleep(500);
                }
            }

            // last try and if failed then original exception is thrown
            if (templateContent == null)
            {
                templateContent = File.ReadAllText(path);
            }

            templateContent = templateContent.Replace("\r", "");

            ParsedTemplate = Template.Parse(templateContent);
        }

        public event TemplateChangedHandler TemplateChanged;

        internal Template ParsedTemplate { get; set; }
        private readonly FileSystemWatcher fileWatcher;
    }

    public delegate void TemplatesChangedHandler();

    public class Templates
    {
        public static TemplateFile CustomTag
        {
            get { return GetCached("customTag.html"); }
        }

        public static TemplateFile ListDefinitionItem
        {
            get { return GetCached("listDefinitionItem.html"); }
        }

        public static TemplateFile ListItem
        {
            get { return GetCached("listItem.html"); }
        }

        public static TemplateFile TableCell
        {
            get { return GetCached("tableCell.html"); }
        }

        public static TemplateFile TableRow
        {
            get { return GetCached("tableRow.html"); }
        }

        public static TemplateFile Region
        {
            get { return GetCached("region.html"); }
        }

        public static TemplateFile ErrorLink
        {
            get { return GetCached("errorLink.html"); }
        }

        public static TemplateFile ErrorHighlight
        {
            get { return GetCached("errorHighlight.html"); }
        }

        public static TemplateFile Link
        {
            get { return GetCached("link.html"); }
        }

        public static TemplateFile FancyLink
        {
            get { return GetCached("fancyLink.html"); }
        }

        public static TemplateFile Header
        {
            get { return GetCached("header.html"); }
        }

        public static TemplateFile NonlocalizedFrame
        {
            get { return GetCached("nonlocalizedFrame.html"); }
        }

        public static TemplateFile ImageFrame
        {
            get { return GetCached("imageFrame.html"); }
        }

        public static TemplateFile Toc
        {
            get { return GetCached("toc.html"); }
        }

        public static TemplateFile Metadata
        {
            get { return GetCached("metadata.html"); }
        }

        public static TemplateFile Strong
        {
            get { return GetCached("strong.html"); }
        }

        public static TemplateFile Paragraph
        {
            get { return GetCached("paragraph.html"); }
        }

        public static TemplateFile Crumbs
        {
            get { return GetCached("crumbs.html"); }
        }

        public static TemplateFile MetadataListInVariables
        {
            get { return GetCached("metadataListInVariables.html"); }
        }

        public static TemplateFile ErrorDetails
        {
            get { return GetCached("errorDetails.html"); }
        }

        public static TemplateFile Bookmark
        {
            get { return GetCached("bookmark.html"); }
        }

        public static TemplateFile ColorFrame
        {
            get { return GetCached("colorFrame.html"); }
        }

        public static TemplateFile Table
        {
            get { return GetCached("table.html"); }
        }

        public static TemplateFile List
        {
            get { return GetCached("list.html"); }
        }

        public static TemplateFile HorizontalRule
        {
            get { return GetCached("horizontalRule.html"); }
        }

        public static TemplateFile HardBreak
        {
            get { return GetCached("hardBreak.html"); }
        }

        public static TemplateFile Emphasis
        {
            get { return GetCached("emphasis.html"); }
        }

        public static TemplateFile Code
        {
            get { return GetCached("code.html"); }
        }

        public static TemplateFile Blockquote
        {
            get { return GetCached("blockquote.html"); }
        }

        public static TemplateFile PrettyPrintCodeBlock
        {
            get { return GetCached("prettyPrintCodeBlock.html"); }
        }

        public static TemplateFile TranslatedPageLink
        {
            get { return GetCached("translatedPageLink.html"); }
        }

        public static TemplateFile TranslatedPages
        {
            get { return GetCached("translatedPages.html"); }
        }

        public static TemplateFile RelatedPages
        {
            get { return GetCached("relatedPages.html"); }
        }

        public static TemplateFile PrereqPages
        {
            get { return GetCached("prereqPages.html"); }
        }

        public static TemplateFile Versions
        {
            get { return GetCached("Version.html"); }
        }

        public static TemplateFile SkillLevels
        {
            get { return GetCached("SkillLevel.html"); }
        }

        public static TemplateFile Tags
        {
            get { return GetCached("Tag.html"); }
        }

        public static TemplateFile MissingTemplate
        {
            get { return new TemplateFile(); }
        }

        public static void Init(TransformationData data, bool thisIsPreviewRun = false)
        {
            transformationData = data;
            _thisIsPreviewRun = thisIsPreviewRun;

            _defaultLanguageTemplateLocation = Path.Combine(Path.Combine(Path.Combine(transformationData.CurrentFolderDetails.AbsoluteMarkdownPath, "Include"), "Templates"), transformationData.CurrentFolderDetails.Language);
            _defaultIntTemplateLocation = Path.Combine(Path.Combine(transformationData.CurrentFolderDetails.AbsoluteMarkdownPath, "Include"), "Templates");
        }

        Templates()
        {
            Template.NamingConvention = new CSharpNamingConvention();
            Template.RegisterTag<UdnInclude>("inc");
        }

        internal static TemplateFile GetCached(string path)
        {
            return Instance.InstGetCached(path);
        }

        internal TemplateFile InstGetCached(string path)
        {
            var templatePath = "";

            // Precedence of WebPages = 
            // LangPreview > IntPreview > Lang > Int

            if (_thisIsPreviewRun)
            {
                var previewPath = Path.GetFileNameWithoutExtension(path) + "_Preview" + Path.GetExtension(path);

                if (transformationData.CurrentFolderDetails.Language.ToLower().Equals("int"))
                {
                    templatePath = Path.Combine(_defaultIntTemplateLocation, previewPath);
                }
                else
                {
                    templatePath = File.Exists(Path.Combine(_defaultLanguageTemplateLocation, previewPath))
                                       ? Path.Combine(_defaultLanguageTemplateLocation, previewPath)
                                       : Path.Combine(_defaultIntTemplateLocation, previewPath);
                }

                templatePath = File.Exists(templatePath) ? templatePath : "";
            }

            if (string.IsNullOrWhiteSpace(templatePath))
            {
                if (transformationData.CurrentFolderDetails.Language.ToLower().Equals("int"))
                {
                    templatePath = Path.Combine(_defaultIntTemplateLocation, path);
                }
                else
                {
                    templatePath = File.Exists(Path.Combine(_defaultLanguageTemplateLocation, path))
                                       ? Path.Combine(_defaultLanguageTemplateLocation, path)
                                       : Path.Combine(_defaultIntTemplateLocation, path);
                }
            }

            if (!_cache.ContainsKey(templatePath))
            {
                if (!File.Exists(templatePath))
                {
                    transformationData.ErrorList.Add(
                        new ErrorDetail(
                            Language.Message("TemplateWasNotFoundIn", path, _defaultLanguageTemplateLocation),
                            MessageClass.Error, "", "", 0, 0));

                    _cache[templatePath] = MissingTemplate;
                    return _cache[templatePath];
                }

                _cache[templatePath] = new TemplateFile(templatePath, transformationData);

                _cache[templatePath].TemplateChanged += () =>
                    {
                        if (TemplatesChanged != null)
                        {
                            TemplatesChanged();
                        }
                    };
            }

            return _cache[templatePath];
        }

        internal static Templates Instance
        {
            get { return _inst ?? (_inst = new Templates()); }
        }

        public static event TemplatesChangedHandler TemplatesChanged;

        private static string _defaultLanguageTemplateLocation = "";
        private static string _defaultIntTemplateLocation = "";
        private static TransformationData transformationData;
        private static bool _thisIsPreviewRun;
        private static Templates _inst;
        private readonly Dictionary<string, TemplateFile> _cache = new Dictionary<string, TemplateFile>();
    }
}
