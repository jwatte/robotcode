
using System;
using System.IO;
using System.Diagnostics;


namespace RoboUSBLink
{
	public class FileWritingWrapper : ISerialPort
	{
		public FileWritingWrapper(ISerialPort port, string fileName)
		{
			this.Port = port;
			this.FileName = fileName;
			this.buffer = new byte[1024];
		}

		public readonly ISerialPort Port;
		public readonly string FileName;
		protected FileStream output;
		protected Stopwatch stopwatch;
		protected byte[] buffer;
		public static readonly byte[] FileMagic = new byte[] { (byte)'S', (byte)'e', (byte)'r', (byte)'i', (byte)'a', (byte)'l', (byte)0, (byte)1 };
		
		#region ISerialPort implementation
		public void Open()
		{
			output = new FileStream(FileName, FileMode.Truncate);
			WriteHeader();
			Port.Open();
			stopwatch = Stopwatch.StartNew();
		}

		public void DiscardInBuffer()
		{
			Port.DiscardInBuffer();
		}

		public int Read(byte[] dst, int offset, int count)
		{
			int ret = Port.Read(dst, offset, count);
			WriteToFile(dst, offset, count);
			return ret;
		}

		public void Close()
		{
			output.Close();
			Port.Close();
		}

		public int BytesToRead
		{
			get
			{
				return Port.BytesToRead;
			}
		}

		#endregion

		protected void WriteHeader()
		{
			output.Write(FileMagic, 0, FileMagic.Length);
			//  pretend there's a packet of zero data first, to make CatchUp easier to implement
			int bp = 0;
			Marshal(buffer, ref bp, (int)0);
			output.Write(buffer, 0, 4);
		}

		protected void WriteToFile(byte[] dst, int offset, int count)
		{
			int bufPtr = 0;
			Marshal(buffer, ref bufPtr, stopwatch.ElapsedMilliseconds);
			Marshal(buffer, ref bufPtr, count);
			Marshal(buffer, ref bufPtr, dst, offset, count);
			output.Write(dst, 0, bufPtr);
		}
		
		protected void Marshal(byte[] buf, ref int bufPtr, long val)
		{
			if (bufPtr + 8 > buf.Length)
			{
				throw new OverflowException("Marshalled data too big in FileWritingWrapper.Marshal()");
			}
			unchecked
			{
				for (int i = 0; i < 8; ++i)
				{
					buf[bufPtr++] = (byte)(val & 0xff);
					val = val >> 8;
				}
			}
		}
		
		protected void Marshal(byte[] buf, ref int bufPtr, int val)
		{
			if (bufPtr + 4 > buf.Length)
			{
				throw new OverflowException("Marshalled data too big in FileWritingWrapper.Marshal()");
			}
			unchecked
			{
				for (int i = 0; i < 4; ++i)
				{
					buf[bufPtr++] = (byte)(val & 0xff);
					val = val >> 8;
				}
			}
		}
		
		protected void Marshal(byte[] buf, ref int bufPtr, byte[] data, int offset, int size)
		{
			if ((bufPtr + size > buf.Length) || (offset + size > data.Length))
			{
				throw new OverflowException("Marshalled data too big in FileWritingWrapper.Marshal()");
			}
			unchecked
			{
				for (int i = 0; i < size; ++i)
				{
					buf[bufPtr++] = data[offset + i];
				}
			}
		}
	}
}

