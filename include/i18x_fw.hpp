#pragma once

#include <cstddef>

/** Minimal firmware i18x — semantic keys, en-US fallback, de-DE table. */
namespace esp2_i18x {

enum class Locale : unsigned char { EnUs = 0, DeDe = 1 };

void setLocale(Locale loc);
Locale locale();

/** Look up key; never returns nullptr (falls back to key itself). */
const char *t(const char *key);

} // namespace esp2_i18x
