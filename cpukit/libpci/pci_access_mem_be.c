/* Registers-over-Memory Space - Generic Big endian PCI bus definitions */

#include <pci.h>

/* Same for Little and Big endian PCI buses */
extern uint8_t pci_mem_ld8(uint8_t *adr);
extern void pci_mem_st8(uint8_t *adr, uint8_t data);

uint16_t pci_mem_be_ld_le16(uint16_t *adr)
{
	return ld_be16(adr);
}

uint16_t pci_mem_be_ld_be16(uint16_t *adr)
{
	return ld_le16(adr);
}

uint32_t pci_mem_be_ld_le32(uint32_t *adr)
{
	return ld_be32(adr);
}

uint32_t pci_mem_be_ld_be32(uint32_t *adr)
{
	return ld_le32(adr);
}

void pci_mem_be_st_le16(uint16_t *adr, uint16_t data)
{
	st_be16(adr, data);
}

void pci_mem_be_st_be16(uint16_t *adr, uint16_t data)
{
	st_le16(adr, data);
}

void pci_mem_be_st_le32(uint32_t *adr, uint32_t data)
{
	st_be32(adr, data);
}

void pci_mem_be_st_be32(uint32_t *adr, uint32_t data)
{
	st_le32(adr, data);
}

struct pci_memreg_ops pci_mem_be_ops = {
	.ld8    = pci_mem_ld8,
	.st8    = pci_mem_st8,

	.ld_le16 = pci_mem_be_ld_le16,
	.st_le16 = pci_mem_be_st_le16,
	.ld_be16 = pci_mem_be_ld_be16,
	.st_be16 = pci_mem_be_st_be16,

	.ld_le32 = pci_mem_be_ld_le32,
	.st_le32 = pci_mem_be_st_le32,
	.ld_be32 = pci_mem_be_ld_be32,
	.st_be32 = pci_mem_be_st_be32,
};
