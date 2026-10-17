"""Approved +5 dB absolute trim; only violin mainVolume curves may change."""
from pathlib import Path
import hashlib,json,math,re,struct,zipfile
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'assets/tone-packages'
BASELINE=ROOT/'assets/incoming-tones/20261017/swam-before-gain-119.zip'
AUDIT=ASSETS/'swam-gain-curve-119-audit.json'
GAIN=10**(5/20)

def sha(data):return hashlib.sha256(data).hexdigest()
def decode(s):
    n,body=s.split('.',1);alphabet='.ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+'
    v=bits=0;out=bytearray()
    for c in body:
        v|=alphabet.index(c)<<bits;bits+=6
        while bits>=8:out.append(v&255);v>>=8;bits-=8
    return bytes(out[:int(n)])
def encode(data):
    alphabet='.ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+'
    v=bits=0;out=[]
    for b in data:
        v|=b<<bits;bits+=8
        while bits>=6:out.append(alphabet[v&63]);v>>=6;bits-=6
    if bits:out.append(alphabet[v&63])
    return str(len(data))+'.'+''.join(out)
def xml_bytes(block):
    assert block[:4]==b'VC2!'
    n=struct.unpack_from('<I',block,4)[0]
    return block[8:8+n]
def replace_xml(block,xml):
    n=struct.unpack_from('<I',block,4)[0]
    return block[:4]+struct.pack('<I',len(xml))+xml+block[8+n:]
def mainvolume_curve(state):
    outer=ET.fromstring(xml_bytes(decode(state)).rstrip(b'\0'))
    body=decode(outer.findtext('IComponent'))
    assert body[:4]==b'VstW' and body[16:20]==b'CcnK' and body[24:28]==b'FBCh'
    inner=ET.fromstring(xml_bytes(body[176:]).rstrip(b'\0'))
    matches=[e.find('MIDIRemappingCurve') for e in inner.iter('MIDIRemappingEntry') if e.get('parameterId')=='mainVolume']
    assert len(matches)==1 and matches[0] is not None
    return matches[0],inner
def patch_curve(old_state,new_state):
    source,_=mainvolume_curve(new_state)
    oldcurve,oldxml=mainvolume_curve(old_state)
    outer=decode(old_state);ox=xml_bytes(outer)
    root=ET.fromstring(ox.rstrip(b'\0'));component=root.findtext('IComponent')
    body=decode(component);chunk=body[176:];ix=xml_bytes(chunk)
    pattern=rb'(<MIDIRemappingEntry\b[^>]*\bparameterId="mainVolume"[^>]*>\s*)(<MIDIRemappingCurve\b[^>]*/>)'
    replacement=ET.tostring(source,encoding='utf-8').strip()
    changed,count=re.subn(pattern,lambda m:m.group(1)+replacement,ix)
    assert count==1
    if oldcurve.attrib==source.attrib:return old_state
    newchunk=replace_xml(chunk,changed)
    newbody=bytearray(body[:176]+newchunk)
    struct.pack_into('>I',newbody,20,len(newbody)-24)
    struct.pack_into('>I',newbody,172,len(newchunk))
    text=encode(newbody)
    assert ox.count(component.encode())==1
    result=encode(replace_xml(outer,ox.replace(component.encode(),text.encode())))
    curve,after=mainvolume_curve(result)
    target=next(e for e in oldxml.iter('MIDIRemappingEntry') if e.get('parameterId')=='mainVolume')
    target.remove(oldcurve);target.append(ET.fromstring(replacement))
    assert ET.tostring(oldxml)==ET.tostring(after), 'Unexpected non-volume-curve change'
    return result

def main():
    if AUDIT.exists():
        audit=json.loads(AUDIT.read_text(encoding='utf-8'))
        assert all(sha((ASSETS/r['file']).read_bytes())==r['afterSha256'] for r in audit['targets'])
        print('Already applied; verified no cumulative gain.');return
    catalog=(ROOT/'native/Source/PublishedToneCatalog.h').read_text(encoding='utf-8')
    names=re.findall(r'PublishedToneKind::swamPackage,\s*"([^"]+)"',catalog)
    assert len(names)==19
    sources={'violin-soft.fytonepack':Path('/Volumes/KINGSTON/新建文件夹/小提琴独奏-小提琴-柔美改音量.fytonepack'),
             'violin-bright.fytonepack':Path('/Volumes/KINGSTON/新建文件夹/小提琴独奏-小提琴-明亮该音量.fytonepack')}
    prepared=[]
    for name in names:
        path=ASSETS/name
        with zipfile.ZipFile(path) as z:infos=z.infolist();data={i.filename:z.read(i) for i in infos};assert z.testzip() is None
        before=data['tone.xml'];xml=ET.fromstring(before);preset=xml.find('PRESET')
        oldgain=preset.get('outputGain');changed,n=re.subn(rb'\boutputGain="[^"]*"',f'outputGain="{GAIN:.17g}"'.encode(),before);assert n==1
        curve_changed=False
        if name in sources:
            with zipfile.ZipFile(sources[name]) as z:source=ET.fromstring(z.read('tone.xml')).find('PRESET')
            # All supplied violin Attack states are Expression; never copy attack or other parameters.
            _,sx=mainvolume_curve(source.findtext('INSTRUMENT_STATE'))
            assert next(p.get('value') for p in sx.iter('PARAM') if p.get('id')=='attackType')=='2.0'
            oldstate=preset.findtext('INSTRUMENT_STATE');newstate=patch_curve(oldstate,source.findtext('INSTRUMENT_STATE'))
            curve_changed=oldstate!=newstate
            assert changed.count(oldstate.encode())==1
            changed=changed.replace(oldstate.encode(),newstate.encode())
        manifest=json.loads(data['manifest.json'])
        for row in manifest['files']:assert sha(data[row['path']])==row['sha256']
        data['tone.xml']=changed
        for row in manifest['files']:row.update(bytes=len(data[row['path']]),sha256=sha(data[row['path']]))
        data['manifest.json']=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()
        prepared.append((path,infos,data,dict(file=name,beforeSha256=sha(path.read_bytes()),oldOutputGain=oldgain,newOutputGain=GAIN,mainVolumeCurveChanged=curve_changed)))
    BASELINE.parent.mkdir(parents=True,exist_ok=True)
    assert not BASELINE.exists()
    with zipfile.ZipFile(BASELINE,'w',zipfile.ZIP_DEFLATED) as z:
        for path,*_ in prepared:z.writestr(path.name,path.read_bytes())
    for path,infos,data,row in prepared:
        with zipfile.ZipFile(path.with_suffix('.tmp'),'w') as z:
            for info in infos:z.writestr(info,data[info.filename])
        path.with_suffix('.tmp').replace(path);row['afterSha256']=sha(path.read_bytes())
    AUDIT.write_text(json.dumps(dict(db=5,absoluteNotAdditional=True,baseline=str(BASELINE.relative_to(ROOT)),targets=[r for *_,r in prepared]),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Applied +5 dB to 19 SWAM packages; only soft violin curve required a change; tremolo untouched.')

if __name__=='__main__':main()
