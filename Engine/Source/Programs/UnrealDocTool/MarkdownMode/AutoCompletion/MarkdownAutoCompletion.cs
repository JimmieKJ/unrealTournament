// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.ComponentModel.Composition;
using System.Text.RegularExpressions;
using System.Collections.Generic;
using System.IO;
using System.Linq;

using MarkdownSharp;

using Microsoft.VisualStudio.Language.Intellisense;
using Microsoft.VisualStudio.Text;

namespace MarkdownMode.AutoCompletion
{
    using System;
    using MarkdownMode.ToolWindow;
    using MarkdownSharp.Preprocessor;

    [Export(typeof(IAutoCompletion))]
    public class MarkdownAutoCompletion : IAutoCompletion
    {
        public void AugmentCompletionSession(SnapshotSpan span, IList<CompletionSet> completionSets)
        {
            var text = span.GetText();

            var map = new PreprocessedTextLocationMap();

            text = VariableManager.CutoutVariableInclusions(text, map);

            var match = LinkStartPattern.Match(text);

            if (!match.Success)
            {
                return;
            }

            var suggestionMatchGroup = match.Groups["suggestion"];
            var pathMatchGroup = match.Groups["path"];

            List<PathElement> list;

            if (match.Groups["link"].Success)
            {
                var isImageLink = match.Groups["image"].Success;

                list =
                    PathCompletionHelper.GetSuggestionList(
                        UDNDocRunningTableMonitor.CurrentUDNDocView.ParsingResultsCache.Results.Document,
                        pathMatchGroup.Value,
                        (!isImageLink ? PathElementType.Attachment : 0) | PathElementType.Folder
                        | ((string.IsNullOrWhiteSpace(pathMatchGroup.Value) && !isImageLink)
                               ? PathElementType.Bookmark
                               : 0) | (isImageLink ? PathElementType.Image : 0));
            }
            else if(match.Groups["include"].Success)
            {
                if (pathMatchGroup.Length == 0)
                {
                    if (match.Groups["hash"].Success)
                    {
                        list = GetLocalSuggestions(PathElementType.Excerpt);
                    }
                    else
                    {
                        list = GetLocalSuggestions(PathElementType.Excerpt)
                                .Select(e => new PathElement("#" + e.Text, e.Type))
                                .ToList();

                        list.AddRange(GetRootFolders());
                    }
                }
                else
                {
                    list =
                        PathCompletionHelper.GetSuggestionList(
                            UDNDocRunningTableMonitor.CurrentUDNDocView.CurrentEMDocument,
                            "%ROOT%/" + pathMatchGroup.Value,
                            match.Groups["hash"].Success ? PathElementType.Excerpt : PathElementType.Folder);
                }
            }
            else if (match.Groups["variable"].Success)
            {
                if (pathMatchGroup.Length == 0)
                {
                    list = GetRootFolders();
                    list.AddRange(GetLocalSuggestions(PathElementType.Variable));
                }
                else
                {
                    var types = match.Groups["colon"].Success ? PathElementType.Variable : PathElementType.Folder;
                    var path = "%ROOT%/" + pathMatchGroup.Value.Substring(0, pathMatchGroup.Length - 1);

                    list = PathCompletionHelper.GetSuggestionList(
                            UDNDocRunningTableMonitor.CurrentUDNDocView.CurrentEMDocument, path, types);
                }
            }
            else
            {
                throw new InvalidOperationException("Should not happen!");
            }

            try
            {
                var trackingSpan = span.Snapshot.CreateTrackingSpan(
                    span.Start + map.GetOriginalPosition(suggestionMatchGroup.Index, PositionRounding.None),
                    suggestionMatchGroup.Length,
                    SpanTrackingMode.EdgeInclusive);

                completionSets.Add(
                    new CompletionSet(
                        "AutoCompletion",
                        "AutoCompletion",
                        trackingSpan,
                        CompletionCreator.SortAndCreateCompletions(list),
                        null));
            }
            catch (UnableToDetectOriginalPositionException)
            {
                // ignore and don't show completions
            }
        }

        private static List<PathElement> GetRootFolders()
        {
            return PathCompletionHelper.GetSuggestionList(
                UDNDocRunningTableMonitor.CurrentUDNDocView.CurrentEMDocument, "%ROOT%", PathElementType.Folder);
        }

        private static List<PathElement> GetLocalSuggestions(PathElementType type)
        {
            return PathCompletionHelper.GetSuggestionList(
                UDNDocRunningTableMonitor.CurrentUDNDocView.CurrentEMDocument,
                Path.GetDirectoryName(UDNDocRunningTableMonitor.CurrentUDNDocView.CurrentEMDocument.LocalPath),
                type);
        }

        private static readonly Regex LinkStartPattern =
            new Regex(
                @"  (
                        (
                            (?<variable>\%)
                            (?<path>
                                (?:
                                    (?:[\w]+)
                                    [/\\]
                                )*
                                (
                                    (?:[\w]+)
                                    (?<colon>:)
                                )?
                            )
                            (?<suggestion>[\w]*)
                        )
                        |
                        (
                            (?<include>\[INCLUDE:)
                            (?:
                                (?:
                                    (?<path>
                                        (?:
                                            (?:[\w]+)
                                            [/\\]
                                        )*
                                    )
                                )
                                |
                                (?:
                                    (?<path>
                                        (?:
                                            (?:[\w]+)
                                            [/\\]
                                        )*
                                        (?:[\w]+)
                                    )?
                                    (?<hash>\#)
                                )
                            )?
                            (?<suggestion>[\w]*)
                        )
                        |
                        (
                            (?<link>
                                (?<image>!)?
                                \[[^]]*\]\s*\(
                            )
                            (?<path>
                                (?:
                                    (?:[\w#]+|(?i:\%root\%))
                                    [/\\]
                                )*
                                (?<suggestion>
                                    [\w\%#]*
                                )
                            )
                        )
                    )$",
                RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);
    }
}
