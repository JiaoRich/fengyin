using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class BleCaptureMidi {
 [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
 public struct Caps { public ushort Manufacturer,Product;public uint DriverVersion;
 [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string Name; public uint Support; }
 [DllImport("winmm.dll")] static extern uint midiInGetNumDevs();
 [DllImport("winmm.dll",CharSet=CharSet.Unicode,EntryPoint="midiInGetDevCapsW")]
 static extern uint midiInGetDevCaps(UIntPtr id,out Caps caps,uint size);
 public static string[] Read(){var rows=new List<string>();var count=midiInGetNumDevs();rows.Add("Count="+count);
 for(uint i=0;i<Math.Min(count,128);i++){Caps c;var e=midiInGetDevCaps(new UIntPtr(i),out c,(uint)Marshal.SizeOf(typeof(Caps)));rows.Add(i+" | "+e+" | "+c.Name+" | "+c.Manufacturer+" | "+c.Product);}
 return rows.ToArray();}
}
