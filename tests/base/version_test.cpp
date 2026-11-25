#include <cstdio>
#include <cstring>

#include "nopeui/base/version.h"

int main() {
    const char* version = nopeui::VersionString();

    if (version == nullptr || std::strcmp(version, "0.0.0.0") != 0) {
        std::fprintf(stderr, "unexpected version: %s\n", version == nullptr ? "(null)" : version);
        return 1;
    }

    std::puts("nopeui_base version test passed");
    return 0;
}
