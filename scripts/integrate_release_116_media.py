"""Integrate explicitly approved straight defaults and reviewed-voice candidates.

ASR screens for the reported extra tail; it is not a claim of human listening QA.
Original incoming KAMs and the immutable 1.1.15 voice audit stay untouched.
"""
import hashlib,json,shutil,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    folder=ROOT/'outputs/folk-straight-116';assets=ROOT/'assets/tone-packages'
    auditfile=assets/'folk-defaults-116-audit.json'
    previous=json.loads(auditfile.read_text(encoding='utf-8')) if auditfile.exists() else None
    before=previous['before'] if previous else {p.name:sha(p) for p in assets.glob('folk-*.fytonepack')}
    targets={'matouqin':('Sus_Layers',0,1),'nanxiao':('Sus_Main',8,1),'dongxiao':('Sus_Main',0,1),'qudi':('Sus_Main',1,2)}
    for name in targets:
        source=folder/('folk-'+name+'.fytonepack')
        with zipfile.ZipFile(source) as z:
            assert z.testzip() is None;m=json.loads(z.read('manifest.json'))
            for f in m['files']:assert hashlib.sha256(z.read(f['path'])).hexdigest()==f['sha256']
        # Native readback and audio sweep logs are mandatory intake evidence.
        log=(folder/(name+'-audio.log')).read_text(encoding='utf-8')
        assert log.count('rmsDb=')==12 and 'plugin=QinEngineV3' in log
        shutil.copy2(source,assets/source.name)
    after={p.name:sha(p) for p in assets.glob('folk-*.fytonepack')}
    for name,digest in before.items():
        if name not in {'folk-'+x+'.fytonepack' for x in targets}:assert after[name]==digest
    auditfile.write_text(json.dumps(dict(before=before,after=after,targets={k:dict(articulation=v[0],index=v[1],slots=v[2]) for k,v in targets.items()},scope='Default articulation only; volume and all other slots preserved'),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    candidates=ROOT/'outputs/voice-116-replacements'
    records={r['id']:r for r in json.loads((candidates/'manifest.json').read_text(encoding='utf-8'))}
    for model in ['tiny','base']:
        results=json.loads((candidates/('asr-'+model+'-replacements.json')).read_text(encoding='utf-8'))
        assert len(results)==35
        assert all(not r['recognized'].rstrip('。，, ').endswith('的') for r in results)
    catalogfile=ROOT/'prototype/voice-catalog.js'
    catalog=json.loads(catalogfile.read_text(encoding='utf-8').split('=',1)[1].strip().rstrip(';'))
    audit=[];replaced=[]
    for item in catalog['items']:
        ident=item['id'];file=ROOT/'assets/voice'/(ident+'.wav')
        if ident in records:
            record=records[ident];source=candidates/(ident+'.wav');assert sha(source)==record['sha256']
            shutil.copy2(source,file);item['speechText']=record['synthesisText'];replaced.append(ident)
            if ident in ['V058_01','NEW_TONE_GENERIC']:item['text']=record['synthesisText']
        audit.append(dict(id=ident,text=item['text'],speechText=item.get('speechText',item['text']),voice=item['voice'],sha256=sha(file)))
    assert len(replaced)==34
    catalogfile.write_text('window.FENGYIN_VOICE_CATALOG = '+json.dumps(catalog,ensure_ascii=False,separators=(',',':'))+';\n',encoding='utf-8')
    (ROOT/'assets/voice/release-116-audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Integrated 4 straight defaults and',len(replaced),'voice recordings. Kensa candidate retained for later catalogue integration.')
if __name__=='__main__':main()
