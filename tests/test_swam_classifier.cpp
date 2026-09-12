#include "../native/Source/SwamPluginClassifier.h"
#include <cassert>

int main()
{
    using fengyin::SwamFamily;
    using fengyin::SwamPluginClassifier;
    assert(SwamPluginClassifier::classify("SWAM Alto Sax", "Audio Modeling") == SwamFamily::saxophone);
    assert(SwamPluginClassifier::classify("SWAM Trumpet", "Audio Modeling") == SwamFamily::brass);
    assert(SwamPluginClassifier::classify("SWAM Flute", "Audio Modeling") == SwamFamily::woodwind);
    assert(SwamPluginClassifier::classify("SWAM Cello", "Audio Modeling") == SwamFamily::strings);
    assert(SwamPluginClassifier::classify("Kontakt", "Native Instruments") == SwamFamily::notSwam);
    assert(std::string(SwamPluginClassifier::familyChineseName(SwamFamily::saxophone)) == "萨克斯管");
    assert(std::string(SwamPluginClassifier::instrumentChineseName("SWAM Alto Sax")) == "中音萨克斯");
    assert(std::string(SwamPluginClassifier::instrumentChineseName("SWAM French Horn")) == "F调圆号");
    assert(std::string(SwamPluginClassifier::instrumentChineseName("SWAM Bass Clarinet")) == "低音单簧管");
    assert(std::string(SwamPluginClassifier::instrumentChineseName("SWAM Contrabassoon")) == "倍低音巴松管");
    assert(std::string(SwamPluginClassifier::instrumentChineseName("SWAM Euphonium")) == "上低音号");
    assert(std::string(SwamPluginClassifier::instrumentKey("SWAM Baritone Sax")) == "baritone-sax");
    assert(std::string(SwamPluginClassifier::instrumentKey("SWAM Tenor Bass Trombone")) == "tenor-bass-trombone");
}
