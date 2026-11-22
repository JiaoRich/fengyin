using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

// Core Audio metadata only: no audio client/stream, playback, setters or notifications.
// COM slot declarations include inherited setters, but no setter is ever called.
public static class FengYinAudioSnapshot {
    [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")] class EnumeratorClass {}
    [ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface Enumerator {
        [PreserveSig] int EnumAudioEndpoints(int flow,uint mask,out Collection devices);
        [PreserveSig] int GetDefaultAudioEndpoint(int flow,int role,out Device device);
    }
    [ComImport, Guid("0BD7A1BE-7A1A-44DB-8397-C0A2D2C223B1"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface Collection {
        [PreserveSig] int GetCount(out uint count);
        [PreserveSig] int Item(uint index,out Device device);
    }
    [ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface Device {
        [PreserveSig] int Activate(ref Guid iid,uint context,IntPtr parameters,[MarshalAs(UnmanagedType.IUnknown)] out object result);
        [PreserveSig] int OpenPropertyStore(uint access,out IntPtr properties);
        [PreserveSig] int GetId([MarshalAs(UnmanagedType.LPWStr)] out string id);
        [PreserveSig] int GetState(out uint state);
    }
    [ComImport, Guid("77AA99A0-1BD6-484F-8BC7-2C654C9A9B6F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface Manager {
        [PreserveSig] int GetAudioSessionControl(ref Guid session,uint flags,out IntPtr control);
        [PreserveSig] int GetSimpleAudioVolume(ref Guid session,uint flags,out IntPtr volume);
        [PreserveSig] int GetSessionEnumerator(out SessionList list);
    }
    [ComImport, Guid("E2F5BB11-0570-40CA-ACDD-3AA01277DEE8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface SessionList {
        [PreserveSig] int GetCount(out int count);
        [PreserveSig] int GetSession(int index,[MarshalAs(UnmanagedType.IUnknown)] out object session);
    }
    [ComImport, Guid("BFB7FF88-7239-4FC9-8FA2-07C950BE9C6D"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface Session {
        [PreserveSig] int GetState(out int state);
        [PreserveSig] int GetDisplayName([MarshalAs(UnmanagedType.LPWStr)] out string name);
        [PreserveSig] int SetDisplayName([MarshalAs(UnmanagedType.LPWStr)] string name,IntPtr context);
        [PreserveSig] int GetIconPath([MarshalAs(UnmanagedType.LPWStr)] out string path);
        [PreserveSig] int SetIconPath([MarshalAs(UnmanagedType.LPWStr)] string path,IntPtr context);
        [PreserveSig] int GetGroupingParam(out Guid grouping);
        [PreserveSig] int SetGroupingParam(ref Guid grouping,IntPtr context);
        [PreserveSig] int RegisterAudioSessionNotification(IntPtr notification);
        [PreserveSig] int UnregisterAudioSessionNotification(IntPtr notification);
        [PreserveSig] int GetSessionIdentifier([MarshalAs(UnmanagedType.LPWStr)] out string identifier);
        [PreserveSig] int GetSessionInstanceIdentifier([MarshalAs(UnmanagedType.LPWStr)] out string identifier);
        [PreserveSig] int GetProcessId(out uint pid);
    }
    [ComImport, Guid("87CE5498-68D6-44E5-9215-6DA47EF883D8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface SessionVolume {
        [PreserveSig] int SetMasterVolume(float volume,IntPtr context);
        [PreserveSig] int GetMasterVolume(out float volume);
        [PreserveSig] int SetMute([MarshalAs(UnmanagedType.Bool)] bool mute,IntPtr context);
        [PreserveSig] int GetMute([MarshalAs(UnmanagedType.Bool)] out bool mute);
    }
    [ComImport, Guid("5CDF2C82-841E-4546-9722-0CF74078229A"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface EndpointVolume {
        [PreserveSig] int RegisterControlChangeNotify(IntPtr callback);
        [PreserveSig] int UnregisterControlChangeNotify(IntPtr callback);
        [PreserveSig] int GetChannelCount(out uint count);
        [PreserveSig] int SetMasterVolumeLevel(float db,IntPtr context);
        [PreserveSig] int SetMasterVolumeLevelScalar(float volume,IntPtr context);
        [PreserveSig] int GetMasterVolumeLevel(out float db);
        [PreserveSig] int GetMasterVolumeLevelScalar(out float volume);
        [PreserveSig] int SetChannelVolumeLevel(uint channel,float db,IntPtr context);
        [PreserveSig] int SetChannelVolumeLevelScalar(uint channel,float volume,IntPtr context);
        [PreserveSig] int GetChannelVolumeLevel(uint channel,out float db);
        [PreserveSig] int GetChannelVolumeLevelScalar(uint channel,out float volume);
        [PreserveSig] int SetMute([MarshalAs(UnmanagedType.Bool)] bool mute,IntPtr context);
        [PreserveSig] int GetMute([MarshalAs(UnmanagedType.Bool)] out bool mute);
    }
    [ComImport, Guid("C02216F6-8C67-4B5B-9D00-D008E73E0064"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)] interface Meter {
        [PreserveSig] int GetPeakValue(out float peak);
    }
    public sealed class SessionRow {public uint PID;public int State;public float? Volume,Peak;public bool? Mute;public string Error;}
    public sealed class EndpointRow {public string Id,Error;public uint State;public float? Volume,Peak;public bool? Mute;public List<SessionRow> Sessions=new List<SessionRow>();}
    public sealed class Result {
        public string Time=DateTimeOffset.Now.ToString("o"),Error;
        public string MeterLimit="Instant snapshot; zero alone does not prove silence. ASIO/exclusive streams may not expose a software meter/session.";
        public List<string> Defaults=new List<string>();public List<EndpointRow> Endpoints=new List<EndpointRow>();
    }
    static void Release(object value){if(value!=null && Marshal.IsComObject(value))Marshal.ReleaseComObject(value);}
    static object Activate(Device d,Type type) {object obj;Guid id=type.GUID;Marshal.ThrowExceptionForHR(d.Activate(ref id,23,IntPtr.Zero,out obj));return obj;}
    static float? Peak(object obj){try{float value;return ((Meter)obj).GetPeakValue(out value)>=0?(float?)value:null;}catch{return null;}}
    public static Result Read(uint[] targetIds) {
        var result=new Result();Enumerator enumerator=null;Collection devices=null;
        var targets=new HashSet<uint>(targetIds??new uint[0]);
        try {
            enumerator=(Enumerator)new EnumeratorClass();
            for(int role=0;role<3;role++) {
                Device d=null;
                try {int hr=enumerator.GetDefaultAudioEndpoint(0,role,out d);string id=null;if(hr>=0)hr=d.GetId(out id);
                    result.Defaults.Add(new[]{"Console","Multimedia","Communications"}[role]+" id="+id+" HRESULT=0x"+hr.ToString("X8"));}
                finally{Release(d);}
            }
            Marshal.ThrowExceptionForHR(enumerator.EnumAudioEndpoints(0,15,out devices));uint n;Marshal.ThrowExceptionForHR(devices.GetCount(out n));
            for(uint i=0;i<Math.Min(n,64);i++) {
                Device d=null;var row=new EndpointRow();result.Endpoints.Add(row);
                try {
                    Marshal.ThrowExceptionForHR(devices.Item(i,out d));Marshal.ThrowExceptionForHR(d.GetId(out row.Id));Marshal.ThrowExceptionForHR(d.GetState(out row.State));
                    if(row.State!=1)continue;
                    object vol=null,meter=null,manager=null;SessionList sessions=null;
                    try {
                        try {vol=Activate(d,typeof(EndpointVolume));var v=(EndpointVolume)vol;float level;bool mute;
                            if(v.GetMasterVolumeLevelScalar(out level)>=0)row.Volume=level;if(v.GetMute(out mute)>=0)row.Mute=mute;}
                        catch(Exception e){row.Error="Endpoint volume: "+e.Message;}
                        try {meter=Activate(d,typeof(Meter));row.Peak=Peak(meter);}catch{}
                        manager=Activate(d,typeof(Manager));Marshal.ThrowExceptionForHR(((Manager)manager).GetSessionEnumerator(out sessions));
                        int count;Marshal.ThrowExceptionForHR(sessions.GetCount(out count));
                        for(int j=0;j<Math.Min(count,256);j++) {
                            object control=null;
                            try {
                                Marshal.ThrowExceptionForHR(sessions.GetSession(j,out control));var s=(Session)control;uint pid;
                                Marshal.ThrowExceptionForHR(s.GetProcessId(out pid));if(!targets.Contains(pid))continue;
                                var sr=new SessionRow{PID=pid};row.Sessions.Add(sr);Marshal.ThrowExceptionForHR(s.GetState(out sr.State));
                                try {var v=(SessionVolume)control;float level;bool mute;if(v.GetMasterVolume(out level)>=0)sr.Volume=level;if(v.GetMute(out mute)>=0)sr.Mute=mute;}catch(Exception e){sr.Error=e.Message;}
                                sr.Peak=Peak(control);
                            } catch(Exception e){row.Error="Session enumeration: "+e.Message;}finally{Release(control);}
                        }
                    } finally{Release(sessions);Release(manager);Release(meter);Release(vol);}
                }catch(Exception e){row.Error=e.Message;}finally{Release(d);}
            }
        }catch(Exception e){result.Error=e.Message;}finally{Release(devices);Release(enumerator);}
        return result;
    }
}
