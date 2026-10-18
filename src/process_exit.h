#pragma once
#include <cstdio>
#include <cstdlib>
// Ubuntu 24.04 LibreOfficeKit can double-destroy locale globals during CRT
// teardown after the office instance has already been destroyed (tdf#165025).
// Call only AFTER explicit application/document/widget cleanup. Preserve the
// real result code and flush streams; skip only process-global destructors.
[[noreturn]] inline void finishProcess(int status) {
    std::fflush(nullptr);
    std::_Exit(status);
}
