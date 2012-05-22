
using System;
using System.Windows.Forms;

namespace RoboUSBLink {
	
	public class RoboUSBLinkMain {
		public static int Main(string[] args) {
      try {
        nvc = new NativeVideoCapture("/dev/video0");
      }
      catch (System.Exception x) {
        MessageBox.Show(String.Format("Loading video exception:\n{0}\n{1}", x.GetType().ToString(), x.Message), "Exception");
        return 1;
      }
			RoboUSBLinkApp app = new RoboUSBLinkApp();
      using (nvc) {
        Application.Idle += HandleApplicationIdle;
        nvc.Start();
  			Application.Run(app.MainForm);
        nvc.Stop();
      }
      return 0;
		}

    static void HandleApplicationIdle (object sender, EventArgs e)
    {
      nvc.Frame();
    }

    static NativeVideoCapture nvc = null;

	}

}
