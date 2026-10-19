// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
// Explorer thumbnail handler for .f360 designs: shows the PNG preview that Fission 360 stores in the
// file's "thumbnail" key. Plain Win32 + WIC, no Qt, so Explorer's thumbnail host loads it quickly.
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <thumbcache.h>
#include <wincrypt.h>
#include <wincodec.h>
#include <new>
#include <string>
#include <vector>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "windowscodecs.lib")

// {6C1E3F9A-2B47-4E8D-9A51-F3600B1D7C42}
static const CLSID CLSID_F360Thumbnail = {0x6c1e3f9a, 0x2b47, 0x4e8d, {0x9a, 0x51, 0xf3, 0x60, 0x0b, 0x1d, 0x7c, 0x42}};

static long g_refs = 0;
static HMODULE g_module = nullptr;

namespace {

// Start of the base64 PNG after "thumbnail": " (with or without the space), or npos
size_t thumbStart(const std::string &text) {
    for (const char *tag : {"\"thumbnail\": \"", "\"thumbnail\":\""}) {
        const size_t at = text.find(tag);
        if (at != std::string::npos) return at + strlen(tag);
    }
    return std::string::npos;
}

// The base64 PNG between "thumbnail": " and the closing quote
bool extractPng(const std::string &text, std::vector<BYTE> &png) {
    const size_t from = thumbStart(text);
    if (from == std::string::npos) return false;
    const size_t to = text.find('"', from);
    if (to == std::string::npos || to <= from) return false;
    DWORD size = 0;
    if (!CryptStringToBinaryA(text.data() + from, DWORD(to - from), CRYPT_STRING_BASE64, nullptr, &size, nullptr, nullptr))
        return false;
    png.resize(size);
    return CryptStringToBinaryA(text.data() + from, DWORD(to - from), CRYPT_STRING_BASE64, png.data(), &size, nullptr, nullptr) != 0;
}

HRESULT pngToBitmap(const std::vector<BYTE> &png, UINT cx, HBITMAP *out) {
    IWICImagingFactory *factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return hr;
    IWICStream *stream = nullptr;
    IWICBitmapDecoder *decoder = nullptr;
    IWICBitmapFrameDecode *frame = nullptr;
    IWICBitmapScaler *scaler = nullptr;
    IWICFormatConverter *converter = nullptr;
    hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromMemory(const_cast<BYTE *>(png.data()), DWORD(png.size()));
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
    UINT w = 0, h = 0;
    if (SUCCEEDED(hr)) hr = frame->GetSize(&w, &h);
    if (SUCCEEDED(hr) && (w == 0 || h == 0)) hr = E_FAIL;
    // Fit inside cx x cx, never enlarged past the stored size
    UINT tw = w, th = h;
    if (SUCCEEDED(hr) && (w > cx || h > cx)) {
        if (w >= h) { tw = cx; th = UINT(max(1.0, double(h) * cx / w)); }
        else { th = cx; tw = UINT(max(1.0, double(w) * cx / h)); }
    }
    if (SUCCEEDED(hr)) hr = factory->CreateBitmapScaler(&scaler);
    if (SUCCEEDED(hr)) hr = scaler->Initialize(frame, tw, th, WICBitmapInterpolationModeFant);
    if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr))
        hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                                   WICBitmapPaletteTypeCustom);
    if (SUCCEEDED(hr)) {
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth = LONG(tw);
        bmi.bmiHeader.biHeight = -LONG(th);   // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void *bits = nullptr;
        HBITMAP bmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bmp) hr = E_OUTOFMEMORY;
        else {
            hr = converter->CopyPixels(nullptr, tw * 4, tw * th * 4, static_cast<BYTE *>(bits));
            if (SUCCEEDED(hr)) *out = bmp;
            else DeleteObject(bmp);
        }
    }
    if (converter) converter->Release();
    if (scaler) scaler->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();
    factory->Release();
    return hr;
}

class Provider : public IInitializeWithStream, public IThumbnailProvider {
public:
    Provider() { InterlockedIncrement(&g_refs); }
    virtual ~Provider() { InterlockedDecrement(&g_refs); }

    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override {
        static const QITAB qit[] = {QITABENT(Provider, IInitializeWithStream), QITABENT(Provider, IThumbnailProvider), {nullptr, 0}};
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refs); }
    IFACEMETHODIMP_(ULONG) Release() override {
        const long r = InterlockedDecrement(&m_refs);
        if (!r) delete this;
        return r;
    }

    IFACEMETHODIMP Initialize(IStream *stream, DWORD) override {
        if (!m_text.empty()) return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
        // Read until the thumbnail value has closed; the writer puts it before the large timeline
        char buf[65536];
        ULONG got = 0;
        const size_t cap = 64u << 20;
        bool closed = false;
        while (!closed && m_text.size() < cap && SUCCEEDED(stream->Read(buf, sizeof buf, &got)) && got > 0) {
            m_text.append(buf, got);
            const size_t from = thumbStart(m_text);
            if (from != std::string::npos && m_text.find('"', from) != std::string::npos) closed = true;
        }
        return m_text.empty() ? E_FAIL : S_OK;
    }

    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *alpha) override {
        if (!phbmp) return E_POINTER;
        *phbmp = nullptr;
        std::vector<BYTE> png;
        if (!extractPng(m_text, png)) return E_FAIL;   // Explorer falls back to the file type icon
        if (alpha) *alpha = WTSAT_ARGB;
        return pngToBitmap(png, cx, phbmp);
    }

private:
    long m_refs = 1;
    std::string m_text;
};

class Factory : public IClassFactory {
public:
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override {
        static const QITAB qit[] = {QITABENT(Factory, IClassFactory), {nullptr, 0}};
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return 2; }
    IFACEMETHODIMP_(ULONG) Release() override { return 1; }
    IFACEMETHODIMP CreateInstance(IUnknown *outer, REFIID riid, void **ppv) override {
        if (outer) return CLASS_E_NOAGGREGATION;
        Provider *p = new (std::nothrow) Provider();
        if (!p) return E_OUTOFMEMORY;
        const HRESULT hr = p->QueryInterface(riid, ppv);
        p->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL lock) override {
        if (lock) InterlockedIncrement(&g_refs);
        else InterlockedDecrement(&g_refs);
        return S_OK;
    }
};

Factory g_factory;

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **ppv) {
    if (!IsEqualCLSID(clsid, CLSID_F360Thumbnail)) return CLASS_E_CLASSNOTAVAILABLE;
    return g_factory.QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow() { return g_refs == 0 ? S_OK : S_FALSE; }
