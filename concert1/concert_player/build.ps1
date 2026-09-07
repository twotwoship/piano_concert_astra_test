param([string]$ToolDir = 'C:\arm-gnu-toolchain-15.2.rel1-mingw-w64-i686-arm-none-eabi')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $cc = Join-Path $ToolDir 'bin\arm-none-eabi-gcc.exe'
    $objcopy = Join-Path $ToolDir 'bin\arm-none-eabi-objcopy.exe'
    $size = Join-Path $ToolDir 'bin\arm-none-eabi-size.exe'
    if (-not (Test-Path -LiteralPath $cc)) { throw "Compiler not found: $cc" }
    $arch = @('-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard')
    $flags = $arch + @('-DSTM32F411xE','-std=gnu99','-Os','-Wall','-Wextra','-Werror','-g','-ffreestanding','-fno-common','-I.')
    $sources = @('main.c','buzzer.c','player.c','clock.c','system_stm32f4xx.c','generated/score_data.c')
    $objects = @('crt0.o')
    & $cc @arch -c crt0.s -o crt0.o
    if ($LASTEXITCODE -ne 0) { throw 'Startup compilation failed' }
    foreach ($source in $sources) {
        $object = [IO.Path]::ChangeExtension($source, '.o')
        & $cc @flags -c $source -o $object
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $source" }
        $objects += $object
    }
    & $cc @arch @objects -nostdlib '-Wl,-Map=concert.map' -T rom_0x08000000.lds -lgcc -o concert.elf
    if ($LASTEXITCODE -ne 0) { throw 'Link failed' }
    & $objcopy -O binary concert.elf concert.bin
    if ($LASTEXITCODE -ne 0) { throw 'BIN generation failed' }
    & $objcopy -O ihex concert.elf concert.hex
    if ($LASTEXITCODE -ne 0) { throw 'HEX generation failed' }
    & $size concert.elf
    if ($LASTEXITCODE -ne 0) { throw 'Size check failed' }
} finally { Pop-Location }
