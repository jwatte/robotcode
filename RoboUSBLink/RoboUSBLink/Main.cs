
using System;
using System.Windows.Forms;

namespace RoboUSBLink {
	
	public class RoboUSBLinkMain {
		public static void Main(string[] args) {
			RoboUSBLinkApp app = new RoboUSBLinkApp();
			Application.Run(app.MainForm);
		}
	}

}
