// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using System.IO;
    using System.Text.RegularExpressions;

    using DotLiquid;

    using MarkdownSharp.Preprocessor;
    using MarkdownSharp.EpicMarkdown.Parsers;

    public class EMObject : EMElement
    {
        private readonly TemplateFile template;
        private readonly Dictionary<string, List<string>> literalParams;
        private readonly Dictionary<string, List<EMObjectParam>> markdownParams;

        private EMObject(EMDocument doc, EMElementOrigin origin, EMElement parent, string templateFile)
            : base(doc, origin, parent)
        {
            template = Templates.GetCached(templateFile);
            this.literalParams = new Dictionary<string, List<string>>();
            this.markdownParams = new Dictionary<string, List<EMObjectParam>>();
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            var parameters = new Hash();

            foreach (var paramPair in literalParams)
            {
                List<string> paramStrings = new List<string>();
                if (paramPair.Value.Count > 1)
                {
                    foreach (var param in paramPair.Value)
                    {
                        paramStrings.Add(param);
                    }
                    parameters.Add(paramPair.Key, paramStrings.ToArray());
                }
                else if (paramPair.Value.Count > 0)
                {
                    parameters.Add(paramPair.Key, paramPair.Value[0]);
                }
            }

            foreach (var paramPair in markdownParams)
            {
                List<string> paramStrings = new List<string>();
                if (paramPair.Value.Count > 1)
                {
                    foreach (var param in paramPair.Value)
                    {
                        paramStrings.Add(param.Elements.GetInnerHTML(includesStack, data));
                    }
                    parameters.Add(paramPair.Key, paramStrings.ToArray());
                }
                else if (paramPair.Value.Count > 0)
                {
                    parameters.Add(paramPair.Key, paramPair.Value[0].Elements.GetInnerHTML(includesStack, data));
                }
            }

            builder.Append(template.Render(parameters));
        }

        private static readonly Regex ParamPattern = new Regex(@"^(?<indentation>[ ]{0,4})(?<contentWithTags>\[(?<tagName>(PARAMLITERAL)|(PARAM)):(?<paramName>[^\]]+?)\](?<content>.*?)(^\k<indentation>)\[\/\k<tagName>\])|(?<contentWithTags>\[(?<tagName>(PARAMLITERAL)|(PARAM)):(?<paramName>[^\]]+?)\](?<content>[^\n]*?)\[\/\k<tagName>\])", RegexOptions.Compiled | RegexOptions.Multiline | RegexOptions.Singleline | RegexOptions.IgnoreCase);

        public static EMElement CreateFromText(string templateName, EMMarkdownTaggedElementMatch contentMatch, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var obj = new EMObject(
                doc,
                origin,
                parent,
                Path.Combine(
                    data.CurrentFolderDetails.AbsoluteMarkdownPath,
                    "Include",
                    "Templates",
                    "Objects",
                    templateName + ".html"));

            obj.ParseParams(contentMatch, data);

            return obj;
        }

        private void ParseParams(EMMarkdownTaggedElementMatch contentMatch, TransformationData data)
        {
            foreach (Match paramMatch in ParamPattern.Matches(contentMatch.Content))
            {
                var isLiteral = paramMatch.Groups["tagName"].Value.ToLower() == "paramliteral";
                var paramName = paramMatch.Groups["paramName"].Value;

                if (isLiteral)
                {
                    var text = new Regex("^" + Regex.Escape(paramMatch.Groups["indentation"].Value), RegexOptions.Multiline).Replace(paramMatch.Groups["content"].Value, "");

                    text = text.Trim('\n');

                    text = Markdown.OutdentIfPossible(text);
                    if(literalParams.ContainsKey(paramName))
                    {
                        literalParams[paramName].Add(text);
                    }
                    else
                    {
                        List<string> literalStrings = new List<string>();
                        literalStrings.Add(text);
                        literalParams.Add(paramName, literalStrings);
                    }
                }
                else
                {
                    var origin = new EMElementOrigin(paramMatch.Groups["contentWithTags"].Index + contentMatch.ContentStart, paramMatch.Groups["contentWithTags"].Value);
                    var tagMatch = new EMMarkdownTaggedElementMatch(origin.Start, origin.Text, paramName,
                        paramMatch.Groups["content"].Index - paramMatch.Groups["contentWithTags"].Index, paramMatch.Groups["content"].Length, paramName);

                    var param = EMObjectParam.CreateParam(origin, Document, this, data,
                        tagMatch, paramName, paramMatch.Groups["indentation"].Value);

                    if (literalParams.ContainsKey(paramName))
                    {
                        markdownParams[paramName].Add(param);
                    }
                    else
                    {
                        List<EMObjectParam> paramList = new List<EMObjectParam>();
                        paramList.Add(param);
                        markdownParams.Add(paramName, paramList);
                    }
                }
            }
        }

        public override void ForEachWithContext<T>(T context)
        {
            context.PreChildrenVisit(this);

            foreach (var paramPair in markdownParams)
            {
                foreach(var param in paramPair.Value)
                param.ForEachWithContext(context);
            }

            context.PostChildrenVisit(this);
        }
    }
}
