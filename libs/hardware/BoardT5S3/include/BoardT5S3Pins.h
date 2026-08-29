#pragma once

#include <Arduino.h>

// LilyGO T5S3-4.7-e-paper-PRO / Lite
// https://github.com/Xinyuan-LilyGO/T5S3-4.7-e-paper-PRO

#define T5S3_WIDTH 960
#define T5S3_HEIGHT 540
#define T5S3_LOGICAL_WIDTH 540
#define T5S3_LOGICAL_HEIGHT 960

#define T5S3_BQ25896_ADDR 0x6B
#define T5S3_BQ27220_ADDR 0x55
#define T5S3_GT911_ADDR 0x5D
#define T5S3_PCF85063_ADDR 0x51
#define T5S3_TPS65185_ADDR 0x68
#define T5S3_PCA9535_ADDR 0x20

#define T5S3_GPS_RXD 44
#define T5S3_GPS_TXD 43

#define T5S3_SCL 40
#define T5S3_SDA 39
#define T5S3_I2C_FREQ 400000

#define T5S3_SPI_MISO 21
#define T5S3_SPI_MOSI 13
#define T5S3_SPI_SCLK 14

#define T5S3_TOUCH_INT 3
#define T5S3_TOUCH_RST 9

#define T5S3_SD_CS 12

#define T5S3_LORA_CS 46
#define T5S3_LORA_IRQ 10
#define T5S3_LORA_RST 1
#define T5S3_LORA_BUSY 47

// The Pro Lite ships the same PCB with the LoRa/GPS module unpopulated, so its
// radio pins reach nothing but empty pads -- pads a user can solder a key to.
// Which build this is decides who owns them: with T5S3_HAS_LORA_GPS the radio
// keeps them and the board parks them at boot; without it they are ordinary
// buttons, wired into BoardConfig's input pins like any other board's keys, and
// everything that follows from that (pull-ups, debounce, edges,
// wake-from-light-sleep, the inactivity timer) comes for free.
//
// THREE of the four, though. GPIO46 (LoRa CS) is spoken for: LilyGoT5S3LgfxConfig
// passes it to the parallel EPD bus as pinPwr, and esp_lcd_new_i80_bus demands a
// real DC pin there, so the LCD peripheral owns the pad and parks it LOW between
// refreshes. It was briefly a key as well, which made that key read permanently
// pressed and — because a bound key counts as user activity — pinned the
// inactivity clock at zero, killing both idle light sleep and the auto-sleep
// timeout. See the BoardConfig profile's input comment.
//
// The other three were measured on a Pro Lite with the internal pull-up on:
// stable HIGH over 600 ms each, so an unsoldered pad reads "not pressed" rather
// than floating.
//
// Not exposed as keys, on purpose: the GPS UART pair (GPIO43/44) is the ROM
// bootloader's console, driven by hardware this firmware does not control.
#ifndef T5S3_HAS_LORA_GPS
#define T5S3_HAS_LORA_GPS 0
#endif

#if !T5S3_HAS_LORA_GPS
#define T5S3_KEY_G10 T5S3_LORA_IRQ   // GPIO10, RTC-capable
#define T5S3_KEY_G1 T5S3_LORA_RST    // GPIO1,  RTC-capable
#define T5S3_KEY_G47 T5S3_LORA_BUSY  // GPIO47
// No T5S3_KEY_G46: GPIO46 belongs to the EPD i80 bus (see above).
#endif

#define T5S3_BL_EN 11
#define T5S3_PCA9535_INT 38
#define T5S3_BOOT_BTN 0

#define EP_D7 8
#define EP_D6 18
#define EP_D5 17
#define EP_D4 16
#define EP_D3 15
#define EP_D2 7
#define EP_D1 6
#define EP_D0 5
#define EP_CKV 48
#define EP_STH 41
#define EP_LEH 42
#define EP_STV 45
#define EP_CKH 4

// PCA9535 linear IO indexes, matching IO0..IO15.
// IO1x maps to port 1 bit x, so IO10 is linear index 8.
#define PCA9535_IO10_EP_OE 8
#define PCA9535_IO11_EP_MODE 9
#define PCA9535_IO12_BUTTON 10
#define PCA9535_IO13_TPS_PWRUP 11
#define PCA9535_IO14_VCOM_CTRL 12
#define PCA9535_IO15_TPS_WAKEUP 13
#define PCA9535_IO16_TPS_PWR_GOOD 14
#define PCA9535_IO17_TPS_INT 15

#define PCA9535_IO00_LORA_GPS_EN 0
