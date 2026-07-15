// Clody - Retro E-Paper Companion (Pala Note)
// Designed for the 200x200 1-bit E-paper board.
//
// 2-Button navigation system:
//   BTN_1 (GPIO0, "REC")  -> SELECT / NEXT (cycles menus and options)
//   BTN_2 (GPIO18, "PWR") -> CONFIRM / ENTER (activates actions, inputs choice)
//
// No external libraries like Adafruit_GFX are required for rendering.
// This code is completely self-contained and highly robust.

#include "Arduino.h"
#include "config.h"
#include "sounds.h"
#include "src/display/epaper_driver_bsp.h"
#include "src/power/board_power_bsp.h"
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>

extern "C" {
#include "src/audio/audio_bsp.h"
}

// Display dimensions
#define W 200
#define H 200
#define BLACK DRIVER_COLOR_BLACK
#define WHITE DRIVER_COLOR_WHITE

// Shared display instances
board_power_bsp_t      board(EPD_PWR_PIN, AUDIO_PWR_PIN, VBAT_PWR_PIN);
epaper_driver_display* display = nullptr;

// -----------------------------------------------------------------------------
// 1-Bit Bitmaps (Custom Pixel Art)
// -----------------------------------------------------------------------------

// 8x8 Heart (Full and Empty)
static const uint8_t HEART_FULL[] = {
  0x66, //  **  **
  0xFF, // ********
  0xFF, // ********
  0x7E, //  ******
  0x3C, //   ****
  0x18, //    **
  0x00,
  0x00
};

static const uint8_t HEART_EMPTY[] = {
  0x66, //  **  **
  0x99, // *  **  *
  0x81, // *      *
  0x42, //  *    *
  0x24, //   *  *
  0x18, //    **
  0x00,
  0x00
};

// 16x16 Menu Icons
static const uint8_t ICON_FEED[] = {
  0x00, 0x00,
  0x00, 0x70, //      ***
  0x00, 0xF8, //     *****
  0x01, 0xFC, //    *******
  0x03, 0xFE, //   *********
  0x07, 0xFE, //  **********
  0x0F, 0xFC, // ***********
  0x1F, 0xF8, // ************
  0x1F, 0xF0, // ***********
  0x0F, 0xC0, //  ********
  0x07, 0x80, //   ******
  0x0F, 0x80, //  ********
  0x19, 0x80, // **  **  *
  0x30, 0x00, // **
  0x20, 0x00, // *
  0x00, 0x00
};

static const uint8_t ICON_PLAY[] = {
  0x00, 0x00,
  0x3F, 0xFC, //   **********
  0x7F, 0xFE, //  ************
  0xE0, 0x07, // ***        ***
  0xC4, 0x23, // **   *    *  **
  0xCE, 0x73, // **  ***  *** **
  0xC4, 0x23, // **   *    *  **
  0xD0, 0x0B, // ** *        ***
  0xDF, 0xFB, // ************* *
  0xCF, 0xF3, // ** ********** **
  0xC0, 0x03, // **            **
  0x60, 0x06, //  **          **
  0x3F, 0xFC, //   **********
  0x1F, 0xF8, //    ********
  0x00, 0x00,
  0x00, 0x00
};

static const uint8_t ICON_CLEAN[] = {
  0x00, 0x00,
  0x00, 0x18, //         **
  0x00, 0x30, //        **
  0x00, 0x60, //       **
  0x00, 0xC0, //      **
  0x01, 0x80, //     **
  0x03, 0x00, //    **
  0x06, 0x00, //   **
  0x0C, 0x00, //  **
  0x1F, 0xF0, //  ************
  0x3F, 0xF8, // *************
  0x30, 0x18, // **        **
  0x30, 0x18, // **        **
  0x7F, 0xFE, // ****************
  0xFF, 0xFF, // ****************
  0x00, 0x00
};

static const uint8_t ICON_HEAL[] = {
  0x00, 0x00,
  0x7F, 0xFE, //  **************
  0xFF, 0xFF, // ****************
  0xC0, 0x03, // **            **
  0xC3, 0xC3, // **   ****     **
  0xC3, 0xC3, // **   ****     **
  0xCF, 0xF3, // ** ********** **
  0xCF, 0xF3, // ** ********** **
  0xCF, 0xF3, // ** ********** **
  0xC3, 0xC3, // **   ****     **
  0xC3, 0xC3, // **   ****     **
  0xC0, 0x03, // **            **
  0xFF, 0xFF, // ****************
  0x7F, 0xFE, //  **************
  0x00, 0x00,
  0x00, 0x00
};

static const uint8_t ICON_SLEEP[] = {
  0x00, 0x00,
  0x03, 0xC0, //     ****
  0x0F, 0x00, //   ****
  0x1E, 0x04, //  ****      *
  0x3C, 0x00, // ****
  0x38, 0x10, // ***      *
  0x78, 0x00, // ****
  0x78, 0x00, // ****
  0x7C, 0x00, // *****
  0x3E, 0x20, //  *****   *
  0x3F, 0x00, //  ******
  0x1F, 0xC0, //   *******
  0x0F, 0xF0, //    **********
  0x03, 0xFC, //      ********
  0x00, 0x00,
  0x00, 0x00
};

static const uint8_t ICON_STATS[] = {
  0x00, 0x00,
  0x0F, 0xF0, //    **********
  0x1F, 0xF8, //   ************
  0x13, 0xC8, //   *  ****  *  *
  0x1F, 0xF8, //   ************
  0x1F, 0xF8, //   ************
  0x10, 0x08, //   *          *
  0x17, 0xE8, //   *  ******  *
  0x10, 0x08, //   *          *
  0x13, 0xC8, //   *  ****    *
  0x10, 0x08, //   *          *
  0x17, 0xE8, //   *  ******  *
  0x10, 0x08, //   *          *
  0x1F, 0xF8, //   ************
  0x0F, 0xF0, //    **********
  0x00, 0x00
};

static const uint8_t ICON_EXIT[] = {
  0x00, 0x00,
  0x3F, 0xFC, //   **********
  0x7F, 0xFE, //  ************
  0xE0, 0x07, // ***        ***
  0xC3, 0xC3, // **   ****   **
  0xC1, 0x83, // **    **    **
  0xC0, 0x03, // **            **
  0xC3, 0xC3, // **   ****   **
  0xC6, 0x63, // **  **  **  **
  0xCC, 0x33, // ** **    ** **
  0xD8, 0x1B, // ** **    ** **
  0xF0, 0x0F, // ****      ****
  0xE0, 0x07, // ***        ***
  0x7F, 0xFE, //  ************
  0x3F, 0xFC, //   **********
  0x00, 0x00
};

// 16x16 Poop Icon
static const uint8_t ICON_POOP[] = {
  0x01, 0x80, //      **
  0x03, 0xC0, //     ****
  0x06, 0x60, //    **  **
  0x0C, 0x30, //   **    **
  0x1F, 0xF8, //  **********
  0x3F, 0xFC, // ************
  0x7C, 0x3E, // *****  *****
  0x78, 0x1E, // ****    ****
  0xF0, 0x0F, // ********
  0xFF, 0xFF, // ****************
  0x7F, 0xFE, //  **************
  0x00, 0x00,
  0x00, 0x00,
  0x00, 0x00,
  0x00, 0x00,
  0x00, 0x00
};

// 16x16 Sickness Skull
static const uint8_t ICON_SKULL[] = {
  0x07, 0xE0, //    ******
  0x1F, 0xF8, //   **********
  0x3F, 0xFC, //  ************
  0x7F, 0xFE, // **************
  0x63, 0xC6, // **  ******  **
  0x63, 0xC6, // **  ******  **
  0x7F, 0xFE, // **************
  0x3F, 0xFC, //  ************
  0x1F, 0xF8, //   **********
  0x1F, 0xF8, //   **********
  0x1B, 0xD8, //   ** **  ** **
  0x1B, 0xD8, //   ** **  ** **
  0x0B, 0xD0, //    * **  ** *
  0x07, 0xE0, //    ******
  0x00, 0x00,
  0x00, 0x00
};

// 32x32 Clody Mascot - Normal Frame 1 (Standing)
static const uint8_t PET_NORMAL_1[] = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0x8F, 0xF1, 0xC0, // Eyes row 1 (white holes)
  0x03, 0x8F, 0xF1, 0xC0, // Eyes row 2
  0x3F, 0x8F, 0xF1, 0xFC, // Eyes row 3 + Arms start
  0x3F, 0xFF, 0xFF, 0xFC, // Arms
  0x3F, 0xFF, 0xFF, 0xFC, // Arms
  0x3F, 0xFF, 0xFF, 0xFC, // Arms
  0x3F, 0xFF, 0xFF, 0xFC, // Arms
  0x3F, 0xFF, 0xFF, 0xFC, // Arms
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0x9C, 0x39, 0xC0, // Legs Y=24 (4 straight pillars)
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0
};

// 32x32 Clody Mascot - Normal Frame 2 (Squish/Breathing)
static const uint8_t PET_NORMAL_2[] = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, // Shifted down 1 row for breath animation
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0x8F, 0xF1, 0xC0,
  0x03, 0x8F, 0xF1, 0xC0,
  0x3F, 0x8F, 0xF1, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x07, 0x1C, 0x38, 0xE0, // Legs squatting Y=25 (outer legs bent outward)
  0x07, 0x1C, 0x38, 0xE0,
  0x07, 0x1C, 0x38, 0xE0,
  0x07, 0x1C, 0x38, 0xE0,
  0x07, 0x1C, 0x38, 0xE0,
  0x07, 0x1C, 0x38, 0xE0,
  0x07, 0x1C, 0x38, 0xE0
};

