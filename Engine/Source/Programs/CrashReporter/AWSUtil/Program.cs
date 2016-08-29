using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using Amazon.Runtime;
using Amazon.SQS;
using Amazon.SQS.Model;

namespace AWSUtil
{
	class Program
	{
		static void Main(string[] args)
		{
			bool ClearSQS = false;
			int MaxDelete = 10;
			string AWSProfileName = "default";
			string AWSCredentialsFilepath = "credentials.ini";
			string AWSSQSServiceURL = "https://sqs.us-east-1.amazonaws.com";
			string AWSSQSQueueUrl = "";

			foreach (var arg in args)
			{
				if (string.Compare("-ClearSQS", arg, StringComparison.OrdinalIgnoreCase) == 0)
				{
					ClearSQS = true;
				}
				else if (arg.StartsWith("-MaxDelete", StringComparison.OrdinalIgnoreCase))
				{
					var ArgSplit = arg.Split('=');
					if (ArgSplit.Length == 2)
					{
						MaxDelete = int.Parse(ArgSplit[1]);
					}
				}
				else if (arg.StartsWith("-AWSProfileName", StringComparison.OrdinalIgnoreCase))
				{
					var ArgSplit = arg.Split('=');
					if (ArgSplit.Length == 2)
					{
						AWSProfileName = ArgSplit[1];
					}
				}
				else if (arg.StartsWith("-AWSCredentialsFilepath", StringComparison.OrdinalIgnoreCase))
				{
					var ArgSplit = arg.Split('=');
					if (ArgSplit.Length == 2)
					{
						AWSCredentialsFilepath = ArgSplit[1].Trim('\"');
					}
				}
				else if (arg.StartsWith("-AWSSQSServiceURL", StringComparison.OrdinalIgnoreCase))
				{
					var ArgSplit = arg.Split('=');
					if (ArgSplit.Length == 2)
					{
						AWSSQSServiceURL = ArgSplit[1];
					}
				}
				else if (arg.StartsWith("-AWSSQSQueueUrl", StringComparison.OrdinalIgnoreCase))
				{
					var ArgSplit = arg.Split('=');
					if (ArgSplit.Length == 2)
					{
						AWSSQSQueueUrl = ArgSplit[1];
					}
				}
			}

			if (ClearSQS)
			{
				Console.WriteLine("DELETING SQS QUEUE RECORDS");

				AWSCredentials Credentials = new StoredProfileAWSCredentials(AWSProfileName, AWSCredentialsFilepath);

				AmazonSQSConfig SqsConfig = new AmazonSQSConfig
				{
					ServiceURL = AWSSQSServiceURL
				};

				var SqsClient = new AmazonSQSClient(Credentials, SqsConfig);

				var AttribRequest = new GetQueueAttributesRequest
				{
					QueueUrl = AWSSQSQueueUrl,
					AttributeNames = new List<string>
					{
						"ApproximateNumberOfMessages"
					}
				};

				var AttribResponse = SqsClient.GetQueueAttributes(AttribRequest);
				Console.WriteLine("INITIAL QUEUE SIZE " + AttribResponse.ApproximateNumberOfMessages);

				int DeletedCount = 0;

				ConsoleColor NormalColor = Console.ForegroundColor;

				while (DeletedCount < MaxDelete)
				{
					int RemainingDeletes = MaxDelete - DeletedCount;

					var ReceiveRequest = new ReceiveMessageRequest
					{
						QueueUrl = AWSSQSQueueUrl,
						MaxNumberOfMessages = Math.Min(10, RemainingDeletes)
					};

					var ReceiveResponse = SqsClient.ReceiveMessage(ReceiveRequest);

					if (ReceiveResponse.Messages.Count == 0)
					{
						break;
					}

					foreach (var Message in ReceiveResponse.Messages)
					{
						if (Message != null)
						{
							DeletedCount++;

							var DeleteRequest = new DeleteMessageRequest
							{
								QueueUrl = AWSSQSQueueUrl,
								ReceiptHandle = Message.ReceiptHandle
							};

							var DeleteResponse = SqsClient.DeleteMessage(DeleteRequest);
							if (DeleteResponse.HttpStatusCode == HttpStatusCode.OK)
							{
								Console.ForegroundColor = NormalColor;
								Console.WriteLine("Deleted record " + Message.Body);
							}
							else
							{
								Console.ForegroundColor = ConsoleColor.Red;
								Console.WriteLine("FAILED to delete record " + Message.Body);
							}
						}
					}					
				}

				AttribRequest = new GetQueueAttributesRequest
				{
					QueueUrl = AWSSQSQueueUrl,
					AttributeNames = new List<string>
					{
						"ApproximateNumberOfMessages"
					}
				};

				AttribResponse = SqsClient.GetQueueAttributes(AttribRequest);
				Console.ForegroundColor = NormalColor;
				Console.WriteLine("FINAL QUEUE SIZE " + AttribResponse.ApproximateNumberOfMessages);
			}

#if DEBUG
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.Write("PRESS ANY KEY");
			Console.ReadKey();
#endif
		}

	}
}
