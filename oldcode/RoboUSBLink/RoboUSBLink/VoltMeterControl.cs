using System;
using System.Windows.Forms;
using System.Drawing;
using System.Drawing.Imaging;

namespace RoboUSBLink
{
	public class VoltMeterControl : Control
	{
		public VoltMeterControl ()
		{
			this.BackgroundImage = Bitmap.FromFile("vumeter-196-57.png");
			this.Size = new Size(196, 57);
			this.Max = 16.0f;
			this.Min = 9.5f;
			this.Value = 0;
			this.ForeColor = Color.DarkRed;
		}
		
		public float Max { get; set; }
		public float Min { get; set; }
		public float Value {
			get { return val; }
			set {
				if (Math.Abs(val - value) > 1.0f / 16.0f) {
					val = value;
					this.Text = val.ToString("0.0") + " V";
					Invalidate();
				}
			}
		}
		float val;
		const float centerY = 175;
		const float centerX = 98;
		const float radiusInner = 145;
		const float radiusOuter = 165;
		
		protected override void OnPaint (PaintEventArgs e)
		{
			Pen p = new Pen(this.ForeColor, 2.0f);
			float v = val;
			if (v < Min) v = Min;
			if (v > Max) v = Max;
			float phi = (v - Min) / (Max - Min) * (303 - 237) + 237;
			float c = (float)(Math.Cos(phi * Math.PI / 180));
			float s = (float)(Math.Sin(phi * Math.PI / 180));
			e.Graphics.DrawLine(p, new PointF(c * radiusOuter + centerX, s * radiusOuter + centerY), 
				new PointF(c * radiusInner + centerX, s * radiusInner + centerY));
			e.Graphics.DrawString(this.Text, this.Font, Brushes.Black, new Point(70, 35));
		}
	}
}

