// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown
{
    using System.Drawing;
    using System.Drawing.Imaging;
    using System.Text.RegularExpressions;

    using DotLiquid;

    using MarkdownSharp.EpicMarkdown.Parsers;
    using MarkdownSharp.EpicMarkdown.PathProviders;
    using MarkdownSharp.Preprocessor;

    public class EMImage : EMElement
    {
        public EMLocalFilePath Path { get; private set; }
        public string Title { get; private set; }
        public string Alt { get; private set; }
        public ImageOptions Options { get; private set; }

        private EMImage(EMDocument doc, EMElementOrigin origin, EMElement parent, EMLocalFilePath path, string title, string alt, ImageOptions options)
            : base(doc, origin, parent)
        {
            Path = path;
            Title = title;
            Alt = alt;
            Options = options;
        }

        public static readonly Regex ImagesRef = new Regex(@"
                    (               # wrap whole match in $1
                    !\[
                        (.*?)       # alt text = $2
                    \]

                    [ ]?            # one optional space
                    (?:\n[ ]*)?     # one optional newline followed by spaces

                    \[
                        (.*?)       # id = $3
                    \]

                    )", RegexOptions.IgnorePatternWhitespace | RegexOptions.Singleline | RegexOptions.Compiled);

        public static readonly Regex ImagesInline = new Regex(String.Format(@"
              (                     # wrap whole match in $1
                !\[
                    (.*?)           # alt text = $2
                \]
                \s?                 # one optional whitespace character
                \(                  # literal paren
                    [ ]*
                    ({0})           # href = $3
                    [ ]*
                    (               # $4
                    (['""])       # quote char = $5
                    (.*?)           # title = $6
                    \5              # matching quote
                    [ ]*
                    )?              # title is optional
                \)
                (
                    \(
                        [ ]*                # Optional Space
                        (
                            w:([0-9]+)      # $9
                        )?                  # Optional Width
                        [ ]*
                        (
                            h:([0-9]+)   # $11
                        )?              # Optional Height
                        [ ]*
                        (
                            a:(left|right|none)   # $13
                        )?              # Alignment
                        [ ]*
                        (
                            convert:([A-Za-z]+)      # $15
                        )?                  # Optional conversion of image
                        [ ]*
                        (
                            type:([A-Za-z]+)      # $17
                        )?                  # Optional convert to
                        [ ]*
                        (
                            quality:([0-9]+)      # $19
                        )?                  # Optional compression quality
                        [ ]*
                        (
                            fill:(\#?[A-H0-9]+)      # $21
                        )?                  # Optional background fill
                        [ ]*
                    \)
                )?                      # @UE3 Optional Image Allignment
              )", Markdown.GetNestedParensPattern()),
          RegexOptions.IgnorePatternWhitespace | RegexOptions.Singleline | RegexOptions.Compiled);

        public override string GetClassificationString()
        {
            return "markdown.image";
        }

        private static EMElement CreateFromReference(Match match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var alt = match.Groups[2].Value;


            var linkId = match.Groups[3].Value.ToLowerInvariant();

            // for shortcut links like ![this][].
            if (linkId == "")
            {
                linkId = alt.ToLowerInvariant();
            }

            alt = alt.Replace("\"", "&quot;");

            if (!data.ReferenceLinks.Urls.ContainsKey(linkId))
            {
                return EMErrorElement.Create(doc, origin, parent, data, "MissingImageRef", linkId);
            }

            var url = data.ReferenceLinks.Urls[linkId];

            var imageFormat = data.ReferenceLinks.ReferenceAttributes[linkId];
            
            var width = imageFormat.Width;
            var height = imageFormat.Height;
            var convert = imageFormat.DoCompressImages;
            var convertType = imageFormat.ConvertImageFormat;
            var convertQuality = imageFormat.ImageQuality;
            var hexFillColor = imageFormat.ImageFill;

            var options = data.ReferenceLinks.ImageAlignment.ContainsKey(linkId)
                              ? data.ReferenceLinks.ImageAlignment[linkId]
                              : new ImageOptions();

            var title = data.ReferenceLinks.Titles.ContainsKey(linkId)
                            ? Markdown.EscapeBoldItalic(data.ReferenceLinks.Titles[linkId])
                            : null;

            return Create(
                data, doc, origin, parent, url, alt, title, convert, width,
                height, convertQuality, hexFillColor, convertType, options);
        }

        private static EMElement CreateFromInline(Match match, EMDocument doc, EMElementOrigin origin, EMElement parent, TransformationData data)
        {
            var alt = match.Groups[2].Value;
            var url = match.Groups[3].Value;
            var title = match.Groups[6].Value;
            var width = match.Groups[9].Value;
            var height = match.Groups[11].Value;
            var align = match.Groups[13].Value;
            var convert = match.Groups[15].Value;
            var convertType = match.Groups[17].Value;
            var convertQuality = match.Groups[19].Value;
            var hexFillColor = match.Groups[21].Value;

            return Create(
                data, doc, origin, parent, url, alt, title, convert, width,
                height, convertQuality, hexFillColor, convertType,
                new ImageOptions(width, height, align));
        }

        private static EMElement Create(
            TransformationData data,
            EMDocument doc,
            EMElementOrigin origin,
            EMElement parent,
            string url,
            string alt,
            string title,
            string convert,
            string width,
            string height,
            string convertQuality,
            string hexFillColor,
            string convertType,
            ImageOptions options)
        {
            ImageFormat imageFormatType;

            if (String.IsNullOrWhiteSpace(url))
            {
                return EMErrorElement.Create(doc, origin, parent, data, "EmptyLink", origin.Text);
            }

            alt = alt.Replace("\"", "&quot;");

            if (title != null)
            {
                title = title.Replace("\"", "&quot;");
            }

            if (url.StartsWith("<") && url.EndsWith(">"))
            {
                url = url.Substring(1, url.Length - 2); // Remove <>'s surrounding URL, if present
            }

            if (String.IsNullOrWhiteSpace(alt))
            {
                //if no alt text provided use the file name.
                alt = Regex.Replace(url, @".*[\\|/](.*)", "$1");
            }

            var doConvert = !convert.ToLower().Equals("false") && data.Markdown.DefaultImageDoCompress;

            int widthIntConverted;
            int heightIntConverted;
            int quality;

            Color fillColor;

            if (doConvert)
            {
                //if we are converting try to get the other values
                //try parse the strings for width and height into integers
                try
                {
                    widthIntConverted = Int32.Parse(width);
                }
                catch (FormatException)
                {
                    widthIntConverted = 0;
                }

                try
                {
                    heightIntConverted = Int32.Parse(height);
                }
                catch (FormatException)
                {
                    heightIntConverted = 0;
                }

                try
                {
                    quality = Int32.Parse(convertQuality);
                }
                catch (FormatException)
                {
                    quality = data.Markdown.DefaultImageQuality;
                }

                try
                {
                    fillColor = ImageConversion.GetColorFromHexString(hexFillColor);
                }
                catch (Exception)
                {
                    fillColor = data.Markdown.DefaultImageFillColor;
                }

                if (String.IsNullOrWhiteSpace(convertType))
                {
                    convertType = data.Markdown.DefaultImageFormatExtension;
                    imageFormatType = data.Markdown.DefaultImageFormat;
                }
                else
                {
                    try
                    {
                        imageFormatType = ImageConversion.GetImageFormat(convertType.ToLower());
                    }
                    catch (Exception)
                    {
                        return EMErrorElement.Create(
                            doc,
                            origin,
                            parent,
                            data,
                            "UnsupportedImageFileTypeConversion",
                            Markdown.Unescape(url),
                            convertType.ToLower());
                    }
                }
            }
            else
            {
                //set width and height to zero indicating to converter that we want to use the images values
                widthIntConverted = 0;
                heightIntConverted = 0;

                //set conversion type to itself, but do check it is a supported image type.
                convertType =
                    Regex.Match(Markdown.Unescape(url), @"\.(png|gif|tga|bmp|jpg)", RegexOptions.IgnoreCase).Groups[1].Value;

                try
                {
                    imageFormatType = ImageConversion.GetImageFormat(convertType.ToLower());
                }
                catch (Exception)
                {
                    return EMErrorElement.Create(
                        doc,
                        origin,
                        parent,
                        data,
                        "UnsupportedImageFileTypeConversion",
                        Markdown.Unescape(url),
                        convertType.ToLower());
                }

                quality = 100;
                fillColor = data.Markdown.DefaultImageFillColor;
            }

            if (!String.IsNullOrWhiteSpace(convertType))
            {
                try
                {
                    var path = new EMLocalFilePath(Markdown.Unescape(url), doc, data,
                        fileName => System.IO.Path.GetFileNameWithoutExtension(fileName) + "." + ImageConversion.GetImageExt(imageFormatType));

                    if (!path.IsImage)
                    {
                        throw new EMPathVerificationException(Language.Message("GivenPathIsNotAnImage", url));
                    }

                    data.ImageDetails.Add(
                        new ImageConversion(
                            path.GetSource(),
                            path.GetAbsolutePath(data),
                            widthIntConverted,
                            heightIntConverted,
                            doConvert,
                            imageFormatType,
                            fillColor,
                            quality));

                    return new EMImage(doc, origin, parent, path, title, alt, options);
                }
                catch (EMPathVerificationException e)
                {
                    return new EMErrorElement(doc, origin, parent, e.AddToErrorList(data, origin.Text));
                }
            }

            throw new InvalidOperationException("Should not happen!");
        }

        public static List<IParser> GetParser()
        {
            return new List<IParser>() { new EMRegexParser(ImagesRef, CreateFromReference), new EMRegexParser(ImagesInline, CreateFromInline) };
        }

        public override void AppendHTML(StringBuilder builder, Stack<EMInclude> includesStack, TransformationData data)
        {
            if (HadReadingProblem)
            {
                if (data.Markdown.ThisIsPreview)
                {
                    builder.Append(Templates.ErrorHighlight.Render(Hash.FromAnonymousObject(
                        new
                        {
                            errorId = ErrorId,
                            errorText = Origin.Text
                        })));
                }

                return;
            }

            var templateParams =
                Hash.FromAnonymousObject(
                    new
                        {
                            imagePath = Path.GetPath(data).Replace("\\", "/"),
                            imageAlt = Alt,
                            imageTitle = !String.IsNullOrEmpty(Title) ? Markdown.EscapeBoldItalic(Title) : null,
                        });

            Options.AppendToHash(templateParams);

            builder.Append(Templates.ImageFrame.Render(templateParams));
        }
    }
}
