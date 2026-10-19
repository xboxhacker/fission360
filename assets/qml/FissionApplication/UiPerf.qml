// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) FissionCAD contributors
import QtQuick

// --uiperf <file>: inserts a mesh and walks through hover, pick, Convert and orbit in the real window, printing how
// long each step blocks and the longest gap between two drawn frames
Item {
    id: t
    property var app
    property int step: 0
    property real started: 0
    property real convertStarted: 0

    function centre(fx, fy) {
        const vp = app.viewport
        return vp.mapToItem(null, vp.width * fx, vp.height * fy)
    }
    function begin() { uiTest.markFrames(); started = uiTest.ms() }
    function report(what, detail) {
        uiTest.perf(what + " (call)", uiTest.ms() - started, detail || "")
    }
    function settle(what) {
        uiTest.perf(what + " (first frame)", uiTest.firstFrame(),
                    uiTest.frames() + " frames, worst gap " + uiTest.maxFrameGap() + " ms")
    }
    function sweep(what) {
        // 24 hover moves across the middle of the view, each one a real mouse move
        let worst = 0, total = 0
        for (let i = 0; i < 24; ++i) {
            const p = centre(0.3 + 0.4 * i / 23, 0.45 + 0.1 * Math.sin(i))
            const a = uiTest.ms()
            uiTest.mouse("move", p.x, p.y)
            const d = uiTest.ms() - a
            worst = Math.max(worst, d)
            total += d
        }
        uiTest.perf(what + " hover, mean of 24 moves", total / 24, "worst " + worst.toFixed(1) + " ms")
    }
    function orbit(what) { app.camera.yaw += 10 }

    readonly property var steps: [
        () => { fissionApp.newDocument(); return 800 },
        () => {
            begin()
            const ok = app.insertMeshFile(uiTest.perfFile)
            report("insert mesh", ok ? "" : "FAILED")
            return 2500
        },
        () => { settle("insert mesh"); return 300 },
        () => { begin(); sweep("mesh"); return 1000 },
        () => { settle("mesh hover"); return 300 },
        () => {
            begin()
            const p = centre(0.5, 0.5)
            uiTest.mouse("press", p.x, p.y)
            uiTest.mouse("release", p.x, p.y)
            report("click mesh", JSON.stringify(app.selection.items.length) + " picked")
            return 1500
        },
        () => { settle("click mesh"); begin(); orbit("mesh"); return 1500 },
        () => { settle("orbit mesh"); app.selection.clear(); return 300 },
        () => {
            begin()
            app.startCommand("MeshConvert", { direction: "toBRep" })
            report("open Convert")
            return 1500
        },
        () => { settle("open Convert"); return 300 },
        () => {
            begin()
            const p = centre(0.5, 0.5)
            uiTest.mouse("press", p.x, p.y)
            uiTest.mouse("release", p.x, p.y)
            const f = app.commandFeature
            report("pick mesh in Convert", f ? JSON.stringify(f.get("bodies")) + " " + f.error : "no command")
            convertStarted = started
            return 100
        },
        () => {
            // Wait for the background conversion, wiggling the mouse so the window keeps drawing
            const f = app.commandFeature
            if (f && !f.healthy && f.error.indexOf("background") >= 0 && uiTest.ms() - convertStarted < 60000) {
                const p = centre(0.45 + 0.1 * Math.random(), 0.5)
                uiTest.mouse("move", p.x, p.y)
                step--
                return 100
            }
            uiTest.perf("Convert ready after", uiTest.ms() - convertStarted,
                        "worst gap between frames meanwhile " + uiTest.maxFrameGap() + " ms, " + uiTest.frames() + " frames")
            uiTest.perf("Convert result", 0, f ? (f.healthy ? "ok, " + f.summary : f.error) : "no command")
            return 1500
        },
        () => {
            const f = app.commandFeature
            settle("Convert preview")
            begin()
            app.finishCommand(true)
            report("OK in Convert")
            return 5000
        },
        () => { settle("OK in Convert"); begin(); sweep("solid"); return 1000 },
        () => { settle("solid hover"); begin(); orbit("solid"); return 1500 },
        () => { settle("orbit solid"); return 300 }
    ]

    function next() {
        if (step >= steps.length) { uiTest.quit(); return }
        let delay = 600
        try {
            delay = steps[step]() || 600
        } catch (e) {
            uiTest.perf("step " + step + " threw", 0, String(e))
        }
        ++step
        timer.interval = delay
        timer.start()
    }

    Timer { id: timer; interval: 1500; onTriggered: t.next() }
    Component.onCompleted: timer.start()
}
