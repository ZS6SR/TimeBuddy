#pragma once

// Copy to locations.h and set your own three locations.
// posixTz is a POSIX TZ string, e.g. "GMT0BST,M3.5.0/1,M10.5.0" for London.
static const Location LOCATIONS[3] = {
    {"London", "United Kingdom", "GMT0BST,M3.5.0/1,M10.5.0", 51.5074f, -0.1278f},
    {"New York", "United States", "EST5EDT,M3.2.0,M11.1.0", 40.7128f, -74.0060f},
    {"Tokyo", "Japan", "JST-9", 35.6762f, 139.6503f},
};
