"""Approved Qin native Gain=6 and per-slot FXAUX=50, no host-level boost."""
from pathlib import Path
import hashlib,json,re,subprocess,tempfile,zipfile
import xml.etree.ElementTree as ET
from apply_swam_gain_curve_119 import decode,encode

ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'assets/tone-packages'
BASELINE=ROOT/'assets/incoming-tones/20261017/folk-before-engine-levels-119.zip'
AUDIT=ASSETS/'folk-engine-levels-119-audit.json'
def sha(data):return hashlib.sha256(data).hexdigest()
def main():
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('--tool',type=Path,required=True);args=parser.parse_args()
    if AUDIT.exists():
        audit=json.loads(AUDIT.read_text(encoding='utf-8'))
        assert all(sha((ASSETS/r['file']).read_bytes())==r['afterSha256'] for r in audit['targets'])
        # Keep the external native KAM entry exactly identical to the XML
        # KAM_PROJECT. The loader deliberately rejects inconsistent copies.
        for row in audit['targets']:
            path=ASSETS/row['file']
            with zipfile.ZipFile(path) as z:
                infos=z.infolist();data={i.filename:z.read(i) for i in infos}
            kam=decode(ET.fromstring(data['tone.xml']).findtext('PRESET/KAM_PROJECT'))
            if data['instrument.KAM']==kam:continue
            data['instrument.KAM']=kam;manifest=json.loads(data['manifest.json'])
            for item in manifest['files']:item.update(bytes=len(data[item['path']]),sha256=sha(data[item['path']]))
            data['manifest.json']=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()
            temporary=path.with_suffix('.119-tmp')
            with zipfile.ZipFile(temporary,'w') as z:
                for i in infos:z.writestr(i,data[i.filename])
            temporary.replace(path);row['afterSha256']=sha(path.read_bytes())
        AUDIT.write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print('Native Qin levels already applied and verified.');return
    names=re.findall(r'PublishedToneKind::kongPackage,\s*"([^"]+)"',(ROOT/'native/Source/PublishedToneCatalog.h').read_text(encoding='utf-8'))
    assert len(names)==13
    prepared=[]
    with tempfile.TemporaryDirectory(prefix='fy119-folk-') as work:
        work=Path(work)
        for name in names:
            path=ASSETS/name
            with zipfile.ZipFile(path) as z:
                assert z.testzip() is None
                infos=z.infolist();data={i.filename:z.read(i) for i in infos}
            before=data['tone.xml'];p=ET.fromstring(before).find('PRESET');changed=before;logs={}
            for tag in ['SAMPLER_STATE','KAM_PROJECT']:
                old=p.findtext(tag);assert old
                src=work/(tag+'.bin');dest=work/(tag+'-new.bin');src.write_bytes(decode(old))
                result=subprocess.run([str(args.tool),str(src),str(dest)],capture_output=True,text=True,encoding='utf-8',check=True)
                new=encode(dest.read_bytes());assert changed.count(old.encode())==1
                changed=changed.replace(old.encode(),new.encode());logs[tag]=result.stdout.strip()
                if tag=='KAM_PROJECT':data['instrument.KAM']=dest.read_bytes()
            verify=ET.fromstring(changed).find('PRESET')
            for tag in ['SAMPLER_STATE','KAM_PROJECT']:verify.find(tag).text=p.findtext(tag)
            assert ET.tostring(verify)==ET.tostring(p),'Unexpected host parameter or technique change'
            manifest=json.loads(data['manifest.json'])
            for item in manifest['files']:
                if item['path']!='instrument.KAM':assert sha(data[item['path']])==item['sha256']
            data['tone.xml']=changed
            for item in manifest['files']:item.update(bytes=len(data[item['path']]),sha256=sha(data[item['path']]))
            data['manifest.json']=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()
            prepared.append((path,infos,data,dict(file=name,beforeSha256=sha(path.read_bytes()),nativeVerification=logs,hostOutputGainUnchanged=p.get('outputGain'))))
    assert not BASELINE.exists();BASELINE.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(BASELINE,'w',zipfile.ZIP_DEFLATED) as z:
        for path,*_ in prepared:z.writestr(path.name,path.read_bytes())
    for path,infos,data,row in prepared:
        temporary=path.with_suffix('.119-tmp')
        with zipfile.ZipFile(temporary,'w') as z:
            for i in infos:z.writestr(i,data[i.filename])
        temporary.replace(path);row['afterSha256']=sha(path.read_bytes())
    AUDIT.write_text(json.dumps(dict(nativeGain=6,nativeSlotFXAUX=50,hostSettingsUnchanged=True,baseline=str(BASELINE.relative_to(ROOT)),targets=[r for *_,r in prepared]),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('13 Qin packages: native Gain=6 / all slots FXAUX=50; native reverse comparison and roundtrip passed.')
if __name__=='__main__':main()
