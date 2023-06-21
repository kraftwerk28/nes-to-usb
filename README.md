# Turning NES/Famicom/Dendy/Famiclone controllers into a USB device

The Famiclone controller plug should look similar to

```
 ________________
|                |
| A  B  C  D  E  |
 \  F  G  H  I  /
  \____________/
```

Below is the default pinout from controller plug to stm32 pins. It can be
changed in [pinout.h](./include/pinout.h). The digit on the left denotes
1<sup>st</sup> or 2<sup>nd</sup> controller (They share all pins except MISO,
where stm32 reads buttons from the controller).

| Plug pin | STM32 pin | Description         |
| -------- | --------- | ------------------- |
| `D1`     | `PA1`     | Controller #1 MISO  |
| `C1`     | `PA2`     | Controller #1 Latch |
| `B1`     | `PA3`     | Controller #1 Clock |
| `I1`     | `+5V`     |                     |
| `G1`     | `Ground`  |                     |
| `D2`     | `PA0`     | Controller #2 MISO  |
| `C2`     | `PA2`     | Controller #2 Latch |
| `B2`     | `PA3`     | Controller #2 Clock |
| `I2`     | `+5V`     |                     |
| `G2`     | `Ground`  |                     |
