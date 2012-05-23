using System;
using System.Runtime.InteropServices;

namespace RoboUSBLink
{
  public static class LibC
  {
    [DllImport("libc")]
    public extern static int system(string args);
  }
}

