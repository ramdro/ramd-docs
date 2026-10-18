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

## Install the development build (Mint 22 / Ubuntu 24.04, amd64)
Open the latest successful run in this repository's Actions tab and download
`RAMD-Docs-development-Mint22` under Artifacts. Extract the ZIP, open a terminal
in that folder, and run:

    sudo apt install ./ramd-docs-0.1.0-dev-mint22-amd64.deb

Launch RAMD Docs from the applications menu or run `ramd-docs`.
This is an early development build; use copies of important documents.

## GitHub build
Actions compiles the editor, tests the document engine and embedded editor,
checks desktop startup, and creates a development DEB artifact. The package
is also installed and launched as a regular user in a clean Ubuntu 24.04
container to check its declared dependencies. It does not publish a public release.

## Current verification
GitHub Actions on Ubuntu 24.04 passed compilation and these automated checks:
- Engine rendering, DOCX/PDF export and DOCX reopening.
- Embedded editor insertion of formatted text, accents and a table.
- Modified state, asynchronous DOCX save, PDF export without changing the
  document path, reopening and selecting the saved text, and normal shutdown.
- DOCX XML content/table checks and extracted PDF text checks.
- Desktop startup and DEB creation.

The editor test saves a screenshot in the RAMD-Docs-validation artifact.
These checks do not cover every toolbar action, physical keyboard interaction,
complex Word files or all desktop environments. Complete tests/ACCEPTANCE.md
before treating this as an everyday writing application.

## Current limitations
- Linux only in this first integration; Windows/Fedora packaging is pending.
- No recovery/autosave, revision history, comments UI, tracked-change UI,
  image insertion UI, page-margin UI, header/footer UI or print dialog yet.
- Font/style selectors apply formatting; selection state is not reflected
  in every selector yet.
- Save from the close prompt saves first; close again after it finishes.
- DOCX import/export uses LibreOffice filters. Layout can differ from Word,
  especially when the original fonts are unavailable.
- Keyboard behavior and dialogs still require manual acceptance testing.

## Architecture
LOKDocView owns rendering and text interaction.
Formatting is sent through the widget's queued UNO-command interface.
Open and save operations disable editing while in progress. Save success
is shown only after the engine's command-result callback reports success.
Documents are not converted to a limited HTML/QTextEdit representation.

## Licensing
RAMD source in this repository is supplied for the project owner's development;
a public source license has not yet been chosen. LibreOfficeKit, GTK and
their dependencies retain their own licenses. The current package depends
on system-installed LibreOffice instead of redistributing it. Resolve
distribution/source obligations before releasing bundled builds.

References:
- https://docs.libreoffice.org/libreofficekit.html
- https://github.com/LibreOffice/core/blob/master/include/LibreOfficeKit/LibreOfficeKitGtk.h
