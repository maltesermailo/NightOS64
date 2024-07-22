ENTRY(_start)

LMA = 0x800000;

SECTIONS
{
    . = LMA;

    .text : AT(ADDR(.text))
    {
        __ehdr_start = .;
        _code = .;
        . = ALIGN(4096);
    }

   .data : AT(ADDR(.data))
   {
        _data = .;
        *(.data)
        . = ALIGN(4096);
   }
   .rodata : AT(ADDR(.rodata))
   {
      _rodata = .;
      *(.rodata*)
      . = ALIGN(4096);
   }

   .eh_frame : AT(ADDR(.eh_frame))
   {
       _ehframe = .;
       *(.eh_frame)
        . = ALIGN(4096);
   }

   .bss : AT(ADDR(.bss))
   {
       _bss = .;
       *(.bss)

       /*
        * You usually need to include generated COMMON symbols
        * under kernel BSS section or use gcc's -fno-common
        */

        *(COMMON)
       . = ALIGN(4096);
   }

   .init_array :
   {
       PROVIDE_HIDDEN (__init_array_start = .);
       KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
       KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
       PROVIDE_HIDDEN (__init_array_end = .);
   }

   .preinit_array :
   {
     PROVIDE_HIDDEN (__preinit_array_start = .);
     KEEP (*(.preinit_array))
     PROVIDE_HIDDEN (__preinit_array_end = .);
   }

   _end = .;

   /DISCARD/ :
   {
        *(.comment)
   }
}