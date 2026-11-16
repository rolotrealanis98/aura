/* SPDX-License-Identifier: MIT AND GPL-2.0-only */

#ifndef CONFIG_H
#define CONFIG_H

// Enable framebuffer console
#define USE_FB
// Disable framebuffer console unless verbose boot is enabled
//#define FB_SILENT_MODE
// Initialize USB early and break into proxy if device is opened within this time (sec)
//#define EARLY_PROXY_TIMEOUT 5

// Minimal build for bring-up
//#define BRINGUP
// Disable display configuration / bringup on desktop devices
//#define NO_DISPLAY

// Print RTKit logs to the console
//#define RTKIT_SYSLOG

// Target for device-specific debug builds
//#define TARGET T8103
// Some devices like Apple TV HD use other uarts for debug console
//#define TARGET_BOARD 0x34

// Switch the DFU USB-C port to debugusb
// #define USE_DEBUG_USB

// Change the 'channel' used by the dockchannel-uart iodev (if unset: 1)
// #define DOCKCHANNEL_UART_CHANNEL 0

// A plain (non-chainloading) stage 2 brings up the USB proxy before it boots its payload and waits
// this long (ms) for a host to open it. A host that does can upload a test image into RAM and
// chainload it (P_CHAINLOAD_IMAGE) or send P_EXIT to boot the payload; nobody there: the payload
// boots as before. 0 disables the window. Stage 1 and ESP stage 2 builds keep their own windows.
#if !defined(CHAINLOADING) && !defined(HOSTWAIT_MS)
# define HOSTWAIT_MS 4000
#endif

// A plain stage 2 started with m1n1.wdt=keep that could not start its payload waits this long (ms)
// for a host to open the proxy, then reboots so that its stage 1 starts the fallback slot (main.c).
#if !defined(CHAINLOADING) && !defined(STAGE2_FALLBACK_WINDOW_MS)
# define STAGE2_FALLBACK_WINDOW_MS 30000
#endif

#ifdef RELEASE
# define FB_SILENT_MODE
#endif

#endif
