#include <kernel/config.h>

#include <arch/i386/acpi.h>
#include <arch/i386/apic.h>
#include <arch/i386/paging.h>
#include <arch/i386/io.h>
#include <mem/vasa.h>
#include <stdlib.h>
#include <kprint.h>

#ifdef BRACKOS_CONF_ACPI

#define ACPI_SIGNATURE_RSDP1 0x20445352
#define ACPI_SIGNATURE_RSDP2 0x20525450
#define ACPI_SIGNATURE_RSDT  0x54445352
#define ACPI_SIGNATURE_FADP  0x46414350
#define ACPI_SIGNATURE_APIC  0x41504943
#define ACPI_SIGNATURE_HPET  0x48504554

extern unsigned long kernel_base;
uint32_t cpu_count = 0;

uint32_t acpi_irq_redir[16] = {
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
};

// Search memory for the RSDP signature
rsdp_desc_t* rsdp_locate() {
    uintptr_t start = 0xC00E0000;
    uintptr_t end   = 0xC00FFFFF;

    uint32_t sig1 = ACPI_SIGNATURE_RSDP1;
    uint32_t sig2 = ACPI_SIGNATURE_RSDP2;

    for ( int i = start; i < end; i += 16 ) {
        if ( *(uint32_t*)(i) == sig1 && *(uint32_t*)(i + 4) == sig2 ) {
            return (rsdp_desc_t*) i;
        }
    }

    return NULL;
}

// Map an ACPI table into physical memory. Right now this wastes an inane amount of memory.
void* acpi_map_ptr(void* phys, unsigned long size) {
    void* virt = vasa_alloc(MEM_PCI, 4096, 0);

    if ( virt == NULL ) {
        return NULL;
    }

    if ( !paging_map((uintptr_t) phys, (uintptr_t) virt, PAGE_PRESENT | PAGE_RW) ) {
        return NULL;
    }

    return virt + ((uintptr_t) phys & 0xFFF);
}

// Validate an ACPI SDT checksum
bool acpi_validate(void* ptr, unsigned long len) {
    uint8_t* bytes = (uint8_t*) ptr;
    unsigned long validate = 0;

    for ( int i = 0; i < len; i++ ) {
        validate += *(bytes + i);
    }

    return (validate & 0xFF) == 0;
}

// Parse the Multi APIC Descriptor Table
void acpi_parse_madt(acpi_madt_t* madt) {
    kprintf("acpi: local apic at %x\n", madt->local_controller_address);
    uintptr_t records_start = ((uintptr_t) madt) + sizeof(acpi_madt_t);
    uintptr_t records_end = records_start + (madt->table.length - sizeof(acpi_madt_t));

    uintptr_t records_head = records_start;

    bool complete = false;
    cpu_count = 0;

    while ( records_head < records_end && !complete ) {
        uint8_t type = *(uint8_t*) records_head;

        switch ( type ) {
            case 0: {
                acpi_madt_lapic_t* lapic = (acpi_madt_lapic_t*) records_head;

                if ( lapic->length == 0 ) {
                    kprintf("acpi: Found zero length record\n");
                    complete = true;
                    break;
                }

                kprintf("acpi: Found processor #%d (0x%x)\n", lapic->processor_id, lapic->flags);
                cpu_count++;
                records_head += lapic->length;
                break;
            }

            case 1: {
                acpi_madt_ioapic_t* ioapic = (acpi_madt_ioapic_t*) records_head;

                if ( ioapic->length == 0 ) {
                    kprintf("acpi: Found zero length record\n");
                    complete = true;
                    break;
                }

                kprintf("acpi: Found I/O APIC %d\n", ioapic->id);
                ioapic_setup(ioapic->address);
                records_head += ioapic->length;
                break;
            }

            case 2: {
                acpi_madt_iso_t* iso = (acpi_madt_iso_t*) records_head;

                if ( iso->length == 0 ) {
                    kprintf("acpi: Found zero length record\n");
                    complete = true;
                    break;
                }

                kprintf("acpi: Found Interrupt Source Overide\n");
                kprintf("acpi: - bus %d intr#%d -> intr#%d (%x)\n", iso->bus_src, iso->irq_src, iso->interrupt, iso->flags);
                acpi_irq_redir[iso->irq_src] = iso->interrupt;

                records_head += iso->length;
                break;
            }

            case 3: {
                acpi_madt_nmi_t* nmi = (acpi_madt_nmi_t*) records_head;

                if ( nmi->length == 0 ) {
                    kprintf("acpi: Found zero length record\n");
                    complete = true;
                    break;
                }

                kprintf("acpi: Found Nonmaskable Interrupt\n");
                records_head += nmi->length;
                break;
            }

            case 4: {
                acpi_madt_lapic_nmi_t* lapic_nmi = (acpi_madt_lapic_nmi_t*) records_head;

                if ( lapic_nmi->length == 0 ) {
                    kprintf("acpi: Found zero length record\n");
                    complete = true;
                    break;
                }

                kprintf("acpi: Found local APIC Nonmaskable Interrupt\n");
                kprintf("acpi: - processor #%d interrupt #%d\n", lapic_nmi->processor_id, lapic_nmi->lint);
                records_head += lapic_nmi->length;
                break;
            }

            case 5: {
                acpi_madt_lapic_ao_t* lapic_ao = (acpi_madt_lapic_ao_t*) records_head;

                if ( lapic_ao->length == 0 ) {
                    kprintf("acpi: Found zero length record\n");
                    complete = true;
                    break;
                }

                kprintf("acpi: Found local APIC Address Override\n");
                records_head += lapic_ao->length;
                break;
            }

            case 6: {
                acpi_madt_io_sapic_t* io_sapic = (acpi_madt_io_sapic_t*) records_head;

                if ( io_sapic->length == 0 ) {
                    kprintf("acpi: Found zero length record\n");
                    complete = true;
                    break;
                }

                kprintf("acpi: Found I/O SAPIC\n");
                records_head += io_sapic->length;
                break;
            }

            case 7: {
                acpi_madt_local_sapic_t* local_sapic = (acpi_madt_local_sapic_t*) records_head;

                if ( local_sapic->length == 0 ) {
                    kprintf("acpi: Found zero length record\n");
                    complete = true;
                    break;
                }

                // Load the address of the string into the string field
                local_sapic->processor_uid_string = (uint8_t*) ((uintptr_t) &local_sapic->processor_uid_value + sizeof(uint32_t));

                kprintf("acpi: Found local SAPIC\n");
                records_head += local_sapic->length;
                break;
            }

            // Left off of page 142 of ACPI 5.0 spec
            case 8:
            case 9:
            case 10:
                kprintf("acpi: Not yet implemented %d\n", type);
                complete = true;
                break;

            default:
                kprintf("acpi: Found unknown %d\n", type);
                records_head += 1;
                break;
        }
    }
}

