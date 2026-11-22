using System;
using System.IO;
using System.Diagnostics;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace FengYin.HangCapture {
    public static class DumpNative {
        [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
        [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetProcessTimes(IntPtr h, out long creation, out long exit, out long kernel, out long user);
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool QueryFullProcessImageName(IntPtr h, int flags, StringBuilder name, ref int size);
        // Windows SDK minidumpapiset.h explicitly uses pshpack4.h, also on x64.
        [StructLayout(LayoutKind.Sequential, Pack=4)] public struct CallbackHeader { public uint PID; public IntPtr Process; public uint Type; }
        [UnmanagedFunctionPointer(CallingConvention.Winapi)] [return: MarshalAs(UnmanagedType.Bool)]
        delegate bool Callback(IntPtr param, IntPtr input, IntPtr output);
        [StructLayout(LayoutKind.Sequential, Pack=4)] struct CallbackInfo { public Callback Function; public IntPtr Parameter; }
        [DllImport("dbghelp.dll", SetLastError=true)] [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool MiniDumpWriteDump(IntPtr process, uint pid, IntPtr file, uint flags, IntPtr exception, IntPtr streams, ref CallbackInfo callback);

        public static long Identity(int pid) {
            using(var p=Process.GetProcessById(pid)) { return p.StartTime.ToUniversalTime().ToFileTimeUtc(); }
        }
        public static bool HasRequiredStreams(string file) {
            using(var s=File.OpenRead(file)) using(var b=new BinaryReader(s)) {
                if(s.Length<32 || b.ReadUInt32()!=0x504d444d) return false;
                b.ReadUInt32(); uint count=b.ReadUInt32(), offset=b.ReadUInt32();
                if(count>256 || (long)offset+count*12>s.Length) return false;
                bool threads=false, modules=false, system=false;
                for(uint i=0;i<count;i++) {
                    s.Position=offset+i*12; uint kind=b.ReadUInt32(), size=b.ReadUInt32(), pos=b.ReadUInt32();
                    if((long)pos+size>s.Length) return false;
                    if(kind==3 && size>4) threads=true;
                    if(kind==4 && size>4) modules=true;
                    if(kind==7 && size>0) system=true;
                }
                return threads && modules && system;
            }
        }
        public static void Write(uint pid, long expectedCreation, string expectedName, string destination) {
            if(!Environment.Is64BitProcess) throw new InvalidOperationException("64-bit collector required");
            if(expectedName!="FengYin" && expectedName!="FengYinDJ" && expectedName!="msedgewebview2")
                throw new InvalidOperationException("Process is outside FengYin scope");
            // Also guard a still-running dump worker after the orchestrator timed out.
            using(var gate=new Mutex(false,"Local\\FengYinHangTarget-"+pid+"-"+expectedCreation)) {
            bool acquired=false;
            try {
                try {acquired=gate.WaitOne(0);} catch(AbandonedMutexException) {acquired=true;}
                if(!acquired) throw new InvalidOperationException("This target already has a diagnostic capture running");
            // Query + VM_READ + DUP_HANDLE, never PROCESS_TERMINATE/VM_WRITE/CREATE_THREAD.
            IntPtr process=OpenProcess(0x450,false,pid);
            if(process==IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
            string partial=destination+".partial";
            try {
                long created,exited,kernel,user;
                if(!GetProcessTimes(process,out created,out exited,out kernel,out user)) throw new Win32Exception(Marshal.GetLastWin32Error());
                var image=new StringBuilder(32768); int capacity=image.Capacity;
                if(!QueryFullProcessImageName(process,0,image,ref capacity)) throw new Win32Exception(Marshal.GetLastWin32Error());
                if(created!=expectedCreation || !String.Equals(Path.GetFileNameWithoutExtension(image.ToString()),expectedName,StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException("Target identity changed; capture refused");
                using(var file=new FileStream(partial,FileMode.CreateNew,FileAccess.ReadWrite,FileShare.Read)) {
                    var clock=Stopwatch.StartNew();
                    int typeOffset=(int)Marshal.OffsetOf(typeof(CallbackHeader),"Type");
                    Callback cb=(param,input,output)=> {
                        try {
                            if(Marshal.ReadInt32(input,typeOffset)==6) {
                                Marshal.WriteInt32(output,0,1);
                                Marshal.WriteInt32(output,4,(clock.Elapsed.TotalSeconds>30 || file.Length>64L*1024*1024)?1:0);
                            }
                            return true;
                        } catch { return false; }
                    };
                    var info=new CallbackInfo {Function=cb,Parameter=IntPtr.Zero};
                    // Compact stacks + handles + unloaded modules + process/thread data +
                    // memory-region metadata + thread info. No FullMemory/private-heap/token dump.
                    bool ok=MiniDumpWriteDump(process,pid,file.SafeFileHandle.DangerousGetHandle(),0x1924,IntPtr.Zero,IntPtr.Zero,ref info);
                    int error=Marshal.GetLastWin32Error(); GC.KeepAlive(cb);
                    if(!ok) throw new Win32Exception(error,"MiniDumpWriteDump failed/cancelled: "+error);
                    file.Flush();
                }
                if(!HasRequiredStreams(partial)) throw new InvalidDataException("Missing dump thread/module/system streams");
                File.Move(partial,destination);
            } finally {CloseHandle(process);}
            } finally {if(acquired) gate.ReleaseMutex();}
            }
        }
    }
}
