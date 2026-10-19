# Third-party notices

Fission 360 is free software licensed under the **GNU General Public License v3.0 or later**
(see `LICENSE`). It is built on, and ships with, the open-source components below. Each remains under
its own licence; the notices here are provided to meet those licences.

## Source code offer

The complete corresponding source of Fission 360 is published in the project's GitHub repository.
For every binary release, the matching source is the repository tag with the same version number
(shown in Help > About). The libraries below are shipped as unmodified dynamic libraries (DLLs), so
you can replace them with your own builds.

## Components

| Component | Version | Licence | Used for |
|---|---|---|---|
| **Qt** (Core, Gui, Qml, Quick, Quick Controls 2, Quick Dialogs, Quick Shapes, Svg, Widgets, OpenGL, Network, Test, WebChannel, Positioning) | 6.8.2 | LGPL-3.0 / GPL-3.0 (open-source edition) | UI, rendering, networking |
| **Qt WebEngine** (includes **Chromium**) | 6.8.2 | LGPL-3.0; Chromium and its bundled components under BSD-3-Clause, MIT, Apache-2.0, MPL-2.0 and others | The McMaster-Carr parts browser dialog |
| **Open CASCADE Technology (OCCT)** | 7.6 | LGPL-2.1 with the Open CASCADE exception | B-rep modelling kernel, STEP/IGES/STL I/O |
| **FreeType** (bundled with Qt) | as shipped by Qt | FreeType Licence (BSD-style) or GPL-2.0 | Text rendering |

Licence texts: LGPL-3.0 <https://www.gnu.org/licenses/lgpl-3.0.html>, LGPL-2.1
<https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html>, Qt licensing <https://www.qt.io/licensing/>,
Chromium licences in `chrome://credits` of any Chromium-based browser and in the Qt WebEngine sources,
Open CASCADE <https://dev.opencascade.org/resources/licensing>.

## Build-time dependency

The OCCT build used for development comes from the dependency build of **OrcaSlicer**
(<https://github.com/SoftFever/OrcaSlicer>, AGPL-3.0). Fission 360 does not include any OrcaSlicer
source code. Point `-DFISSION_ORCA_ROOT=<checkout>` at a built OrcaSlicer checkout, or provide
your own OCCT 7.6 build.

## Slicer integration and trademarks

*Send to Slicer* writes a temporary STL and launches **OrcaSlicer** (AGPL-3.0) or **Bambu Studio**
(AGPL-3.0) as a separate program. Fission 360 does not link to or include either of them.

* The OrcaSlicer logo (`assets/icons/orca.svg`, `orca.png`) is copied from the OrcaSlicer
  distribution (AGPL-3.0) and is used only to identify that program.
* "OrcaSlicer", "Bambu Lab" and "Bambu Studio" are names and trademarks of their respective owners.
  Fission 360 is not affiliated with, endorsed by, or sponsored by them.

## Other trademarks and reference material

* "Fusion" and "Autodesk" are trademarks of Autodesk, Inc. Fission 360 is not affiliated with or
  endorsed by Autodesk. Fusion's behaviour, UI layout and public API documentation were used as
  reference only. No Autodesk code, QML, icons or other assets are included in Fission 360.
* "McMaster-Carr" is a trademark of McMaster-Carr Supply Company. The parts dialog displays their
  public website; Fission 360 does not redistribute their catalogue or CAD files.
