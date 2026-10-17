from pathlib import Path
import unittest,subprocess,shutil

ROOT=Path(__file__).resolve().parents[1]

class WindowDrag119(unittest.TestCase):
    def test_native_mouse_uses_physical_desktop_and_release(self):
        src=(ROOT/'native/Source/Main.cpp').read_text(encoding='utf-8')
        self.assertIn('GetAsyncKeyState(VK_LBUTTON)',src)
        self.assertIn('position.x-gesturePhysicalStart.x',src)
        self.assertIn('GetWindowRect(hwnd,&gesturePhysicalBounds)',src)
        self.assertIn('delta.x*gestureScale',src)
        self.assertIn('!isActiveWindow()',src)
        self.assertIn('SWP_NOZORDER|SWP_NOACTIVATE',src)
        self.assertIn('now-lastVisualUpdate<1000.0/24.0',src)
        glow=(ROOT/'native/Source/CompactWindowGlow.h').read_text(encoding='utf-8')
        self.assertIn('GetWindowRect(host,&hostRect)',glow)
        self.assertIn('hostRect.left-pad,hostRect.top-pad,w,h',glow)
        self.assertIn('innerBounds=inner',glow)
        self.assertIn('windowIgnoresMouseClicks',glow)

    def test_pointer_lifecycle_executes_without_capture_leaks(self):
        node=shutil.which('node')
        if node is None:
            fallback=Path('/Users/mengyang.jmy/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node')
            if fallback.is_file():node=str(fallback)
        self.assertIsNotNone(node,'Node required for pointer lifecycle execution')
        script=r"""
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const listeners={},events=[];let captured=false,fail=false,released=0;
const target={setPointerCapture(){if(fail)throw Error('capture failed');captured=true;},hasPointerCapture(){return captured;},releasePointerCapture(){captured=false;released++;}};
const surface={addEventListener(type,fn){listeners[type]=fn;}};
const context={document:surface,window:surface,Math,setTimeout:()=>1,clearTimeout(){},nativeEvent:(type,p)=>events.push(p.action)};
vm.createContext(context);
vm.runInContext(fs.readFileSync('prototype/app.js','utf8').split('const windowControls')[0],context);
context.e={pointerId:1,isPrimary:true,pointerType:'mouse',screenX:100,screenY:100,currentTarget:target};
vm.runInContext("beginTouchWindowGesture(e,'drag')",context);
assert.equal(events.pop(),'drag');assert.equal(captured,false);
context.e.pointerType='touch';
vm.runInContext("beginTouchWindowGesture(e,'drag')",context);assert.equal(captured,true);
listeners.pointermove({...context.e,screenX:130,screenY:150,preventDefault(){}});
listeners.pointerup(context.e);
assert.equal(captured,false);assert.equal(released,1);
assert.deepEqual(events.splice(0),['pointer-begin:drag','pointer-move:30:50','pointer-end']);
vm.runInContext("beginTouchWindowGesture(e,'drag')",context);listeners.blur();
assert.equal(captured,false);assert.equal(events.pop(),'pointer-end');events.length=0;
fail=true;vm.runInContext("beginTouchWindowGesture(e,'drag')",context);
assert.equal(events.length,0);fail=false;
vm.runInContext("beginTouchWindowGesture(e,'drag')",context);listeners.pointercancel(context.e);
assert.equal(captured,false);assert.equal(events.pop(),'pointer-end');
"""
        result=subprocess.run([node,'-e',script],cwd=ROOT,capture_output=True,text=True,encoding='utf-8')
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)

if __name__=='__main__':unittest.main()
