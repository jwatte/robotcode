using System;
using System.Windows.Forms;
using System.Drawing;
using System.Text;

namespace RoboUSBLink
{
	public class RoboBoardTile : Panel
	{
		public RoboBoardTile (RoboBoardModel model)
		{
			this.Model = model;
			this.Size = new Size(200, 100);
			CreateControls();
			SyncControls();
		}
		
		public readonly RoboBoardModel Model;
		
		protected void CreateControls()
		{
			boardNumber = new Label();
			boardNumber.Size = new Size(30, 16);
			boardNumber.Location = new Point(3, 3);
      Controls.Add(boardNumber);

			boardOnline = new Label();
			boardOnline.Size = new Size(70, 16);
			boardOnline.Location = new Point(36, 3);
      Controls.Add(boardOnline);
			
			boardDropouts = new Label();
			boardDropouts.Size = new Size(89, 16);
			boardDropouts.Location = new Point(109, 3);
      Controls.Add(boardDropouts);
			
			boardData = new TextBox();
			boardData.Size = new Size(194, 75);
			boardData.Location = new Point(3, 22);
      Controls.Add(boardData);
		}
		
		protected void SyncControls()
		{
			string t = Model.Id.ToString();
			if (t != boardNumber.Text)
			{
				boardNumber.Text = t;
			}
			t = Model.Online ? "OK" : "Offline";
			if (t != boardOnline.Text)
			{
				boardOnline.Text = t;
			}
			t = Model.NumDropouts.ToString();
			if (t != boardDropouts.Text)
			{
				boardDropouts.Text = t;
			}
			t = HexFormat(Model.LastData);
			if (t != boardData.Text)
			{
				boardData.Text = t;
			}
		}
		
		protected static string HexFormat(byte[] data)
		{
			StringBuilder sb = new StringBuilder();
			foreach (byte b in data)
			{
				sb.AppendFormat("{0.2x}", b);
			}
			return sb.ToString();
		}
		
		private Label boardNumber;
		private Label boardOnline;
		private Label boardDropouts;
		private TextBox boardData;
	}
}

