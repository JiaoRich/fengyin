#pragma once
#include "MasterOutputService.h"
#include "SwamToneProfile.h"
namespace fengyin {
struct ToneStyleDefinition { juce::String id, name, description; ToneStyleSettings settings; SwamToneProfile swam; };
class ToneStyleCatalog { public:
static juce::Array<ToneStyleDefinition> forInstrument(const juce::String& key) {
juce::Array<ToneStyleDefinition> result;
const auto addReleaseStyle = [&result](const char* id, const char* name,
                                       ToneStyleSettings settings)
{
    ToneStyleDefinition style;
    style.id = id;
    style.name = juce::String::fromUTF8(name);
    style.settings = settings;
    // These first-pass release tones are deliberately implemented by the
    // version-independent FengYin output chain.  SWAM parameter indexes are
    // different between instrument families even when the version label is
    // identical, so forcing four guessed indexes produced a false 3.9.4
    // compatibility warning and could alter the wrong controls.
    style.swam.enabled = false;
    result.add(style);
};
if (key == "soprano-sax") { ToneStyleDefinition s; s.id="kenny"; s.name=juce::String::fromUTF8("肯尼基");
s.settings = {0.0f,0.626179993f,0.344393998f,0.579999983f,1.64999998f,0.0350000001f,0.0700000003f,0.568009973f,0.560000002f,0.879999995f,1.0f,0.0f,0.0f};
s.swam.enabled=true; s.swam.releaseModel="SAX 0 FLAT";
s.swam.releaseParameters = {
ToneParameterValue {"index:1:vibratodepth",0.0f},
ToneParameterValue {"index:2:vibratorate",0.337499976f},
ToneParameterValue {"index:3:growl",0.0f},
ToneParameterValue {"index:4:flutter",0.0f},
ToneParameterValue {"index:5:pipemodel",0.0f},
ToneParameterValue {"index:6:chromatic",1.0f},
ToneParameterValue {"index:7:attackstart",0.5f},
ToneParameterValue {"index:8:legatomode",0.0f},
ToneParameterValue {"index:9:keynoise",0.180000007f},
ToneParameterValue {"index:10:altfingering",0.0f},
ToneParameterValue {"index:11:overblowthresh",0.0f},
ToneParameterValue {"index:12:overblow",0.0f},
ToneParameterValue {"index:13:falldown",0.0f},
ToneParameterValue {"index:15:harmonicstructure",0.519999981f},
ToneParameterValue {"index:16:subharm",0.0944881886f},
ToneParameterValue {"index:17:formant",0.519999981f},
ToneParameterValue {"index:18:modalresgain",0.5f},
ToneParameterValue {"index:19:breathnoise",0.0f},
ToneParameterValue {"index:20:timbralcorrection",0.0f},
ToneParameterValue {"index:21:harmonicagain",0.5f},
ToneParameterValue {"index:22:harma",0.200000003f},
ToneParameterValue {"index:23:harmonicbgain",0.5f},
ToneParameterValue {"index:24:harmb",0.400000006f},
ToneParameterValue {"index:25:compressor",0.5f},
ToneParameterValue {"index:26:eqenabled",0.0f},
ToneParameterValue {"index:27:eqlowgain",0.5f},
ToneParameterValue {"index:28:eqmidgain",0.5f},
ToneParameterValue {"index:29:eqmidfreq",0.500601351f},
ToneParameterValue {"index:30:eqhighgain",0.5f},
ToneParameterValue {"index:31:earlyreflectiongain",0.5f},
ToneParameterValue {"index:32:reverbmix",0.5f},
ToneParameterValue {"index:33:reverbtime",0.5f},
ToneParameterValue {"index:34:mainvolume",1.0f},
ToneParameterValue {"index:35:mainvolumeprocessing",0.0f},
ToneParameterValue {"index:36:panpot",0.5f},
ToneParameterValue {"index:74:portamentocontrol",0.0f},
ToneParameterValue {"index:75:portamentomaxtime",0.333333313f},
ToneParameterValue {"index:76:transitionvelsens",0.5f},
ToneParameterValue {"index:77:noteoffvelocity\nifsupported",1.0f},
ToneParameterValue {"index:78:attackcontrol",1.0f},
ToneParameterValue {"index:83:attackexprbias",1.0f},
ToneParameterValue {"index:85:ksoctave",0.0f},
ToneParameterValue {"index:86:ksvelocityremap",0.5f},
ToneParameterValue {"index:87:randomdynamic",0.272727281f},
ToneParameterValue {"index:88:dynamicpitch",0.236220479f},
ToneParameterValue {"index:89:panpottype",0.666666687f},
ToneParameterValue {"index:90:dynamicharmonic",0.249999985f},
ToneParameterValue {"index:91:releasetime",0.333333343f},
ToneParameterValue {"index:92:attacktosustain",0.5f},
ToneParameterValue {"index:93:autoexpression",0.5f},
ToneParameterValue {"index:94:breathyppp",0.0f},
ToneParameterValue {"index:95:vibrrandrate",0.300000012f},
ToneParameterValue {"index:98:portamentotime",0.0f},
ToneParameterValue {"index:106:absorptionmaterials",0.5f},
ToneParameterValue {"index:107:roomsizes",0.400000006f},
ToneParameterValue {"index:108:listenerposition",0.588235319f},
ToneParameterValue {"index:109:sourcedistance",0.130434781f},
ToneParameterValue {"index:110:sourceangle",0.5f},
ToneParameterValue {"index:111:sourcedelaymode",0.0f},
ToneParameterValue {"index:112:reverbmodulation",1.0f},
ToneParameterValue {"index:113:micsspread",0.0674641505f},
ToneParameterValue {"index:114:ambiente\nroomsimulator",1.0f}}; result.add(s); }
if (key == "soprano-sax") { ToneStyleDefinition s; s.id="pop-high"; s.name=juce::String::fromUTF8("流行高音");
s.settings = {0.218140006f,0.626179993f,0.389562011f,0.579999983f,1.04875004f,0.0919170007f,0.101797499f,0.732249975f,0.560000002f,0.879999995f,1.0f,0.0f,0.0f};
s.swam.enabled=true; s.swam.releaseModel="SAX 0 FLAT";
s.swam.releaseParameters = {
ToneParameterValue {"index:1:vibratodepth",0.0f},
ToneParameterValue {"index:2:vibratorate",0.337499976f},
ToneParameterValue {"index:3:growl",0.0f},
ToneParameterValue {"index:4:flutter",0.0f},
ToneParameterValue {"index:5:pipemodel",0.0f},
ToneParameterValue {"index:6:chromatic",1.0f},
ToneParameterValue {"index:7:attackstart",0.5f},
ToneParameterValue {"index:8:legatomode",0.0f},
ToneParameterValue {"index:9:keynoise",0.180000007f},
ToneParameterValue {"index:10:altfingering",0.0f},
ToneParameterValue {"index:11:overblowthresh",0.0f},
ToneParameterValue {"index:12:overblow",0.0f},
ToneParameterValue {"index:13:falldown",0.0f},
ToneParameterValue {"index:15:harmonicstructure",0.519999981f},
ToneParameterValue {"index:16:subharm",0.0944881886f},
ToneParameterValue {"index:17:formant",0.519999981f},
ToneParameterValue {"index:18:modalresgain",0.5f},
ToneParameterValue {"index:19:breathnoise",0.0f},
ToneParameterValue {"index:20:timbralcorrection",0.0f},
ToneParameterValue {"index:21:harmonicagain",0.5f},
ToneParameterValue {"index:22:harma",0.200000003f},
ToneParameterValue {"index:23:harmonicbgain",0.5f},
ToneParameterValue {"index:24:harmb",0.400000006f},
ToneParameterValue {"index:25:compressor",0.5f},
ToneParameterValue {"index:26:eqenabled",0.0f},
ToneParameterValue {"index:27:eqlowgain",0.5f},
ToneParameterValue {"index:28:eqmidgain",0.5f},
ToneParameterValue {"index:29:eqmidfreq",0.500601351f},
ToneParameterValue {"index:30:eqhighgain",0.5f},
ToneParameterValue {"index:31:earlyreflectiongain",0.5f},
ToneParameterValue {"index:32:reverbmix",0.5f},
ToneParameterValue {"index:33:reverbtime",0.5f},
ToneParameterValue {"index:34:mainvolume",1.0f},
ToneParameterValue {"index:35:mainvolumeprocessing",0.0f},
ToneParameterValue {"index:36:panpot",0.5f},
ToneParameterValue {"index:74:portamentocontrol",0.0f},
ToneParameterValue {"index:75:portamentomaxtime",0.333333313f},
ToneParameterValue {"index:76:transitionvelsens",0.5f},
ToneParameterValue {"index:77:noteoffvelocity\nifsupported",1.0f},
ToneParameterValue {"index:78:attackcontrol",1.0f},
ToneParameterValue {"index:83:attackexprbias",1.0f},
ToneParameterValue {"index:85:ksoctave",0.0f},
ToneParameterValue {"index:86:ksvelocityremap",0.5f},
ToneParameterValue {"index:87:randomdynamic",0.272727281f},
ToneParameterValue {"index:88:dynamicpitch",0.236220479f},
ToneParameterValue {"index:89:panpottype",0.666666687f},
ToneParameterValue {"index:90:dynamicharmonic",0.249999985f},
ToneParameterValue {"index:91:releasetime",0.333333343f},
ToneParameterValue {"index:92:attacktosustain",0.5f},
ToneParameterValue {"index:93:autoexpression",0.5f},
ToneParameterValue {"index:94:breathyppp",0.0f},
ToneParameterValue {"index:95:vibrrandrate",0.300000012f},
ToneParameterValue {"index:98:portamentotime",0.0f},
ToneParameterValue {"index:106:absorptionmaterials",0.5f},
ToneParameterValue {"index:107:roomsizes",0.400000006f},
ToneParameterValue {"index:108:listenerposition",0.588235319f},
ToneParameterValue {"index:109:sourcedistance",0.130434781f},
ToneParameterValue {"index:110:sourceangle",0.5f},
ToneParameterValue {"index:111:sourcedelaymode",0.0f},
ToneParameterValue {"index:112:reverbmodulation",1.0f},
ToneParameterValue {"index:113:micsspread",0.0674641505f},
ToneParameterValue {"index:114:ambiente\nroomsimulator",1.0f}}; result.add(s); }
if (key == "alto-sax") { ToneStyleDefinition s; s.id="jazz"; s.name=juce::String::fromUTF8("爵士");
s.settings = {0.0f,0.1f,0.18f,0.58f,1.0f,0.0f,0.0f,0.72f,0.4f,1.0f,0.5f,0.0f,0.0f};
s.swam.enabled=true; s.swam.releaseModel="SAX 0 FLAT";
s.swam.releaseParameters = {
ToneParameterValue {"index:15:harmonicstructure",0.875f},
ToneParameterValue {"index:16:subharm",0.0944881886f},
ToneParameterValue {"index:17:formant",0.59375f},
ToneParameterValue {"index:18:modalresgain",0.579999983f},
ToneParameterValue {"index:19:breathnoise",0.251968503f},
ToneParameterValue {"index:9:keynoise",0.0629921257f},
ToneParameterValue {"index:20:timbralcorrection",0.5f},
ToneParameterValue {"index:22:harma",0.200000003f},
ToneParameterValue {"index:21:harmonicagain",0.8125f},
ToneParameterValue {"index:24:harmb",0.800000012f},
ToneParameterValue {"index:23:harmonicbgain",0.75f},
ToneParameterValue {"index:90:dynamicharmonic",0.0f},
ToneParameterValue {"index:87:randomdynamic",0.0f},
ToneParameterValue {"index:88:dynamicpitch",0.0944881886f},
ToneParameterValue {"index:94:breathyppp",0.0f},
ToneParameterValue {"index:7:attackstart",0.0f},
ToneParameterValue {"index:92:attacktosustain",0.5f},
ToneParameterValue {"index:91:releasetime",0.333333343f},
ToneParameterValue {"index:26:eqenabled",0.0f},
ToneParameterValue {"index:25:compressor",0.0f},
ToneParameterValue {"index:32:reverbmix",0.0f},
ToneParameterValue {"index:34:mainvolume",1.0f}}; result.add(s); }
if (key == "alto-sax") { ToneStyleDefinition s; s.id="deep"; s.name=juce::String::fromUTF8("深情");
s.settings = {0.2f,0.1f,0.32f,0.58f,1.0f,0.0f,0.0f,0.55f,0.3f,1.0f,0.5f,0.0f,0.15f};
s.swam.enabled=true; s.swam.releaseModel="SAX 2 DRY";
s.swam.releaseParameters = {
ToneParameterValue {"index:15:harmonicstructure",0.5f},
ToneParameterValue {"index:16:subharm",0.0629921257f},
ToneParameterValue {"index:17:formant",0.450000018f},
ToneParameterValue {"index:18:modalresgain",0.5f},
ToneParameterValue {"index:19:breathnoise",0.314960629f},
ToneParameterValue {"index:9:keynoise",0.0787401572f},
ToneParameterValue {"index:20:timbralcorrection",0.5f},
ToneParameterValue {"index:22:harma",0.400000006f},
ToneParameterValue {"index:21:harmonicagain",0.5f},
ToneParameterValue {"index:24:harmb",0.800000012f},
ToneParameterValue {"index:23:harmonicbgain",0.3125f},
ToneParameterValue {"index:90:dynamicharmonic",0.12500003f},
ToneParameterValue {"index:87:randomdynamic",0.0454545468f},
ToneParameterValue {"index:88:dynamicpitch",0.125984251f},
ToneParameterValue {"index:94:breathyppp",0.0f},
ToneParameterValue {"index:7:attackstart",0.0f},
ToneParameterValue {"index:92:attacktosustain",0.5f},
ToneParameterValue {"index:91:releasetime",0.333333343f},
ToneParameterValue {"index:26:eqenabled",0.0f},
ToneParameterValue {"index:25:compressor",0.0f},
ToneParameterValue {"index:32:reverbmix",0.0f},
ToneParameterValue {"index:34:mainvolume",1.0f}}; result.add(s); }
if (key == "alto-sax") { ToneStyleDefinition s; s.id="pop"; s.name=juce::String::fromUTF8("流行");
s.settings = {0.4f,0.1f,0.32f,0.58f,1.0f,0.0f,0.0f,0.55f,0.3f,1.0f,0.5f,-0.08f,0.3f};
s.swam.enabled=true; s.swam.releaseModel="SAX 2 BRIGHT";
s.swam.releaseParameters = {
ToneParameterValue {"index:15:harmonicstructure",0.75f},
ToneParameterValue {"index:16:subharm",0.0787401572f},
ToneParameterValue {"index:17:formant",0.53125f},
ToneParameterValue {"index:18:modalresgain",0.439999998f},
ToneParameterValue {"index:19:breathnoise",0.362204731f},
ToneParameterValue {"index:9:keynoise",0.0944881886f},
ToneParameterValue {"index:20:timbralcorrection",0.5f},
ToneParameterValue {"index:22:harma",0.600000024f},
ToneParameterValue {"index:21:harmonicagain",0.75f},
ToneParameterValue {"index:24:harmb",1.0f},
ToneParameterValue {"index:23:harmonicbgain",0.6875f},
ToneParameterValue {"index:90:dynamicharmonic",0.750000119f},
ToneParameterValue {"index:87:randomdynamic",0.0454545468f},
ToneParameterValue {"index:88:dynamicpitch",0.157480314f},
ToneParameterValue {"index:94:breathyppp",0.0f},
ToneParameterValue {"index:7:attackstart",0.0f},
ToneParameterValue {"index:92:attacktosustain",0.5f},
ToneParameterValue {"index:91:releasetime",0.333333343f},
ToneParameterValue {"index:26:eqenabled",0.0f},
ToneParameterValue {"index:25:compressor",0.0f},
ToneParameterValue {"index:32:reverbmix",0.0f},
ToneParameterValue {"index:34:mainvolume",1.0f}}; result.add(s); }
if (key == "tenor-sax") { ToneStyleDefinition s; s.id="mellow"; s.name=juce::String::fromUTF8("醇厚");
s.settings = {-0.5f,0.32f,0.22f,0.65f,1.0f,0.0f,0.08f,0.58f,0.58f,1.0f,1.0f,0.12f,0.0f};
s.swam.enabled=true; s.swam.releaseModel="SAX 3 WARM";
s.swam.releaseParameters = {
ToneParameterValue {"index:15:harmonicstructure",0.5f},
ToneParameterValue {"index:16:subharm",0.0629921257f},
ToneParameterValue {"index:17:formant",0.387500018f},
ToneParameterValue {"index:18:modalresgain",0.5f},
ToneParameterValue {"index:19:breathnoise",0.299212605f},
ToneParameterValue {"index:9:keynoise",0.0314960629f},
ToneParameterValue {"index:20:timbralcorrection",0.5f},
ToneParameterValue {"index:22:harma",0.400000006f},
ToneParameterValue {"index:21:harmonicagain",0.3125f},
ToneParameterValue {"index:24:harmb",0.800000012f},
ToneParameterValue {"index:23:harmonicbgain",0.125f},
ToneParameterValue {"index:90:dynamicharmonic",0.0f},
ToneParameterValue {"index:87:randomdynamic",0.0f},
ToneParameterValue {"index:88:dynamicpitch",0.0f},
ToneParameterValue {"index:94:breathyppp",0.0f},
ToneParameterValue {"index:7:attackstart",0.0f},
ToneParameterValue {"index:92:attacktosustain",0.5f},
ToneParameterValue {"index:91:releasetime",0.333333343f},
ToneParameterValue {"index:26:eqenabled",0.0f},
ToneParameterValue {"index:25:compressor",0.0f},
ToneParameterValue {"index:32:reverbmix",0.0f},
ToneParameterValue {"index:34:mainvolume",1.0f}}; result.add(s); }
if (key == "tenor-sax") { ToneStyleDefinition s; s.id="warm"; s.name=juce::String::fromUTF8("温暖");
s.settings = {0.05f,0.16f,0.12f,0.65f,1.0f,0.0f,0.0f,0.58f,0.58f,1.0f,1.0f,0.02f,0.0f};
s.swam.enabled=true; s.swam.releaseModel="SAX 3 SMOOTH";
s.swam.releaseParameters = {
ToneParameterValue {"index:15:harmonicstructure",0.5f},
ToneParameterValue {"index:16:subharm",0.0629921257f},
ToneParameterValue {"index:17:formant",0.5f},
ToneParameterValue {"index:18:modalresgain",0.5f},
ToneParameterValue {"index:19:breathnoise",0.14173229f},
ToneParameterValue {"index:9:keynoise",0.0314960629f},
ToneParameterValue {"index:20:timbralcorrection",0.5f},
ToneParameterValue {"index:22:harma",0.400000006f},
ToneParameterValue {"index:21:harmonicagain",0.53125f},
ToneParameterValue {"index:24:harmb",0.800000012f},
ToneParameterValue {"index:23:harmonicbgain",0.4375f},
ToneParameterValue {"index:90:dynamicharmonic",0.12500003f},
ToneParameterValue {"index:87:randomdynamic",0.0f},
ToneParameterValue {"index:88:dynamicpitch",0.0f},
ToneParameterValue {"index:94:breathyppp",0.0f},
ToneParameterValue {"index:7:attackstart",0.0f},
ToneParameterValue {"index:92:attacktosustain",0.5f},
ToneParameterValue {"index:91:releasetime",0.333333343f},
ToneParameterValue {"index:26:eqenabled",0.0f},
ToneParameterValue {"index:25:compressor",0.0f},
ToneParameterValue {"index:32:reverbmix",0.0f},
ToneParameterValue {"index:34:mainvolume",1.0f}}; result.add(s); }
if (key == "baritone-sax") {
    addReleaseStyle("jazz", "爵士", {-0.08f,0.28f,0.18f,0.58f,1.6f,0.04f,0.10f,0.48f,0.56f,0.90f,1.0f,0.08f,0.02f});
    addReleaseStyle("pop", "流行", {0.12f,0.12f,0.12f,0.56f,1.8f,0.05f,0.14f,0.44f,0.54f,0.90f,1.0f,0.03f,0.04f});
}
if (key == "trumpet") addReleaseStyle("natural", "自然原声", {0.0f,0.12f,0.14f,0.58f,1.7f,0.04f,0.16f,0.42f,0.54f,0.88f,1.0f,0.0f,0.04f});
if (key == "piccolo-trumpet") addReleaseStyle("natural", "自然原声", {-0.05f,0.10f,0.13f,0.58f,1.7f,0.03f,0.22f,0.40f,0.56f,0.86f,1.0f,0.0f,0.03f});
if (key == "alto-trombone") addReleaseStyle("natural", "自然原声", {-0.04f,0.18f,0.16f,0.60f,1.6f,0.04f,0.12f,0.45f,0.55f,0.90f,1.0f,0.02f,0.02f});
if (key == "bass-trombone") addReleaseStyle("natural", "自然原声", {-0.08f,0.22f,0.15f,0.56f,1.8f,0.04f,0.12f,0.46f,0.56f,0.90f,1.0f,0.08f,0.01f});
if (key == "flute") addReleaseStyle("natural", "自然原声", {0.05f,0.10f,0.20f,0.62f,1.4f,0.02f,0.10f,0.50f,0.60f,0.92f,1.0f,0.0f,0.08f});
if (key == "piccolo") addReleaseStyle("natural", "自然原声", {-0.05f,0.08f,0.18f,0.60f,1.5f,0.02f,0.22f,0.46f,0.60f,0.90f,1.0f,0.0f,0.05f});
if (key == "clarinet") addReleaseStyle("natural", "自然原声", {-0.03f,0.22f,0.18f,0.62f,1.4f,0.02f,0.08f,0.48f,0.58f,0.90f,1.0f,0.01f,0.03f});
if (key == "oboe") addReleaseStyle("natural", "自然原声", {-0.06f,0.16f,0.20f,0.60f,1.5f,0.03f,0.20f,0.50f,0.58f,0.90f,1.0f,0.0f,0.04f});
if (key == "bassoon") addReleaseStyle("natural", "自然原声", {-0.08f,0.24f,0.18f,0.60f,1.5f,0.03f,0.10f,0.48f,0.58f,0.90f,1.0f,0.05f,0.02f});
if (key == "violin") addReleaseStyle("natural", "自然原声", {0.02f,0.18f,0.22f,0.62f,1.4f,0.02f,0.14f,0.52f,0.60f,0.94f,1.0f,0.0f,0.06f});
if (key == "viola") addReleaseStyle("natural", "自然原声", {-0.04f,0.24f,0.21f,0.61f,1.45f,0.02f,0.10f,0.52f,0.60f,0.94f,1.0f,0.03f,0.04f});
if (key == "cello") addReleaseStyle("natural", "自然原声", {-0.06f,0.28f,0.22f,0.60f,1.5f,0.02f,0.08f,0.54f,0.60f,0.94f,1.0f,0.06f,0.03f});
if (key == "violin-section") addReleaseStyle("natural", "自然原声", {0.0f,0.16f,0.25f,0.60f,1.5f,0.02f,0.12f,0.58f,0.62f,1.0f,1.0f,0.0f,0.05f});
if (key == "viola-section") addReleaseStyle("natural", "自然原声", {-0.04f,0.22f,0.24f,0.60f,1.5f,0.02f,0.10f,0.58f,0.62f,1.0f,1.0f,0.03f,0.04f});
if (key == "cello-section") addReleaseStyle("natural", "自然原声", {-0.06f,0.26f,0.24f,0.58f,1.55f,0.02f,0.08f,0.58f,0.62f,1.0f,1.0f,0.06f,0.03f});
// The imported reference packs carried -6 dB effect-output trims in addition
// to SWAM's -9/-12 dB main level. Do not duplicate that attenuation in release.
for (auto& style : result) style.settings.outputGain = juce::jmax(1.0f, style.settings.outputGain);
if (!result.isEmpty()) return result;
ToneStyleDefinition s; s.id="natural"; s.name=juce::String::fromUTF8("自然原声"); return {s};
}
static ToneStyleDefinition find(const juce::String& key,const juce::String& id) { const auto styles=forInstrument(key); for(const auto& s:styles) if(s.id==id)return s; return styles.getFirst(); }
}; }
