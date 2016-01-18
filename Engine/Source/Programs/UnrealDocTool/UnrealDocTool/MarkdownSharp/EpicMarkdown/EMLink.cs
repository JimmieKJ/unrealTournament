// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;
using System.IO;
using System.Text.RegularExpressions;
using System.Globalization;

using DotLiquid;

using MarkdownSharp.EpicMarkdown.Parsers;
using MarkdownSharp.EpicMarkdown.PathProviders;
using MarkdownSharp.Preprocessor;

namespace MarkdownSharp.EpicMarkdown
{

    public class EMLink : EMElement
    {
        public EMPathProvider Path { get; private set; }

        public string Title { get; private set; }
        public EMSpanElements Content { get; private set; }
        public bool isFancyLink { get; private set; }
        public Hash metadata { get; private set; }

        public EMLink(EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data, EMPathProvider path, string title, EMSpanElements content, bool isFancyLinkIn, Hash metadataIn)
            : base(doc, origin, parent)
        {
            Path = path;

            Title = title;
            Content = content;
            isFancyLink = isFancyLinkIn;
            metadata = metadataIn;

            if (path.ChangedLanguage)
            {
                AddMessage(
                    new EMReadingMessage(
                        MessageClass.Info, "BadLinkOrMissingMarkdownFileForLinkINTUsed", path.GetPath(data)),
                    data);
            }
        }

        public override string GetClassificationString()
        {
            return "markdown.link";
        }

