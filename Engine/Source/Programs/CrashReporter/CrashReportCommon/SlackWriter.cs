using System;
using System.Net;
using System.Text;
using System.Threading.Tasks;

namespace Tools.CrashReporter.CrashReportCommon
{
	/// <summary>
	/// Allows us to write messages to Slack
	/// </summary>
	public class SlackWriter
	{
		/// <summary>
		/// Full URL from your Incoming Webhooks Integration on the Slack website
		/// </summary>
		public string WebhookUrl { get; set; }

		/// <summary>
		/// Overrides the channel to which the message is posted. The Incoming Webhooks Integration created on the Slack website will have a default channel.
		/// </summary>
		public string Channel { get; set; }

		/// <summary>
		/// Overrides the username with which the message is posted. The Incoming Webhooks Integration created on the Slack website will have a default username.
		/// </summary>
		public string Username { get; set; }

		/// <summary>
		/// Overrides the emoji for the username with which the message is posted. The Incoming Webhooks Integration created on the Slack website will have a default emoji.
		/// </summary>
		public string IconEmoji { get; set; }

		/// <summary>
		/// Sends a message to Slack
		/// </summary>
		/// <param name="MessageText"></param>
		public void Write(string MessageText)
		{
			lock (Lock)
			{
				if (!string.IsNullOrEmpty(WebhookUrl))
				{
					string WebhookSnapshot = WebhookUrl;
					string ChannelSnapshot = Channel;
					string UsernameSnapshot = Username;
					string IconEmojiSnapshot = IconEmoji;

					WriteTask = WriteTask.ContinueWith(PreviousTask => WriteAsync(WebhookSnapshot, ChannelSnapshot, UsernameSnapshot, IconEmojiSnapshot, MessageText));
				}
			}
		}

		/// <summary>
		/// Shutdown and flush the Slack writer
		/// </summary>
		public void Dispose()
		{
			lock (Lock)
			{
				WriteTask.Wait();
			}
		}

		private string WriteAsync(string InWebhook, string InChannel, string InUsername, string InIconEmoji, string InMessageText)
		{
			try
			{
				using (WebClient Client = new WebClient())
				{
					Client.Headers[HttpRequestHeader.ContentType] = "application/json";
					Client.Encoding = Encoding;

					string Payload = GetPayload(InChannel, InUsername, InIconEmoji, InMessageText);

					return Client.UploadString(new Uri(InWebhook), "POST", Payload);
				}
			}
			catch (WebException Ex)
			{
				return Ex.Message;
			}
		}

		private static string GetPayload(string InChannel, string InUsername, string InIconEmoji, string InMessageText)
		{
			StringBuilder Payload = new StringBuilder();
			Payload.Append("{");

			if (!string.IsNullOrEmpty(InChannel))
			{
				Payload.Append(string.Format("\"channel\": \"{0}\", ", JsonEncode(InChannel)));
			}
			if (!string.IsNullOrEmpty(InUsername))
			{
				Payload.Append(string.Format("\"username\": \"{0}\", ", JsonEncode(InUsername)));
			}
			if (!string.IsNullOrEmpty(InIconEmoji))
			{
				Payload.Append(string.Format("\"icon_emoji\": \"{0}\", ", JsonEncode(InIconEmoji)));
			}
			Payload.Append(string.Format("\"text\": \"{0}\"}}", JsonEncode(InMessageText)));

			return Payload.ToString();
		}

		private static string JsonEncode(string InString)
		{
			return InString
				.Replace(@"\", @"\\")
				.Replace("\r\n", @"\n")
				.Replace(Environment.NewLine, @"\n")
				.Replace("\r", @"\n")
				.Replace("\n", @"\n")
				.Replace("\"", "\\\"");
		}

		private readonly Encoding Encoding = new UTF8Encoding();
		private readonly Object Lock = new Object();
		private Task<string> WriteTask = Task.FromResult("init");
	}
}
