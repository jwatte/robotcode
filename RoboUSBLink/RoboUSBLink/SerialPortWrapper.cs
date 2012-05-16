using System;
using System.IO.Ports;

namespace RoboUSBLink
{
	public class SerialPortWrapper : ISerialPort
	{
		public SerialPortWrapper (SerialPort port)
		{
			this.Port = port;
		}
		
		SerialPort Port;
		
		
		#region ISerialPort implementation
		public void Open ()
		{
			Port.Open();
		}

		public void DiscardInBuffer ()
		{
			Port.DiscardInBuffer();
		}

		public int Read (byte[] dst, int offset, int count)
		{
			return Port.Read(dst, offset, count);
		}

		public void Close ()
		{
			Port.Close();
		}

		public int BytesToRead {
			get {
				return Port.BytesToRead;
			}
		}
		#endregion
	}
}

