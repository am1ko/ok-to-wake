# Kids Night Clock for Zephyr

This project builds a simple animated analog clock for the STM32F746G-DISCO board.

Features:
- Light green background during wake time (08:00-18:59)
- Gray background during sleep time (21:00-06:59)
- Orange background during the transition window (19:00-20:59)
- A simple analog clock face drawn on the LCD

## Build

```sh
west init -m https://github.com/zephyrproject-rtos/zephyr.git zephyrproject
cd zephyrproject
west update
west build -b stm32f746g_disco ../kids-alarm-clock-zephyr
```
