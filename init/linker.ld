ENTRY(_start)
OUTPUT_FORMAT(binary)

LMA = 0x800000;

SECTIONS
{
    . = LMA;

    .text : AT(ADDR(.text))
    {
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

   _end = .;

   /DISCARD/ :
   {
        *(.comment)
   }
}