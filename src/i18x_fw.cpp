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
    {"cc.title", "ESP][ Menu", "ESP][ Menue"},
    {"cc.back", "Back", "Zurueck"},
    {"cc.resume", "Resume Apple II", "Apple II fortsetzen"},
    {"cc.resume_short", "Resume", "Weiter"},
    {"cc.btn_hint", "BOOT next | KEY OK | hold back | PWR power",
     "BOOT weiter | KEY OK | halten zurueck | PWR Strom"},
    {"cc.page_prev", "Prev", "Hoch"},
    {"cc.page_next", "Next", "Runter"},
    {"cc.display", "Display", "Anzeige"},
    {"cc.disks", "Disks", "Disketten"},
    {"cc.input", "Input", "Eingabe"},
    {"cc.bluetooth", "Bluetooth", "Bluetooth"},
    {"cc.storage", "Storage", "Speicher"},
    {"cc.settings", "Settings", "Einstellungen"},
    {"cc.reset", "Reset / System", "Reset / System"},
    {"cc.orient.classic", "Classic", "Classic"},
    {"cc.orient.landscape", "Landscape", "Landscape"},
    {"cc.monitor.white", "White", "Weiss"},
    {"cc.monitor.green", "Green", "Gruen"},
    {"cc.monitor.amber", "Amber", "Amber"},
    {"cc.monitor.artifact", "Artifact", "Artifact"},
    {"cc.monitor.mono_note", "Mono RLCD — no color modes", "Mono-RLCD — keine Farbmodi"},
    {"cc.effect.clean", "Clean", "Clean"},
    {"cc.effect.crt", "CRT", "CRT"},
    {"cc.invert.normal", "Normal (dark on light)", "Normal (dunkel auf hell)"},
    {"cc.invert.invert", "Inverted (light on dark)", "Invertiert (hell auf dunkel)"},
    {"cc.scale.integer", "1:1 (sharp)", "1:1 (scharf)"},
    {"cc.scale.full", "Fit panel (full)", "Vollflaechig"},
    {"cc.drive1", "Drive 1", "Laufwerk 1"},
    {"cc.drive2_unavail", "Drive 2 (not yet)", "Laufwerk 2 (noch nicht)"},
    {"cc.disk.empty", "Empty", "Leer"},
    {"cc.disk.mount", "Mount image", "Abbild einlegen"},
    {"cc.disk.eject", "Eject", "Auswerfen"},
    {"cc.input.keyboard", "Keyboard", "Tastatur"},
    {"cc.input.gamepad", "Gamepad", "Gamepad"},
    {"cc.input.pointer", "Pointer", "Zeiger"},
    {"cc.input.paddle", "Paddle", "Paddle"},
    {"cc.input.joytest", "Joystick test", "Joystick-Test"},
    {"cc.joy.hint", "Move stick, press buttons", "Stick bewegen, Tasten druecken"},
    {"cc.owner.esp2", "ESP][", "ESP]["},
    {"cc.owner.computer", "Computer", "Computer"},
    {"cc.sd.present", "SD present", "SD vorhanden"},
    {"cc.sd.missing", "No SD card", "Keine SD-Karte"},
    {"cc.sd.fat", "Filesystem FAT", "Dateisystem FAT"},
    {"cc.sd.no_fs", "Filesystem none", "Kein Dateisystem"},
    {"cc.sd.share", "Share with computer", "Mit Computer teilen"},
    {"cc.sd.share_warn", "Apple II cannot use disks while the computer owns the SD.",
     "Apple II kann Disketten nicht nutzen, solange der Computer die SD hat."},
    {"cc.sd.return", "Return to ESP][", "An ESP][ zurueckgeben"},
    {"cc.reset.apple", "Apple II Reset", "Apple-II-Reset"},
    {"cc.reset.cold", "Apple II Cold Start", "Apple-II-Kaltstart"},
    {"cc.reset.esp", "Restart ESP][", "ESP][ neu starten"},
    {"cc.confirm", "Confirm", "Bestaetigen"},
    {"cc.cancel", "Cancel", "Abbrechen"},
    {"cc.confirm_body", "This cannot be undone from here.",
     "Das laesst sich hier nicht rueckgaengig machen."},
    {"cc.locale.en", "English", "English"},
    {"cc.locale.de", "Deutsch", "Deutsch"},
    {"cc.err.no_sd", "No SD card", "Keine SD-Karte"},
    {"cc.err.disk", "Disk image unavailable", "Diskettenabbild nicht verfuegbar"},
    {"cc.err.ble_pair", "BLE pairing failed", "BLE-Kopplung fehlgeschlagen"},
    {"cc.err.ble_off", "Bluetooth not available on this build", "Bluetooth in diesem Build nicht verfuegbar"},
    {"cc.err.msc_unsupported", "USB Mass Storage not available", "USB-Massenspeicher nicht verfuegbar"},
    {"cc.err.sd_computer", "SD card is being used by computer", "SD-Karte wird vom Computer benutzt"},
    {"settings.title", "Settings", "Einstellungen"},
    {"settings.back", "Back", "Zurueck"},
    {"settings.input_devices", "Input devices", "Eingabegeraete"},
    {"settings.hint_touch", "Tap row to select", "Zeile antippen"},
    {"ble.add_device", "Add BLE device", "BLE-Geraet hinzufuegen"},
    {"ble.devices", "Devices", "Geraete"},
    {"ble.searching", "Searching...", "Suche..."},
    {"ble.scan_done", "Scan done", "Scan fertig"},
    {"ble.no_devices", "No HID devices found", "Keine HID-Geraete"},
    {"ble.no_known", "No known devices", "Keine gespeicherten Geraete"},
    {"ble.tap_to_pair", "Tap device to pair", "Geraet antippen zum Koppeln"},
    {"ble.pair_success", "Paired", "Gekoppelt"},
    {"ble.device_info", "Device", "Geraet"},
    {"ble.action.connect", "Connect", "Verbinden"},
    {"ble.action.disconnect", "Disconnect", "Trennen"},
    {"ble.action.forget", "Forget", "Vergessen"},
    {"ble.forget_confirm_title", "Forget device?", "Geraet vergessen?"},
    {"ble.forget_confirm_body", "Bond removed; reconnect needs Add BLE.", "Bond geloescht; neu koppeln noetig."},
    {"ble.forget_yes", "Forget", "Vergessen"},
    {"ble.forget_no", "Cancel", "Abbrechen"},
    {"ble.state.connected", "Connected", "Verbunden"},
    {"ble.state.connecting", "Connecting...", "Verbinde..."},
    {"ble.state.pairing", "Pairing...", "Kopple..."},
    {"ble.state.error", "Failed", "Fehlgeschlagen"},
    {"ble.state.discovered", "Discovered", "Gefunden"},
    {"ble.state.offline", "Offline", "Offline"},
    {"ble.kind.keyboard", "Keyboard", "Tastatur"},
    {"ble.kind.gamepad", "Gamepad", "Gamepad"},
    {"ble.kind.hid", "Input", "Eingabe"},
    {"ble.kind.composite", "Keyboard+Pad", "Tastatur+Pad"},
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
