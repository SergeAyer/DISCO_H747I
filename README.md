# DISCO_H747I
DISCO_H747I library to be used for integration into mbed-os

After importing this library, one needs to 
The following changes are required after importing this library (with mbed os 6.17.0):

- Add "mbed-os/targets/TARGET_STM/TARGET_STM32H7/STM32Cube_FW/*" in the .mbedignore file
- Modify line 73 of file mbed-os\targets\TARGET_STM\stm_spi_api.c -> extern HAL_StatusTypeDef HAL_SPIEx_FlushRxFifo(const SPI_HandleTypeDef *hspi);
- Modify line 87 of file mbed-os\tools\toolchains\mbed_toolchain.py ->   "Cortex-M7FD":     ["__CORTEX_M7", "ARM_MATH_CM7", "__FPU_PRESENT=1U",
- Add "USBDEVICE" at line 3474 of file mbed-os\targets\targets.json




