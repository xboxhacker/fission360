// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once
#include <QString>

// Windows file type registration for .f360, per user (no administrator rights): icon, double-click to
// open, and the Explorer thumbnail handler (Fission360Thumbs.dll next to the exe)
namespace ShellIntegration {
// Writes the keys when they are missing or point at another copy of the app; returns true when written
bool registerFileType(bool force = false);
bool unregisterFileType();
QString status();
}
