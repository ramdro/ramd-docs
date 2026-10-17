# RAMD Docs — development source 0.1

A native document-writing workspace by RAMD Studios, using the real
LibreOfficeKit document engine. This is an initial source implementation,
not a finished or validated Word replacement.

## Implemented in source
- Embedded Writer canvas with document pagination, typing and selection.
- Open DOCX, ODT, DOC and RTF.
- New A4 document; save as DOCX; save existing ODT; export PDF.
- Paragraph styles, font family/size, bold/italic/underline, alignment,
  lists, page breaks, fixed 3×3 table insertion, find and zoom.
- Unsaved-change prompts and asynchronous save completion handling.
- Separate temporary LibreOffice profile.

The interface uses C++ and GTK3 because LibreOffice provides an integrated
editing widget for GTK3. It does not launch a separate Writer window.

## Build on Mint 22 / Ubuntu 24.04

    sudo apt update
    sudo apt install build-essential cmake ninja-build pkg-config libgtk-3-dev libjson-glib-dev libreofficekit-dev liblibreofficekitgtk libreoffice-writer libreoffice-gtk3
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --parallel 2
    ./build/ramd-docs

An existing document can be passed as the first command-line argument.

## GitHub build
Upload the CONTENTS of this folder to a new ramd-docs repository, including
.github/workflows/linux.yml. Actions compiles the editor, runs an engine
DOCX/PDF roundtrip check, checks desktop startup, and produces a development
DEB artifact. It does not publish a public release.

## Current verification
The build dependencies could not be installed in the development workspace
because the environment denies the user/group changes required by apt.
Compilation, engine tests and GUI interaction tests have NOT run yet.
The included GitHub workflow must succeed before distributing the build.
Engine checks do not prove the GTK save commands or every formatting action
work; use the manual acceptance checklist too.

## Current limitations
- Linux only in this first integration; Windows/Fedora packaging is pending.
- No recovery/autosave, revision history, comments UI, tracked-change UI,
  image insertion UI, page-margin UI, header/footer UI or print dialog yet.
- Font/style selectors apply formatting; selection state is not reflected
  in every selector yet.
- Save from the close prompt saves first; close again after it finishes.
- DOCX import/export uses LibreOffice filters. Layout can differ from Word,
  especially when the original fonts are unavailable.
- Engine save commands, keyboard behavior and dialogs require integration
  validation before this is suitable for everyday writing.

## Architecture
LOKDocView owns rendering and text interaction.
Formatting is sent through the widget's queued UNO-command interface.
Open and save operations disable editing while in progress. Save success
is shown only after the engine's command-result callback reports success.
Documents are not converted to a limited HTML/QTextEdit representation.

## Licensing
RAMD source in this archive is supplied for the project owner's development;
a public source license has not yet been chosen. LibreOfficeKit, GTK and
their dependencies retain their own licenses. The current package depends
on system-installed LibreOffice instead of redistributing it. Resolve
distribution/source obligations before releasing bundled builds.

References:
- https://docs.libreoffice.org/libreofficekit.html
- https://github.com/LibreOffice/core/blob/master/include/LibreOfficeKit/LibreOfficeKitGtk.h
