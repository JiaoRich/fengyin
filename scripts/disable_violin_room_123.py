"""User-approved internal room switch only; retain all other preset/state bytes."""
from pathlib import Path
import hashlib, json, re, struct, zipfile
import xml.etree.ElementTree as ET
from apply_swam_gain_curve_119 import decode, encode, xml_bytes, replace_xml, mainvolume_curve

ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'assets/tone-packages'
AUDIT=ASSETS/'violin-room-123-audit.json'
BASELINE=ROOT/'assets/incoming-tones/20261018/violin-room-before-123.zip'

def patch_room(state, value='0.0'):
    outer=decode(state);ox=xml_bytes(outer)
    component=ET.fromstring(ox.rstrip(b'\0')).findtext('IComponent')
    body=decode(component);chunk=body[176:];ix=xml_bytes(chunk)
    changed,count=re.subn(rb'(<PARAM\b[^>]*\bid="roomEnabled"\s+value=")[^"]+("\s*/>)',
                         lambda m:m[1]+value.encode()+m[2],ix)
    assert count==1, 'Room PARAM id must be unambiguous'
    if changed==ix:return state
    newchunk=replace_xml(chunk,changed)
    newbody=bytearray(body[:176]+newchunk)
    struct.pack_into('>I',newbody,20,len(newbody)-24)
    struct.pack_into('>I',newbody,172,len(newchunk))
    return encode(replace_xml(outer,ox.replace(component.encode(),encode(newbody).encode())))

def main():
    if AUDIT.exists():
        audit=json.loads(AUDIT.read_text())
        assert all(hashlib.sha256((ASSETS/r['file']).read_bytes()).hexdigest()==r['afterSha256'] for r in audit['targets'])
        print('Violin room already disabled; no additional changes.');return
    names=['violin-soft.fytonepack','violin-bright.fytonepack']
    prepared=[]
    for name in names:
        path=ASSETS/name
        original=path.read_bytes()
        with zipfile.ZipFile(path) as z:infos=z.infolist();data={i.filename:z.read(i) for i in infos}
        xml=ET.fromstring(data['tone.xml']);p=xml.find('PRESET');state=p.findtext('INSTRUMENT_STATE')
        _,before=mainvolume_curve(state)
        assert next(x.get('value') for x in before.iter('PARAM') if x.get('id')=='attackType')=='2.0'
        old=next(x for x in before.iter('PARAM') if x.get('id')=='roomEnabled')
        oldvalue=old.get('value');newstate=patch_room(state)
        _,after=mainvolume_curve(newstate)
        old.set('value','0.0');assert ET.tostring(before)==ET.tostring(after)
        changed=newstate!=state
        if changed:
            assert data['tone.xml'].count(state.encode())==1
            data['tone.xml']=data['tone.xml'].replace(state.encode(),newstate.encode())
            manifest=json.loads(data['manifest.json'])
            for entry in manifest['files']:
                entry.update(bytes=len(data[entry['path']]),sha256=hashlib.sha256(data[entry['path']]).hexdigest())
            data['manifest.json']=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()
        prepared.append((path,original,infos,data,dict(file=name,changed=changed,roomBefore=oldvalue,roomAfter='0.0',
            beforeSha256=hashlib.sha256(original).hexdigest())))
    BASELINE.parent.mkdir(parents=True,exist_ok=True)
    assert not BASELINE.exists()
    with zipfile.ZipFile(BASELINE,'w',zipfile.ZIP_DEFLATED) as z:
        for path,original,*_ in prepared:z.writestr(path.name,original)
    for path,original,infos,data,row in prepared:
        if row['changed']:
            with zipfile.ZipFile(path,'w') as z:
                for info in infos:z.writestr(info,data[info.filename])
        row['afterSha256']=hashlib.sha256(path.read_bytes()).hexdigest()
    AUDIT.write_text(json.dumps(dict(baseline=str(BASELINE.relative_to(ROOT)),targets=[r for *_,r in prepared]),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Room disabled on soft violin; bright already off. ReverbMix, effects, curves, tremolo and snapshots preserved.')

if __name__=='__main__':main()
