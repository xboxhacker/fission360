// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
#pragma once

class FissionDocument;
class QString;

namespace SelfTest {
// Runs the kernel regression suite headlessly; returns the number of failures.
int run();
// Times reading a mesh file and each Convert Mesh method on it (--meshbench).
int meshBench(const QString &path);
// Imports a STEP / IGES file and extrudes every face of every body (--stepbench).
int stepBench(const QString &path);
// Fills a document with a sample bracket (used by --demo and screenshots).
void buildDemo(FissionDocument *doc);
}
