using System;
using System.Windows.Forms;
using System.Collections.Generic;

namespace RoboUSBLink
{
	public class RoboUSBLinkApp
	{
		public RoboUSBLinkApp ()
		{
			MainThreadQueue = new Queue<RoboUSBLinkComm.VoidFunc>();
			AsyncResults = new List<IAsyncResult>();
			Application.EnableVisualStyles();
			Application.Idle += new EventHandler(this.InvokeApplicationThreadItems);
			Comm = new RoboUSBLinkComm("/dev/ttyACM0", this.MarshalToMainThread);
			MainForm = new RoboUSBLinkForm(Comm);
			MainForm.FormClosing += HandleMainFormFormClosing;
			MainForm.Show(null);
			System.Threading.Thread.Sleep(100);	/* without this, the framework crashes with an invocation exception! */
			Comm.Start();
		}

		void HandleMainFormFormClosing (object sender, FormClosingEventArgs e)
		{
			Comm.Stop();
		}
		
		private void AttachToComm(RoboUSBLinkComm comm)
		{
		}
		
		private void InvokeOnMainThread()
		{
			InvokeApplicationThreadItems(this, EventArgs.Empty);
		}
		
		private void InvokeApplicationThreadItems(object sender, EventArgs e)
		{
			while (true)
			{
				RoboUSBLinkComm.VoidFunc vf;
				lock (MainThreadQueue)
				{
					if (MainThreadQueue.Count == 0)
					{
						isMarshaling = false;
						return;
					}
					vf = MainThreadQueue.Dequeue();
				}
				vf();
			}
		}

		private void MarshalToMainThread(RoboUSBLinkComm.VoidFunc func)
		{
			lock(MainThreadQueue)
			{
				MainThreadQueue.Enqueue(func);
				if (!isMarshaling)
				{
					isMarshaling = true;
					//	if this generates too much garbage collection pressure, fix it then
					IAsyncResult ar = MainForm.BeginInvoke(new RoboUSBLinkComm.VoidFunc(this.InvokeOnMainThread));
					AsyncResults.Add(ar);
				}
			}
			for (int i = 0; i < AsyncResults.Count; ++i)
			{
				IAsyncResult ar = AsyncResults[i];
				if (ar.IsCompleted)
				{
					MainForm.EndInvoke(ar);
					AsyncResults.RemoveAt(i);
					--i;
				}
			}
		}
		
		private readonly RoboUSBLinkComm Comm;
		private volatile bool isMarshaling;
		
		private readonly Queue<RoboUSBLinkComm.VoidFunc> MainThreadQueue;
		private readonly List<IAsyncResult> AsyncResults;
		
		public readonly Form MainForm;
	}
}

