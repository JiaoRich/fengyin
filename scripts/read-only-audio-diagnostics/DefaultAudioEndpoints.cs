using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

// Read-only Core Audio calls: no endpoint activation or policy writes.
public static class DefaultAudioEndpoints
{
    [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
    private class EnumeratorClass { }
    [ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface Enumerator
    {
        [PreserveSig] int EnumAudioEndpoints(int flow, uint mask, out IntPtr devices);
        [PreserveSig] int GetDefaultAudioEndpoint(int flow, int role, out Device device);
    }
    [ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface Device
    {
        [PreserveSig] int Activate(ref Guid iid, uint context, IntPtr parameters, out IntPtr result);
        [PreserveSig] int OpenPropertyStore(uint access, out IntPtr properties);
        [PreserveSig] int GetId([MarshalAs(UnmanagedType.LPWStr)] out string id);
        [PreserveSig] int GetState(out uint state);
    }
    public static string[] Read()
    {
        var rows = new List<string>();
        var enumerator = (Enumerator)new EnumeratorClass();
        try
        {
            for (int flow = 0; flow < 2; flow++)
                for (int role = 0; role < 3; role++)
                {
                    Device device;
                    int hr = enumerator.GetDefaultAudioEndpoint(flow, role, out device);
                    string prefix = (flow == 0 ? "Render" : "Capture") + "/" + new [] { "Console", "Multimedia", "Communications" }[role];
                    if (hr < 0) { rows.Add(prefix + " HRESULT=0x" + hr.ToString("X8")); continue; }
                    try
                    {
                        string id; uint state;
                        Marshal.ThrowExceptionForHR(device.GetId(out id));
                        Marshal.ThrowExceptionForHR(device.GetState(out state));
                        rows.Add(prefix + " id=" + id + " state=" + state);
                    }
                    finally { Marshal.ReleaseComObject(device); }
                }
        }
        finally { Marshal.ReleaseComObject(enumerator); }
        return rows.ToArray();
    }
}
