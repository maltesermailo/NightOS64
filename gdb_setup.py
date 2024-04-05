# Licensed under MIT by Ian Norris
# https://github.com/IanNorris/Enkel

import gdb
import os.path

def GetRegisterValue(registerName):
    # Execute the "info registers <reg>" command and capture its output
    output = gdb.execute("info registers {}".format(registerName), to_string=True)

    # Extract the PDBR value from the output string
    return int(output.split()[1], 16)

def GetRegisterString(registerName):
	address = GetRegisterValue(registerName)
	message = int(address.cast(gdb.lookup_type('string')))
	return message

def GetFunctionAddress(functionName):
	address = gdb.parse_and_eval("&{}".format(functionName))
	address = int(address.cast(gdb.lookup_type('int'))) # Magic incantation to get the address only
	return address

def get_pml4_base():
    # Extract the PDBR value from the output string
    pml4_base = GetRegisterValue("cr3")
    pml4_base = pml4_base | 0xfffffe8000000000

    return pml4_base

def translate_address(virtual_address):
    # Get the base address of the PML4 from the CR3 register
    pml4_base = get_pml4_base()

    # Calculate the address of the PML4 entry
    pml4_entry = pml4_base + ((virtual_address >> 39) & 0x1FF) * 8

    # Get the PML4 entry
    pml4_value = gdb.parse_and_eval("*((unsigned long long *)%s)" % pml4_entry)

    # Check if the PML4 entry is present
    if not pml4_value & 1:
        raise Exception("Address not mapped (PML4 entry not present)")

    # Print flags for PML4
    print_flags("PML4", pml4_value)
	
    pml4_value = pml4_value | 0xfffffe8000000000

    # Get the base address of the PDPT from the PML4 entry
    pdpt_base = pml4_value & 0xfffffffffffff000
    pt_entry_value = None

    # Repeat the process for the PDPT, PDT, and PT
    for level, shift in [("PDPT", 30), ("PDT", 21), ("PT", 12)]:
        entry_address = pdpt_base + ((virtual_address >> shift) & 0x1FF) * 8
        entry_value = gdb.parse_and_eval("*((unsigned long long *)%s)" % entry_address)

        # Print flags for the current level
        print_flags(level, entry_value)

        # Check if the entry is present
        if not entry_value & 1:
            raise Exception("Address not mapped (%s entry not present)" % level)
		
        entry_value = entry_value | 0xfffffe8000000000

        pdpt_base = entry_value & 0xfffffffffffff000

        if level == "PT":
            pt_entry_value = entry_value | 0x7fffffffff
            pdpt_base = entry_value | 0x7fffffffff

    # Calculate the physical address
    physical_address = pdpt_base + (virtual_address & 0xFFF)

    return physical_address

def print_flags(level, entry_value):
    p = entry_value & 1
    rw = (entry_value >> 1) & 1
    us = (entry_value >> 2) & 1
    pwt = (entry_value >> 3) & 1
    pcd = (entry_value >> 4) & 1
    a = (entry_value >> 5) & 1
    d = (entry_value >> 6) & 1
    pat = (entry_value >> 7) & 1
    g = (entry_value >> 8) & 1
    nx = (entry_value >> 63) & 1
    print("%s value: %x, Present: %d, R/W: %d, User/Supervisor: %d, PageWriteThrough: %d, PageCacheDisable: %d, Accessed: %d, Dirty: %d, PageAttributeTable/PS: %d, Global: %d, NoExecute: %d" % (level, entry_value, p, rw, us, pwt, pcd, a, d, pat, g, nx))


class TranslateCommand(gdb.Command):
    def __init__(self):
        super(TranslateCommand, self).__init__("translate_address", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        virtual_address = int(arg, 0)
        try:
            physical_address = translate_address(virtual_address)
            print("Virtual address 0x%x translates to physical address 0x%x" % (virtual_address, physical_address))
        except Exception as e:
            print(str(e))

TranslateCommand()