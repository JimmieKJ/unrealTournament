// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;

namespace MarkdownSharp
{
    public class ImageConversion
    {
        /// <summary>
        /// Constructor.
        /// </summary>
        public ImageConversion(string InImageName, string OutImageName, int Width, int Height, bool DoCompressImages, ImageFormat ConvertImageFormat, Color ImageFill, int ImageQuality)
        {
            this.inImageName = InImageName;
            this.outImageName = OutImageName;
            this.width = Width;
            this.height = Height;
            this.doCompressImages = DoCompressImages;
            this.convertImageFormat = ConvertImageFormat;
            this.imageFill = ImageFill;
            this.imageQuality = ImageQuality;

        }
        
        private string inImageName;
        private string outImageName;
        private int width;
        private int height;
        private bool doCompressImages;
        private ImageFormat convertImageFormat;
        private Color imageFill;
        private int imageQuality;

        /// <summary>
        /// File name in markdown.text/image folder
        /// </summary>
        public string InImageName
        {
            get { return inImageName; }
        }

        /// <summary>
        /// File name in hmtl/image folder
        /// </summary>
        public string OutImageName
        {
            get { return outImageName; }
        }


        /// <summary>
        /// The width the image will be displayed at, optional, value 0 should be ignored.
        /// </summary>
        public int Width
        {
            get { return width; }
        }

        /// <summary>
        /// The height the image will be displayed at, optional, value 0 should be ignored.
        /// </summary>
        public int Height
        {
            get { return height; }
        }

        public bool DoCompressImages
        {
            get {return doCompressImages; }
        }

        /// <summary>
        /// Prepare image for web.
        /// </summary>
        /// <returns></returns>
        public byte[] WebImage()
        {
            var imageIn = File.ReadAllBytes(inImageName);

            if (doCompressImages)
            {
                var imageOutStream = new MemoryStream();
                using (var imageInProcessing = Image.FromStream(new MemoryStream(imageIn)))
                {
                    width = imageInProcessing.Width;
                    height = imageInProcessing.Height;

                    using (var b = new Bitmap(width, height))
                    using (var g = Graphics.FromImage(b))
                    using (var resizedImage = (Image)b)
                    {
                        g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;

                        if (convertImageFormat == ImageFormat.Jpeg)
                        {
                            g.FillRectangle(new SolidBrush(imageFill), 0, 0, width, height);
                        }

                        g.DrawImage(imageInProcessing, 0, 0, width, height);

                        var qualityEncoder = System.Drawing.Imaging.Encoder.Quality;
                        var encoderParameters = new EncoderParameters(1);

                        encoderParameters.Param[0] = new EncoderParameter(qualityEncoder, imageQuality);

                        resizedImage.Save(imageOutStream, GetEncoderInfo(convertImageFormat), encoderParameters);

                        return imageOutStream.ToArray();
                    }
                }
            }

            return imageIn;
        }

        /// <summary>
        /// Return a color from a string hex value
        /// </summary>
        /// <param name="HexColor"></param>
        /// <returns></returns>
        public static Color GetColorFromHexString(string HexColor)
        {
            try
            {
                HexColor = HexColor.Replace("#", "");
                byte a = System.Convert.ToByte("FF", 16);
                byte r = System.Convert.ToByte(HexColor.Substring(0, 2), 16);
                byte g = System.Convert.ToByte(HexColor.Substring(2, 2), 16);
                byte b = System.Convert.ToByte(HexColor.Substring(4, 2), 16);
                return Color.FromArgb(a, r, g, b);
            }
            catch (Exception Error)
            {
                throw (Error);
            }

        }

        /// <summary>
        /// Gets the imageFormat from a string representation.
        /// </summary>
        /// <param name="convertType"></param>
        /// <returns></returns>
        public static ImageFormat GetImageFormat(String convertType)
        {
            //Only support conversin to jpg, gif and png for web images
            if (convertType.Equals("jpg"))
            {
                return ImageFormat.Jpeg;
            }
            if (convertType.Equals("gif"))
            {
                return ImageFormat.Gif;
            }
            if (convertType.Equals("png"))
            {
                return ImageFormat.Png;
            }
            else
            {
                throw new Exception(Language.Message("UnknowFormatGivenForConversion", convertType));
            }
        }

        public static String GetImageExt(ImageFormat formatType)
        {
            //Only support conversin to jpg, gif and png for web images
            if (formatType == ImageFormat.Jpeg)
            {
                return "jpg";
            }
            if (formatType == ImageFormat.Gif)
            {
                return "gif";
            }
            if (formatType == ImageFormat.Png)
            {
                return "png";
            }
            else
            {
                throw new Exception(Language.Message("UnknowFormatGivenForConversion", formatType.ToString()));
            }
        }

        /// <summary>
        /// returns a codec we use for image compression
        /// </summary>
        /// <param name="Format"></param>
        /// <returns></returns>
        private static ImageCodecInfo GetEncoderInfo(ImageFormat Format)
        {
            if (Format == null)
            {
                throw new Exception(Language.Message("FormatIsNullInCallToGetEncoderInfo"));
            }
            ImageCodecInfo[] Codecs = ImageCodecInfo.GetImageEncoders();
            foreach (ImageCodecInfo Codec in Codecs)
            {
                if (Codec.FormatID == Format.Guid)
                    return Codec;
            }
            return null;
        }
    }
}