        public static readonly Regex AnchorRefShortcut = new Regex(@"
            (                               # wrap whole match in $1
              \[
                 ([^\[\]|^\s]+)                 # link text = $2; can't contain whitespace, [ or ]
              \]
            )", RegexOptions.Singleline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        private static readonly Regex AnchorRefParametersPattern = new Regex(@"
            (                               # wrap whole match in $1
                \[
                    (?<id>.*?)                   # id = $3
                \]
            )", RegexOptions.Singleline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        public static EMElement CreateFromAnchorRef(EMDocument doc, EMElementOrigin orig, EMElement parent, TransformationData data, EMSpanElements content, string parameters)
        {
            var match = AnchorRefParametersPattern.Match(parameters);

            var linkId = match.Groups["id"].Value.ToLowerInvariant();

            if (!data.ReferenceLinks.Urls.ContainsKey(linkId))
            {
                throw new EMSkipParsingException();
            }

            var url = data.ReferenceLinks.Urls[linkId];

            return Create(doc, orig, parent, data, content, url,
                GetReferenceTitle(data, linkId));
        }

        private static string GetReferenceTitle(TransformationData data, string linkId)
        {
            return data.ReferenceLinks.Titles.ContainsKey(linkId)
                       ? Markdown.EscapeBoldItalic(data.ReferenceLinks.Titles[linkId])
                       : null;
        }

        private static readonly Regex InlineParametersPattern = new Regex(@"
                        \(
                        [ ]*
                        (?<path>[^"")]+)    # href
                        [ ]*
                        (                   # $4
                        (?<quote>['""])     # quote char
                        (?<title>.*?)       # title
                        \k<quote>           # matching quote
                        [ ]*                # ignore any spaces between closing quote and )
                        )?                  # title is optional
                        \)",
                        RegexOptions.Singleline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        public static EMElement CreateFromInline(EMDocument doc, EMElementOrigin orig, EMElement parent, TransformationData data, EMSpanElements content, string parameters)
        {
            var match = InlineParametersPattern.Match(parameters);
            var path = match.Groups["path"].Value.Trim();
            var title = match.Groups["title"].Value;

            return Create(doc, orig, parent, data, content, path, title);
        }

        private static EMElement Create(
            EMDocument doc,
            EMElementOrigin orig,
            EMElement parent,
            TransformationData data,
            EMSpanElements content,
            string path,
            string title)
        {
            try
            {
                bool isAPILink = false;
                bool isFancyLinkLocal = false;
                Hash metadataLocal = Hash.FromAnonymousObject(
                    new
                    { 
                    });
                string APIMember = "";
                if (path.StartsWith("API:"))
                {
                    APIMember = path.Substring(4, path.Length - 4);
                    if (Markdown.APIFileLocation.ContainsKey(APIMember))
                    {
                        path = "https://docs.unrealengine.com/latest/INT/" + Markdown.APIFileLocation[APIMember].Replace("\\", "/") + "/index.html";
                    }
                    else
                    {
                        content.Parse(APIMember, data);
                        return content;
                    }
                    isAPILink = true;
                }
                else if (path.StartsWith("FANCY:"))
                {
                    path = path.Substring(6, path.Length - 6);
                    isFancyLinkLocal = true;
                    metadataLocal = ProcessFancyLink(path, data);
                }
                var pathProvider = EMPathProvider.CreatePath(path, doc, data);
                var errorId = -1;

                if (pathProvider is EMLocalFilePath)
                {
                    var filePath = pathProvider as EMLocalFilePath;
                    if (!filePath.IsImage)
                    {
                        data.AttachNames.Add(
                            new AttachmentConversionDetail(filePath.GetSource(), filePath.GetAbsolutePath(data)));
                    }
                    else
                    {
                        data.ImageDetails.Add(
                            new ImageConversion(
                                filePath.GetSource(),
                                filePath.GetAbsolutePath(data),
                                0,
                                0,
                                false,
                                null,
                                Color.Transparent,
                                0));
                    }
                }

                if (content.ElementsCount == 0)
                {
                    if (pathProvider.IsBookmark)
                    {
                        errorId = data.ErrorList.Count;
                        data.ErrorList.Add(
                            Markdown.GenerateError(
                                Language.Message("LinkTextMustBeProvidedForBookmarkLinks", orig.Text),
                                MessageClass.Error,
                                Markdown.Unescape(orig.Text),
                                errorId,
                                data));
                    }
                    else if (pathProvider is EMLocalDocumentPath)
                    {
                        if (isAPILink)
                        {
                            content.Parse(APIMember, data);
                        }
                        else
                        {
                            content.Parse(
                                data.ProcessedDocumentCache.Variables.ReplaceVariables(
                                    content.Document, "%" + path + ":title%", data),
                                data);
                        }
                    }
                    else if (pathProvider is EMLocalFilePath)
                    {
                        if (isAPILink)
                        {
                            content.Parse(APIMember, data);
                        }
                        else
                        {
                            // If no link text provided use the file name.
                            content.Parse(Regex.Replace(pathProvider.GetPath(data), @".*[\\|/](.*)", "$1"), data);
                        }
                    }
                    else
                    {
                        if (isAPILink)
                        {
                            content.Parse(APIMember, data);
                        }
                        else
                        {
                            // If no link text provided use the url (remove protocol, e.g. ftp:// http://).
                            content.Parse(Regex.Replace(pathProvider.GetPath(data), @"([^:]+://)?(.*)", "$2"), data);
                        }
                    }
                }

                if (errorId != -1)
                {
                    return new EMErrorElement(doc, orig, parent, errorId);
                }

                if (isAPILink)
                {
                    return new EMLink(doc, orig, parent, data, pathProvider, APIMember, content, false, metadataLocal);
                }
                else
                {
                    return new EMLink(doc, orig, parent, data, pathProvider, title, content, isFancyLinkLocal, metadataLocal);
                }
            }
            catch (EMPathVerificationException e)
            {
				if (doc.PerformStrictConversion())
				{
					return new EMErrorElement(doc, orig, parent, e.AddToErrorList(data, orig.Text));
				}
				else
				{
					EMFormattedText Text = new EMFormattedText(doc, orig, parent);
					Text.Parse(orig.Text, data);
					return Text;
				}
            }
        }

        public static EMElement CreateFromAnchorRefShortcut(Match match, EMDocument doc, EMElementOrigin orig, EMElement parent, TransformationData data)
        {
            var linkText = match.Groups[2].Value;
            var linkId = Regex.Replace(linkText.ToLowerInvariant(), @"[ ]*\n[ ]*", " ");

            if (!data.ReferenceLinks.Urls.ContainsKey(linkId))
            {
                throw new EMSkipParsingException();
            }

            var url = data.ReferenceLinks.Urls[linkId];

            var content = new EMSpanElements(
                doc, new EMElementOrigin(orig.Start + match.Groups[2].Index, linkText), parent);
            content.Parse(match.Groups[2].Value, data);

            return Create(doc, orig, parent, data, content, url, GetReferenceTitle(data, linkId));
        }

        public static Hash ProcessFancyLink(string path, TransformationData data)
        {
            ClosestFileStatus status;
            var linkedDoc = data.ProcessedDocumentCache.GetClosest(path, out status);

            var parameters = Hash.FromAnonymousObject(
                new
                {
                });

            foreach (var metadata in linkedDoc.PreprocessedData.Metadata.MetadataMap)
            {
                if(metadata.Key == "skilllevel")
                {
                    parameters.Add("skilllevellabel", string.Join(", ", metadata.Value));
                    for(int i=0; i < metadata.Value.Count; i++)
                    {
                        metadata.Value[i] = metadata.Value[i].ToLower();
                    }
                }
                parameters.Add(metadata.Key, string.Join(", ", metadata.Value));
            }

            return parameters;
        }

        public override void ForEachWithContext<T>(T context)
        {
            context.PreChildrenVisit(this);

            Content.ForEachWithContext(context);

            context.PostChildrenVisit(this);
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            var linkText = Content.GetInnerHTML(includesStack, data);

            if (linkText == "")
            {
                var externalLink = Path is EMExternalPath;  //assuming all external links are starting with HTTP or HTTPS protocol
                linkText = !externalLink ? data.Metadata.DocumentTitle : Path.GetPath(data);
            }
            try
            {
                if (justTextOutput)
                {
                    builder.Append(linkText);
                    return;
                }

                var result = "";
                if (isFancyLink)
                {
                    result = Templates.FancyLink.Render(
                        Hash.FromAnonymousObject(
                            new
                                {
                                    version = metadata["version"],
                                    skilllevel = metadata["skilllevel"],
                                    skilllevellabel = metadata["skilllevellabel"],
                                    linkUrl = Path.GetPath(data).Replace("\\", "/"),
                                    linkTitle =
                                        !String.IsNullOrWhiteSpace(Title) ? Markdown.EscapeBoldItalic(Title) : null,
                                    linkText =
                                        Path.ChangedLanguage
                                            ? linkText
                                              + Templates.ImageFrame.Render(
                                                  Hash.FromAnonymousObject(
                                                      new
                                                          {
                                                              imageClass = "languageinline",
                                                              imagePath =
                                                    System.IO.Path.Combine(
                                                        data.CurrentFolderDetails.RelativeHTMLPath,
                                                        "Include",
                                                        "Images",
                                                        "INT_flag.jpg")
                                                          }))
                                            : linkText,
                                    relativeHTMLPath = data.CurrentFolderDetails.RelativeHTMLPath
                                }));
                }
                else
                {
                    result = Templates.Link.Render(
                        Hash.FromAnonymousObject(
                            new
                            {
                                linkUrl = Path.GetPath(data).Replace("\\", "/"),
                                linkTitle =
                                    !String.IsNullOrWhiteSpace(Title) ? Markdown.EscapeBoldItalic(Title) : null,
                                linkText =
                                    Path.ChangedLanguage
                                        ? linkText
                                          + Templates.ImageFrame.Render(
                                              Hash.FromAnonymousObject(
                                                  new
                                                  {
                                                      imageClass = "languageinline",
                                                      imagePath =
                                            System.IO.Path.Combine(
                                                data.CurrentFolderDetails.RelativeHTMLPath,
                                                "Include",
                                                "Images",
                                                "INT_flag.jpg")
                                                  }))
                                        : linkText
                            }));
                }

                if (Path.ChangedLanguage)
                {
                    result = Templates.NonlocalizedFrame.Render(Hash.FromAnonymousObject(new { content = result }));
                }

                builder.Append(result);
            }
            catch (EMPathVerificationException e)
            {
                e.AddToErrorListAndAppend(builder, data, Origin.Text);
            }
        }

        private bool justTextOutput = false;

        public void ConvertToJustTextOutput()
        {
            justTextOutput = true;
        }

        public void ConvertToInternal()
        {
            (Path as EMLocalDocumentPath).ConvertToInternalOutput();
        }
    }
}
