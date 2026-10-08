#include "CompactWindowGlow.h"
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
        const auto read = [](int x,int y) {
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
        std::cout<<"Windows desktop halo changed with breath; software layered peer verified\n";
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
