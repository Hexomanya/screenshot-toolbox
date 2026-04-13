<p align="center">
  <img src="docs/titleImage_01_lowRes.jpg" alt="Tool Thumbnail" width="600"/>
</p>

# Screenshot Toolbox – Bone Manipulator

A Workbench tool for **Arma Reforger** that enables direct skeleton posing through custom viewport interaction.
This tool is designed to significantly speed up pose manipulation for screenshots taken in the Workbench by providing a custom workflow, including hand-built gizmos and overlay rendering. It integrates seamlessly with animation tracks, effectively replacing the traditional bone manipulator workflow.

---

## Overview
<p align="left">
  <img src="docs/preview_01_lowRes.jpg" alt="Tool Overview" width="600"/>
</p>

The Bone Manipulator allows you to directly pose skeletons inside the Workbench viewport by interacting with bones via custom visual handles.

Unlike the default workflow, which requires **one track per bone**, this tool only requires **one track per entity**, significantly simplifying setup and editing.

It is designed to:

* Enable fast iteration on poses
* Provide direct, visual control over individual bones
* Operate independently of mod dependencies

This project deliberately pushes the limits of what Workbench tools were originally designed for, which results in some unconventional behavior and implementation details.

---

## Features

* Direct bone selection via viewport overlays
* Custom-built gizmos for:
  * Position
  * Rotation
  * Scale
* Real-time pose manipulation
* Filter options for a clearer workspace
* Works on most entities that have bones

---

## Usage (Workbench)
1. Open your project in the Arma Reforger Workbench
2. Select the entity you want to adjust the pose of
3. If you want to work with an animation, set it up normally at this point
4. Select the **Bone Manipulator** tool at the top (bone symbol)
5. Use the **Create Component → Create Config → Create Track** buttons to set up the entity
6. Click on a bone (displayed as a sphere) to select it
7. Use hotkeys to switch manipulation modes
8. Drag the gizmo to manipulate the bone
9. Press the **Save Edits** button or exit the tool to cancel your actions

---
## Hotkeys
* <kbd>U</kbd> – Rotation gizmo
* <kbd>I</kbd> – Position gizmo
* <kbd>O</kbd> – Scale gizmo
* <kbd>J</kbd> – Reset selected bone

> Hotkeys are intentionally unusual because Workbench tools can only bind keys that are not already used by the editor.

---

## Technical Notes
This project stretches the Workbench tool system far beyond its typical use case:

* All gizmos are **fully custom-built**, not engine-provided
* Bone overlays and interaction logic are entirely manual
* Input handling is constrained by Workbench limitations
* Some UI behaviors are limited by the API

Example limitation:

* Certain settings (e.g. filters) only apply when the mouse re-enters the viewport, rather than updating immediately on change

These constraints are a direct result of the tool API limits.

---

## Installation

### As a mod

1. Download the mod from the Workshop
2. Open the Workbench and add it as a project
3. Right-click your project → Open with Addons
4. Select ScreenshotToolbox

### Manual Installation

1. Download or clone this repository
2. Copy the `scripts` folder into your project directory
3. Compile the scripts
4. Reload WB scripts

This tool does **not** need to be added as a dependency.

---

## Known Issues

* Stackable pose modifications are not visualized during editing

---

## Planned Features

* Basic IK support (hands and feet)
* Invert poses / animation frames
* Toggle for world/local position and rotation

---

## Planned Fixes

* Warning for entity names containing underscores (track compatibility)
* Gizmo code restructuring

---

## License

This project is licensed under the MIT License.
You are free to use, modify, and distribute the software, provided that the original license and copyright notice are included.

---

## Development Notes

This project was written manually. AI tools were only used for:

* Bug identification
* Formatting and documentation
* Gizmo shape generation