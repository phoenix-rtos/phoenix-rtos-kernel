/*
 * Phoenix-RTOS
 *
 * SEGGER's Real Time Transfer - simplified driver
 *
 * Copyright 2023-2024 Phoenix Systems
 * Author: Gerard Swiderski, Daniel Sawka
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "include/errno.h"
#include "syspage.h"
#include "hal/arm/barriers.h"

#include <board_config.h>
#include "rtt.h"

#ifndef RTT_SYSPAGE_MAP_NAME
#define RTT_SYSPAGE_MAP_NAME "rtt"
#endif

#ifndef RTT_CB_SIZE
#define RTT_CB_SIZE 256U
#endif


struct rtt_pipe {
	const char *name;
	volatile unsigned char *ptr;
	unsigned int sz;
	volatile unsigned int wr;
	volatile unsigned int rd;
	unsigned int flags;
};


struct rtt_desc {
	char tag[16];
	unsigned int txChannels;
	unsigned int rxChannels;
	struct rtt_pipe channels[];
};


static struct {
	volatile struct rtt_desc *rtt;
} common;


static int rtt_check(unsigned int chan, rtt_dir_t dir)
{
	if ((dir == rtt_dir_up) && (chan >= common.rtt->txChannels)) {
		return -ENODEV;
	}

	if ((dir == rtt_dir_down) && (chan >= common.rtt->rxChannels)) {
		return -ENODEV;
	}

	return 0;
}


int _hal_rttWrite(unsigned int chan, const void *buf, unsigned int count)
{
	unsigned int sz;
	unsigned int rd;
	unsigned int wr;
	unsigned int todo;
	volatile unsigned char *dstBuf;

	if (rtt_check(chan, rtt_dir_up) < 0) {
		return -ENODEV;
	}

	hal_cpuDataMemoryBarrier();
	dstBuf = common.rtt->channels[chan].ptr;
	sz = common.rtt->channels[chan].sz - 1U;
	rd = (common.rtt->channels[chan].rd + sz) & sz;
	wr = common.rtt->channels[chan].wr & sz;
	todo = count;

	/* TODO: Support all buffer modes (currently only trim is used, regardless of flags) */
	while ((todo != 0U) && (rd != wr)) {
		dstBuf[wr] = *(const unsigned char *)buf++;
		wr = (wr + 1U) & sz;
		todo--;
	}

	hal_cpuDataMemoryBarrier();
	common.rtt->channels[chan].wr = wr;

	return (int)(unsigned int)(count - todo);
}


int _hal_rttTxAvail(unsigned int chan)
{
	unsigned int sz;
	unsigned int rd;
	unsigned int wr;

	if (rtt_check(chan, rtt_dir_up) < 0) {
		return -ENODEV;
	}

	hal_cpuDataMemoryBarrier();
	sz = common.rtt->channels[chan].sz - 1U;
	rd = (common.rtt->channels[chan].rd + sz) & sz;
	wr = common.rtt->channels[chan].wr & sz;

	if (wr > rd) {
		return (int)(unsigned int)(sz + 1U - (wr - rd));
	}
	else {
		return (int)(unsigned int)(rd - wr);
	}
}


int _hal_rttReset(unsigned int chan, rtt_dir_t dir)
{
	if (rtt_check(chan, dir) < 0) {
		return -ENODEV;
	}

	hal_cpuDataMemoryBarrier();
	if (dir == rtt_dir_up) {
		common.rtt->channels[chan].wr = common.rtt->channels[chan].rd;
	}
	else {
		chan = common.rtt->txChannels + chan;
		common.rtt->channels[chan].rd = common.rtt->channels[chan].wr;
	}
	hal_cpuDataMemoryBarrier();
	return 0;
}


int _hal_rttInit(void)
{
	const syspage_map_t *map = syspage_mapNameResolve(RTT_SYSPAGE_MAP_NAME);

	if (map == NULL) {
		return -ENOENT;
	}

	if (map->start + RTT_CB_SIZE > map->end) {
		return -EINVAL;
	}

	/* TODO: Place CB always at the start of the map? */
	common.rtt = (void *)(map->end - RTT_CB_SIZE);
	return 0;
}
