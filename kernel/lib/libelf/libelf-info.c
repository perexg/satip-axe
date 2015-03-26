/*
 * File     : lib/libelf/info.c
 * Synopsis : Utility library for handling ELF files (info functions)
 * Author   : Carl Shaw <carl.shaw@st.com>
 * Author   : Giuseppe Condorelli <giuseppe.condorelli@st.com>
 *
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 */

#include <linux/libelf.h>

/* Useful for debug, this function shows elf header informations */
void ELF_printHeaderInfo(const struct ELFinfo *elfinfo)
{
	char *typestr[] = {"NONE", "REL", "EXEC", "DYN", "CORE"};
	char *osstr[] = {"NONE", "HPUX", "NETBSD", "Linux", "unknown",
			"unknown", "Solaris", "AIX", "IRIX", "FreeBSD",
			 "TRU64", "Modesto", "OpenBSD", "OpenVMS", "NSK"};

	if ((elfinfo->header)->e_type <= 4)
		printk(KERN_INFO"type       : %s\n",
				typestr[(elfinfo->header)->e_type]);
	else
		printk(KERN_INFO"type       : %u\n", (elfinfo->header)->e_type);
	printk(KERN_INFO"machine    : %u\n", (elfinfo->header)->e_machine);
	printk(KERN_INFO"entry      : 0x%08x\n", (elfinfo->header)->e_entry);
	printk(KERN_INFO"flags      : %u\n", (elfinfo->header)->e_flags);
	printk(KERN_INFO"sections   : %u\n", elfinfo->numsections);
	printk(KERN_INFO"segments   : %u\n", elfinfo->numpheaders);
	printk(KERN_INFO"format     : %s %s\n",
		((elfinfo->header)->e_ident[EI_CLASS] == ELFCLASS32) ?
						"32 bit" : "64 bit",
		((elfinfo->header)->e_ident[EI_DATA] == ELFDATA2LSB) ?
					"little endian" : "big endian");

	if ((elfinfo->header)->e_ident[EI_OSABI] < 15)
		printk(KERN_INFO"OS	: %s\n",
			osstr[(elfinfo->header)->e_ident[EI_OSABI]]);
	else if ((elfinfo->header)->e_ident[EI_OSABI] == 97)
		printk(KERN_INFO"OS         : ARM\n");
	else if ((elfinfo->header)->e_ident[EI_OSABI] == 255)
		printk(KERN_INFO"OS         : STANDALONE\n");
}
EXPORT_SYMBOL(ELF_printHeaderInfo);

/* Useful for debug, this function shows elf section informations */
void ELF_printSectionInfo(const struct ELFinfo *elfinfo)
{
	uint32_t 	i, n;
	char 		*str, *type = NULL;
	Elf32_Shdr	*sec;
	char		flags[10];
	struct typess	elftypes[] = {ELF_TYPES};

	printk(KERN_INFO"Number of sections: %u\n", elfinfo->numsections);
	for (i = 0; i < elfinfo->numsections; i++) {
		sec = ELF_getSectionByIndex(elfinfo, i);
		if (*str == '\0')
			str = "<no name>";

		str = elfinfo->strtab + sec->sh_name;

		for (n = 0; elftypes[n].name != NULL; n++) {
			if (elftypes[n].val == sec->sh_type)
				type = elftypes[n].name;
		}

		if (!type)
			type = "UNKNOWN";

		n = 0;
		if (sec->sh_flags & SHF_WRITE)
			flags[n++] = 'W';
		else if (sec->sh_flags & SHF_ALLOC)
			flags[n++] = 'A';
		else if (sec->sh_flags & SHF_EXECINSTR)
			flags[n++] = 'X';
		else if (sec->sh_flags & SHF_MERGE)
			flags[n++] = 'M';
		else if (sec->sh_flags & SHF_STRINGS)
			flags[n++] = 'S';
		flags[n] = '\0';

		printk(KERN_INFO"[%02u] %s %s %s \n", i, str, type, flags);
	}
}
EXPORT_SYMBOL(ELF_printSectionInfo);
