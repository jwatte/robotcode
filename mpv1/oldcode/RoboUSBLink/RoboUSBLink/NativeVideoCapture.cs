using System;
using System.Runtime.InteropServices;

namespace RoboUSBLink
{
  public class NativeVideoCapture : IDisposable
  {
    [DllImport("NativeVideoCapture", CharSet=CharSet.Ansi)]
    private static extern int open_dev (string devName)

;
    [DllImport("NativeVideoCapture")]
    private static extern IntPtr make_capture_info ()

;
    [DllImport("NativeVideoCapture")]
    private static extern int config_dev (int fd, IntPtr capi)

;
    [DllImport("NativeVideoCapture")]
    private static extern int enqueue_all_buffers (int fd, IntPtr ci)

;
    [DllImport("NativeVideoCapture")]
    private static extern int start_capture (int fd, IntPtr ci)

;
    [DllImport("NativeVideoCapture")]
    private static extern int capture_one_frame_and_re_enqueue (int fd, IntPtr ci)

;
    [DllImport("NativeVideoCapture")]
    private static extern int stop_capture (int fd, IntPtr ci)

;
    [DllImport("NativeVideoCapture")]
    private static extern int close_video (int fd, IntPtr capi)

;
    [DllImport("NativeVideoCapture")]
    private static extern IntPtr get_error ();

    public NativeVideoCapture (string devname)
    {
      fd = open_dev (devname);
      if (fd == -1) {
        throw new System.IO.IOException (Marshal.PtrToStringAnsi (get_error ()));
      }
      capi = make_capture_info ();
      if (config_dev (fd, capi) == -1) {
        throw new System.IO.IOException (Marshal.PtrToStringAnsi (get_error ()));
      }
    }

    public void Start ()
    {
      enqueue_all_buffers (fd, capi);
      start_capture (fd, capi);
    }

    public void Frame ()
    {
      capture_one_frame_and_re_enqueue (fd, capi);
    }

    public void Stop ()
    {
      stop_capture (fd, capi);
    }

    #region IDisposable implementation
    public void Dispose ()
    {
      if (fd > -1 || capi.ToInt64 () != 0) {
        close_video (fd, capi);
        fd = -1;
        capi = (IntPtr)0;
      }
    }

    #endregion
    int fd;
    IntPtr capi;
  }
}

