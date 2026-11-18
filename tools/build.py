"""Standalone build with the same sources and target as the CubeIDE project."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--toolchain', type=Path, help='directory containing arm-none-eabi-gcc')
    args = parser.parse_args()
    prefix = str(args.toolchain / 'arm-none-eabi-') if args.toolchain else 'arm-none-eabi-'
    gcc = shutil.which(prefix + 'gcc')
    if not gcc:
        parser.error('arm-none-eabi-gcc not found; set PATH or --toolchain')
    fw = ROOT / 'PVS4'
    build = ROOT / 'build'
    build.mkdir(exist_ok=True)
    cpu = ['-mcpu=cortex-m4', '-mthumb', '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard']
    inc = ['Core/Inc', 'Drivers/STM32F4xx_HAL_Driver/Inc',
           'Drivers/STM32F4xx_HAL_Driver/Inc/Legacy', 'Drivers/CMSIS/Include',
           'Drivers/CMSIS/Device/ST/STM32F4xx/Include',
           'Middlewares/FreeRTOS/include', 'Middlewares/FreeRTOS/portable/GCC/ARM_CM4F']
    flags = cpu + ['-std=gnu11', '-Og', '-g3', '-Wall', '-Wextra',
                   '-ffunction-sections', '-fdata-sections', '-fno-common',
                   '-DUSE_HAL_DRIVER', '-DSTM32F427xx'] + ['-I' + str(fw / p) for p in inc]
    sources = sorted((fw / 'Core/Src').glob('*.c'))
    sources += sorted((fw / 'Drivers/STM32F4xx_HAL_Driver/Src').glob('*.c'))
    sources += sorted((fw / 'Middlewares/FreeRTOS').glob('*.c'))
    sources += [fw / 'Middlewares/FreeRTOS/portable/GCC/ARM_CM4F/port.c',
                fw / 'Middlewares/FreeRTOS/portable/MemMang/heap_4.c',
                fw / 'Core/Startup/startup_stm32f427vitx.s']

    def compile_source(source):
        obj = build / (source.relative_to(fw).as_posix().replace('/', '_') + '.o')
        subprocess.run([gcc, *flags, '-c', str(source), '-o', str(obj)], check=True)
        return str(obj)

    with ThreadPoolExecutor(max_workers=8) as pool:
        objects = list(pool.map(compile_source, sources))
    elf = build / 'PVS4.elf'
    subprocess.run([gcc, *cpu, *objects, '-T' + str(fw / 'STM32F427VITX_FLASH.ld'),
                    '--specs=nano.specs', '--specs=nosys.specs',
                    '-Wl,--gc-sections', '-Wl,-Map=' + str(build / 'PVS4.map'),
                    '-lm', '-lc', '-o', str(elf)], check=True)
    for fmt, ext in [('binary', 'bin'), ('ihex', 'hex')]:
        subprocess.run([prefix + 'objcopy', '-O', fmt, str(elf), str(build / ('PVS4.' + ext))], check=True)
    subprocess.run([prefix + 'size', str(elf)], check=True)
    print('Built:', elf)


if __name__ == '__main__':
    main()
