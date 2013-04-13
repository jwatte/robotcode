
using System;
using System.IO;
using System.Diagnostics;
using System.Threading;


namespace RoboUSBLink
{
    public class FileReadingWrapper : ISerialPort
    {
        public FileReadingWrapper(string fileName)
        {
            this.FileName = fileName;
            this.buffer = new byte[1024];
            this.bufCount = 0;
        }

        public readonly string FileName;
        protected FileStream input;
        protected Stopwatch stopwatch;
        protected byte[] buffer;
        protected int bufCount;
        protected int bufBase;
        protected long nextTime;

     #region ISerialPort implementation
        public void Open()
        {
            input = new FileStream(FileName, FileMode.Open);
            byte[] d = new byte[8];
            input.Read(d, 0, 8);
            for (int i = 0; i != 8; ++i)
            {
                if (d[i] != FileWritingWrapper.FileMagic[i])
                {
                    throw new InvalidDataException("The file '" + FileName +
                        "' is not a serial dump file in FileReadingWrapper.Open().");
                }
            }
            nextTime = 0;
            stopwatch = Stopwatch.StartNew();
        }

        public void DiscardInBuffer()
        {
            //  This is not quite right -- it's actually input timing dependent, and
            //  should be recorded in the file. However, it's only used at the start
            //  to throw away pre-reset data from the Arduino Uno board, so this is
            //  a perfectly fine implementation.
            bufBase = 0;
            bufCount = 0;
        }

        public int Read(byte[] dst, int offset, int count)
        {
            int nread = 0;
            while (count > 0)
            {
                if (!CatchUp())
                {
                    Thread.Sleep(1);
                }
                else
                {
                    int toget = bufCount - bufBase;
                    if (toget > count)
                    {
                        toget = count;
                    }
                    Array.Copy(buffer, bufBase, dst, offset, toget);
                    nread += toget;
                    bufBase += toget;
                    offset += toget;
                    count -= toget;
                }
            }
            return nread;
        }

        public
            void Close()
        {
            input.Close();
        }

        public int BytesToRead
        {
            get
            {
                CatchUp();
                return bufCount - bufBase;
            }
        }
		
		public string Name() {
			return this.FileName;
		}

     #endregion

        private byte[] lenBuf = new byte[8];

        protected bool CatchUp()
        {
            if (bufCount == bufBase)
            {
                bufCount = 0;
                bufBase = 0;
            }
            long now = stopwatch.ElapsedMilliseconds;
            int nread = 0;
            while (nextTime <= now)
            {
                int n = input.Read(lenBuf, 0, 4);
                if (n < 4)
                {
                    //  At end of file
                    break;
                }
                int len = lenBuf[0] | (lenBuf[1] << 8) | (lenBuf[2] << 16) | (lenBuf[3] << 24);
                if (bufCount + len > buffer.Length)
                {
                    if ((bufCount - bufBase) + len > buffer.Length)
                    {
                        byte[] newBuf = new byte[buffer.Length + (bufCount - bufBase) + len];
                        Array.Copy(buffer, bufBase, newBuf, 0, bufCount - bufBase);
                        bufCount -= bufBase;
                        bufBase = 0;
                        buffer = newBuf;
                    }
                    else
                    {
                        Array.Copy(buffer, bufBase, buffer, 0, bufCount - bufBase);
                        bufBase = 0;
                    }
                }
                input.Read(buffer, bufCount, len);
                bufCount += len;
                nread += len;
                if (input.Read(lenBuf, 0, 8) != 8)
                {
                    //  At end of file
                    nextTime = long.MaxValue;
                    break;
                }
                else
                {
                    nextTime = (long)(((ulong)(lenBuf[0] | (lenBuf[1] << 8) | (lenBuf[2] << 16) | (lenBuf[3] << 24))) |
                        (((ulong)(lenBuf[4] | (lenBuf[5] << 8) | (lenBuf[6] << 16) | (lenBuf[7] << 24))) << 32));
                }
            }
            return bufCount > bufBase;
        }
    }
}
