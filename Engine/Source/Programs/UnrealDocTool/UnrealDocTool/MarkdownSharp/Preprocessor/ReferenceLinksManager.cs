// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.Preprocessor
{
    using System.Text.RegularExpressions;

    using DotLiquid;

    public struct ReferenceAttributes
    {
        public ReferenceAttributes(string Width, string Height, string DoCompressImages, string ConvertImageFormat, string ImageFill, string ImageQuality)
            : this()
        {
            this.Width = Width;
            this.Height = Height;
            this.DoCompressImages = DoCompressImages;
            this.ConvertImageFormat = ConvertImageFormat;
            this.ImageFill = ImageFill;
            this.ImageQuality = ImageQuality;
        }

        /// <summary>
        /// The width the image will be displayed at, optional, value 0 should be ignored.
        /// </summary>
        public string Width { get; private set; }

        /// <summary>
        /// The height the image will be displayed at, optional, value 0 should be ignored.
        /// </summary>
        public string Height { get; private set; }

        /// <summary>
        /// Should the image be compressed?
        /// </summary>
        public string DoCompressImages { get; private set; }

        /// <summary>
        /// Covert the image to this format
        /// </summary>
        public string ConvertImageFormat { get; set; }

        /// <summary>
        /// Image background color
        /// </summary>
        public string ImageFill { get; private set; }

        /// <summary>
        /// jpeg quality
        /// </summary>
        public string ImageQuality { get; private set; }
    }
    

    public class ReferenceLinksManager
    {
        private readonly PreprocessedData data;

        public ReferenceLinksManager(PreprocessedData data)
        {
            this.data = data;
        }

        public readonly Dictionary<string, ReferenceAttributes> ReferenceAttributes = new Dictionary<string, ReferenceAttributes>();
        public readonly Dictionary<string, string> Urls = new Dictionary<string, string>();
        public readonly Dictionary<string, string> Titles = new Dictionary<string, string>();
        public readonly Dictionary<string, ImageOptions> ImageAlignment = new Dictionary<string, ImageOptions>();

        public string Parse(string text, PreprocessedData data)
        {
            var map = data != null ? data.TextMap : null;

            var changes = new List<PreprocessingTextChange>();

            var output = Preprocessor.Replace(LinkDef, text, LinkEvaluator, null, changes);

            Preprocessor.AddChangesToBounds(map, changes, data, PreprocessedTextType.Reference);

            if (map != null)
            {
                map.ApplyChanges(changes);
            }

            return output;
        }

        private static readonly Regex LinkDef = new Regex(string.Format(@"
                        ^[ ]{{0,{0}}}\[(.+)\]:  # id = $1
                          [ ]*
                          \n?                   # maybe *one* newline
                          [ ]*
                        <?(\S+?)>?              # url = $2
                          [ ]*
                          \n?                   # maybe one newline
                          [ ]*
                        (?:
                            (?<=\s)             # lookbehind for whitespace
                            [""(]
                            (.+?)               # title = $3
                            ["")]
                            [ ]*
                        )?                      # title is optional
                        (
                            \(
                                [ ]*                # Optional Space
                                (
                                    w:([0-9]+)      # $6
                                )?                  # Optional Width
                                [ ]*
                                (
                                    h:([0-9]+)   # $8
                                )?              # Optional Height
                                [ ]*
                                (
                                    a:(left|right|none)   # $10
                                )?              # Alignment
                                [ ]*
                                (
                                    convert:([A-Za-z]+)      # $12
                                )?                  # Optional conversion of image
                                [ ]*
                                (
                                    type:([A-Za-z]+)      # $14
                                )?                  # Optional convert to
                                [ ]*
                                (
                                    quality:([0-9]+)      # $16
                                )?                  # Optional compression quality
                                [ ]*
                                (
                                    fill:(\#?[A-H0-9]+)      # $18
                                )?                  # Optional background fill
                                [ ]*
                            \)
                        )?                      #@UE3 Optional Image Allignment
                        (?:\n+|\Z)", Markdown.TabWidth - 1), RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        private string LinkEvaluator(Match match)
        {
            var linkId = match.Groups[1].Value.ToLowerInvariant();

            Urls[linkId] = Markdown.EncodeAmpsAndAngles(match.Groups[2].Value);

            var width = match.Groups[6].Value;
            var height = match.Groups[8].Value;
            var align = match.Groups[10].Value;

            var convert = match.Groups[12].Value;
            var convertType = match.Groups[14].Value;
            var convertQuality = match.Groups[16].Value;
            var hexFillColor = match.Groups[18].Value;

            //Store the refence image attributes
            var imageFormat = new ReferenceAttributes(width, height, convert, convertType, hexFillColor, convertQuality);
            ReferenceAttributes[linkId] = imageFormat;

            if (!String.IsNullOrWhiteSpace(match.Groups[3].Value))
            {
                Titles[linkId] = match.Groups[3].Value.Replace("\"", "&quot;");
            }

            ImageAlignment[linkId] = new ImageOptions(width, height, align);

            return "";
        }

    }
}
