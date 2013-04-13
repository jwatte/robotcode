using System;
using System.Text;
using System.Collections.Generic;
using Diagnostics = System.Diagnostics;
using Drawing = System.Drawing;
using Text = System.Drawing.Text;
using Re = System.Text.RegularExpressions;
using IO = System.IO;
using Forms = System.Windows.Forms;

namespace fontmaker {
    struct FontInfo {
        public Drawing.FontFamily family;
        public float fontsize;
        public int fontint;
        public bool latin;
        public string outname;
    }

	public class MainClass	{
		public static void Main(string[] args)
        {
            Text.FontCollection fonts = new Text.InstalledFontCollection ();

            if (args.Length == 0 || args.Length % 3 != 0) {
                if (args.Length == 1 && args [0] == "list") {
                    foreach (var family in fonts.Families) {
                        Console.WriteLine ("{0}", family.Name);
                    }
                    return;
                }
                Console.WriteLine ("Usage: fontmaker fontname size {ascii|latin} [fontname size {ascii|latin} ...]");
                Console.WriteLine ("Usage: fontmaker list");
                return;
            }

            List<FontInfo> tomake = new List<FontInfo> ();
            for (int i = 0, n = args.Length; i != n; i += 3) {
                string fontname = args [i];
                Drawing.FontFamily fontfamily = null;
                foreach (var family in fonts.Families) {
                    if (family.Name == fontname) {
                        fontfamily = family;
                        break;
                    }
                }

                float fontsize = 0;
                if (!float.TryParse(args[i + 1], out fontsize) || fontsize < 4 || fontsize > 64) {
                    Console.WriteLine ("Not a font size: {0} (must be between 4 and 64)", args [i + 1]);
                    return;
                }

                bool latin = args [i + 2] == "latin";
                if (!latin && args [i + 2] != "ascii") {
                    Console.WriteLine("Unknown encoding: {0}", args [i + 2]);
                    return;
                }

                FontInfo item = new FontInfo();
                item.family = fontfamily;
                item.fontsize = fontsize;
                item.fontint = CalcHeight(fontfamily, fontsize);
                item.latin = latin;
                item.outname = MakeOutName(fontname, fontsize, latin);
                tomake.Add(item);
            }
            foreach (var item in tomake) {
                Console.WriteLine("Generating {0} (be patient)", item.outname);
                string text = MakeFont(item);
                using (var sw = new IO.StreamWriter(item.outname + ".cpp")) {
                    sw.Write(text);
                }
            }
		}

        static Re.Regex badchars = new Re.Regex("[^a-zA-Z0-9]+");
        static Drawing.Bitmap bitmap = new Drawing.Bitmap(256, 256, Drawing.Imaging.PixelFormat.Format32bppRgb);
        static Drawing.Graphics graphics = Drawing.Graphics.FromImage(bitmap);

        static int CalcHeight(Drawing.FontFamily family, float size)
        {
            return (int)Math.Ceiling(family.GetLineSpacing(Drawing.FontStyle.Regular) * size / family.GetEmHeight(Drawing.FontStyle.Regular));
        }

        static string MakeOutName(string fontname, float fontsize, bool latin)
        {
            return badchars.Replace(string.Format("{0}_{1:g3}_{2}", fontname, fontsize, latin ? "latin" : "ascii"), "_");
        }

        static string MakeFont(FontInfo info)
        {
            graphics.TextRenderingHint = Text.TextRenderingHint.SingleBitPerPixelGridFit;
            Drawing.FontFamily ff = info.family;
            Drawing.Font f = new Drawing.Font(ff, info.fontsize, Drawing.GraphicsUnit.Pixel);
            StringBuilder sb = new StringBuilder();
            sb.Append("// Use this declaration to get at this font:\n");
            sb.AppendFormat("// extern unsigned char {0}_data[];\n", info.outname);
            sb.AppendFormat("// Font {0}({0}_data);\n", info.outname);
            sb.AppendFormat("\n");
            sb.AppendFormat("unsigned char {0}_data[] = {1}\n", info.outname, "{");
            sb.Append("  0xf0, ");
            int offset = 4;
            if (info.latin) {
                sb.Append("0x20, 0xff, ");
                offset += (0x101 - 0x20) * 2;
            } else {
                sb.Append("0x20, 0x7e, ");
                offset += (0x80 - 0x20) * 2;
            }
            sb.AppendFormat("0x{0:x},\n", info.fontint);

            List<ushort> offsets = new List<ushort>();
            List<byte> data = new List<byte>();
            int top = 0x7e;
            if (info.latin) {
                top = 0xff;
            }
            for (int i = 0x20; i <= top; ++i) {
                graphics.Clear(Drawing.Color.Black);
                string ch = string.Format("{0}", (char)i);
                graphics.DrawString(ch, f, Drawing.Brushes.White, 0, 0);
                graphics.Flush();
                //  measure this string
                var sz = graphics.MeasureString(ch, f);
                if (sz.Height > info.fontint) {
                    Console.WriteLine("Warning: Character '{0}' (0x{1:x}) has height {2} which is greater than font size {3}.",
                                      ch, (int)i, sz.Height, info.fontint);
                }
                //  extract bits
                byte obyt = 0;
                int nbit = 0;
                offsets.Add((ushort)(data.Count + offset));
                for (int x = 0, n = (int)Math.Ceiling(sz.Width); x != n; ++x) {
                    for (int y = 0; y != info.fontint; ++y) {
                        var c = bitmap.GetPixel(x, y);
                        if (c.G > 127) {
                            //  shift here so I don't have to compensate when emitting half bytes at the end
                            obyt |= (byte)(0x80 >> nbit);
                        }
                        ++nbit;
                        if (nbit == 8) {
                            data.Add(obyt);
                            obyt = 0;
                            nbit = 0;
                        }
                    }
                }
                if (nbit > 0) {
                    data.Add(obyt);
                }
            }
            offsets.Add((ushort)(data.Count + offset));
            int glyph = 0x20;
            foreach (var o in offsets) {
                if (glyph - 0x20 + 1 < offsets.Count) {
                    sb.AppendFormat("0x{0:x},0x{1:x}, // {2}\n", o & 0xff, (o >> 8) & 0xff, glyph);
                }
                else {
                    sb.AppendFormat("0x{0:x},0x{1:x}, // end\n", o & 0xff, (o >> 8) & 0xff);
                }
                glyph += 1;
            }
            glyph = 0x20;
            int acc = offset;
            int end = offsets[glyph - 0x20 + 1];
            foreach (var d in data) {
                sb.AppendFormat("0x{0:x},", d);
                ++acc;
                if (acc == end) {
                    sb.AppendFormat(" // {0}\n", glyph);
                    ++glyph;
                    if (glyph - 0x20 + 1 < offsets.Count) {
                        end = offsets[glyph - 0x20 + 1];
                    }
                }
            }
            sb.AppendFormat("{0};\n\n", "}");
            return sb.ToString();
        }
	}
}
