using System;

namespace RoboUSBLink
{
	public class RoboBoardModel
	{
		public RoboBoardModel (byte id)
		{
			this.Id = id;
		}
		
		public readonly byte Id;
		public bool Online;
		public int NumDropouts;
		public byte[] LastData;
		
		internal void SetOnline(bool ol)
		{
			if (Online && !ol)
			{
				++NumDropouts;
			}
			Online = ol;
		}
		
		internal void SetData(byte[] buf, int offset, int len)
		{
		}
	}
}

