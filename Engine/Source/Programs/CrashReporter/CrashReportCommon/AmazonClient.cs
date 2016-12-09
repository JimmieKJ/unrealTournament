using System;
using System.Collections.Generic;
using System.Net;
using Amazon.Runtime;
using Amazon.S3;
using Amazon.S3.Model;
using Amazon.SQS;
using Amazon.SQS.Model;

namespace Tools.CrashReporter.CrashReportCommon
{
	/// <summary>
	/// Helper class for accessing AWS
	/// </summary>
	public class AmazonClient : IDisposable
	{
		/// <summary>
		/// Is the AmazonClient initialized and ready to use with S3?
		/// </summary>
		public bool IsS3Valid
		{
			get { return S3Client != null; }
		}

		/// <summary>
		/// Is the AmazonClient initialized and ready to use with SQS?
		/// </summary>
		public bool IsSQSValid
		{
			get { return SqsClient != null; }
		}

		/// <summary>
		/// Create a new AmazonClient
		/// </summary>
		/// <param name="Credentials">Amazon Credentials for the connection to AWS</param>
		/// <param name="SqsConfig">SQS connection details</param>
		/// <param name="S3Config">S3 connection details</param>
		/// <param name="OutError">Out error, receives the error string if an error occurs</param>
		public AmazonClient(AWSCredentials Credentials, AmazonSQSConfig SqsConfig, AmazonS3Config S3Config, out string OutError)
		{
			OutError = string.Empty;

			if (SqsConfig != null)
			{
				try
				{
					SqsClient = new AmazonSQSClient(Credentials, SqsConfig);
				}
				catch (Exception Ex)
				{
					OutError += Ex.Message;
				}
			}

			if (S3Config != null)
			{
				try
				{
					S3Client = new AmazonS3Client(Credentials, S3Config);
				}
				catch (Exception Ex)
				{
					OutError += Ex.Message;
				}
			}
		}

		/// <summary>
		/// Dispose of this object
		/// </summary>
		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		/// <summary>
		/// Dispose of this object
		/// </summary>
		/// <param name="disposing">Dispose of managed resources?</param>
		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
				if (SqsClient != null) SqsClient.Dispose();
				if (S3Client != null) S3Client.Dispose();
			}
		}

		/// <summary>
		/// Get the number of items in an SQS queue
		/// </summary>
		/// <param name="QueueUrl">The SQS queue URL</param>
		/// <returns>Number of messages in the queue. Returns zero in the event of an error.</returns>
		public int GetSQSQueueCount(string QueueUrl)
		{
			if (!IsSQSValid) return 0;

			GetQueueAttributesRequest AttribRequest = new GetQueueAttributesRequest
			{
				QueueUrl = QueueUrl,
				AttributeNames = new List<string>
				{
					"ApproximateNumberOfMessages"
				}
			};

			GetQueueAttributesResponse AttribResponse = SqsClient.GetQueueAttributes(AttribRequest);
			return AttribResponse.ApproximateNumberOfMessages;
		}

		/// <summary>
		/// Gets a message from an SQS queue and if successful, deletes the message from the queue.
		/// Safe method for multiple concurrent consumers of a single SQS because the initial message read makes the message unavailable to other callers.
		/// </summary>
		/// <param name="QueueUrl">The SQS queue URL</param>
		/// <param name="bForceNoDelete">Debug option. Skips the message delete when testing reading messages from a live queue</param>
		/// <returns>A dequeued SQS message or null if it fails.</returns>
		public Message SQSDequeueMessage(string QueueUrl, bool bForceNoDelete)
		{
			if (!IsSQSValid) return null;

			ReceiveMessageRequest ReceiveRequest = new ReceiveMessageRequest
			{
				QueueUrl = QueueUrl,
				MaxNumberOfMessages = 1
			};

			ReceiveMessageResponse ReceiveResponse = SqsClient.ReceiveMessage(ReceiveRequest);

			if (ReceiveResponse.Messages.Count == 1)
			{
				Message Message = ReceiveResponse.Messages[0];

				if (Message != null && (bForceNoDelete || DeleteRecordSQS(QueueUrl, Message)))
				{
					return Message;
				}
			}

			return null;
		}

		/// <summary>
		/// Retrieves an object from S3. (Caller can use the response to read the response stream)
		/// </summary>
		/// <param name="BucketName">S3 bucket name</param>
		/// <param name="Key">S3 object key</param>
		/// <returns>S3 object response for the requested object or null if it fails.</returns>
		public GetObjectResponse GetS3Object(string BucketName, string Key)
		{
			if (!IsS3Valid) return null;

			GetObjectRequest ObjectRequest = new GetObjectRequest
			{
				BucketName = BucketName,
				Key = Key
			};

			return S3Client.GetObject(ObjectRequest);
		}

		/// <summary>
		/// Uploads a file to S3 synchronously (blocks until complete).
		/// </summary>
		/// <param name="BucketName">S3 bucket name</param>
		/// <param name="Key">S3 object key</param>
		/// <param name="Filepath">Full path of the file to upload</param>
		/// <returns>S3 object response for the uploaded object or null if it fails.</returns>
		public PutObjectResponse PutS3ObjectFromFile(string BucketName, string Key, string Filepath)
		{
			if (!IsS3Valid) return null;

			PutObjectRequest ObjectRequest = new PutObjectRequest
			{
				BucketName = BucketName,
				Key = Key,
				FilePath = Filepath
			};

			return S3Client.PutObject(ObjectRequest);
		}

		private bool DeleteRecordSQS(string QueueUrl, Message InMessage)
		{
			DeleteMessageRequest DeleteRequest = new DeleteMessageRequest
			{
				QueueUrl = QueueUrl,
				ReceiptHandle = InMessage.ReceiptHandle
			};

			var DeleteResponse = SqsClient.DeleteMessage(DeleteRequest);
			return DeleteResponse.HttpStatusCode == HttpStatusCode.OK;
		}

		private readonly AmazonSQSClient SqsClient;
		private readonly AmazonS3Client S3Client;
	}
}
