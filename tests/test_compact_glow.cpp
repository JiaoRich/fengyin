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