// 32x32 Clody Mascot - Happy (Jumping & legs kicking)
static const uint8_t PET_HAPPY[] = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x03, 0xFF, 0xFF, 0xC0, // Head top shifted up to Y=2 (jumping)
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0x8F, 0xF1, 0xC0,
  0x03, 0x8F, 0xF1, 0xC0,
  0x3F, 0x8F, 0xF1, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x00, 0x00, 0x00, 0x00, // Mid-air gap
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x0E, 0x38, 0x1C, 0x70, // Legs kicking outward star-style
  0x0E, 0x38, 0x1C, 0x70,
  0x0E, 0x38, 0x1C, 0x70,
  0x0E, 0x38, 0x1C, 0x70,
  0x0E, 0x38, 0x1C, 0x70,
  0x0E, 0x38, 0x1C, 0x70,
  0x0E, 0x38, 0x1C, 0x70,
  0x0E, 0x38, 0x1C, 0x70
};

// 32x32 Clody Mascot - Sad / Sick (Wobbly legs and diagonal sad eyes)
static const uint8_t PET_SAD_SICK[] = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xDF, 0xFB, 0xC0, // Sad diagonal eyes row 1
  0x03, 0xEF, 0xF7, 0xC0, // Sad diagonal eyes row 2
  0x03, 0xF7, 0xEF, 0xC0, // Sad diagonal eyes row 3
  0x3F, 0xFF, 0xFF, 0xFC, // Arms drooped downwards
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x01, 0xF8, 0x3F, 0x00, // Legs shaking/wobbling inward
  0x01, 0xF8, 0x3F, 0x00,
  0x01, 0xF8, 0x3F, 0x00,
  0x01, 0xF8, 0x3F, 0x00,
  0x01, 0xF8, 0x3F, 0x00,
  0x01, 0xF8, 0x3F, 0x00,
  0x01, 0xF8, 0x3F, 0x00
};

// 32x32 Clody Mascot - Sleeping (Closed horizontal slit eyes)
static const uint8_t PET_SLEEPING[] = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0x8F, 0xF1, 0xC0, // Closed horizontal eyes (single pixel row)
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0x9C, 0x39, 0xC0, // Legs resting
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0
};

