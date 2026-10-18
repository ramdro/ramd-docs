# RAMD Docs 0.3 — extended development preview

A native document workspace by RAMD Studios, powered by LibreOffice Writer.
This is an expanded development build, not a finished Microsoft Word replacement.

## Document tools
- File, Edit, Insert, Format, View, Layout, Tools and Help menus.
- Grouped Home, Insert, Layout, Extensions and View tabs; dedicated app icon.
- Open DOCX/ODT/DOC/RTF; save DOCX or existing ODT; export PDF.
- Fonts, paragraph styles, alignment, lists, indentation and formatting states.
- Configurable tables, embedded images, page-number fields and orientation.
- Find, zoom, keyboard shortcuts, text clipboard controls and live word count.

## Extensions
Tools → Extensions manages bundled and installed text tools. Tools → Build a
text extension creates and installs a reusable template. Four tools are included:
Meeting notes, Project brief, Business letter and Today's date.
See extensions/README.md and the example JSON. These are declarative text
extensions, not executable plugins or LibreOffice OXT packages.

## Recovery
While a document has unsaved changes, a recovery copy is written every two
minutes. File → Write recovery copy now writes one immediately. It does not
save or rename the original document. File → Open recovery copy opens an ODT
snapshot as a new document; use Save As to keep it.

Snapshots are stored in ~/.local/share/ramd-docs/recovery (or under
$XDG_DATA_HOME). A normal save or clean exit removes this session's snapshot;
an interrupted session leaves it for manual recovery through the File menu.
Changes made since the last successful snapshot can still be lost.

## Install (Mint 22.x / Ubuntu 24.04, amd64)
Download the DEB from the 0.3 development release, then:

    sudo apt install ./ramd-docs-0.3.0-dev-mint22-amd64.deb

Use copies of important documents while testing. Existing installations are
upgraded by this command. Windows, Fedora and macOS builds are not provided.

## Build
    sudo apt install build-essential cmake ninja-build pkg-config libgtk-3-dev libjson-glib-dev libreofficekit-dev liblibreofficekitgtk libreoffice-writer libreoffice-gtk3 adwaita-icon-theme librsvg2-common
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --parallel 2
    ./build/ramd-docs

The Linux workflow builds and runs document-engine, editor and clean-package
installation checks. Its validation artifact includes screenshots and saved
files. Check the workflow result for the exact revision being downloaded.
Manual acceptance criteria are in tests/ACCEPTANCE.md.

## Still limited
No comments or tracked-changes interface, collaboration, header/footer editor,
page-margin editor or integrated print dialog. Export PDF for printing.
Font/style selectors apply formatting but do not always follow the selection.
Toolbar clipboard actions transfer plain text. Complex Word layouts can differ.
Save from the close prompt requires closing again after the save completes.

## Architecture and licensing
C++/GTK3 embeds the official LibreOfficeKit editing widget. Commands are queued
through its worker interface. Recovery uses a separate ODT copy operation.
RAMD source has not been assigned a public source license. Dependencies retain
their own licenses; the DEB uses system packages rather than bundling LibreOffice.
