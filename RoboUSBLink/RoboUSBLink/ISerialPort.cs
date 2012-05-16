using System;

namespace RoboUSBLink
{
	public interface ISerialPort
	{
		void Open();
		void DiscardInBuffer();
		int BytesToRead { get; }
		int Read(byte[] dst, int offset, int count);
		void Close();
	}
}

