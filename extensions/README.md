# RAMD Docs extensions

Tools → Extensions opens the manager. Four tools are bundled: Meeting notes,
Project brief, Business letter, and Today's date. Enable/disable any tool;
install or remove local text extensions without restarting the application.

Tools → Build a text extension creates a reusable text template. Enter its
name, description and text, save a JSON file, and it is installed immediately.
Share that file with other RAMD Docs users, who can install it in the manager.

The example.ramdext.json file shows the format. Required string fields are
format (ramd-docs-extension-1), name (1–80 bytes), description (up to 500 bytes)
and text (up to 60,000 bytes). The complete file must be no larger than 64 KB.
Text is inserted at the current caret or replaces selected text; undo works
through the document editor. These extensions cannot execute scripts, access
networks, add native controls or install LibreOffice OXT plugins.

Installed extensions live in $XDG_DATA_HOME/ramd-docs/extensions, defaulting
to ~/.local/share/ramd-docs/extensions. Filenames are derived from content,
not from untrusted extension names. Invalid files are ignored when loading.
