#include "CompactWindowGlow.h"
#include "NativeCompactDragSurface.h"
#include <iostream>

struct CompactWindowGlowTestAccess
{
    static juce::Image frame(double breath, double phase)
    {
        CompactWindowGlow glow;
        glow.setBounds(0, 0, 640, 800);
        glow.rebuild();
        glow.energy = breath;
        glow.phase = phase;
        glow.render();
        return glow.frame.createCopy();
    }
};

int main(int argc, char** argv)
{
   #if JUCE_WINDOWS
    if (argc > 1 && juce::String(argv[1]) == "--desktop-smoke")
    {
        juce::ScopedJuceInitialiser_GUI gui;
        struct Owner : juce::Component {
            void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff214365)); }
        } owner;
        owner.setOpaque(true);
        owner.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        owner.setBounds(160,160,320,360); owner.setVisible(true); owner.toFront(true);
        CompactWindowGlow glow;
        const auto pump = [&](float breath) {
            for (int i=0;i<24;++i) {
                glow.update(owner,true,breath);
                juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
            }
        };
        pump(0);
        const auto read = [](int x,int y) -> COLORREF {
            auto dc=GetDC(nullptr); if(!dc) return CLR_INVALID;
            const auto colour=GetPixel(dc,x,y); ReleaseDC(nullptr,dc); return colour;
        };
        const auto scale=owner.getPeer()->getPlatformScaleFactor();
        const auto screen=owner.getScreenBounds();
        const int x=juce::roundToInt((screen.getX()+160)*scale);
        const int y=juce::roundToInt(screen.getY()*scale)-2;
        const auto centre=read(x,juce::roundToInt((screen.getY()+180)*scale));
        if(centre!=RGB(0x21,0x43,0x65)) {
            std::cout<<"SKIP: runner has no readable interactive desktop\n";return 77;
        }
        const auto idlePixel=read(x,y);pump(1);const auto strongPixel=read(x,y);
        if(idlePixel==CLR_INVALID || strongPixel==CLR_INVALID || idlePixel==strongPixel) {
            std::cerr<<"Layered halo did not change on the actual Windows desktop\n";return 1;
        }
        for(const auto bounds:{juce::Rectangle<int>(240,120,400,420),juce::Rectangle<int>(80,180,320,360)}) {
            owner.setBounds(bounds);pump(1);
            RECT host{},halo{};
            GetWindowRect(static_cast<HWND>(owner.getPeer()->getNativeHandle()),&host);
            GetWindowRect(static_cast<HWND>(glow.getPeer()->getNativeHandle()),&halo);
            const auto pad=juce::roundToInt(48*owner.getPeer()->getPlatformScaleFactor());
            if(halo.left!=host.left-pad||halo.top!=host.top-pad||halo.right!=host.right+pad||halo.bottom!=host.bottom+pad) {
                std::cerr<<"Halo does not follow the physical owner rectangle\n";return 1;
            }
            // JUCE implements windowIgnoresMouseClicks through WM_NCHITTEST,
            // not WS_EX_TRANSPARENT. Verify the actual input contract.
            const auto haloWindow=static_cast<HWND>(glow.getPeer()->getNativeHandle());
            for(const auto point:{POINT{host.left+10,host.top+10},POINT{host.left-2,host.top+20}}) {
                const auto hit=SendMessage(haloWindow,WM_NCHITTEST,0,MAKELPARAM(point.x,point.y));
                if(hit!=HTTRANSPARENT){std::cerr<<"Halo must return HTTRANSPARENT\n";return 1;}
            }
        }
        NativeCompactDragSurface drag;
        const auto host=static_cast<HWND>(owner.getPeer()->getNativeHandle());
        drag.update(host,10,10,140,30,true);
        const auto surface=FindWindowExW(host,nullptr,L"FengYinCompactDragSurface",nullptr);
        if(!surface){std::cerr<<"Native title drag surface missing\n";return 1;}
        struct RestoreCursor {POINT old{};RestoreCursor(){GetCursorPos(&old);}~RestoreCursor(){SetCursorPos(old.x,old.y);}} restore;
        RECT before{},after{};GetWindowRect(host,&before);
        SetCursorPos(before.left+20,before.top+20);
        SendMessage(surface,WM_LBUTTONDOWN,MK_LBUTTON,0);
        for(int i=1;i<=8;++i) {
            // Repeated geometry synchronization must not reset the gesture.
            drag.update(host,10,10,140,30,true);
            SetCursorPos(before.left+20+i*10,before.top+20+i*5);
            SendMessage(surface,WM_MOUSEMOVE,MK_LBUTTON,0);
        }
        SendMessage(surface,WM_LBUTTONUP,0,0);GetWindowRect(host,&after);
        if(after.left-before.left!=80||after.top-before.top!=40||GetCapture()==surface) {
            std::cerr<<"Native physical title drag/capture cleanup failed\n";return 1;
        }
        if(!InitializeTouchInjection(1,TOUCH_FEEDBACK_NONE)) {
            std::cerr<<"Cannot initialize Windows native touch injection: "<<GetLastError()<<"\n";return 1;
        }
        RECT touchStart{};GetWindowRect(host,&touchStart);
        POINTER_TOUCH_INFO contact{};
        contact.pointerInfo.pointerType=PT_TOUCH;contact.pointerInfo.pointerId=1;
        contact.touchMask=TOUCH_MASK_CONTACTAREA|TOUCH_MASK_ORIENTATION|TOUCH_MASK_PRESSURE;
        contact.orientation=90;contact.pressure=512;
        const auto inject=[&](int dx,int dy,POINTER_FLAGS flags) {
            contact.pointerInfo.ptPixelLocation={touchStart.left+20+dx,touchStart.top+20+dy};
            contact.pointerInfo.pointerFlags=flags;
            const auto p=contact.pointerInfo.ptPixelLocation;
            contact.rcContact={p.x-2,p.y-2,p.x+2,p.y+2};
            if(!InjectTouchInput(1,&contact)){std::cerr<<"Touch injection failed: "<<GetLastError()<<"\n";return false;}
            juce::MessageManager::getInstance()->runDispatchLoopUntil(25);return true;
        };
        if(!inject(0,0,POINTER_FLAG_DOWN|POINTER_FLAG_INRANGE|POINTER_FLAG_INCONTACT))return 1;
        for(int i=1;i<=4;++i)if(!inject(i*10,i*5,POINTER_FLAG_UPDATE|POINTER_FLAG_INRANGE|POINTER_FLAG_INCONTACT))return 1;
        if(!inject(40,20,POINTER_FLAG_UP))return 1;
        RECT touchEnd{};GetWindowRect(host,&touchEnd);
        if(touchEnd.left-touchStart.left!=40||touchEnd.top-touchStart.top!=20) {
            std::cerr<<"Native injected touch title drag jumped or failed\n";return 1;
        }
        drag.update(host,10,10,140,30,false);
        if(IsWindowVisible(surface)){std::cerr<<"Native drag strip must hide outside compact mode\n";return 1;}
        std::cout<<"Windows desktop halo and native physical title drag/capture cleanup verified\n";
        return 0;
    }
   #endif
    const auto idle = CompactWindowGlowTestAccess::frame(0, 0);
    const auto strong = CompactWindowGlowTestAccess::frame(1, 0);
    const auto moved = CompactWindowGlowTestAccess::frame(1, .25);
    auto require = [](bool ok, const char* message) {
        if (!ok) { std::cerr << message << '\n'; std::exit(1); }
    };
    require(strong.getPixelAt(320,400).getAlpha() == 0, "interior must remain transparent");
    require(strong.getPixelAt(100,100).getAlpha() == 0, "content must not be tinted");
    require(strong.getPixelAt(0,0).getAlpha() == 0, "outer corners must remain transparent");
    require(strong.getPixelAt(320,47).getAlpha() > idle.getPixelAt(320,47).getAlpha() * 3,
            "strong breath must visibly brighten the edge");
    require(strong.getPixelAt(320,47) != moved.getPixelAt(320,47), "colours must circulate");
    require(strong.getPixelAt(320,47).getAlpha() > strong.getPixelAt(320,12).getAlpha(),
            "glow must fade outwards");
    if (argc > 1)
    {
        juce::Image preview(juce::Image::RGB, strong.getWidth(), strong.getHeight(), true);
        juce::Graphics graphics(preview);
        graphics.fillAll(juce::Colour(0xff11151b));
        graphics.drawImageAt(strong, 0, 0);
        graphics.setColour(juce::Colour(0xff07101d));
        graphics.fillRoundedRectangle(48.0f, 48.0f, 544.0f, 704.0f, 28.0f);
        juce::FileOutputStream output {juce::File(argv[1])};
        require(output.openedOk(), "cannot open preview output");
        require(juce::PNGImageFormat().writeImageToStream(preview, output), "cannot write preview");
    }
    std::cout << "Compact glow rendering checks passed\n";
}
