/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Syspage
 *
 * Copyright 2021 Phoenix Systems
 * Authors: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "lib/lib.h"
#include "syspage.h"


static struct {
	/* parasoft-suppress-next-line MISRAC2012-RULE_5_8 "Variable inside the structure so it shouldn't cause this violation" */
	syspage_t *syspage;
} syspage_common;


size_t syspage_mapSize(void)
{
	size_t nb = 0;
	const syspage_map_t *map = syspage_common.syspage->maps;

	if (map == NULL) {
		return nb;
	}

	do {
		++nb;
		map = map->next;
	} while (map != syspage_common.syspage->maps);

	return nb;
}


const syspage_map_t *syspage_mapList(void)
{
	return syspage_common.syspage->maps;
}


const syspage_map_t *syspage_mapIdResolve(unsigned int id)
{
	const syspage_map_t *map = syspage_common.syspage->maps;

	if (map == NULL) {
		return NULL;
	}

	do {
		if (id == map->id) {
			return map;
		}
		map = map->next;
	} while (map != syspage_common.syspage->maps);

	return NULL;
}


const syspage_map_t *syspage_mapAddrResolve(addr_t addr)
{
	const syspage_map_t *map = syspage_common.syspage->maps;

	if (map == NULL) {
		return NULL;
	}

	do {
		if (addr < map->end && addr >= map->start) {
			return map;
		}
		map = map->next;
	} while (map != syspage_common.syspage->maps);

	return NULL;
}


const syspage_map_t *syspage_mapNameResolve(const char *name)
{
	const syspage_map_t *map = syspage_common.syspage->maps;

	if (map == NULL) {
		return NULL;
	}

	do {
		if (hal_strcmp(name, map->name) == 0) {
			return map;
		}
		map = map->next;
	} while (map != syspage_common.syspage->maps);

	return NULL;
}


size_t syspage_progSize(void)
{
	size_t nb = 0;
	const syspage_prog_t *prog = syspage_common.syspage->progs;

	if (prog == NULL) {
		return nb;
	}

	do {
		++nb;
		prog = prog->next;
	} while (prog != syspage_common.syspage->progs);

	return nb;
}


syspage_prog_t *syspage_progList(void)
{
	return syspage_common.syspage->progs;
}


const syspage_prog_t *syspage_progIdResolve(unsigned int id)
{
	unsigned int i = 0;
	const syspage_prog_t *prog = syspage_common.syspage->progs;

	if (prog == NULL) {
		return NULL;
	}

	do {
		if (id == i++) {
			return prog;
		}
		prog = prog->next;
	} while (prog != syspage_common.syspage->progs);

	return NULL;
}


const syspage_prog_t *syspage_progNameResolve(const char *name)
{
	const syspage_prog_t *prog = syspage_common.syspage->progs;

	if (prog == NULL) {
		return NULL;
	}

	do {
		if (hal_strcmp(name, prog->argv) == 0) {
			return prog;
		}
		prog = prog->next;
	} while (prog != syspage_common.syspage->progs);

	return NULL;
}


void syspage_progShow(void)
{
	const char *name;
	const syspage_prog_t *prog = syspage_common.syspage->progs, *next;

	if (prog != NULL) {
		do {
			name = (prog->argv[0] == 'X') ? prog->argv + 1 : prog->argv;
			next = prog->next;
			lib_printf(" '%s'%c", name, (next == syspage_common.syspage->progs) ? '\n' : ',');
			prog = next;
		} while (prog != syspage_common.syspage->progs);
	}
}


void syspage_init(void)
{
	syspage_prog_t *prog;
	syspage_part_t *part;
	syspage_map_t *map;
	mapent_t *entry;

	syspage_common.syspage = (syspage_t *)hal_syspageAddr();

	/* Map's relocation */
	if (syspage_common.syspage->maps != NULL) {
		syspage_common.syspage->maps = hal_syspageRelocate(syspage_common.syspage->maps);
		map = syspage_common.syspage->maps;
		do {
			map->next = hal_syspageRelocate(map->next);
			map->prev = hal_syspageRelocate(map->prev);
			map->name = hal_syspageRelocate(map->name);

			if (map->entries != NULL) {
				map->entries = hal_syspageRelocate(map->entries);
				entry = map->entries;
				do {
					entry->next = hal_syspageRelocate(entry->next);
					entry->prev = hal_syspageRelocate(entry->prev);
					entry = entry->next;
				} while (entry != map->entries);
			}
			map = map->next;
		} while (map != syspage_common.syspage->maps);
	}

	/* Program's relocation */
	if (syspage_common.syspage->progs != NULL) {
		syspage_common.syspage->progs = hal_syspageRelocate(syspage_common.syspage->progs);
		prog = syspage_common.syspage->progs;

		do {
			prog->next = hal_syspageRelocate(prog->next);
			prog->prev = hal_syspageRelocate(prog->prev);

			prog->dmaps = hal_syspageRelocate(prog->dmaps);
			prog->imaps = hal_syspageRelocate(prog->imaps);
			prog->argv = hal_syspageRelocate(prog->argv);
			prog->partition = hal_syspageRelocate(prog->partition);
			prog = prog->next;
		} while (prog != syspage_common.syspage->progs);
	}

	/* Partition's relocation */
	if (syspage_common.syspage->partitions != NULL) {
		syspage_common.syspage->partitions = hal_syspageRelocate(syspage_common.syspage->partitions);
		part = syspage_common.syspage->partitions;

		do {
			part->next = hal_syspageRelocate(part->next);
			part->prev = hal_syspageRelocate(part->prev);

			part->allocMaps = hal_syspageRelocate(part->allocMaps);
			part->accessMaps = hal_syspageRelocate(part->accessMaps);
			part->name = hal_syspageRelocate(part->name);
			part = part->next;
		} while (part != syspage_common.syspage->partitions);
	}
}
