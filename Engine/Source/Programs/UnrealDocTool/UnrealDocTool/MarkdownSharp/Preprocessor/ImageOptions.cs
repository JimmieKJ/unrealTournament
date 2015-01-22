// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace MarkdownSharp.Preprocessor
{
    using System;
    using System.Globalization;

    using DotLiquid;

    public enum AlignOption
    {
        Left,
        Right,
        Middle,
        Top,
        Bottom,
        None
    }

    public class ImageOptions
    {
        public int Width { get; private set; }
        public int Height { get; private set; }
        public AlignOption Align { get; private set; }

        public ImageOptions(string width, string height, string align)
        {
            Width = string.IsNullOrWhiteSpace(width) ? -1 : int.Parse(width);
            Height = string.IsNullOrWhiteSpace(height) ? -1 : int.Parse(height);
            Align = string.IsNullOrWhiteSpace(align) ? AlignOption.None : GetAlignOption(align);
        }

        public ImageOptions()
            : this(null, null, null)
        {
            
        }

        public ImageOptions(ImageOptions other)
        {
            Width = other.Width;
            Height = other.Height;
            Align = other.Align;
        }

        private static AlignOption GetAlignOption(string align)
        {
            switch (align.ToLower())
            {
                case "left":
                    return AlignOption.Left;
                case "right":
                    return AlignOption.Right;
                case "middle":
                    return AlignOption.Middle;
                case "top":
                    return AlignOption.Top;
                case "bottom":
                    return AlignOption.Bottom;
                default:
                    throw new InvalidOperationException("Not supported alignment option.");
            }
        }

        public void AppendToHash(Hash imageParams)
        {
            if (Width != -1)
            {
                imageParams.Add("imageWidth", Width.ToString(CultureInfo.InvariantCulture));
            }

            if (Height != -1)
            {
                imageParams.Add("imageHeight", Height.ToString(CultureInfo.InvariantCulture));
            }

            if (Align != AlignOption.None)
            {
                imageParams.Add("imageAlign", GetAlignString(Align));
            }
        }

        private static string GetAlignString(AlignOption align)
        {
            switch (align)
            {
                case AlignOption.Left:
                    return "left";
                case AlignOption.Right:
                    return "right";
                case AlignOption.Middle:
                    return "middle";
                case AlignOption.Top:
                    return "top";
                case AlignOption.Bottom:
                    return "bottom";
                case AlignOption.None:
                    return "none";
                default:
                    throw new InvalidOperationException("Not supported align enum value.");
            }
        }
    }
}