// Parse the Fixed ACPI Descriptor Table
void acpi_parse_fadt(acpi_fadt_t* fadt) {
    // Make sure ACPI is enabled
    if ( fadt->smi_cmd_port == 0 || (fadt->acpi_enable == 0 && fadt->acpi_disable == 0) ) {
        kprintf("acpi: enabled\n");
    }
    else {
        uint8_t res = inportw(fadt->pm1a_ctrl_block) & 1;

        if ( res == 0 ) {
            outportb(fadt->smi_cmd_port, fadt->acpi_enable);

            // Wait a good second.
            for ( int i = 0; i < 100000; i++){}

            while ( (inportw(fadt->pm1a_ctrl_block) & 1) == 0 ) {
                // ... TODO: if this doesn't work?
            }

            kprintf("acpi: enabled\n");
        }
        else {
            kprintf("acpi: enabled\n");
        }
    }
}

// Parse the Root System Descriptor Table
bool acpi_parse_rsdt(acpi_rsdt_t* rsdt) {
    if ( !acpi_validate(rsdt, rsdt->table.length) ) {
        kprintf("acpi: failed to validate rsdt\n");
        return false;
    }

    uintptr_t ptrs_start = ((uintptr_t) rsdt) + sizeof(acpi_sdt_t);
    uintptr_t ptrs_end = ptrs_start + (rsdt->table.length - sizeof(acpi_sdt_t));

    for ( uintptr_t ptr = ptrs_start; ptr < ptrs_end; ptr += sizeof(void*) ) {
        uintptr_t phys_sdt = *(uintptr_t*) ptr;
        acpi_sdt_t* sdt = acpi_map_ptr((void*) phys_sdt, sizeof(acpi_sdt_t));

        if ( sdt == NULL ) {
            kprintf("acpi: Failed to map!\n");
            continue;
        }

        acpi_parse_table(sdt);
    }

    return true;
}

// Parse the High Precision Event Timer table
void acpi_parse_hpet(acpi_hpet_t* hpet) {
    kprintf("acpi: hpet #%d\n", hpet->number);
}

// Find the correct SDT parser based off of signature
void acpi_parse_table(acpi_sdt_t* ptr) {
    uint32_t name = (ptr->signature[0] << 24) |
                    (ptr->signature[1] << 16) |
                    (ptr->signature[2] << 8) |
                     ptr->signature[3];

    switch ( name ) {
        case ACPI_SIGNATURE_RSDT:
            kprintf("acpi: Found Root System Descriptor Table!\n");
            acpi_parse_rsdt((acpi_rsdt_t*) ptr);
            break;

        case ACPI_SIGNATURE_FADP:
            kprintf("acpi: Found Fixed Address Descriptor Table!\n");
            acpi_parse_fadt((acpi_fadt_t*) ptr);
            break;

        case ACPI_SIGNATURE_APIC:
            kprintf("acpi: Found the Multiple APIC Descriptor Table!\n");
            acpi_parse_madt((acpi_madt_t*) ptr);
            break;

        case ACPI_SIGNATURE_HPET:
            kprintf("acpi: Found the High Precision Event Timer!\n");
            acpi_parse_hpet((acpi_hpet_t*) ptr);
            break;

        default:
            kprintf("acpi: Found unknown table: %c%c%c%c\n", ptr->signature[0], ptr->signature[1], ptr->signature[2], ptr->signature[3]);
            break;
    }
}

// Remap an IRQ
uint32_t acpi_irq_remap(uint32_t irq) {
    if ( acpi_irq_redir[irq] != 0xFFFFFFFF) {
        return acpi_irq_redir[irq];
    }

    return irq;
}

// Initialize ACPI
bool acpi_init() {
    rsdp_desc_t* rsdp = rsdp_locate();

    if ( rsdp == NULL ) {
        kprintf("acpi: failed to find RSDP");
        return false;
    }

    if ( !acpi_validate(rsdp, sizeof(rsdp_desc_t)) ) {
        kprintf("acpi: failed to validate RSDP");
        return false;
    }

    kprintf("acpi oem: %s\n", rsdp->oem);

    acpi_rsdt_t* rsdt = acpi_map_ptr((void*) rsdp->rsdt, sizeof(acpi_rsdt_t));

    if ( rsdt == NULL ) {
        kprintf("acpi: failed to map rsdt\n");
        return false;
    }

    acpi_parse_rsdt(rsdt);

    kprintf("acpi setup\n");

    return true;
}

#endif
