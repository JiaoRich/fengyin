#include "ToneStyleCatalog.h"
#include "KongInstrumentCatalog.h"
#include <cassert>

int main()
{
    const char* swamKeys[] { "soprano-sax", "alto-sax", "tenor-sax", "baritone-sax",
        "flugelhorn-eb", "flugelhorn", "piccolo-trumpet", "trumpet-c", "trumpet",
        "double-bass-trombone", "tenor-bass-trombone", "bass-trombone", "alto-trombone",
        "tenor-trombone", "bass-tuba", "tuba-eb", "euphonium", "horn-bb", "horn-f",
        "piccolo", "bass-flute", "alto-flute", "flute", "bass-clarinet", "clarinet",
        "english-horn", "oboe", "contrabassoon", "bassoon", "double-bass", "violin", "viola", "cello" };
    for (const auto* key : swamKeys)
        assert(fengyin::ToneStyleCatalog::forInstrument(key).size() == 3);
    for (const auto& instrument : fengyin::KongInstrumentCatalog::all())
        assert(fengyin::ToneStyleCatalog::forInstrument(instrument.key).size() == 3);
}
