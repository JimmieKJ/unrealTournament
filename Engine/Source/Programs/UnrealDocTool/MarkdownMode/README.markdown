# Markdown Mode for Visual Studio 2010

To open the MarkdownMode project you have to have installed Visual Studio 2010 SDK (http://www.microsoft.com/en-us/download/confirmation.aspx?id=2680) or Visual Studio 2010 SP1 SDK (http://www.microsoft.com/en-us/download/confirmation.aspx?id=21835).

This is a VS 2010 extension for editing Markdown files.  It has (or is planned to have) a number of features to help users write Markdown files, and I use it for writing [my blog][blog].

Here is a screenshot:
<a href="http://blogs.msdn.com/photos/noahric/images/9946409/original.aspx">
  <img src="http://blogs.msdn.com/photos/noahric/images/9946409/425x230.aspx" alt="Markdown preview tool window, editing the Markdown Part 2 article" />
</a>

Here are the existing features:

 * **Markdown colorization**, a classifier built on [MarkdownSharp][] that understands Markdown syntax.
 * **Preview tool window**, so you can see live updates of what you are typing
 * **HTML colorization**, for regular HTML elements (that Markdown will just pass through to the output).

Markdown.cs is released under the MIT license, which can be found in that file, as it is a derivative work of [MarkdownSharp][].

All other source code is released under the [Ms-PL license](http://www.opensource.org/licenses/ms-pl.html).

Thanks to [John Gruber][john-gruber] for giving me permission to use the term Markdown in reference to this extension.

 [MarkdownSharp]:http://code.google.com/p/markdownsharp/
 [blog]:http://blogs.msdn.com/noahric
 [john-gruber]:http://daringfireball.net/
