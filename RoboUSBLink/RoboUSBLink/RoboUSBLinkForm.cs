using System;
using System.Windows.Forms;
using System.Drawing;

namespace RoboUSBLink
{
	public class RoboUSBLinkForm : Form
	{
		public RoboUSBLinkForm (RoboUSBLinkComm comm)
		{
			Comm = comm;
			this.Size = new Size(800, 600);
			this.DoubleBuffered = true;
			this.Text = "Robot Status";
			InitFonts();
			InitControls();
			Data = new RoboDataModel(comm);
			Data.BoardData += HandleDataBoardData;
			Data.BoardOffline += HandleDataBoardOffline;
			Data.CommOffline += HandleCommOffline;
			Data.CommOnline += HandleCommOnline;
			Data.RangeChanged += HandleRangeChanged;
			Data.VoltageChanged += HandleVoltageChanged;
			Data.DebugMessage += HandleDebugMessage;
		}
		
		void InitFonts()
		{
			smallFont = new Font(FontFamily.GenericSansSerif, 8.0f);
			largeFont = new Font(FontFamily.GenericSansSerif, 12.0f, FontStyle.Bold);
		}
		
		void InitControls()
		{
			Size pt = this.ClientSize;
			
			eventLog = new ListBox();
			eventLog.Font = smallFont;
			eventLog.IntegralHeight = false;
			eventLog.Location = new Point(0, pt.Height - 105);
			pt.Height = pt.Height - 105;
			eventLog.Size = new Size(pt.Width, 105);
			eventLog.Anchor = AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
			Controls.Add(eventLog);
			
			rangeLayout = new FlowLayoutPanel();
			rangeLayout.Location = new Point(2, pt.Height - 105);
			pt.Height = pt.Height - 105;
			rangeLayout.Size = new Size(pt.Width - 4, 100);
			rangeLayout.Anchor = AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
			rangeLayout.BorderStyle = BorderStyle.FixedSingle;
			Controls.Add(rangeLayout);
			
			boardLayout = new FlowLayoutPanel();
			boardLayout.Location = new Point(2, pt.Height - 205);
			pt.Height = pt.Height - 205;
			boardLayout.Size = new Size(pt.Width - 4, 200);
			boardLayout.Anchor = AnchorStyles.Bottom | AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
			boardLayout.BorderStyle = BorderStyle.FixedSingle;
			Controls.Add(boardLayout);
			
			commStatus = new Label();
			commStatus.Font = largeFont;
			commStatus.Location = new Point(4, 4);
			commStatus.Size = new Size(394, 20);
			commStatus.Text = "Offline";
			Controls.Add(commStatus);
			
			voltageStatus = new VoltMeterControl();
			voltageStatus.Font = largeFont;
			voltageStatus.Location = new Point(400, 2);
			voltageStatus.Text = "???";
			Controls.Add(voltageStatus);

      testLowVoltage = new Button();
      testLowVoltage.Location = new Point(600, 2);
      testLowVoltage.Text = "Simulate Low Voltage";
      testLowVoltage.Click += (sender, e) => SimulateVoltage(9.4f);
      Controls.Add(testLowVoltage);
		}
		
		private FlowLayoutPanel rangeLayout;
		private FlowLayoutPanel boardLayout;
		private Label commStatus;
		private VoltMeterControl voltageStatus;
		private Font smallFont;
		private Font largeFont;
		private Button testLowVoltage;

		void HandleDataBoardData (byte board)
		{
			AddToEventLog(String.Format("Board Data: {0}", board));
		}

		void HandleDataBoardOffline (byte board)
		{
			AddToEventLog(String.Format("Board Offline: {0}", board));
		}
		
		void HandleCommOffline()
		{
			commStatus.Text = "Offline " + DateTime.UtcNow.ToString("o");
			commStatus.BackColor = System.Drawing.Color.Red;
			commStatus.ForeColor = System.Drawing.Color.Yellow;
			AddToEventLog("Comm offline");
		}
		
		void HandleCommOnline()
		{
			commStatus.Text = "Online " + DateTime.UtcNow.ToString("o");
			commStatus.BackColor = System.Drawing.Color.Green;
			commStatus.ForeColor = System.Drawing.Color.White;
			AddToEventLog("Comm online");
		}
		
		void HandleRangeChanged(byte sensor, byte range)
		{
			AddToEventLog(String.Format("Range changed: sensor {0}: {1}", sensor, range));
		}

    bool hasShutdownWarned;
    bool isShuttingDown;
    DateTime shutdownTime;

    void SimulateVoltage(float val)
    {
      shutdownTime = DateTime.Now.Subtract(new TimeSpan(0, 2, 0));
      HandleVoltageChanged(val);
    }

		void HandleVoltageChanged(float val)
		{
			voltageStatus.Value = val;
      if (val <= 9.5f) {
        if (!hasShutdownWarned) {
          AddToEventLog("Voltage too low -- will shut down in 60 seconds!");
          hasShutdownWarned = true;
          shutdownTime = DateTime.Now;
        }
        else if (!isShuttingDown) {
          if (DateTime.Now.Subtract(shutdownTime).TotalSeconds >= 60) {
            AddToEventLog("Voltage too low -- shutting down!");
            isShuttingDown = true;
            shutdownTime = DateTime.Now;
            System.Diagnostics.Process p = new System.Diagnostics.Process();
            p.EnableRaisingEvents = false;
            p.StartInfo.FileName = "/usr/local/bin/poweroff";
            p.Start();
          }
        }
        else if (DateTime.Now.Subtract(shutdownTime).TotalSeconds > 60) {
          AddToEventLog("Nice shutdown didn't work -- do it the hard way!");
          hasShutdownWarned = false;
          isShuttingDown = false;
            System.Diagnostics.Process p = new System.Diagnostics.Process();
            p.EnableRaisingEvents = false;
            p.StartInfo.FileName = "/usr/local/bin/poweroff";
            p.StartInfo.Arguments = "-f";
            p.Start();
        }
      }
		}
		
		void HandleDebugMessage(string txt)
		{
			AddToEventLog("Debug message: " + txt);
		}
		
		void AddToEventLog(string text)
		{
			eventLog.Items.Add(String.Format("{0} {1}", DateTime.UtcNow.ToString("o"), text));
			if (eventLog.Items.Count > 100) {
				//	Why is there no RemoveRange()?
				eventLog.Items.RemoveAt(0);
			}
			eventLog.SelectedIndex = eventLog.Items.Count - 1;
		}

		
		public readonly RoboUSBLinkComm Comm;
		public readonly RoboDataModel Data;

		private ListBox eventLog;
	}
}

