// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#include "ShellIntegration.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>

namespace {

const wchar_t *kClsid = L"{6C1E3F9A-2B47-4E8D-9A51-F3600B1D7C42}";
const wchar_t *kThumbHandler = L"{e357fccd-a995-4576-b01f-234630154e96}";   // IThumbnailProvider
const wchar_t *kProgId = L"Fission360.Design";

std::wstring w(const QString &s) { return s.toStdWString(); }

bool setValue(const std::wstring &key, const wchar_t *name, const std::wstring &value) {
    return RegSetKeyValueW(HKEY_CURRENT_USER, (L"Software\\Classes\\" + key).c_str(), name, REG_SZ, value.c_str(),
                           DWORD((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

std::wstring getValue(const std::wstring &key, const wchar_t *name) {
    wchar_t buf[2048];
    DWORD size = sizeof buf;
    if (RegGetValueW(HKEY_CURRENT_USER, (L"Software\\Classes\\" + key).c_str(), name, RRF_RT_REG_SZ, nullptr, buf, &size)
        != ERROR_SUCCESS)
        return {};
    return buf;
}

QString exePath() { return QDir::toNativeSeparators(QCoreApplication::applicationFilePath()); }
QString dllPath() { return QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath()).filePath("Fission360Thumbs.dll")); }

} // namespace

bool ShellIntegration::registerFileType(bool force) {
    const QString exe = exePath(), dll = dllPath();
    const bool haveDll = QFileInfo::exists(dll);
    const std::wstring clsidKey = std::wstring(L"CLSID\\") + kClsid;
    const std::wstring command = L"\"" + w(exe) + L"\" \"%1\"";
    if (!force && getValue(std::wstring(kProgId) + L"\\shell\\open\\command", nullptr) == command
        && (!haveDll || getValue(clsidKey + L"\\InprocServer32", nullptr) == w(dll)))
        return false;
    bool ok = setValue(L".f360", nullptr, kProgId);
    ok &= setValue(L".f360", L"Content Type", L"application/x-fission360");
    ok &= setValue(L".f360", L"PerceivedType", L"document");
    ok &= setValue(kProgId, nullptr, L"Fission 360 Design");
    ok &= setValue(std::wstring(kProgId) + L"\\DefaultIcon", nullptr, L"\"" + w(exe) + L"\",0");
    ok &= setValue(std::wstring(kProgId) + L"\\shell\\open\\command", nullptr, command);
    if (haveDll) {
        ok &= setValue(clsidKey, nullptr, L"Fission 360 Thumbnail");
        ok &= setValue(clsidKey + L"\\InprocServer32", nullptr, w(dll));
        ok &= setValue(clsidKey + L"\\InprocServer32", L"ThreadingModel", L"Apartment");
        ok &= setValue(std::wstring(L".f360\\ShellEx\\") + kThumbHandler, nullptr, kClsid);
        ok &= setValue(std::wstring(kProgId) + L"\\ShellEx\\" + kThumbHandler, nullptr, kClsid);
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return ok;
}

bool ShellIntegration::unregisterFileType() {
    auto del = [](const std::wstring &key) {
        return RegDeleteTreeW(HKEY_CURRENT_USER, (L"Software\\Classes\\" + key).c_str());
    };
    if (getValue(L".f360", nullptr) == kProgId) del(L".f360");
    del(kProgId);
    del(std::wstring(L"CLSID\\") + kClsid);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

QString ShellIntegration::status() {
    const QString open = QString::fromStdWString(getValue(std::wstring(kProgId) + L"\\shell\\open\\command", nullptr));
    const QString thumb = QString::fromStdWString(getValue(std::wstring(L"CLSID\\") + kClsid + L"\\InprocServer32", nullptr));
    const QString handler = QString::fromStdWString(getValue(std::wstring(L".f360\\ShellEx\\") + kThumbHandler, nullptr));
    return QString(".f360 opens with: %1\nthumbnail handler: %2 (%3)")
        .arg(open.isEmpty() ? "not registered" : open, thumb.isEmpty() ? "not registered" : thumb,
             handler.isEmpty() ? "not linked" : "linked");
}

#else
bool ShellIntegration::registerFileType(bool) { return false; }
bool ShellIntegration::unregisterFileType() { return false; }
QString ShellIntegration::status() { return "File type registration is Windows only"; }
#endif
