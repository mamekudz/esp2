#include "i18x_fw.hpp"

#include <cstring>

namespace esp2_i18x {
namespace {

Locale g_locale = Locale::EnUs;

struct Entry {
    const char *key;
    const char *en;
    const char *de;
};

// Firmware UI strings for USB storage ownership screen.
static const Entry kTable[] = {
    {"usb_storage.title", "USB STORAGE", "USB-SPEICHER"},
    {"usb_storage.mounted", "SD card mounted", "SD-Karte gemountet"},
    {"usb_storage.on_computer", "on computer", "am Computer"},
    {"usb_storage.eject_hint", "Eject in Windows", "In Windows auswerfen"},
    {"usb_storage.before_return", "before returning", "vor der Rueckgabe"},
    {"usb_storage.unavailable", "SD unavailable", "SD nicht verfuegbar"},
    {"usb_storage.busy_esp", "ESP][ owns SD", "ESP][ besitzt SD"},
};

} // namespace

void setLocale(Locale loc) {
    g_locale = loc;
}

Locale locale() {
    return g_locale;
}

const char *t(const char *key) {
    if (!key) {
        return "";
    }
    for (const Entry &e : kTable) {
        if (std::strcmp(e.key, key) == 0) {
            return (g_locale == Locale::DeDe) ? e.de : e.en;
        }
    }
    return key;
}

} // namespace esp2_i18x
