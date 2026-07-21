#include "vasm.h"
#include "output_coff.h"
#include <time.h>


/* COFF output module structure
 * + COFF header
 * + Section headers list
 * + Relocation tables for all sections
 * + Symbol headers list
 * + (uint32_t)size of the symbols name list
 * + Symbol names array for long symbol names (each name must end by 0)
 * + Section contents in the order defined by the section headers list
 */


#if defined(OUTCOFF) && (defined(VASM_CPU_X86) || defined(VASM_CPU_ARM) || defined(VASM_CPU_M68K) || defined(VASM_CPU_PPC))
static char* copyright = "vasm coff output module 0.5a by Jean-Paul Mari";
static uint16_t fmagic = 0xffff;

/* Get the magic's header */
static uint16_t get_header_fmagic(void)
{
    uint16_t fmagic = 0xffff;
#ifdef VASM_CPU_X86
    fmagic = COFF_I386_MAGIC;
#elif VASM_CPU_ARM
    if (cpu_type & AA4) {
        fmagic = COFF_ARMV4_MAGIC;
    } else {
        if (cpu_type & AA4T) {
            fmagic = COFF_ARMV4T_MAGIC;
        }
    }
#elif VASM_CPU_M68K
    fmagic = COFF_M68K_MAGIC;  /* value for M68k supported by most of the coff object reader */
#elif VASM_CPU_PPC
    if (bytespertaddr == 4) {
        fmagic = COFF_PPC32_MAGIC;
    }
    else if (bytespertaddr == 8) {
        fmagic = COFF_PPC64_MAGIC;
    }
#endif
    return fmagic;
}

/* Get the flags's header */
static unsigned int get_header_fflags(void)
{
    unsigned int fflags = COFF_F_LNNO;
#ifdef VASM_CPU_X86
    fflags = (COFF_F_AR32WR | COFF_F_LNNO);
#elif VASM_CPU_ARM
    fflags = (COFF_F_LNNO);
#elif VASM_CPU_M68K
    /* 0x204: Standard m68k flags */
    fflags = (COFF_F_AR16WR | COFF_F_LNNO);
#elif VASM_CPU_PPC
    fflags = (COFF_F_LNNO);
#endif
    return fflags;
}

/* Get COFF relocation type based on vasm relocation type */
/* Return 0xffff if type cannot be found */
static uint16_t get_coff_reloc_type(rlist *rl)
{
    if (is_std_reloc(rl)) {
        switch (STD_REL_TYPE(rl->type)) {
            case REL_NONE:
                return R_ABS;
                
            case REL_ABS:
                {
                    nreloc *r = (nreloc *)rl->reloc;
                    switch (r->size) {
                        case 8:  return R_RELBYTE;
                        case 16: return R_RELWORD;
                        case 32: return R_RELLONG;
                        default: return 0xffff;
                    }
                }
                break;
                
            case REL_PC:
                {
                    nreloc *r = (nreloc *)rl->reloc;
                    switch (r->size) {
                        case 8:  return R_PCRBYTE;
                        case 16: return R_PCRWORD;
                        case 32: return R_PCRLONG;
                        default: return 0xffff;
                    }
                }
                break;
                
            default:
                return R_ABS;
        }
    }

    /* Default type */
    return R_ABS;
}

