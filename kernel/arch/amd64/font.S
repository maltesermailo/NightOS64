section .rodata
global font_data_start
global font_data_end
extern font_data_start
extern font_data_end

font_data_start:
    incbin "./kernel/arch/amd64/unifont.sfn"
font_data_end: