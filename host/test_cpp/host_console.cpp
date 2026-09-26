/**
 * Minimal console host runner: inject keys into HostAppleIIMachine.
 *
 * Usage: host/.out/host_console.exe
 * Commands: type printable chars; "quit" to exit; "reset" Apple reset;
 *           "peek C000" diagnostic peek; "run N" run N cycles.
 */
#include "esp_bracket/apple2_machine_host.hpp"
#include "esp_bracket/text_decoder.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace esp_bracket;

static void printTextPage(const Apple2Bus& bus) {
    uint8_t chars[40 * 24];
    TextDecoder::decodeScreen(bus.ram(), 0x0400, chars);
    std::printf("---- TEXT PAGE1 ----\n");
    for (int r = 0; r < 24; ++r) {
        for (int c = 0; c < 40; ++c) {
            char ch = static_cast<char>(chars[r * 40 + c] & 0x7F);
            if (ch < 0x20 || ch > 0x7E) {
                ch = '.';
            }
            std::fputc(ch, stdout);
        }
        std::fputc('\n', stdout);
    }
}

int main() {
    HostAppleIIMachine m;
    if (m.loadSyntheticRom() != RomError::Ok) {
        std::fprintf(stderr, "synthetic ROM load failed\n");
        return 1;
    }
    m.reset();
    m.runCycles(50000);
    printTextPage(m.bus());
    std::printf("host_console: keys inject to Apple II latch; 'quit' exits\n");

    char line[256];
    while (std::fgets(line, sizeof(line), stdin)) {
        if (std::strncmp(line, "quit", 4) == 0) {
            break;
        }
        if (std::strncmp(line, "reset", 5) == 0) {
            m.reset();
            continue;
        }
        if (std::strncmp(line, "run ", 4) == 0) {
            const unsigned long n = std::strtoul(line + 4, nullptr, 10);
            m.runCycles(static_cast<uint32_t>(n));
            continue;
        }
        if (std::strncmp(line, "peek ", 5) == 0) {
            const unsigned long a = std::strtoul(line + 5, nullptr, 16);
            std::printf("%04lX -> %02X\n", a,
                        m.bus().peek(static_cast<uint16_t>(a)));
            continue;
        }
        if (std::strncmp(line, "text", 4) == 0) {
            printTextPage(m.bus());
            continue;
        }
        // Inject first character of line
        if (line[0] && line[0] != '\n') {
            m.keyDown(static_cast<uint8_t>(line[0]));
            m.runCycles(1000);
            std::printf("injected '%c' latch=%02X\n", line[0],
                        m.bus().keyboard().latch());
        }
    }
    return 0;
}
