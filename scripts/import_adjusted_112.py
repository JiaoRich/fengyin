"""Import the sixteen approved packages; retain all state/curves and audit changes."""
import argparse, hashlib, io, json, re, zipfile
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
MULTI={'次中萨-醇厚-调整后':'tenor-mellow','次中萨-温暖-已调整':'tenor-warm',
       '次中萨-气包音-已调':'tenor-air-pocket','中萨-爵士-调整后':'alto-jazz',
       '中萨-流行-调整后':'alto-pop','中萨-深情-调整后':'alto-deep'}
SINGLE={'cello','violin','clarinet','bass-trombone','piccolo','piccolo-trumpet','oboe','trumpet','flute','alto-trombone'}

def main():
    parser=argparse.ArgumentParser();parser.add_argument('archive',type=Path)
    parser.add_argument('--expression-approved',action='store_true',required=True)
    args=parser.parse_args()
    spec=json.loads((ROOT/'交付包/风吟-全部37套音色方案-1.1.9-20261012/验证资料/export-specifications.json').read_text(encoding='utf-8'))
    audit=[];seen=set()
    with zipfile.ZipFile(args.archive) as source:
        assert source.testzip() is None
        for entry in source.infolist():
            if entry.is_dir():continue
            with zipfile.ZipFile(io.BytesIO(source.read(entry))) as package:
                assert package.testzip() is None
                records={i.filename:package.read(i) for i in package.infolist()}
            manifest=json.loads(records['manifest.json'])
            for record in manifest['files']:
                data=records[record['path']]
                assert len(data)==record['bytes'] and hashlib.sha256(data).hexdigest()==record['sha256']
            before=records['tone.xml'];preset=ET.fromstring(before).find('PRESET')
            key=preset.get('instrumentKey');name=preset.get('name')
            ident=MULTI.get(name,key if key in SINGLE else None)
            assert ident and ident not in seen,(name,key)
            seen.add(ident)
            attack=next(p for p in preset.iter('TONE_PARAMETER') if p.get('id')==spec[ident]['attackId'])
            expected=spec[ident]['attackValue'];value=float(attack.get('value'))
            changed=ident=='tenor-air-pocket' and abs(value-expected)>1e-6
            if changed:
                assert value==0.75 and expected==1.0
                after,count=re.subn(rb'(<TONE_PARAMETER\s+id="index:78:attackcontrol"\s+value=")[^"]+("/>)',rb'\g<1>1.0\2',before)
                assert count==1
            else:
                assert abs(value-expected)<1e-6,(name,value,expected)
                after=before
            records['tone.xml']=after
            for record in manifest['files']:
                if record['path']=='tone.xml':
                    record.update(bytes=len(after),sha256=hashlib.sha256(after).hexdigest())
            records['manifest.json']=json.dumps(manifest,ensure_ascii=False,indent=2).encode()
            destination=ROOT/'assets/tone-packages'/f'{ident}.fytonepack'
            with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED) as result:
                for path,data in records.items():result.writestr(path,data)
            audit.append({'id':ident,'sourceName':name,'instrumentKey':key,'attackExpression':expected,
                          'attackChangedWithApproval':changed,'opaqueStateUnchanged':True,
                          'toneXmlSha256':hashlib.sha256(after).hexdigest()})
    assert len(seen)==16
    (ROOT/'assets/tone-packages/adjusted-112-audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2))
    print('Imported 16 packages; one explicitly approved Attack override; opaque state and curves preserved')

if __name__=='__main__':main()
