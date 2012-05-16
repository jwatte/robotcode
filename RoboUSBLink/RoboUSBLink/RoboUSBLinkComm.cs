using System;
using System.IO.Ports;
using System.Threading;
using System.Collections.Generic;

namespace RoboUSBLink
{
	public class RoboUSBLinkComm
	{
		public RoboUSBLinkComm (string devName, MarshalFuncFunc func)
		{
			DevName = devName;
			MarshalToMainThread = func;
			buffer = new byte[256];
			bufPtr = 0;
			commands = new Dictionary<byte, CmdDesc>();
			port = new SerialPortWrapper(new System.IO.Ports.SerialPort(devName, 115200, Parity.None, 8, StopBits.One));
		}
		
		protected readonly MarshalFuncFunc MarshalToMainThread;

		public void Start()
		{
			if (thread != null)
			{
				throw new InvalidOperationException("Can't start a running RoboUSBLinkComm");
			}
			port.Open();
			port.DiscardInBuffer();
			running = true;
			thread = new Thread(new ThreadStart(this.PortThread));
			thread.Start();
		}
		
		public void Stop()
		{
			running = false;
			thread.Join();
			thread = null;
			port.Close();
		}
		
		private void PortThread()
		{
			while (running)
			{
				if (bufPtr < buffer.Length)
				{
					int n = port.BytesToRead;
					if (n + bufPtr > buffer.Length)
					{
						n = buffer.Length - bufPtr;
					}
					if (n > 0)
					{
						n = port.Read(buffer, bufPtr, n);
						bufPtr += n;
					}
					if (n == 0)
					{
						//	no data -- there is no async read for SerialPort, so sleep instead
						Thread.Sleep(10);
					}
					else
					{
						//	there can be three outcomes:
						//	1. a command is recognized, and complete
						//	2. a command is recognized, but not complete
						//	3. the leading character is not recognized
						bool parsing = true;
						while (parsing && (bufPtr > 0))
						{
							switch (DispatchCmd(buffer, ref bufPtr))
							{
							case DispatchResult.RecognizedRemoved:
								break;
							case DispatchResult.RecognizedPending:
								parsing = false;
								break;
							case DispatchResult.NotRecognized:
								OnUnrecognizedCmd(buffer[0]);
								Array.Copy(buffer, 1, buffer, 0, bufPtr - 1);
								--bufPtr;
								break;
							}
						}
					}
				}
				else
				{
					OnBufferOverflow();
					bufPtr = 0;
				}
			}
		}
		
		public readonly string DevName;
		private readonly ISerialPort port;

		private Thread thread;

		private volatile bool running;
		private byte[] buffer;
		private int bufPtr;

		protected enum DispatchResult
		{
			RecognizedRemoved,
			RecognizedPending,
			NotRecognized
		}
		
		public struct CmdDesc
		{
			public byte Cmd;
			public byte Length;
			public byte AdditionalLengthOffset;
			public CmdFunc Func;
			public CmdDesc(byte c, byte l, byte a, CmdFunc f)
			{
				Cmd = c;
				Length = l;
				AdditionalLengthOffset = a;
				Func = f;
			}
		};
		public delegate void CmdFunc(CmdDesc desc, byte[] data);
		private readonly Dictionary<byte, CmdDesc> commands;

		public void AddCommand(byte cmd, byte len, byte addlLenOffset, CmdFunc func)
		{
			if (running)
			{
				throw new InvalidOperationException("Cannot AddCommand() on a running RoboUSBLinkComm");
			}
			commands.Remove(cmd);
			commands.Add(cmd, new CmdDesc(cmd, len, addlLenOffset, func));
		}
		
		protected DispatchResult DispatchCmd(byte[] buf, ref int bufPtr)
		{
			CmdDesc desc;
			if (!commands.TryGetValue(buf[0], out desc))
			{
				return DispatchResult.NotRecognized;
			}
			if (bufPtr < desc.Length)
			{
				return DispatchResult.RecognizedPending;
			}
			int len = desc.Length;
			if (desc.AdditionalLengthOffset != 0)
			{
				len += buf[desc.AdditionalLengthOffset];
			}
			if (bufPtr < len)
			{
				return DispatchResult.RecognizedPending;
			}
			//	It's a little weird that I both "recognize" the command and dispatch it
			//	in the same context, but that's what it is.
			OnCommand(desc, buf, 0, len);
			if (bufPtr > len)
			{
				Array.Copy(buf, len, buf, 0, bufPtr - len);
			}
			bufPtr -= len;
			return DispatchResult.RecognizedRemoved;
		}

		protected void OnBufferOverflow()
		{
			MarshalToMainThread(delegate() {
				if (this.BufferOverflow != null) {
					this.BufferOverflow();
				}
			});
		}
		
		protected void OnUnrecognizedCmd(byte b)
		{
			MarshalToMainThread(delegate() {
				if (this.UnrecognizedCmd != null) {
					this.UnrecognizedCmd(b);
				}
			});
		}
		
		protected void OnCommand(CmdDesc desc, byte[] buf, int offset, int length)
		{
			//	This may generate garbage collection pressure -- I'll deal with that if it's an actual problem
			byte[] sr = Subrange(buf, offset, length);
			MarshalToMainThread(delegate() {
				desc.Func(desc, sr);
			});
		}
		
		public static byte[] Subrange(byte[] buf, int offset, int length)
		{
			//	this may be a bit of garbage collection pressure, but I'll deal with that if it's an actual problem
			byte[] ret = new byte[length];
			Array.Copy(buf, offset, ret, 0, length);
			return ret;
		}

		public delegate void VoidFunc();
		public delegate void MarshalFuncFunc(VoidFunc vf);
		public delegate void OverflowFunc();
		public delegate void UnrecognizedFunc(byte a);
		
		public event OverflowFunc BufferOverflow;
		public event UnrecognizedFunc UnrecognizedCmd;
	}
	
}

