// Read-only Win32 state. No injection, hooks, messages, thread suspension or focus changes.
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace FengYin.StallCapture {
    public static class Windows {
        [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
        [StructLayout(LayoutKind.Sequential)] struct Point { public int X, Y; }
        [StructLayout(LayoutKind.Sequential)] struct WindowInfo {
            public uint Size; public Rect Window, Client;
            public uint Style, ExStyle, Status, BorderX, BorderY;
            public ushort Atom, CreatorVersion;
        }
        [StructLayout(LayoutKind.Sequential)] struct GuiInfo {
            public uint Size, Flags;
            public IntPtr Active, Focus, Capture, MenuOwner, MoveSize, Caret;
            public Rect CaretRect;
        }
        delegate bool EnumProc(IntPtr window, IntPtr arg);
        [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr arg);
        [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr arg);
        [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
        [DllImport("user32.dll")] static extern bool GetWindowInfo(IntPtr window, ref WindowInfo info);
        [DllImport("user32.dll")] static extern bool GetGUIThreadInfo(uint thread, ref GuiInfo info);
        [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder text, int count);
        [DllImport("user32.dll")] static extern IntPtr GetParent(IntPtr window);
        [DllImport("user32.dll")] static extern bool IsWindowEnabled(IntPtr window);
        [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr window);
        [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] static extern bool GetCursorPos(out Point point);
        [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point point);
        [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr window, uint flags);
        public sealed class WindowRow {
            public string Handle, Parent, Class, Style, ExStyle;
            public uint PID, TID; public bool Visible, Enabled, InfoRead;
            public Rect Bounds, Client;
        }
        public sealed class ThreadRow {
            public uint TID, Flags; public bool Read;
            public string Active, Focus, Capture, MenuOwner, MoveSize;
        }
        public sealed class HitRow { public int X, Y; public WindowRow Window; }
        public sealed class Snapshot {
            public string Time; public uint TargetPID;
            public bool Truncated;
            public WindowRow Foreground, CursorHit;
            public List<WindowRow> Items = new List<WindowRow>();
            public List<ThreadRow> Threads = new List<ThreadRow>();
            public List<HitRow> Grid = new List<HitRow>();
        }
        static string Hex(IntPtr p) { return "0x" + p.ToInt64().ToString("x"); }
        static WindowRow Read(IntPtr w) {
            uint pid; uint tid=GetWindowThreadProcessId(w,out pid);
            var name=new StringBuilder(128); GetClassName(w,name,name.Capacity);
            var info=new WindowInfo { Size=(uint)Marshal.SizeOf(typeof(WindowInfo)) };
            bool ok=GetWindowInfo(w,ref info);
            return new WindowRow { Handle=Hex(w), Parent=Hex(GetParent(w)), PID=pid,TID=tid,
                Class=name.ToString(), Visible=IsWindowVisible(w),Enabled=IsWindowEnabled(w),InfoRead=ok,
                Style="0x"+info.Style.ToString("x"),ExStyle="0x"+info.ExStyle.ToString("x"),
                Bounds=info.Window,Client=info.Client };
        }
        public static Snapshot Take(uint target) {
            var result=new Snapshot { Time=DateTimeOffset.Now.ToString("o"),TargetPID=target };
            var ids=new HashSet<long>(); var tids=new HashSet<uint>();
            Action<IntPtr> add=w=> {
                if(!ids.Add(w.ToInt64()))return;
                if(result.Items.Count>=512){result.Truncated=true;return;}
                var row=Read(w);result.Items.Add(row);tids.Add(row.TID);
            };
            EnumProc children=(w,arg)=>{add(w);return result.Items.Count<512;};
            EnumProc roots=(w,arg)=> {
                uint pid;GetWindowThreadProcessId(w,out pid);
                if(pid==target){add(w);EnumChildWindows(w,children,IntPtr.Zero);}
                return true;
            };
            EnumWindows(roots,IntPtr.Zero);
            foreach(uint tid in tids) {
                var gui=new GuiInfo { Size=(uint)Marshal.SizeOf(typeof(GuiInfo)) };
                bool ok=GetGUIThreadInfo(tid,ref gui);
                result.Threads.Add(new ThreadRow {TID=tid,Read=ok,Flags=gui.Flags,
                    Active=Hex(gui.Active),Focus=Hex(gui.Focus),Capture=Hex(gui.Capture),
                    MenuOwner=Hex(gui.MenuOwner),MoveSize=Hex(gui.MoveSize)});
            }
            var foreground=GetForegroundWindow();result.Foreground=Read(foreground);
            Point cursor;if(GetCursorPos(out cursor))result.CursorHit=Read(WindowFromPoint(cursor));
            // Fixed normalized sample points, not injected input; no customer page text is read.
            var root=GetAncestor(foreground,2);uint owner;GetWindowThreadProcessId(root,out owner);
            var rootInfo=Read(root);
            if(owner==target && rootInfo.InfoRead) {
                var r=rootInfo.Client;
                foreach(int x in new[]{8,35,65,92})foreach(int y in new[]{8,30,60,90}) {
                    var point=new Point {X=r.Left+(r.Right-r.Left)*x/100,Y=r.Top+(r.Bottom-r.Top)*y/100};
                    result.Grid.Add(new HitRow{X=point.X,Y=point.Y,Window=Read(WindowFromPoint(point))});
                }
            }
            GC.KeepAlive(children);GC.KeepAlive(roots);
            return result;
        }
    }
}