/* Write complete output */
static void write_output(FILE* f, section* sec, symbol* sym)
{
    int be = BIGENDIAN;
    time_t current_time = time(NULL);
    uint32_t symptr, symsizeh = 0, symsizen = 0, numsyms = 0, symnameo;
    section* s;
    symbol* symptr_iter;
    uint16_t numsects = 0;
    uint32_t sections_size = 0, section_index_counter = 1;
    uint32_t relocations_size = 0;
    uint16_t fflags = 0;
    uint32_t tmp;
    uint32_t section_headers_size, reloc_tables_offset, section_content_offset;
    uint32_t current_reloc_ptr, current_section_offset;
    uint16_t rtype, nrelocs;
    struct coff_hdr h;
    atom* p;

    /* Get fields's header */
    if (fmagic == 0xffff) {
        fmagic = get_header_fmagic();
        /* Cannot create a COFF output without a magic ID */
        if (fmagic == 0xffff) {
            output_error(1, cpuname);
            return;
        }
    }
    fflags = get_header_fflags();

    /* Count sections */
    for (s = sec; s; (s = s->next)) {
        numsects++;
    }

    /* Calculate relocations size */
    for (s = sec; s; s = s->next) {
        atom* p;
        for (p = s->first; p!=s->end; p = p->next) {
            if (p->type == DATA || p->type == SPACE) {
                rlist* current_reloc = get_relocs(p);
                while (current_reloc != NULL) {
                    relocations_size += sizeof(struct coff_reloc);
                    current_reloc = current_reloc->next;
                }
            }
        }
    }

    /* Get count symbols, symbols header size & symbols name list */
    for (symptr_iter = sym; symptr_iter; (symptr_iter = symptr_iter->next)) {
        if (!symptr_iter->flags || (symptr_iter->flags & ~VASMINTERN)) {
            symsizeh += sizeof(struct coff_symbol);
            symptr_iter->idx = numsyms++;
            /* Longer name will fit in the symbol's names list */
            if (strlen(symptr_iter->name) > 8) {
                symsizen += (strlen(symptr_iter->name) + 1);    /* name must ends by 0 */
            }
        } else {
            symptr_iter->idx = -1;
        }
    }

    /* Calculate offsets according to COFF structure */
    section_headers_size = numsects * sizeof(struct coff_section_header);
    reloc_tables_offset = sizeof(struct coff_hdr) + section_headers_size;
    symptr = reloc_tables_offset + relocations_size;
    symnameo = sizeof(uint32_t);
    section_content_offset = symptr + symsizeh + symnameo + symsizen;

    /* Write COFF header */
    memset(&h, 0, sizeof(h));
    setval(be, &h.f_magic, sizeof(h.f_magic), fmagic);
    setval(be, &h.f_nscns, sizeof(h.f_nscns), numsects);
    setval(be, &h.f_timdat, sizeof(h.f_timdat), (uint32_t)current_time);
    setval(be, &h.f_symptr, sizeof(h.f_symptr), symptr);
    setval(be, &h.f_nsyms, sizeof(h.f_nsyms), numsyms);
    setval(be, &h.f_flags, sizeof(h.f_flags), fflags);
    fwdata(f, &h, sizeof(h));

    /* Create the section */
    current_reloc_ptr = reloc_tables_offset;
    current_section_offset = section_content_offset;
    sections_size = 0;
    for (s = sec; s; s = s->next) {
        struct coff_section_header sh;
        memset(&sh, 0, sizeof(sh));
        memcpy(sh.s_name, s->name, sizeof(sh.s_name));
        setval(be, &sh.s_size, sizeof(sh.s_size), get_sec_size(s));
        setval(be, &sh.s_scnptr, sizeof(sh.s_scnptr), current_section_offset);
        setval(be, &sh.s_paddr, sizeof(sh.s_paddr), sections_size);
        setval(be, &sh.s_vaddr, sizeof(sh.s_vaddr), sections_size);

        /* Set the index number in the section (to be used by symbols) */
        s->idx = section_index_counter++;

        /* Get the number of relocations in the section */
        nrelocs = 0;
        for (p = s->first; p!=s->end; p = p->next) {
            if (p->type == DATA || p->type == SPACE) {
                /* Count each relocation */
                rlist* current_reloc = get_relocs(p);
                while (current_reloc != NULL) {
                    nrelocs++;
                    current_reloc = current_reloc->next;
                }
            }
        }

        /* Set the relocation's information in the section header */
        if (nrelocs > 0) {
            setval(be, &sh.s_nreloc, sizeof(sh.s_nreloc), nrelocs);
            setval(be, &sh.s_relptr, sizeof(sh.s_relptr), current_reloc_ptr);
            current_reloc_ptr += (nrelocs * sizeof(struct coff_reloc));
        }

        /* Set the section type */
        switch (get_sec_type(s)) {
        case S_TEXT:
            setval(be, &sh.s_flags, sizeof(sh.s_flags), COFF_STYP_TEXT);
            break;

        case S_DATA:
            setval(be, &sh.s_flags, sizeof(sh.s_flags), COFF_STYP_DATA);
            break;

        case S_BSS:
            setval(be, &sh.s_flags, sizeof(sh.s_flags), COFF_STYP_BSS);
            break;

        default:
            break;
        }

        fwdata(f, &sh, sizeof(sh));

        sections_size += (uint32_t)get_sec_size(s);
        current_section_offset += (uint32_t)get_sec_size(s);
    }

    /* Write relocations for all sections */
    for (s = sec; s; s = s->next) {
        atom* a;
        utaddr pc = 0, npc;

        for (a = s->first; a!=s->end; a = a->next) {
            npc = pcalign(a, pc);

            if (a->type == DATA || a->type == SPACE) {
                rlist* rl = get_relocs(a);

                while (rl != NULL) {
                    if (is_nreloc(rl)) {
                        nreloc* r = (nreloc*)rl->reloc;
                        struct coff_reloc cr;

                        memset(&cr, 0, sizeof(cr));

                        /* Set virtual address (section offset + atom offset + reloc offset) */
                        setval(be, &cr.r_vaddr, sizeof(cr.r_vaddr), npc + r->byteoffset);
                        /* Set symbol index */
                        setval(be, &cr.r_symndx, sizeof(cr.r_symndx), r->sym->idx);
                        /* Get relocation type */
                        rtype = get_coff_reloc_type(rl);
                        if (rtype == 0xffff) {
                            unsupp_reloc_error(a, rl);
                        } else {
                            /* Set relocation type */
                            setval(be, &cr.r_type, sizeof(cr.r_type), rtype);
                            /* Write relocation structure */
                            fwdata(f, &cr, sizeof(struct coff_reloc));
                        }
                    }
                    rl = rl->next;
                }
            }

            pc = npc + atom_size(a, s, npc);
        }
    }

    /* Write symbols table */
    symnameo = sizeof(uint32_t); /* Reset for actual writing */
    for (symptr_iter = sym; symptr_iter; symptr_iter = symptr_iter->next) {
        /* if ((symptr_iter->type != IMPORT || (symptr_iter->flags & EXPORT)) && !(symptr_iter->flags & EXPORT)) { */ /* Static symbols first */
        if (!symptr_iter->flags || (symptr_iter->flags & ~VASMINTERN)) {
            struct coff_symbol cs;
            memset(&cs, 0, sizeof(cs));

            /* Handle symbol name */
            if (strlen(symptr_iter->name) <= 8) {
                strncpy(cs.n.n_name, symptr_iter->name, 8);
            }
            else {
                /* Handle long symbol name */
                /* strncpy(cs.n.n_name, symptr_iter->name, 8); */
                setval(be, &cs.n.n_n.n_offset, sizeof(cs.n.n_n.n_offset), symnameo);
                symnameo += (strlen(symptr_iter->name) + 1);
            }

            setval(be, &cs.n_value, sizeof(cs.n_value), (uint32_t)get_sym_value(symptr_iter));

            /* Section number (1-based) */
            if (symptr_iter->sec) {
                setval(be, &cs.n_scnum, sizeof(cs.n_scnum), symptr_iter->sec->idx);
                /* 
                if (!strcmp(symptr_iter->sec->name, ".text") || !strcmp(symptr_iter->sec->name, "text")) {
                    setval(be, &cs.n_scnum, sizeof(cs.n_scnum), 1);
                }
                else if (!strcmp(symptr_iter->sec->name, ".data") || !strcmp(symptr_iter->sec->name, "data")) {
                    setval(be, &cs.n_scnum, sizeof(cs.n_scnum), 2);
                }
                else if (!strcmp(symptr_iter->sec->name, ".bss") || !strcmp(symptr_iter->sec->name, "bss")) {
                    setval(be, &cs.n_scnum, sizeof(cs.n_scnum), 3);
                }
                else {
                    setval(be, &cs.n_scnum, sizeof(cs.n_scnum), 1);
                } */
                /* Undefined */
            } /* else {
                setval(be, &cs.n_scnum, sizeof(cs.n_scnum), 0);
            }  */

            /* setval(be, &cs.n_type, sizeof(cs.n_type), 0); */

            switch (symptr_iter->type)
            {
                /* The symbol is externally defined and its value is unknown */
            case IMPORT:
                break;

                /* The symbol is defined using an expression (equate) */
            case EXPRESSION:
                switch (symptr_iter->flags) {
                case USED:
                    cs.n_sclass = COFF_C_TPDEF;
                    setval(be, &cs.n_scnum, sizeof(cs.n_scnum), COFF_N_ABS);
                    break;

                default:
                    cs.n_sclass = COFF_C_TPDEF;
                    setval(be, &cs.n_scnum, sizeof(cs.n_scnum), COFF_N_ABS);
                    break;
                }
                break;

                /* The symbol is a label defined at a specific location */
            case LABSYM:
                switch (symptr_iter->flags) {
                case 0:
                    cs.n_sclass = COFF_C_LABEL;
                    break;

                case EXPORT:
                    cs.n_sclass = COFF_C_EXT;
                    break;

                default:
                    cs.n_sclass = COFF_C_LABEL;
                    break;
                }
                break;

            default:
                break;
            }

            /* cs.n_numaux = 0; */

            /* Write the symbol */
            fwdata(f, &cs, sizeof(struct coff_symbol));
        }
    }

    /* Write the size of the symbols name list */
    setval(be, &tmp, sizeof(tmp), symnameo);
    fwdata(f, &tmp, sizeof(uint32_t));  
    /* Write symbols name list */
    for (symptr_iter = sym; symptr_iter; symptr_iter = symptr_iter->next) {
        if (!symptr_iter->flags || (symptr_iter->flags & ~VASMINTERN)) {
            if (strlen(symptr_iter->name) > 8) {
                fwdata(f, symptr_iter->name, (strlen(symptr_iter->name) + 1));
            }
        }
    }

    /* Write the section's content */
    for (s = sec; s; s = s->next) {
        atom* a;
        utaddr pc = 0, npc;

        for (a = s->first; a!=s->end; a = a->next) {
            npc = fwpcalign(f, a, s, pc);

            if (a->type == DATA) {
                fwdata(f, a->content.db->data, a->content.db->size);
            }
            else if (a->type == SPACE) {
                fwsblock(f, a->content.sb);
            }

            pc = npc + atom_size(a, s, npc);
        }
    }
}

/* Arguments specific for the output */
static int output_args(char* p)
{
    /* fmagic argument must be in 16 bits range */
    if (!strncmp(p, "-fmagic=", 8)) {
        int tmp;
        tmp = atoi(p + 8);
        if (tmp < 0x10000) {
            fmagic = (uint16_t)tmp;
            return 1;
        }
    }
    return 0;
}

/* Initialisations specific to the output */
int init_output_coff(char** cp, void (**wo)(FILE*, section*, symbol*), int (**oa)(char*))
{
    *cp = copyright;
    *wo = write_output;
    *oa = output_args;
    unnamed_sections = 0;           /* 1: output format doesn't support named sections, needs to be set to 0 */
    secname_attr = 1;               /* 1: attribute is used to differentiate between sections */
    return 1;
}

#else

int init_output_coff(char** cp, void (**wo)(FILE*, section*, symbol*),
    int (**oa)(char*))
{
    return 0;
}
#endif
