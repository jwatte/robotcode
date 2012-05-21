
using System;
using System.Windows.Forms;

namespace RoboUSBLink {
	
	public class RoboUSBLinkMain {
		public static void Main(string[] args) {
      try {
        NativeVideoCapture nvc = new NativeVideoCapture("/dev/video0");
        nvc.Start();
        nvc.Frame();
        nvc.Stop();
        nvc.Dispose();
      }
      catch (System.Exception x) {
        MessageBox.Show(x.Message);
      }
			RoboUSBLinkApp app = new RoboUSBLinkApp();
			Application.Run(app.MainForm);
		}
	}

}
