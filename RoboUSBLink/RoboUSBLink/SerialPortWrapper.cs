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
    bool open;
		
		#region ISerialPort implementation
    public void Open ()
    {
      open = true;
      Port.Open ();
    }

    public void DiscardInBuffer ()
    {
      Port.DiscardInBuffer ();
    }

    public int Read (byte[] dst, int offset, int count)
    {
      return Port.Read (dst, offset, count);
    }

    public void Close ()
    {
      open = false;
      Port.Close ();
    }

    public int BytesToRead {
      get {
        try {
          return Port.BytesToRead;
        } catch (System.IO.IOException x) {
          Console.WriteLine ("Re-opening port {0}: {1}", Port.PortName, x.Message);
          if (open) {
            Port.Close ();
            Port.Open ();
          }
          return 0;
        }
      }
    }
		#endregion
  }
}

