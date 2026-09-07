ENTRY(__start)
MEMORY {
    ROM (rx) : ORIGIN = 0x08000000, LENGTH = 512K
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}
SECTIONS {
    .text : {
        __RO_BASE__ = .;
        KEEP(*crt0.o(.text))
        *(.text .text.*)
        . = ALIGN(4);
    } > ROM
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > ROM
    .ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > ROM
    .rodata : {
        *(.rodata .rodata.*)
        . = ALIGN(8);
        __RO_LIMIT__ = .;
    } > ROM
    .data : AT(__RO_LIMIT__) {
        __RW_BASE__ = .;
        *(.data .data.*)
        . = ALIGN(4);
        __RW_LIMIT__ = .;
    } > RAM
    .bss (NOLOAD) : {
        __ZI_BASE__ = .;
        *(.bss .bss.*) *(COMMON)
        . = ALIGN(4);
        __ZI_LIMIT__ = .;
    } > RAM
    ASSERT(__ZI_LIMIT__ <= ORIGIN(RAM) + LENGTH(RAM) - 4096, "No stack space")
}
