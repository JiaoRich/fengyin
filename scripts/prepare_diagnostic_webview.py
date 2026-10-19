"""Instrument JUCE 9.0.2 standalone diagnostics only; never the release app."""
import hashlib
from pathlib import Path
import sys

UPSTREAM_SHA='041ce850faeb2098093a1e9f65c43ba93d782cba3cbac1e08e5f98ccc4849e74'
NEEDLE='        auto webViewOptions = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();'
PATCH='''
        // FengYin diagnostic-only: programmatic arguments also work in elevated CI.
        const auto diagnosticArgs = SystemStats::getEnvironmentVariable ("FENGYIN_DIAGNOSTIC_WEBVIEW_ARGS", {});
        if (diagnosticArgs.isNotEmpty())
            webViewOptions->put_AdditionalBrowserArguments (diagnosticArgs.toWideCharPointer());
'''

def apply(path,expected=UPSTREAM_SHA):
    path=Path(path);source=path.read_bytes().replace(b'\r\n',b'\n')
    original=source.replace(PATCH.encode(),b'')
    if hashlib.sha256(original).hexdigest()!=expected:raise ValueError('Unexpected JUCE Windows WebView source')
    if original.count(NEEDLE.encode())!=1:raise ValueError('Ambiguous WebView options anchor')
    patched=original.replace(NEEDLE.encode(),NEEDLE.encode()+PATCH.encode())
    path.write_bytes(patched)
    return hashlib.sha256(patched).hexdigest()

if __name__=='__main__':print('Verified diagnostic-only JUCE WebView instrumentation',apply(sys.argv[1]))
