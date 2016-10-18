using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using ProtoBuf;
using Tools.CrashReporter.CrashReportCommon;

namespace Tools.CrashReporter.CrashReportProcess
{
	class DataRouterConsumer
	{
		[ProtoContract]
		public class ProtocolBufferRecord
		{
			[ProtoMember(1)]
			public string RecordId { get; set; }
			[ProtoMember(2)]
			public string AppId { get; set; }
			[ProtoMember(3)]
			public string AppVersion { get; set; }
			[ProtoMember(4)]
			public string Environment { get; set; }
			[ProtoMember(5)]
			public string UserId { get; set; }
			[ProtoMember(6)]
			public string UserAgent { get; set; }
			[ProtoMember(7)]
			public string UploadType { get; set; }
			[ProtoMember(8)]
			public string Meta { get; set; }
			[ProtoMember(9)]
			public string IpAddress { get; set; }
			[ProtoMember(10)]
			public string FilePath { get; set; }
			[ProtoMember(11)]
			public string ReceivedTimestamp { get; set; }
			[ProtoMember(12)]
			public byte[] Payload { get; set; }
			[ProtoMember(13)]
			public string Geo { get; set; }
			[ProtoMember(14)]
			public string SessionId { get; set; }

			public bool HasPayload { get { return Payload != null && Payload.Length > 0; } }
		}

		public string LastError { get; private set; }

		public bool TryParse(Stream InStream, out ProtocolBufferRecord Message)
		{
			try
			{
				Message = Serializer.DeserializeWithLengthPrefix<ProtocolBufferRecord>(InStream, PrefixStyle.Base128);

				return Message.HasPayload;
			}
			catch (Exception ex)
			{
				LastError = ex.ToString();
				Message = new ProtocolBufferRecord();
				return false;
			}
		}
	}
}
