using System;
using System.Collections.Generic;

namespace RoboUSBLink
{
	public class RoboDataModel
	{
		public RoboDataModel (RoboUSBLinkComm comm)
		{
			FatalCodes = new List<byte>();
			Boards = new Dictionary<int, RoboBoardModel>();
			Ranges = new Dictionary<int, byte>();
			Comm = comm;
			AttachToComm(comm);
		}

		private void AttachToComm(RoboUSBLinkComm comm)
		{
			comm.AddCommand((byte)'O', 1, 0, new RoboUSBLinkComm.CmdFunc(this.Cmd_Online));
			comm.AddCommand((byte)'F', 2, 0, new RoboUSBLinkComm.CmdFunc(this.Cmd_Fatal));
			comm.AddCommand((byte)'D', 3, 2, new RoboUSBLinkComm.CmdFunc(this.Cmd_Data));
			comm.AddCommand((byte)'N', 2, 0, new RoboUSBLinkComm.CmdFunc(this.Cmd_Nak));
			comm.AddCommand((byte)'R', 3, 0, new RoboUSBLinkComm.CmdFunc(this.Cmd_Range));
		}
		
		protected void Cmd_Online(RoboUSBLinkComm.CmdDesc desc, byte[] data)
		{
			Online = true;
			OnCommOnline();
		}
		
		protected void Cmd_Fatal(RoboUSBLinkComm.CmdDesc desc, byte[] data)
		{
			Online = false;
			FatalCodes.Add(data[1]);
			++NumDropouts;
			OnCommOffline();
		}
		
		protected void Cmd_Data(RoboUSBLinkComm.CmdDesc desc, byte[] data)
		{
			MakeBoard(data[1]).SetOnline(true);
			MakeBoard(data[1]).SetData(data, 3, data.Length-3);
			OnBoardData(data[1]);
		}
		
		protected void Cmd_Nak(RoboUSBLinkComm.CmdDesc desc, byte[] data)
		{
			MakeBoard(data[1]).SetOnline(false);
			OnBoardOffline(data[1]);
		}
		
		protected void Cmd_Range(RoboUSBLinkComm.CmdDesc desc, byte[] data)
		{
			if (!Ranges.ContainsKey(data[1]))
			{
				Ranges.Add(data[1], data[2]);
			}
			else
			{
				Ranges[data[1]] = data[2];
			}
			OnRangeChanged(data[1], data[2]);
		}
		
		public readonly RoboUSBLinkComm Comm;
		public bool Online;
		public readonly List<byte> FatalCodes;
		public int NumDropouts;
		public readonly Dictionary<int, RoboBoardModel> Boards;
		public readonly Dictionary<int, byte> Ranges;
		
		protected RoboBoardModel MakeBoard(byte id)
		{
			RoboBoardModel ret = null;
			if (Boards.TryGetValue(id, out ret))
			{
				return ret;
			}
			ret = new RoboBoardModel(id);
			Boards.Add(id, ret);
			return ret;
		}
		
		public RoboBoardModel GetBoard(byte id)
		{
			RoboBoardModel ret = null;
			Boards.TryGetValue(id, out ret);
			return ret;
		}
		
		protected void OnCommOnline()
		{
			if (CommOnline != null)
			{
				CommOnline();
			}
		}
		
		protected void OnCommOffline()
		{
			if (CommOffline != null)
			{
				CommOffline();
			}
		}
		
		protected void OnBoardData(byte board)
		{
			if (BoardData != null)
			{
				BoardData(board);
			}
		}
		
		protected void OnBoardOffline(byte board)
		{
			if (BoardOffline != null)
			{
				BoardOffline(board);
			}
		}
		
		protected void OnRangeChanged(byte sensor, byte range)
		{
			if (RangeChanged != null)
			{
				RangeChanged(sensor, range);
			}
		}
		
		public delegate void ChangeFunc();
		public delegate void BoardFunc(byte board);
		public delegate void RangeFunc(byte sensor, byte range);
		
		public event ChangeFunc CommOnline;
		public event ChangeFunc CommOffline;
		public event BoardFunc BoardData;
		public event BoardFunc BoardOffline;
		public event RangeFunc RangeChanged;
	}
}