// 32x32 Clody Mascot - Dead (Turned completely upside down with X eyes!)
static const uint8_t PET_DEAD[] = {
  0x03, 0x9C, 0x39, 0xC0, // Legs sticking straight up! (Vertically flipped)
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x3F, 0xFF, 0xFF, 0xFC, // Arms
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0x8F, 0xF1, 0xFC, // X eyes row 1
  0x03, 0xDF, 0xFB, 0xC0, // X eyes row 2
  0x03, 0xEF, 0xF7, 0xC0, // X eyes row 3
  0x03, 0xDF, 0xFB, 0xC0, // X eyes row 4
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// =============================================================================
// 🐾 EVOLUTION SYSTEM SPRITES & RESOLVER
// =============================================================================

// --- STAGE 1: BABY CLODY (Age 0-1) - Cutest tiny terminal capsule >_ ---
static const uint8_t BABY_NORMAL_1[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00,
  0x01, 0xFF, 0x80, 0x00, 0x03, 0xFF, 0xC0, 0x00, 0x07, 0x1F, 0xE0, 0x00, 0x07, 0x1F, 0xE0, 0x00,
  0x07, 0xFF, 0xE0, 0x00, 0x03, 0xFF, 0xC0, 0x00, 0x01, 0xFF, 0x80, 0x00, 0x00, 0x7E, 0x00, 0x00,
  0x00, 0x3C, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t BABY_NORMAL_2[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x7E, 0x00, 0x00, 0x01, 0xFF, 0x80, 0x00, 0x03, 0xFF, 0xC0, 0x00, 0x03, 0x83, 0xC0, 0x00,
  0x03, 0x83, 0xC0, 0x00, 0x03, 0xFF, 0xC0, 0x00, 0x01, 0xFF, 0x80, 0x00, 0x00, 0x7E, 0x00, 0x00,
  0x00, 0x3C, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t BABY_HAPPY[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x7E, 0x00, 0x00, 0x01, 0xFF, 0x80, 0x00, 0x03, 0xFF, 0xC0, 0x00, 0x03, 0xBD, 0xC0, 0x00,
  0x03, 0xBD, 0xC0, 0x00, 0x03, 0xFF, 0xC0, 0x00, 0x01, 0xFF, 0x80, 0x00, 0x00, 0x7E, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t BABY_SAD_SICK[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00,
  0x01, 0xFF, 0x80, 0x00, 0x03, 0xFF, 0xC0, 0x00, 0x03, 0xDF, 0xC0, 0x00, 0x03, 0xEF, 0xC0, 0x00,
  0x03, 0xFF, 0xC0, 0x00, 0x01, 0xFF, 0x80, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00,
  0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t BABY_SLEEPING[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00,
  0x01, 0xFF, 0x80, 0x00, 0x03, 0xFF, 0xC0, 0x00, 0x03, 0x83, 0xC0, 0x00, 0x03, 0xFF, 0xC0, 0x00,
  0x03, 0xFF, 0xC0, 0x00, 0x01, 0xFF, 0x80, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t BABY_DEAD[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x24, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x01, 0xFF, 0x80, 0x00,
  0x03, 0xFF, 0xC0, 0x00, 0x03, 0xFF, 0xC0, 0x00, 0x03, 0xDF, 0xC0, 0x00, 0x03, 0xEF, 0xC0, 0x00,
  0x03, 0xDF, 0xC0, 0x00, 0x01, 0xFF, 0x80, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// --- STAGE 2: TEEN CLODY (Age 2-4) - Interactive command prompt box ---
static const uint8_t TEEN_NORMAL_1[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xCF, 0xF3, 0xF0,
  0x0F, 0x8F, 0xF1, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x07, 0x1C, 0x38, 0xE0, 0x07, 0x1C, 0x38, 0xE0, 0x07, 0x1C, 0x38, 0xE0,
  0x07, 0x1C, 0x38, 0xE0, 0x07, 0x1C, 0x38, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t TEEN_NORMAL_2[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xCF, 0xF3, 0xF0, 0x0F, 0x8F, 0xF1, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70,
  0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t TEEN_HAPPY[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xCF, 0xF3, 0xF0, 0x0F, 0x8F, 0xF1, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x38, 0x1C, 0x70,
  0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70,
  0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t TEEN_SAD_SICK[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xDF, 0xFB, 0xF0,
  0x0F, 0xEF, 0xF7, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x01, 0xF8, 0x3F, 0x00, 0x01, 0xF8, 0x3F, 0x00, 0x01, 0xF8, 0x3F, 0x00,
  0x01, 0xF8, 0x3F, 0x00, 0x01, 0xF8, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t TEEN_SLEEPING[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0x8F, 0xF1, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x07, 0x1C, 0x38, 0xE0, 0x07, 0x1C, 0x38, 0xE0, 0x07, 0x1C, 0x38, 0xE0,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t TEEN_DEAD[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x1C, 0x38, 0xE0, 0x07, 0x1C, 0x38, 0xE0,
  0x07, 0x1C, 0x38, 0xE0, 0x07, 0x1C, 0x38, 0xE0, 0x07, 0x1C, 0x38, 0xE0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0x8F, 0xF1, 0xF0,
  0x0F, 0xDF, 0xFB, 0xF0, 0x0F, 0xEF, 0xF7, 0xF0, 0x0F, 0xDF, 0xFB, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// --- STAGE 4: LEGEND CLODY (Age 10+) - Premium adult with Staff Developer graduation hat & shades ---
static const uint8_t LEGEND_NORMAL_1[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x03, 0xFF, 0xFF, 0xC0,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0x8F, 0xF1, 0xC0, 0x03, 0x8F, 0xF1, 0xC0,
  0x3F, 0x8F, 0xF1, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0x9C, 0x39, 0xC0, 0x03, 0x9C, 0x39, 0xC0, 0x03, 0x9C, 0x39, 0xC0, 0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0, 0x03, 0x9C, 0x39, 0xC0, 0x03, 0x9C, 0x39, 0xC0, 0x03, 0x9C, 0x39, 0xC0
};

static const uint8_t LEGEND_NORMAL_2[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x00, 0x3C, 0x3C, 0x00,
  0x03, 0xFF, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0x8F, 0xF1, 0xC0, 0x03, 0x8F, 0xF1, 0xC0,
  0x3F, 0x8F, 0xF1, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70,
  0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70
};

static const uint8_t LEGEND_HAPPY[] = {
  0x00, 0x3C, 0x3C, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x03, 0xFF, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0, 0x03, 0x8F, 0xF1, 0xC0, 0x03, 0x8F, 0xF1, 0xC0, 0x3F, 0x8F, 0xF1, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x38, 0x1C, 0x70,
  0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70,
  0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70, 0x0E, 0x38, 0x1C, 0x70
};

static const uint8_t LEGEND_SAD_SICK[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x03, 0xFF, 0xFF, 0xC0,
  0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xDF, 0xFB, 0xC0, 0x03, 0xEF, 0xF7, 0xC0, 0x3F, 0xDF, 0xFB, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x01, 0xF8, 0x3F, 0x00,
  0x01, 0xF8, 0x3F, 0x00, 0x01, 0xF8, 0x3F, 0x00, 0x01, 0xF8, 0x3F, 0x00, 0x01, 0xF8, 0x3F, 0x00,
  0x01, 0xF8, 0x3F, 0x00, 0x01, 0xF8, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t LEGEND_SLEEPING[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x03, 0xFF, 0xFF, 0xC0,
  0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0, 0x03, 0x8F, 0xF1, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0x9C, 0x39, 0xC0,
  0x03, 0x9C, 0x39, 0xC0, 0x03, 0x9C, 0x39, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t LEGEND_DEAD[] = {
  0x00, 0x00, 0x00, 0x00, 0x03, 0x9C, 0x39, 0xC0, 0x03, 0x9C, 0x39, 0xC0, 0x03, 0x9C, 0x39, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0x8F, 0xF1, 0xFC, 0x03, 0x8F, 0xF1, 0xC0, 0x03, 0xFF, 0xFF, 0xC0,
  0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00,
  0x03, 0xFF, 0xFF, 0xC0, 0x00, 0x3C, 0x3C, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// --- DYNAMIC SPRITE RESOLVER ---
const uint8_t* getPetSprite(int type) {
  extern int petAge;
  // type: 0=Normal1, 1=Normal2, 2=Happy, 3=Sad/Sick, 4=Sleeping, 5=Dead
  if (petAge < 2) {
    if (type == 0) return BABY_NORMAL_1;
    if (type == 1) return BABY_NORMAL_2;
    if (type == 2) return BABY_HAPPY;
    if (type == 3) return BABY_SAD_SICK;
    if (type == 4) return BABY_SLEEPING;
    return BABY_DEAD;
  } else if (petAge < 5) {
    if (type == 0) return TEEN_NORMAL_1;
    if (type == 1) return TEEN_NORMAL_2;
    if (type == 2) return TEEN_HAPPY;
    if (type == 3) return TEEN_SAD_SICK;
    if (type == 4) return TEEN_SLEEPING;
    return TEEN_DEAD;
  } else if (petAge < 10) {
    if (type == 0) return PET_NORMAL_1;
    if (type == 1) return PET_NORMAL_2;
    if (type == 2) return PET_HAPPY;
    if (type == 3) return PET_SAD_SICK;
    if (type == 4) return PET_SLEEPING;
    return PET_DEAD;
  } else {
    if (type == 0) return LEGEND_NORMAL_1;
    if (type == 1) return LEGEND_NORMAL_2;
    if (type == 2) return LEGEND_HAPPY;
    if (type == 3) return LEGEND_SAD_SICK;
    if (type == 4) return LEGEND_SLEEPING;
    return LEGEND_DEAD;
  }
}

// -----------------------------------------------------------------------------
// 8x8 Character Font (F[][8])
// -----------------------------------------------------------------------------
static const uint8_t F[][8] = {
  {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0},  // A
  {0x3E,0x66,0x66,0x3E,0x66,0x66,0x3E,0},  // B
  {0x3C,0x66,0x06,0x06,0x06,0x66,0x3C,0},  // C
  {0x1E,0x36,0x66,0x66,0x66,0x36,0x1E,0},  // D
  {0x7E,0x06,0x06,0x3E,0x06,0x06,0x7E,0},  // E
  {0x7E,0x06,0x06,0x3E,0x06,0x06,0x06,0},  // F
  {0x3C,0x66,0x06,0x76,0x66,0x66,0x3C,0},  // G
  {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0},  // H
  {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0},  // I
  {0x78,0x30,0x30,0x30,0x30,0x36,0x1C,0},  // J
  {0x66,0x36,0x1E,0x0E,0x1E,0x36,0x66,0},  // K
  {0x06,0x06,0x06,0x06,0x06,0x06,0x7E,0},  // L
  {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0},  // M
  {0x66,0x6E,0x7E,0x76,0x66,0x66,0x66,0},  // N
  {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0},  // O
  {0x3E,0x66,0x66,0x3E,0x06,0x06,0x06,0},  // P
  {0x3C,0x66,0x66,0x66,0x76,0x36,0x5C,0},  // Q
  {0x3E,0x66,0x66,0x3E,0x36,0x66,0x66,0},  // R
  {0x3C,0x66,0x06,0x3C,0x60,0x66,0x3C,0},  // S
  {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0},  // T
  {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0},  // U
  {0x66,0x66,0x66,0x66,0x3C,0x3C,0x18,0},  // V
  {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0},  // W
  {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0},  // X
  {0x66,0x66,0x3C,0x18,0x18,0x18,0x18,0},  // Y
  {0x7E,0x60,0x30,0x18,0x0C,0x06,0x7E,0},  // Z
  {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0},  // 0
  {0x18,0x18,0x38,0x18,0x18,0x18,0x7E,0},  // 1
  {0x3C,0x66,0x60,0x30,0x18,0x0C,0x7E,0},  // 2
  {0x3C,0x66,0x60,0x38,0x60,0x66,0x3C,0},  // 3
  {0x30,0x38,0x3C,0x36,0x7E,0x30,0x30,0},  // 4
  {0x7E,0x06,0x3E,0x60,0x60,0x66,0x3C,0},  // 5
  {0x38,0x0C,0x06,0x3E,0x66,0x66,0x3C,0},  // 6
  {0x7E,0x60,0x30,0x18,0x0C,0x0C,0x0C,0},  // 7
  {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0},  // 8
  {0x3C,0x66,0x66,0x7C,0x60,0x30,0x1C,0},  // 9
  {0,0,0,0,0,0x18,0x18,0},                  // .
  {0,0,0,0x7E,0,0,0,0},                     // -
  {0x60,0x30,0x18,0x0C,0x06,0x03,0,0},      // /
  {0,0x18,0x18,0,0x18,0x18,0,0},            // :
  {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0},   // #
  {0,0,0,0,0,0,0,0},                        // space
};

// -----------------------------------------------------------------------------
// Drawing Helpers
// -----------------------------------------------------------------------------

inline void drawPixel(int x, int y, uint8_t color) {
  if (x < 0 || x >= W || y < 0 || y >= H) return;
  uint8_t* buf = display->getBuffer();
  int idx = y * 25 + (x / 8);
  uint8_t bit = 1 << (7 - (x % 8));
  if (color == BLACK) {
    buf[idx] &= ~bit;
  } else {
    buf[idx] |= bit;
  }
}

void fillRect(int x, int y, int w, int h, uint8_t color) {
  for (int dy = 0; dy < h; dy++) {
    for (int dx = 0; dx < w; dx++) {
      drawPixel(x + dx, y + dy, color);
    }
  }
}

void drawLine(int x0, int y0, int x1, int y1, uint8_t color) {
  int dx = abs(x1 - x0), dy = abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  while (true) {
    drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void drawRect(int x, int y, int w, int h, uint8_t color) {
  drawLine(x, y, x + w - 1, y, color);
  drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
  drawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
  drawLine(x, y, x, y + h - 1, color);
}

void fillCircle(int cx, int cy, int r, uint8_t color) {
  for (int dy = -r; dy <= r; dy++) {
    int dx = (int)sqrtf((float)(r * r - dy * dy));
    drawLine(cx - dx, cy + dy, cx + dx, cy + dy, color);
  }
}

void drawCircle(int cx, int cy, int r, uint8_t color) {
  int x = r, y = 0, err = 0;
  while (x >= y) {
    drawPixel(cx + x, cy + y, color);
    drawPixel(cx + y, cy + x, color);
    drawPixel(cx - y, cy + x, color);
    drawPixel(cx - x, cy + y, color);
    drawPixel(cx - x, cy - y, color);
    drawPixel(cx - y, cy - x, color);
    drawPixel(cx + y, cy - x, color);
    drawPixel(cx + x, cy - y, color);
    y++;
    err += 1 + 2 * y;
    if (2 * (err - x) + 1 > 0) {
      x--;
      err += 1 - 2 * x;
    }
  }
}

void drawBitmap(int x0, int y0, const uint8_t* bits, int bw, int bh, uint8_t color, bool mirrorX = false) {
  int rb = (bw + 7) / 8;
  for (int y = 0; y < bh; y++) {
    for (int x = 0; x < bw; x++) {
      int sourceX = mirrorX ? (bw - 1 - x) : x;
      if ((bits[y * rb + sourceX / 8] >> (7 - (sourceX % 8))) & 1) {
        drawPixel(x0 + x, y0 + y, color);
      }
    }
  }
}

int getFontIdx(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a';
  if (c >= '0' && c <= '9') return 26 + c - '0';
  if (c == '.') return 36;
  if (c == '-') return 37;
  if (c == '/') return 38;
  if (c == ':') return 39;
  if (c == '#') return 40;
  return 41; // space fallback
}

void drawChar(int x, int y, char c, int scale, uint8_t color) {
  int idx = getFontIdx(c);
  const uint8_t* g = F[idx];
  for (int row = 0; row < 8; row++) {
    uint8_t bits = g[row];
    for (int col = 0; col < 8; col++) {
      if (bits & (1 << col)) {
        fillRect(x + col * scale, y + row * scale, scale, scale, color);
      }
    }
  }
}

void drawString(int x, int y, const char* s, int scale, uint8_t color) {
  while (*s) {
    drawChar(x, y, *s, scale, color);
    x += 8 * scale;
    s++;
  }
}

void drawStringCentered(int cx, int y, const char* s, int scale, uint8_t color) {
  int len = strlen(s);
  int w = len * 8 * scale;
  drawString(cx - w / 2, y, s, scale, color);
}

// -----------------------------------------------------------------------------
// Clody Game States & Stats
// -----------------------------------------------------------------------------

enum ClodyState {
  STATE_IDLE,
  STATE_MENU,
  STATE_FEED,
  STATE_PLAY_START,
  STATE_PLAY_ROUND,
  STATE_PLAY_OVER,
  STATE_CLEAN,
  STATE_HEAL,
  STATE_SLEEP,
  STATE_STATS_AGE,
  STATE_STATS_HUNGER,
  STATE_STATS_HAPPY,
  STATE_DEAD
};

enum WeatherType {
  WEATHER_SUNNY,
  WEATHER_RAINY,
  WEATHER_SNOWY
};

ClodyState gameState = STATE_IDLE;
WeatherType currentWeather = WEATHER_SUNNY;

String wifiSSID = "";
String wifiPASS = "";
String weatherAPIKey = "";

// Pet Attributes
int petAge = 0;              // years
int petWeight = 5;           // kg
int petHunger = 2;           // 0 to 4 (hearts)
int petHappiness = 2;        // 0 to 4 (hearts)
bool petIsSick = false;
int poopCount = 0;
bool lightsOn = true;

// Timing Constants (Balanced standard for a durable daily desk/pocket companion)
#define ANIM_INTERVAL_MS      1500    // breathing/bouncing interval
#define STATS_DECAY_INTERVAL  2700000 // stats decay every 45 minutes (2,700,000 ms)
#define AGE_INTERVAL          43200000 // pet ages 1 year every 12 hours (43,200,000 ms)
#define WEATHER_SYNC_INTERVAL 21600000 // 6 hours (6 * 3600 * 1000 ms)

unsigned long lastAnimMs = 0;
unsigned long lastDecayMs = 0;
unsigned long lastAgeMs = 0;
unsigned long lastWeatherSyncMs = 0;
unsigned long lastInactivityMs = 0; // Return to idle after inactivity
#define INACTIVITY_TIMEOUT_MS 8000

// Menu Variables
int menuSelectedIdx = 0;
#define MENU_COUNT 6 // Feed, Play, Clean, Heal, Sleep, Stats

// Feed Sub-menu Variables
int feedSelectedIdx = 0; // 0 = Meal, 1 = Snack

// Play Game Variables
int gameRound = 1;
int gameCorrectCount = 0;
int userGuess = 0;      // 0 = Left, 1 = Right
int petTargetTurn = 0;  // 0 = Left, 1 = Right
unsigned long gameRoundTimer = 0;
bool roundResultCorrect = false;

// Clean / Broom Animation
int cleanAnimTick = 0;

// Sick Flash / Attention Alert
bool statAlertFlash = false;

// Pet visual coordinates & walking animation
int petX = 84;
int petY = 103; // floor line is at 135. Pet height is 32. 135 - 32 = 103
int petTargetX = 84;
bool isMoving = false;
int isEatingTicks = 0;

// Sound Wrapper
void playBeep(float freq, int durationMs, float volume = 0.1f) {
  playToneUI(freq, durationMs, volume);
}

// -----------------------------------------------------------------------------
// State Persistence (NVS/Flash Saving using Preferences)
// -----------------------------------------------------------------------------
Preferences preferences;

void saveState() {
  Serial.println("[NVS] Saving Clody state to Flash...");
  preferences.begin("clody", false);
  preferences.putInt("age", petAge);
  preferences.putInt("weight", petWeight);
  preferences.putInt("hunger", petHunger);
  preferences.putInt("happy", petHappiness);
  preferences.putBool("sick", petIsSick);
  preferences.putInt("poop", poopCount);
  preferences.putInt("state", (int)gameState);
  preferences.putBool("lights", lightsOn);
  preferences.putInt("weather", (int)currentWeather);
  preferences.end();
  Serial.println("[NVS] State saved successfully.");
}

void loadState() {
  preferences.begin("clody", true); // open read-only
  if (preferences.isKey("age")) {
    petAge = preferences.getInt("age", 0);
    petWeight = preferences.getInt("weight", 5);
    petHunger = preferences.getInt("hunger", 2);
    petHappiness = preferences.getInt("happy", 2);
    petIsSick = preferences.getBool("sick", false);
    poopCount = preferences.getInt("poop", 0);
    gameState = (ClodyState)preferences.getInt("state", (int)STATE_IDLE);
    lightsOn = preferences.getBool("lights", true);
    currentWeather = (WeatherType)preferences.getInt("weather", (int)WEATHER_SUNNY);
    Serial.printf("[NVS] Loaded existing Clody! Age:%d, Wt:%d, Hgr:%d, Hap:%d, Sick:%d, Poop:%d, State:%d, Lights:%d, Weather:%d\n",
                  petAge, petWeight, petHunger, petHappiness, petIsSick, poopCount, (int)gameState, (int)lightsOn, (int)currentWeather);
  } else {
    Serial.println("[NVS] No saved state found. Starting fresh Clody journey!");
  }
  preferences.end();
}
void renderScreen();

void loadWifiCredentials() {
  preferences.begin("wificonf", true);
  wifiSSID = preferences.getString("ssid", "YOUR_WIFI_SSID");
  wifiPASS = preferences.getString("pass", "YOUR_WIFI_PASSWORD");
  weatherAPIKey = preferences.getString("apikey", "YOUR_OPENWEATHERMAP_API_KEY");
  preferences.end();
}

void saveWifiCredentials(String ssid, String pass) {
  preferences.begin("wificonf", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.end();
  wifiSSID = ssid;
  wifiPASS = pass;
}

void saveWeatherAPIKey(String keyVal) {
  preferences.begin("wificonf", false);
  preferences.putString("apikey", keyVal);
  preferences.end();
  weatherAPIKey = keyVal;
}

void connectWifiAndFetchWeather(bool force = false) {
  if (wifiSSID.length() == 0 || wifiSSID == "YOUR_WIFI_SSID") {
    Serial.println("[WIFI] No Wi-Fi credentials stored. Use serial command: 'wifi SSID PASSWORD' to connect!");
    return;
  }
  if (weatherAPIKey.length() == 0 || weatherAPIKey == "YOUR_OPENWEATHERMAP_API_KEY") {
    Serial.println("[WIFI] OpenWeatherMap API key not set. Set it via serial command: 'weather key YOUR_API_KEY'");
    return;
  }

  // Rate-limiting check: Skip fetch unless forced or lastWeatherSyncMs is 0 (initial boot fetch)
  if (!force && lastWeatherSyncMs != 0 && (millis() - lastWeatherSyncMs < WEATHER_SYNC_INTERVAL)) {
    Serial.println("[WIFI] Skipping dynamic fetch; Berlin weather was synced recently (less than 6 hours ago).");
    return;
  }

  Serial.printf("[WIFI] Connecting to %s ...\n", wifiSSID.c_str());
  WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());

  // Attempt to connect (max 10 seconds timeout)
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WIFI] Connected successfully!");
    Serial.printf("[WIFI] IP Address: %s\n", WiFi.localIP().toString().c_str());

    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=Berlin&appid=" + weatherAPIKey;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("[WIFI] Weather API payload received successfully.");

      int weatherIdx = payload.indexOf("\"weather\":");
      if (weatherIdx != -1) {
        String weatherPart = payload.substring(weatherIdx, weatherIdx + 150);
        int idIdx = weatherPart.indexOf("\"id\":");
        if (idIdx != -1) {
          String idSub = weatherPart.substring(idIdx + 5);
          int endComma = idSub.indexOf(',');
          int endBrace = idSub.indexOf('}');
          int lengthOfVal = 3;
          if (endComma != -1 && endComma < lengthOfVal) lengthOfVal = endComma;
          if (endBrace != -1 && endBrace < lengthOfVal) lengthOfVal = endBrace;
          
          int idVal = idSub.substring(0, lengthOfVal).toInt();
          Serial.printf("[WIFI] Berlin Weather ID: %d\n", idVal);
          lastWeatherSyncMs = millis();

          WeatherType oldWeather = currentWeather;
          if (idVal >= 200 && idVal < 600) {
            currentWeather = WEATHER_RAINY;
          } else if (idVal >= 600 && idVal < 700) {
            currentWeather = WEATHER_SNOWY;
          } else {
            currentWeather = WEATHER_SUNNY;
          }

          if (currentWeather != oldWeather) {
            Serial.println("[WIFI] Weather state changed based on Berlin API!");
            saveState();
            renderScreen();
          } else {
            Serial.println("[WIFI] Weather state is already synced with Berlin API.");
          }
        }
      }
    } else {
      Serial.printf("[WIFI] HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();

    WiFi.disconnect(true);
    Serial.println("[WIFI] Wi-Fi radio disconnected to save battery power.");
  } else {
    Serial.println("[WIFI] Failed to connect to Wi-Fi. Connection timed out.");
  }
}

// -----------------------------------------------------------------------------
// Setup & Initialization
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0); // Prevent blocking/hanging when USB is disconnected!
  // Wait up to 1 second for native USB Serial to connect if plugged in
  for (int i = 0; i < 100 && !Serial; i++) {
    delay(10);
  }
  Serial.println("\n=== Clody Boot ===");
  Serial.flush();

  // Initialize input buttons (pullups active)
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  Serial.println("[DEBUG] Buttons configured"); Serial.flush();

  // Turn board power on
  board.VBAT_POWER_ON();
  delay(50);
  Serial.println("[DEBUG] Board power VBAT_POWER_ON complete"); Serial.flush();

  // Latch the 3V3 power rail
  pinMode(PWR_HOLD_PIN, OUTPUT);
  digitalWrite(PWR_HOLD_PIN, HIGH);
  delay(20);
  Serial.println("[DEBUG] Power rail 3V3 latched"); Serial.flush();

  // Power up EPD and audio codec
  board.POWEER_EPD_ON();
  board.POWEER_Audio_ON();
  delay(200);
  Serial.println("[DEBUG] Power rails for EPD and Audio enabled"); Serial.flush();

  // Initialize audio hardware
  // (The codec init below sets up its own I2C bus in src/codec_board; this
  // sketch doesn't use the separate RTC/SHTC3 I2C bus from src/i2c_bsp, so
  // it's not included — calling both would try to claim the same I2C port.)
  Serial.println("[DEBUG] Initializing Audio BSP..."); Serial.flush();
  audio_bsp_init();
  Serial.println("[DEBUG] Initializing Audio Play..."); Serial.flush();
  audio_play_init();
  Serial.println("[DEBUG] Audio hardware initialized"); Serial.flush();
  
  // Set volume (0-100)
  palaSoundSetEnabled(true);
  audio_playback_set_vol(75);
  Serial.println("[DEBUG] Sound volume set"); Serial.flush();

  // Initialize E-Paper display
  custom_lcd_spi_t dispCfg = {};
  dispCfg.cs         = EPD_CS_PIN;
  dispCfg.dc         = EPD_DC_PIN;
  dispCfg.rst        = EPD_RST_PIN;
  dispCfg.busy       = EPD_BUSY_PIN;
  dispCfg.mosi       = EPD_MOSI_PIN;
  dispCfg.scl        = EPD_SCK_PIN;
  dispCfg.spi_host   = EPD_SPI_NUM;
  dispCfg.buffer_len = (200 * 200) / 8;

  Serial.println("[DEBUG] Creating display object..."); Serial.flush();
  display = new epaper_driver_display(200, 200, dispCfg);
  Serial.println("[DEBUG] Initializing EPD display (EPD_Init)..."); Serial.flush();
  display->EPD_Init();
  Serial.println("[DEBUG] Clearing EPD buffer (EPD_Clear)..."); Serial.flush();
  display->EPD_Clear();
  Serial.println("[DEBUG] Rendering partial base image (EPD_DisplayPartBaseImage)..."); Serial.flush();
  display->EPD_DisplayPartBaseImage();
  Serial.println("[DEBUG] Initializing partial updates (EPD_Init_Partial)..."); Serial.flush();
  display->EPD_Init_Partial();
  Serial.println("[DEBUG] E-Paper display successfully initialized!"); Serial.flush();

  // Seed random generator
  randomSeed(analogRead(BAT_ADC_PIN) + micros());
  Serial.println("[DEBUG] Random generator seeded"); Serial.flush();

  // Welcome chime
  Serial.println("[DEBUG] Playing success chime..."); Serial.flush();
  soundSuccess();
  Serial.println("[DEBUG] Success chime complete"); Serial.flush();

  // Set reference timestamps
  lastAnimMs = millis();
  lastDecayMs = millis();
  lastAgeMs = millis();
  lastInactivityMs = millis();

  // Load saved state (if any exists)
  loadState();

  // Load Wi-Fi credentials and sync initial weather from Berlin
  loadWifiCredentials();
  connectWifiAndFetchWeather();

  Serial.println("Clody initialization complete!");
}

// Forward declaration of render function
void renderScreen();

// -----------------------------------------------------------------------------
// Core Game Logic Loops
// -----------------------------------------------------------------------------

void updateStats() {
  unsigned long now = millis();

  // 1. Age progression
  if (now - lastAgeMs >= AGE_INTERVAL) {
    lastAgeMs = now;
    if (gameState != STATE_DEAD) {
      petAge++;
      Serial.printf("[TIME] Pet aged up! Age: %d\n", petAge);
      playBeep(880, 200);
      delay(100);
      playBeep(1200, 250);
      saveState();
      renderScreen();
    }
  }

  // 2. Hunger and happiness decay
  if (now - lastDecayMs >= STATS_DECAY_INTERVAL) {
    lastDecayMs = now;
    if (gameState != STATE_DEAD && lightsOn) {
      // Hunger decays by 1
      if (petHunger > 0) {
        petHunger--;
      }
      
      // Happiness decays by 1
      if (petHappiness > 0) {
        petHappiness--;
      }

      // Poop generation (15% chance if fed and not dead)
      if (petHunger >= 2 && poopCount < 3 && random(100) < 15) {
        poopCount++;
        Serial.printf("[STATE] Oh no, poop spawned! Count: %d\n", poopCount);
        playBeep(330, 400); // sad sound alert
      }

      // Sickness check
      if (!petIsSick) {
        // High risk if poop is left on screen
        if (poopCount > 0 && random(100) < (25 * poopCount)) {
          petIsSick = true;
          Serial.println("[STATE] Pet became SICK due to dirty environment.");
          soundDelete();
        }
        // Risk if starving
        else if (petHunger == 0 && random(100) < 40) {
          petIsSick = true;
          Serial.println("[STATE] Pet became SICK due to starvation.");
          soundDelete();
        }
      }

      // Check death conditions
      if ((petHunger == 0 && petHappiness == 0) || (petIsSick && petHunger == 0)) {
        // Starved or untreated sickness for a full cycle results in death
        gameState = STATE_DEAD;
        renderScreen();
        Serial.println("[STATE] Pet has DIED.");
        soundDelete();
      }

      // Fetch dynamic real-world weather for Berlin over Wi-Fi
      connectWifiAndFetchWeather();

      saveState();
      renderScreen();
    }
  }

  // 3. Simple random walking in IDLE mode
  if (gameState == STATE_IDLE && !isMoving && isEatingTicks == 0 && lightsOn) {
    if (random(100) < 5) { // 5% chance to decide to walk
      petTargetX = random(20, 148);
      isMoving = true;
      Serial.printf("[AI] Walking to X: %d\n", petTargetX);
    }
  }

  if (isMoving) {
    if (abs(petX - petTargetX) <= 3) {
      petX = petTargetX;
      isMoving = false;
    } else {
      if (petX < petTargetX) {
        petX += 3;
      } else {
        petX -= 3;
      }
    }
  }

  // Eating timer decay
  if (isEatingTicks > 0) {
    isEatingTicks--;
  }
}

void handleButtonPress(int btn) {
  lastInactivityMs = millis(); // reset timeout

  if (gameState == STATE_DEAD) {
    // Press BTN_2 to hatch a new egg and restart!
    if (btn == 2) {
      petAge = 0;
      petWeight = 5;
      petHunger = 2;
      petHappiness = 2;
      petIsSick = false;
      poopCount = 0;
      lightsOn = true;
      gameState = STATE_IDLE;
      lastDecayMs = millis();
      lastAgeMs = millis();
      soundSuccess();
      saveState();
      renderScreen();
    }
    return;
  }

  switch (gameState) {
    case STATE_IDLE:
      if (btn == 1) {
        // BTN_1: Enter action menu
        soundNext();
        gameState = STATE_MENU;
        menuSelectedIdx = 0;
        renderScreen();
      } else if (btn == 2) {
        // BTN_2: Quick check stats shortcut
        soundSelect();
        gameState = STATE_STATS_AGE;
        renderScreen();
      }
      break;

    case STATE_MENU:
      if (btn == 1) {
        // BTN_1: Move selection cursor
        soundNext();
        menuSelectedIdx = (menuSelectedIdx + 1) % MENU_COUNT;
        renderScreen();
      } else if (btn == 2) {
        // BTN_2: Activate action
        soundSelect();
        switch (menuSelectedIdx) {
          case 0: // FEED
            gameState = STATE_FEED;
            feedSelectedIdx = 0;
            break;
          case 1: // PLAY
            gameState = STATE_PLAY_START;
            gameRound = 1;
            gameCorrectCount = 0;
            break;
          case 2: // CLEAN
            if (poopCount > 0) {
              gameState = STATE_CLEAN;
              cleanAnimTick = 0;
            } else {
              // Nothing to clean, return
              gameState = STATE_IDLE;
            }
            break;
          case 3: // HEAL
            gameState = STATE_HEAL;
            break;
          case 4: // SLEEP
            lightsOn = !lightsOn;
            gameState = lightsOn ? STATE_IDLE : STATE_SLEEP;
            if (!lightsOn) {
              soundBack();
            } else {
              soundSuccess();
            }
            saveState();
            break;
          case 5: // STATS
             gameState = STATE_STATS_AGE;
             break;
         }
        renderScreen();
      }
      break;

    case STATE_FEED:
      if (btn == 1) {
        // Toggle Meal / Snack
        soundNext();
        feedSelectedIdx = (feedSelectedIdx + 1) % 2;
        renderScreen();
      } else if (btn == 2) {
        // Confirm feed
        soundSelect();
        if (petIsSick) {
          // Sick pet won't eat!
          playBeep(400, 300);
          delay(100);
          playBeep(300, 300);
          gameState = STATE_IDLE;
          renderScreen();
          break;
        }

        if (feedSelectedIdx == 0) {
          // Meal: +2 Hunger, +1kg Weight
          petHunger = min(4, petHunger + 2);
          petWeight += 1;
        } else {
          // Snack: +1 Hunger, +1 Happiness, +2kg Weight. Slight sickness risk!
          petHunger = min(4, petHunger + 1);
          petHappiness = min(4, petHappiness + 1);
          petWeight += 2;
          if (random(100) < 20) {
            petIsSick = true;
          }
        }
        isEatingTicks = 6; // Trigger eating animation
        gameState = STATE_IDLE;
        soundSuccess();
        saveState();
        renderScreen();
      }
      break;

    case STATE_PLAY_START:
      // Start guessing round
      if (btn == 1 || btn == 2) {
        userGuess = (btn == 1) ? 0 : 1; // 0 = Left, 1 = Right
        petTargetTurn = random(2);       // Randomly face Left (0) or Right (1)
        
        roundResultCorrect = (userGuess == petTargetTurn);
        if (roundResultCorrect) {
          gameCorrectCount++;
          soundNext();
        } else {
          soundDelete();
        }

        gameState = STATE_PLAY_ROUND;
        gameRoundTimer = millis();
        renderScreen();
      }
      break;

    case STATE_PLAY_ROUND:
      // Block input while showing result of round
      break;

    case STATE_PLAY_OVER:
      // Block input while showing final result
      break;

    case STATE_CLEAN:
      // Block input during cleaning
      break;

    case STATE_HEAL:
      if (btn == 1 || btn == 2) {
        // Perform heal if sick
        if (petIsSick) {
          petIsSick = false;
          soundSuccess();
          saveState();
        } else {
          soundBack();
        }
        gameState = STATE_IDLE;
        renderScreen();
      }
      break;

    case STATE_SLEEP:
      // Any button press turns light back on!
      soundSelect();
      lightsOn = true;
      gameState = STATE_IDLE;
      saveState();
      renderScreen();
      break;

    case STATE_STATS_AGE:
      if (btn == 1) {
        soundNext();
        gameState = STATE_STATS_HUNGER;
      } else {
        soundBack();
        gameState = STATE_IDLE;
      }
      renderScreen();
      break;

    case STATE_STATS_HUNGER:
      if (btn == 1) {
        soundNext();
        gameState = STATE_STATS_HAPPY;
      } else {
        soundBack();
        gameState = STATE_IDLE;
      }
      renderScreen();
      break;

    case STATE_STATS_HAPPY:
      soundBack();
      gameState = STATE_IDLE;
      renderScreen();
      break;
  }
}

int animFrame = 0;

void drawWeatherWindow(int x, int y, int w, int h) {
  // 1. Draw outer pane window border
  drawRect(x, y, w, h, BLACK);
  drawRect(x + 1, y + 1, w - 2, h - 2, BLACK); // thicker frame

  // 2. Window pane divider lines
  int midX = x + w / 2;
  int midY = y + h / 2;
  drawLine(midX, y + 2, midX, y + h - 3, BLACK);
  drawLine(x + 2, midY, x + w - 3, midY, BLACK);

  // 3. Draw weather contents inside
  if (currentWeather == WEATHER_SUNNY) {
    // A cute retro sun in the top-right corner of the window
    int sunX = x + w - 8;
    int sunY = y + 8;
    drawCircle(sunX, sunY, 3, BLACK);

    // Dynamic sunbeams that rotate / animate
    if (animFrame == 0) {
      // Perpendicular rays
      drawLine(sunX - 5, sunY, sunX - 4, sunY, BLACK);
      drawLine(sunX + 4, sunY, sunX + 5, sunY, BLACK);
      drawLine(sunX, sunY - 5, sunX, sunY - 4, BLACK);
      drawLine(sunX, sunY + 4, sunX, sunY + 5, BLACK);
    } else {
      // Diagonal rays
      drawLine(sunX - 4, sunY - 4, sunX - 3, sunY - 3, BLACK);
      drawLine(sunX + 3, sunY - 3, sunX + 4, sunY - 4, BLACK);
      drawLine(sunX - 4, sunY + 4, sunX - 3, sunY + 3, BLACK);
      drawLine(sunX + 3, sunY + 3, sunX + 4, sunY + 4, BLACK);
    }

    // Cozy solar sunbeams streaming from the window onto the floor (outside of frame!)
    for (int i = 0; i < 2; i++) {
      int rayOffset = i * 15;
      if (animFrame == 0) {
        // Dotted rays (dancing dust motes)
        for (int d = 0; d < 12; d += 2) {
          int rx = x + 10 + rayOffset - d * 1.5;
          int ry = y + h + d * 1.5;
          if (ry < 135) drawPixel(rx, ry, BLACK);
        }
      } else {
        // Solid rays
        drawLine(x + 10 + rayOffset, y + h, x + rayOffset - 18, y + h + 18, BLACK);
      }
    }
  } 
  else if (currentWeather == WEATHER_RAINY) {
    // Draw small rain clouds at the top
    drawCircle(x + 8, y + 6, 4, BLACK);
    drawCircle(x + 13, y + 5, 5, BLACK);
    drawCircle(x + 18, y + 6, 4, BLACK);
    fillRect(x + 6, y + 8, 14, 2, BLACK); // flat cloud bottom

    drawCircle(x + 24, y + 7, 3, BLACK);
    drawCircle(x + 28, y + 6, 4, BLACK);
    drawCircle(x + 32, y + 7, 3, BLACK);
    fillRect(x + 22, y + 8, 12, 2, BLACK);

    // Falling raindrop lines inside the panes (avoiding divider lines)
    if (animFrame == 0) {
      // Drops set A
      drawLine(x + 6, y + 13, x + 4, y + 17, BLACK);
      drawLine(x + 14, y + 11, x + 12, y + 15, BLACK);
      drawLine(x + 26, y + 12, x + 24, y + 16, BLACK);
      drawLine(x + 34, y + 10, x + 32, y + 14, BLACK);

      drawLine(x + 8, y + 23, x + 6, y + 27, BLACK);
      drawLine(x + 16, y + 21, x + 14, y + 25, BLACK);
      drawLine(x + 24, y + 24, x + 22, y + 28, BLACK);
      drawLine(x + 32, y + 22, x + 30, y + 26, BLACK);
    } else {
      // Drops set B (shifted down and left)
      drawLine(x + 5, y + 15, x + 3, y + 19, BLACK);
      drawLine(x + 13, y + 13, x + 11, y + 17, BLACK);
      drawLine(x + 25, y + 14, x + 23, y + 18, BLACK);
      drawLine(x + 33, y + 12, x + 31, y + 16, BLACK);

      drawLine(x + 7, y + 25, x + 5, y + 29, BLACK);
      drawLine(x + 15, y + 23, x + 13, y + 27, BLACK);
      drawLine(x + 23, y + 26, x + 21, y + 30, BLACK);
      drawLine(x + 31, y + 24, x + 29, y + 28, BLACK);
    }
  } 
  else if (currentWeather == WEATHER_SNOWY) {
    // Draw puffy winter clouds at the top
    drawCircle(x + 10, y + 6, 5, BLACK);
    drawCircle(x + 16, y + 5, 5, BLACK);
    drawCircle(x + 22, y + 6, 4, BLACK);
    fillRect(x + 8, y + 8, 16, 2, BLACK);

    // Floating snow stars inside the window panes
    if (animFrame == 0) {
      // Snowflakes position 1
      drawPixel(x + 6, y + 12, BLACK);
      drawPixel(x + 5, y + 13, BLACK); drawPixel(x + 7, y + 13, BLACK);
      drawPixel(x + 6, y + 14, BLACK);

      drawPixel(x + 26, y + 11, BLACK);
      drawPixel(x + 25, y + 12, BLACK); drawPixel(x + 27, y + 12, BLACK);
      drawPixel(x + 26, y + 13, BLACK);

      drawPixel(x + 12, y + 23, BLACK);
      drawPixel(x + 11, y + 24, BLACK); drawPixel(x + 13, y + 24, BLACK);
      drawPixel(x + 12, y + 25, BLACK);

      drawPixel(x + 32, y + 22, BLACK);
      drawPixel(x + 31, y + 23, BLACK); drawPixel(x + 33, y + 23, BLACK);
      drawPixel(x + 32, y + 24, BLACK);
    } else {
      // Snowflakes position 2 (drifted down and right)
      drawPixel(x + 9, y + 15, BLACK);
      drawPixel(x + 8, y + 16, BLACK); drawPixel(x + 10, y + 16, BLACK);
      drawPixel(x + 9, y + 17, BLACK);

      drawPixel(x + 29, y + 14, BLACK);
      drawPixel(x + 28, y + 15, BLACK); drawPixel(x + 30, y + 15, BLACK);
      drawPixel(x + 29, y + 16, BLACK);

      drawPixel(x + 15, y + 26, BLACK);
      drawPixel(x + 14, y + 27, BLACK); drawPixel(x + 16, y + 27, BLACK);
      drawPixel(x + 15, y + 28, BLACK);

      drawPixel(x + 29, y + 25, BLACK);
      drawPixel(x + 28, y + 26, BLACK); drawPixel(x + 30, y + 26, BLACK);
      drawPixel(x + 29, y + 27, BLACK);
    }
  }
}



void renderScreen() {
  display->EPD_Clear();

  // If sleep lights are off, we draw inverted (or starry night background)
  if (gameState == STATE_SLEEP) {
    // Black sleeping background
    fillRect(0, 0, W, H, BLACK);
    
    // Starry dots
    drawPixel(20, 30, WHITE);
    drawPixel(180, 50, WHITE);
    drawPixel(45, 90, WHITE);
    drawPixel(160, 110, WHITE);
    drawPixel(90, 40, WHITE);

    // Crescent Moon
    fillCircle(140, 50, 15, WHITE);
    fillCircle(132, 47, 13, BLACK); // shadow overlap

    // Drawing sleeping pet
    drawBitmap(84, petY, getPetSprite(4), 32, 32, WHITE);

    // Zzz...
    drawString(124, 75, "Z", 1, WHITE);
    drawString(132, 65, "Z", 2, WHITE);

    // Double bottom bar separator
    fillRect(0, 168, W, 2, WHITE);
    fillRect(0, 171, W, 2, WHITE);

    drawStringCentered(W / 2, 180, "ZZZ... SLEEPING...", 1, WHITE);
    display->EPD_DisplayPart();
    return;
  }

  // Draw house decorations (Window, Floor line)
  // Double top bar separator
  fillRect(0, 26, W, 2, BLACK);
  fillRect(0, 29, W, 2, BLACK);

  // Floor line
  drawLine(0, 135, W - 1, 135, BLACK);

  // Draw weather window in background
  drawWeatherWindow(80, 45, 40, 35);

  // Double bottom bar separator
  fillRect(0, 168, W, 2, BLACK);
  fillRect(0, 171, W, 2, BLACK);

  // --- DRAW ACTIVE ICON MENU (TOP BAR) ---
  const uint8_t* iconPointers[] = {ICON_FEED, ICON_PLAY, ICON_CLEAN, ICON_HEAL, ICON_SLEEP, ICON_STATS};
  int iconXCoords[] = {8, 41, 74, 107, 140, 173};

  // Draw all 6 standard icons across top bar
  for (int i = 0; i < 6; i++) {
    drawBitmap(iconXCoords[i], 5, iconPointers[i], 16, 16, BLACK);
  }

  // Draw cursor box around selected icon slot in Menu state
  if (gameState == STATE_MENU) {
    drawRect(iconXCoords[menuSelectedIdx] - 4, 2, 24, 22, BLACK);
    drawRect(iconXCoords[menuSelectedIdx] - 3, 3, 22, 20, BLACK);
  }

  // --- DRAW POOP ---
  if (poopCount >= 1) drawBitmap(150, 119, ICON_POOP, 16, 16, BLACK);
  if (poopCount >= 2) drawBitmap(30, 119, ICON_POOP, 16, 16, BLACK);
  if (poopCount >= 3) drawBitmap(55, 119, ICON_POOP, 16, 16, BLACK);

  // --- DRAW PET (MAIN AREA) ---
  if (gameState == STATE_DEAD) {
    drawBitmap(84, petY, getPetSprite(5), 32, 32, BLACK);
  } 
  else if (gameState == STATE_HEAL && petIsSick) {
    // Draw pet sad next to syringe
    drawBitmap(petX, petY, getPetSprite(3), 32, 32, BLACK);
    // Draw medical cross briefcase
    drawBitmap(petX - 25, petY + 12, ICON_HEAL, 16, 16, BLACK);
  }
  else if (isEatingTicks > 0) {
    // Eating bounce animation
    int eatOffset = (isEatingTicks % 2 == 0) ? -4 : 0;
    drawBitmap(petX, petY + eatOffset, getPetSprite(1), 32, 32, BLACK);
    // Draw little bowl of food
    drawBitmap(petX - 18, petY + 16, ICON_FEED, 16, 16, BLACK);
  }
  else if (gameState == STATE_PLAY_ROUND) {
    // Guess game turn result
    bool petTurnDir = (petTargetTurn == 1); // mirror if right (1)
    if (roundResultCorrect) {
      drawBitmap(84, petY - 5, getPetSprite(2), 32, 32, BLACK, petTurnDir);
    } else {
      drawBitmap(84, petY, getPetSprite(3), 32, 32, BLACK, petTurnDir);
    }
  }
  else if (petIsSick) {
    drawBitmap(petX, petY, getPetSprite(3), 32, 32, BLACK);
    // Sick flashing skull
    if (statAlertFlash) {
      drawBitmap(petX + 35, petY - 8, ICON_SKULL, 16, 16, BLACK);
    }
  }
  else {
    // Idle state alternating frames
    const uint8_t* activePetFrame = getPetSprite(animFrame == 0 ? 0 : 1);
    drawBitmap(petX, petY, activePetFrame, 32, 32, BLACK, isMoving && (petTargetX > petX));
  }

  // --- DRAW STATE-SPECIFIC INFO & BOTTOM STATUS BAR ---
  switch (gameState) {
    case STATE_IDLE:
      if (petIsSick) {
        drawStringCentered(W / 2, 180, "CLODY IS SICK! T-T", 1, BLACK);
      } else if (petHunger <= 1) {
        drawStringCentered(W / 2, 180, "CLODY IS STARVING!", 1, BLACK);
      } else if (petHappiness <= 1) {
        drawStringCentered(W / 2, 180, "CLODY IS DEPRESSED", 1, BLACK);
      } else {
        // Alternate between standard idle stats and a cozy weather status dialogue based on animation frame
        if (animFrame == 0) {
          char idleMsg[25];
          sprintf(idleMsg, "CLODY IS IDLE  AG:%d", petAge);
          drawStringCentered(W / 2, 180, idleMsg, 1, BLACK);
        } else {
          if (currentWeather == WEATHER_SUNNY) {
            drawStringCentered(W / 2, 180, "IT'S A SUNNY DAY! *.*", 1, BLACK);
          } else if (currentWeather == WEATHER_RAINY) {
            drawStringCentered(W / 2, 180, "LISTENING TO RAIN... v.v", 1, BLACK);
          } else {
            drawStringCentered(W / 2, 180, "IT'S SNOWING OUTSIDE! ^.^", 1, BLACK);
          }
        }
      }
      break;

    case STATE_MENU:
      // Status bar hints for menu navigation
      switch (menuSelectedIdx) {
        case 0: drawStringCentered(W / 2, 180, "FEED: MEALS & SNACKS", 1, BLACK); break;
        case 1: drawStringCentered(W / 2, 180, "PLAY: GUESS GAME", 1, BLACK); break;
        case 2: drawStringCentered(W / 2, 180, "CLEAN: SWEEP POOP", 1, BLACK); break;
        case 3: drawStringCentered(W / 2, 180, "HEAL: CURE SICKNESS", 1, BLACK); break;
        case 4: drawStringCentered(W / 2, 180, "SLEEP: TOGGLE LIGHTS", 1, BLACK); break;
        case 5: drawStringCentered(W / 2, 180, "STATS: CHECK WELLBEING", 1, BLACK); break;
        case 6: drawStringCentered(W / 2, 180, "EXIT MENU", 1, BLACK); break;
      }
      break;

    case STATE_FEED:
      // Draw sub menu selector
      fillRect(40, 48, 120, 50, WHITE);
      drawRect(40, 48, 120, 50, BLACK);
      drawRect(42, 50, 116, 46, BLACK);

      drawString(52, 58, (feedSelectedIdx == 0) ? ">MEAL  +1KG" : " MEAL  +1KG", 1, BLACK);
      drawString(52, 78, (feedSelectedIdx == 1) ? ">SNACK +2KG" : " SNACK +2KG", 1, BLACK);

      drawStringCentered(W / 2, 180, "REC=TOGGLE   PWR=EAT", 1, BLACK);
      break;

    case STATE_PLAY_START:
      {
        char rdStr[20];
        sprintf(rdStr, "ROUND %d / 3", gameRound);
        drawStringCentered(W / 2, 50, rdStr, 1, BLACK);
        drawStringCentered(W / 2, 70, "WHERE WILL I TURN?", 1, BLACK);
        
        // Bubbles "?"
        drawCircle(124, 75, 5, BLACK);
        drawString(122, 71, "?", 1, BLACK);

        drawStringCentered(W / 2, 180, "GUESS: REC=L  PWR=R", 1, BLACK);
      }
      break;

    case STATE_PLAY_ROUND:
      {
        char rRes[25];
        sprintf(rRes, "ROUND %d: %s", gameRound, roundResultCorrect ? "WIN!" : "MISS!");
        drawStringCentered(W / 2, 50, rRes, 1, BLACK);
        
        char guessRes[25];
        sprintf(guessRes, "YOU:%s PET:%s", (userGuess == 0) ? "L" : "R", (petTargetTurn == 0) ? "L" : "R");
        drawStringCentered(W / 2, 70, guessRes, 1, BLACK);

        drawStringCentered(W / 2, 180, "WAITING...", 1, BLACK);
      }
      break;

    case STATE_PLAY_OVER:
      {
        drawStringCentered(W / 2, 50, "GAME COMPLETED!", 1, BLACK);
        char scoreStr[20];
        sprintf(scoreStr, "SCORE: %d / 3", gameCorrectCount);
        drawStringCentered(W / 2, 70, scoreStr, 1, BLACK);

        if (gameCorrectCount >= 2) {
          drawStringCentered(W / 2, 180, "WINNER! HAPPY +2 WT-2", 1, BLACK);
        } else {
          drawStringCentered(W / 2, 180, "LOSE! COMFORT +1 WT-1", 1, BLACK);
        }
      }
      break;

    case STATE_CLEAN:
      // Sweep broom line animation
      {
        int broomX = cleanAnimTick * 40;
        drawLine(broomX, 35, broomX - 15, 134, BLACK);
        drawLine(broomX, 35, broomX - 12, 134, BLACK);
        drawLine(broomX, 35, broomX - 9, 134, BLACK);
        fillRect(broomX - 18, 120, 10, 14, BLACK); // broom brush fibers

        drawStringCentered(W / 2, 180, "SWEEPING SCREEN...", 1, BLACK);
      }
      break;

    case STATE_HEAL:
      if (petIsSick) {
        drawStringCentered(W / 2, 50, "GIVING ANTIBIOTICS", 1, BLACK);
        drawStringCentered(W / 2, 180, "PRESS ANY BTN TO OK", 1, BLACK);
      } else {
        drawStringCentered(W / 2, 60, "PET IS ALREADY FIT!", 1, BLACK);
        drawStringCentered(W / 2, 180, "PRESS ANY BTN TO OK", 1, BLACK);
      }
      break;

    case STATE_STATS_AGE:
      fillRect(30, 40, 140, 70, WHITE);
      drawRect(30, 40, 140, 70, BLACK);
      drawRect(32, 42, 136, 66, BLACK);

      if (petAge < 2) {
        drawStringCentered(100, 52, "CLODY (BABY)", 1, BLACK);
      } else if (petAge < 5) {
        drawStringCentered(100, 52, "CLODY (TEEN)", 1, BLACK);
      } else if (petAge < 10) {
        drawStringCentered(100, 52, "CLODY (ADULT)", 1, BLACK);
      } else {
        drawStringCentered(100, 52, "CLODY (LEGEND)", 1, BLACK);
      }
      
      char ageLine[20];
      sprintf(ageLine, "AGE: %d YEARS", petAge);
      drawString(42, 72, ageLine, 1, BLACK);

      char wtLine[20];
      sprintf(wtLine, "WEIGHT: %d KG", petWeight);
      drawString(42, 88, wtLine, 1, BLACK);

      drawStringCentered(W / 2, 180, "REC=NEXT   PWR=EXIT", 1, BLACK);
      break;

    case STATE_STATS_HUNGER:
      fillRect(30, 40, 140, 70, WHITE);
      drawRect(30, 40, 140, 70, BLACK);
      drawRect(32, 42, 136, 66, BLACK);

      drawStringCentered(100, 52, "HUNGER STATUS", 1, BLACK);
      
      // Draw 4 hearts representing hunger
      for (int i = 0; i < 4; i++) {
        const uint8_t* hType = (i < petHunger) ? HEART_FULL : HEART_EMPTY;
        drawBitmap(50 + i * 22, 78, hType, 8, 8, BLACK);
      }

      drawStringCentered(W / 2, 180, "REC=NEXT   PWR=EXIT", 1, BLACK);
      break;

    case STATE_STATS_HAPPY:
      fillRect(30, 40, 140, 70, WHITE);
      drawRect(30, 40, 140, 70, BLACK);
      drawRect(32, 42, 136, 66, BLACK);

      drawStringCentered(100, 52, "HAPPINESS STATUS", 1, BLACK);
      
      // Draw 4 hearts representing happiness
      for (int i = 0; i < 4; i++) {
        const uint8_t* hType = (i < petHappiness) ? HEART_FULL : HEART_EMPTY;
        drawBitmap(50 + i * 22, 78, hType, 8, 8, BLACK);
      }

      drawStringCentered(W / 2, 180, "REC=DONE   PWR=EXIT", 1, BLACK);
      break;

    case STATE_DEAD:
      drawStringCentered(W / 2, 50, "DIED OF STARVATION", 1, BLACK);
      drawStringCentered(W / 2, 70, "OR UNTREATED ILLNESS", 1, BLACK);
      drawStringCentered(W / 2, 180, "PWR: HATCH NEW EGG", 1, BLACK);
      break;
  }

  // Push buffer to display
  display->EPD_DisplayPart();
}

// -----------------------------------------------------------------------------
// Arduino Loop Execution
// -----------------------------------------------------------------------------

void loop() {
  static bool btn1Last = HIGH;
  static bool btn2Last = HIGH;

  // Track standard ticks
  unsigned long now = millis();

  // 1. Check statistics and time decay progression
  updateStats();

  // 2. Ticker for breathing/flashing animations
  if (now - lastAnimMs >= ANIM_INTERVAL_MS) {
    lastAnimMs = now;
    animFrame = (animFrame + 1) % 2;
    statAlertFlash = !statAlertFlash;

    // Refresh display to reflect breathe animations or sick flasher
    if (gameState == STATE_IDLE || gameState == STATE_SLEEP) {
      renderScreen();
    }
  }

  // 3. Process game temporary states with non-blocking delays
  if (gameState == STATE_PLAY_ROUND && (now - gameRoundTimer >= 2200)) {
    // Auto advance round or finish game
    if (gameRound < 3) {
      gameRound++;
      gameState = STATE_PLAY_START;
    } else {
      gameState = STATE_PLAY_OVER;
      gameRoundTimer = now; // reuse timer for play over display

      // Reward calculations
      if (gameCorrectCount >= 2) {
        petHappiness = min(4, petHappiness + 2);
        petWeight = max(5, petWeight - 2);
        soundSuccess();
      } else {
        petHappiness = min(4, petHappiness + 1);
        petWeight = max(5, petWeight - 1);
        soundNext();
      }
      saveState();
    }
    renderScreen();
  }

  if (gameState == STATE_PLAY_OVER && (now - gameRoundTimer >= 3500)) {
    // Return to idle after game over screens
    gameState = STATE_IDLE;
    renderScreen();
  }

  if (gameState == STATE_CLEAN) {
    // Broom sweeping step timer
    static unsigned long lastCleanTickMs = 0;
    if (now - lastCleanTickMs >= 200) {
      lastCleanTickMs = now;
      cleanAnimTick++;
      playBeep(2200 - cleanAnimTick * 300, 30); // brushing sound

      if (cleanAnimTick > 5) {
        poopCount = 0;
        gameState = STATE_IDLE;
        soundSuccess();
        saveState();
      }
      renderScreen();
    }
  }

  // 4. Inactivity timeout: auto return to IDLE
  if (gameState != STATE_IDLE && gameState != STATE_SLEEP && gameState != STATE_DEAD &&
      gameState != STATE_PLAY_ROUND && gameState != STATE_PLAY_OVER && gameState != STATE_CLEAN) {
    if (now - lastInactivityMs >= INACTIVITY_TIMEOUT_MS) {
      Serial.println("[TIMEOUT] Returning to IDLE due to inactivity.");
      gameState = STATE_IDLE;
      soundBack();
      renderScreen();
    }
  }

  // 5. Input button polling (Debounced active LOW)
  bool btn1Now = digitalRead(BTN_1);
  bool btn2Now = digitalRead(BTN_2);

  if (btn1Last == HIGH && btn1Now == LOW) {
    delay(20); // physical debounce
    if (digitalRead(BTN_1) == LOW) {
      handleButtonPress(1);
    }
  }

  if (btn2Last == HIGH && btn2Now == LOW) {
    delay(20); // physical debounce
    if (digitalRead(BTN_2) == LOW) {
      handleButtonPress(2);
    }
  }

  btn1Last = btn1Now;
  btn2Last = btn2Now;

  // 6. Process incoming USB Serial commands (for easy weather triggers, wifi config & testing)
  if (Serial.available() > 0) {
    String rawCmd = Serial.readStringUntil('\n');
    String cmd = rawCmd;
    cmd.trim();
    cmd.toLowerCase();

    if (cmd.startsWith("wifi ")) {
      String params = rawCmd.substring(5);
      params.trim();
      int firstSpace = params.indexOf(' ');
      String ssid = "";
      String pass = "";
      if (firstSpace != -1) {
        ssid = params.substring(0, firstSpace);
        pass = params.substring(firstSpace + 1);
      } else {
        ssid = params;
      }
      ssid.trim();
      pass.trim();

      saveWifiCredentials(ssid, pass);
      Serial.printf("[WIFI] Credentials updated - SSID: '%s'. Connecting and syncing weather...\n", ssid.c_str());
      connectWifiAndFetchWeather(true);
    }
    else if (cmd.startsWith("weather ")) {
      String type = cmd.substring(8);
      type.trim();
      if (type.startsWith("key ")) {
        String keyVal = rawCmd.substring(12);
        keyVal.trim();
        saveWeatherAPIKey(keyVal);
        Serial.printf("[COMMAND] OpenWeatherMap API key saved to flash: '%s'. Triggering sync...\n", keyVal.c_str());
        connectWifiAndFetchWeather(true);
      } else if (type == "sunny" || type == "sun") {
        currentWeather = WEATHER_SUNNY;
        Serial.println("[COMMAND] Weather forced to SUNNY.");
        saveState();
        renderScreen();
      } else if (type == "rainy" || type == "rain") {
        currentWeather = WEATHER_RAINY;
        Serial.println("[COMMAND] Weather forced to RAINY.");
        saveState();
        renderScreen();
      } else if (type == "snowy" || type == "snow") {
        currentWeather = WEATHER_SNOWY;
        Serial.println("[COMMAND] Weather forced to SNOWY.");
        saveState();
        renderScreen();
      } else if (type == "sync") {
        Serial.println("[COMMAND] Forcing manual weather sync from Berlin API...");
        connectWifiAndFetchWeather(true);
      } else {
        Serial.println("[COMMAND] Unknown weather parameter. Try: key <API_KEY>, sunny, rainy, snowy, sync");
      }
    } else if (cmd == "weather") {
      Serial.printf("[COMMAND] Current weather: %s\n", 
                    (currentWeather == WEATHER_SUNNY) ? "SUNNY" : 
                    (currentWeather == WEATHER_RAINY) ? "RAINY" : "SNOWY");
    } else if (cmd == "weather sync" || cmd == "sync") {
      Serial.println("[COMMAND] Forcing manual weather sync from Berlin API...");
      connectWifiAndFetchWeather(true);
    }
  }

  delay(10);
}
