// domain: pure C++ values and rules. No macOS headers in this layer.
//
// M1: Screenshot lives in domain/screenshot.h. The scanner itself belongs in
// adapters/macos/screenshots.cpp because it touches the filesystem and xattr.
//
// See docs/ARCHITECTURE.md, section 4 and milestone M1.

#include "domain/screenshot.h"