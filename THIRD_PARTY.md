# Происхождение исходников и зависимости

- Прикладная часть, настройки платы, HAL/CMSIS и ПК-клиент получены из пользовательского архива ЛР3 `PVS3_FULL.zip`.
  Исходные комментарии и лицензионные уведомления сохранены. Отдельная лицензия на пользовательскую прикладную часть архивом не предоставлена.
- Отсутствовавшие HAL I2C `stm32f4xx_hal_i2c.c/.h`, `stm32f4xx_hal_i2c_ex.c/.h` взяты из
  [STM32CubeF4 v1.27.0](https://github.com/STMicroelectronics/STM32CubeF4/tree/v1.27.0/Drivers/STM32F4xx_HAL_Driver).
  Лицензия ST сохранена в `PVS4/Drivers/STM32F4xx_HAL_Driver/LICENSE.txt` и заголовках файлов.
- CMSIS: ARM, лицензия из исходного архива находится в `PVS4/Drivers/CMSIS/LICENSE.txt`;
  файлы устройства ST — `PVS4/Drivers/CMSIS/Device/ST/STM32F4xx/LICENSE.txt`.
- FreeRTOS Kernel **V10.6.2**, commit `18ed8886fe3eed9dbe433a82cdc62ce5dc0315eb`,
  [официальный репозиторий](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V10.6.2), MIT.
  Включены ядро, заголовки, порт GCC ARM_CM4F и heap_4; лицензия — `PVS4/Middlewares/FreeRTOS/LICENSE.md`.
- pyserial устанавливается отдельно по `client/requirements.txt` и не включен в исходники.

Версии зависимостей зафиксированы исходниками в репозитории. Загрузка зависимостей для сборки прошивки не нужна.
Глобальная лицензия репозитория не переопределяет права на файлы из исходного архива и компоненты ST/ARM/FreeRTOS.
