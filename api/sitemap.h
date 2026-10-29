#pragma once

#include "../defines.h"

#include <cstdint>

// Revision of the stored site map; increments on every change and is 0 when no map has been saved.
// Reported as "smap" in /jo and /ja so apps can detect support and notice changes.
uint32_t sitemap_revision();

#if defined(SITEMAP_DEVICE_IMAGE)
// Whether the image is being relayed between apps: 0 no, 1 an app is waiting for one that has it, 2 it is being sent.
// Reported as "smx" in /jo and /ja so apps with the image notice that another app needs it.
uint8_t sitemap_relay_summary();
#endif

// Removes the temporary files of uploads and map writes that a restart interrupted; call once the file system is ready.
void sitemap_remove_temp_files();
