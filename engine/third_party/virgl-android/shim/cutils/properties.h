/* Minimal stand-in for AOSP's <cutils/properties.h>, using the NDK's public
 * system property API. */
#pragma once
#include <string.h>
#include <sys/system_properties.h>
#define PROPERTY_KEY_MAX PROP_NAME_MAX
#define PROPERTY_VALUE_MAX PROP_VALUE_MAX
static inline int property_get(const char* key, char* value, const char* default_value) {
    int length = __system_property_get(key, value);
    if (length > 0) return length;
    if (default_value) {
        strncpy(value, default_value, PROPERTY_VALUE_MAX - 1);
        value[PROPERTY_VALUE_MAX - 1] = '\0';
        return (int)strlen(value);
    }
    value[0] = '\0';
    return 0;
}
