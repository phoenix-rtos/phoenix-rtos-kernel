/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory page transfer benchmark
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "include/errno.h"
#include "lib/lib.h"
#include "proc/msg.h"
#include "vm/vm.h"


#define VM_BENCHMARK_MAX_PAGES        16U
#define VM_BENCHMARK_BATCH_PAGES      128U
#define VM_BENCHMARK_REPETITIONS      1000U
#define VM_BENCHMARK_BUFFER_SIZE      (VM_BENCHMARK_BATCH_PAGES * SIZE_PAGE)
#define VM_BENCHMARK_UNALIGNED_OFFSET 1U
#define VM_BENCHMARK_MAX_SIZE         (VM_BENCHMARK_MAX_PAGES * SIZE_PAGE)

#ifndef VM_BENCHMARK_STEP_SIZE
#define VM_BENCHMARK_STEP_SIZE 1024U
#endif

#if VM_BENCHMARK_STEP_SIZE == 0
#error "VM_BENCHMARK_STEP_SIZE must be greater than zero"
#endif


#ifndef NOMMU

typedef struct {
	const char *name;
	unsigned int startAligned;
	unsigned int endAligned;
} vm_benchmark_case_t;


static const vm_benchmark_case_t vm_benchmarkCases[] = {
	{ "aligned_aligned", 1U, 1U },
	{ "unaligned_aligned", 0U, 1U },
	{ "aligned_unaligned", 1U, 0U },
	{ "unaligned_unaligned", 0U, 0U },
};


static struct {
	cycles_t memcpySamples[VM_BENCHMARK_REPETITIONS];
	cycles_t pageMapSamples[VM_BENCHMARK_REPETITIONS];
	cycles_t msgMapSamples[VM_BENCHMARK_REPETITIONS];
	vm_map_t sourceMap;
	vm_map_t destinationMap;
} vm_benchmarkCommon;


static void vm_benchmarkPercentiles(cycles_t *samples, cycles_t *p50, cycles_t *p90, cycles_t *p99)
{
	cycles_t sample;
	unsigned int i, j;

	for (i = 1; i < VM_BENCHMARK_REPETITIONS; ++i) {
		sample = samples[i];
		for (j = i; (j > 0U) && (samples[j - 1U] > sample); --j) {
			samples[j] = samples[j - 1U];
		}
		samples[j] = sample;
	}

	*p50 = samples[VM_BENCHMARK_REPETITIONS / 2U];
	*p90 = samples[(VM_BENCHMARK_REPETITIONS * 9U) / 10U];
	*p99 = samples[(VM_BENCHMARK_REPETITIONS * 99U) / 100U];
}


static int vm_benchmarkAlignmentMatches(const vm_benchmark_case_t *testCase, const void *data, size_t size)
{
	const unsigned int startAligned = (((ptr_t)data & (SIZE_PAGE - 1U)) == 0U) ? 1U : 0U;
	const unsigned int endAligned = ((((ptr_t)data + size) & (SIZE_PAGE - 1U)) == 0U) ? 1U : 0U;

	return ((startAligned == testCase->startAligned) && (endAligned == testCase->endAligned)) ? 1 : 0;
}


static int vm_benchmarkGeometry(const vm_benchmark_case_t *testCase, size_t transferSize,
		size_t *startOffset, unsigned int *allocationPages, unsigned int *fullPages)
{
	const size_t sizeRemainder = transferSize & (SIZE_PAGE - 1U);
	size_t endOffset, fullPagesStart, fullPagesEnd;

	if (testCase->startAligned != 0U) {
		*startOffset = 0U;
		if (((testCase->endAligned != 0U) && (sizeRemainder != 0U)) ||
				((testCase->endAligned == 0U) && (sizeRemainder == 0U))) {
			return 0;
		}
	}
	else {
		if (testCase->endAligned != 0U) {
			if (sizeRemainder == 0U) {
				return 0;
			}
			*startOffset = SIZE_PAGE - sizeRemainder;
		}
		else {
			*startOffset = VM_BENCHMARK_UNALIGNED_OFFSET;
			if (((*startOffset + transferSize) & (SIZE_PAGE - 1U)) == 0U) {
				(*startOffset)++;
			}
		}
	}

	endOffset = (*startOffset + transferSize) & (SIZE_PAGE - 1U);
	if (((*startOffset == 0U) != (testCase->startAligned != 0U)) ||
			((endOffset == 0U) != (testCase->endAligned != 0U))) {
		return 0;
	}

	*allocationPages = (unsigned int)((*startOffset + transferSize + SIZE_PAGE - 1U) / SIZE_PAGE);
	fullPagesStart = (*startOffset + SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U);
	fullPagesEnd = (*startOffset + transferSize) & ~(SIZE_PAGE - 1U);
	*fullPages = (fullPagesEnd > fullPagesStart) ? (unsigned int)((fullPagesEnd - fullPagesStart) / SIZE_PAGE) : 0U;

	return 1;
}


static int vm_benchmarkValidateCopy(const void *source, const void *destination,
		size_t startOffset, size_t transferSize, unsigned int allocationPages, unsigned int iterations)
{
	unsigned int iteration;
	size_t slotOffset;

	for (iteration = 0; iteration < iterations; ++iteration) {
		slotOffset = iteration * allocationPages * SIZE_PAGE;
		if (hal_memcmp(source + slotOffset + startOffset,
					destination + slotOffset + startOffset, transferSize) != 0) {
			return -EFAULT;
		}
	}

	return EOK;
}


static int vm_benchmarkMsgMapSample(vm_map_t *kmap, vm_map_t *sourceMap, vm_map_t *destinationMap,
		void *sourceVaddr, const void *source, const vm_benchmark_case_t *testCase,
		size_t startOffset, size_t transferSize,
		unsigned int allocationPages, unsigned int iterations, kmsg_t *messages, cycles_t *cycles)
{
	const size_t allocationSize = allocationPages * SIZE_PAGE;
	cycles_t begin, end;
	void *originalData;
	size_t slotOffset;
	unsigned int attempted = 0U, iteration;
	int err = EOK;

	for (iteration = 0; iteration < iterations; ++iteration) {
		slotOffset = iteration * allocationSize;
		hal_memset(&messages[iteration], 0, sizeof(messages[iteration]));
		messages[iteration].msg.i.data = sourceVaddr + slotOffset + startOffset;
		messages[iteration].msg.i.size = transferSize;
		if (vm_benchmarkAlignmentMatches(testCase, messages[iteration].msg.i.data, transferSize) == 0) {
			return -EINVAL;
		}
	}

	hal_cpuDisableInterrupts();
	pmap_switch(&destinationMap->pmap);
	hal_cpuGetCycles(&begin);
	for (iteration = 0; iteration < iterations; ++iteration) {
		originalData = (void *)(ptr_t)messages[iteration].msg.i.data;
		messages[iteration].msg.i.data = proc_msgMap(0, &messages[iteration], originalData, transferSize,
				sourceMap, destinationMap, 1);
		attempted++;
		if (messages[iteration].msg.i.data == NULL) {
			messages[iteration].msg.i.data = originalData;
			err = -EFAULT;
			break;
		}
		if (vm_benchmarkAlignmentMatches(testCase, messages[iteration].msg.i.data, transferSize) == 0) {
			err = -EFAULT;
			break;
		}
	}
	hal_cpuGetCycles(&end);
	*cycles = end - begin;

	if (err == EOK) {
		for (iteration = 0; iteration < iterations; ++iteration) {
			slotOffset = iteration * allocationSize;
			if (hal_memcmp(messages[iteration].msg.i.data,
						source + slotOffset + startOffset, transferSize) != 0) {
				err = -EFAULT;
				break;
			}
		}
	}

	pmap_switch(&kmap->pmap);
	hal_cpuEnableInterrupts();

	for (iteration = 0; iteration < attempted; ++iteration) {
		proc_msgMapRelease(&messages[iteration], destinationMap);
	}

	return err;
}


static int vm_benchmarkPageTransfer(vm_map_t *sourceMap, vm_map_t *destinationMap,
		void *sourceVaddr, void *destinationVaddr, void *source, void *destination)
{
	const vm_attr_t attr = PGHD_READ | PGHD_USER | PGHD_PRESENT;
	void *sourceCurrent, *destinationCurrent;
	addr_t paddr;
	cycles_t begin, end, memcpyP50, memcpyP90, memcpyP99, pageMapP50, pageMapP90, pageMapP99;
	size_t allocationSize, slotOffset, transferSize;
	unsigned int pageCount, iterations, repetition, iteration, page, mappedPages;
	int err = EOK;

	lib_printf("benchmark,size_bytes,mapped_pages,memcpy_p50_cycles_per_iteration,memcpy_p90_cycles_per_iteration,memcpy_p99_cycles_per_iteration,page_map_p50_cycles_per_iteration,page_map_p90_cycles_per_iteration,page_map_p99_cycles_per_iteration\n");

	for (transferSize = VM_BENCHMARK_STEP_SIZE; transferSize <= VM_BENCHMARK_MAX_SIZE; transferSize += VM_BENCHMARK_STEP_SIZE) {
		pageCount = (unsigned int)((transferSize + SIZE_PAGE - 1U) / SIZE_PAGE);
		allocationSize = pageCount * SIZE_PAGE;
		iterations = VM_BENCHMARK_BATCH_PAGES / pageCount;

		for (repetition = 0; repetition < VM_BENCHMARK_REPETITIONS; ++repetition) {
			hal_cpuGetCycles(&begin);
			for (iteration = 0; iteration < iterations; ++iteration) {
				slotOffset = iteration * allocationSize;
				hal_memcpy(destination + slotOffset, source + slotOffset, transferSize);
			}
			hal_cpuGetCycles(&end);
			vm_benchmarkCommon.memcpySamples[repetition] = end - begin;

			if (vm_benchmarkValidateCopy(source, destination, 0U, transferSize, pageCount, iterations) < 0) {
				return -EFAULT;
			}

			sourceCurrent = sourceVaddr;
			destinationCurrent = destinationVaddr;
			mappedPages = 0U;
			hal_cpuGetCycles(&begin);
			for (iteration = 0; iteration < iterations; ++iteration) {
				for (page = 0; page < pageCount; ++page) {
					paddr = pmap_resolve(&sourceMap->pmap, sourceCurrent) & ~(SIZE_PAGE - 1U);
					if ((paddr == 0U) || (page_map(&destinationMap->pmap, destinationCurrent, paddr, attr) < 0)) {
						err = -EFAULT;
						break;
					}
					sourceCurrent += SIZE_PAGE;
					destinationCurrent += SIZE_PAGE;
					mappedPages++;
				}
				if (err < 0) {
					break;
				}
			}
			hal_cpuGetCycles(&end);
			vm_benchmarkCommon.pageMapSamples[repetition] = end - begin;

			if ((err == EOK) &&
					(((pmap_resolve(&destinationMap->pmap, destinationVaddr) & ~(SIZE_PAGE - 1U)) !=
							 (pmap_resolve(&sourceMap->pmap, sourceVaddr) & ~(SIZE_PAGE - 1U))) ||
							((pmap_resolve(&destinationMap->pmap, destinationVaddr + ((mappedPages - 1U) * SIZE_PAGE)) & ~(SIZE_PAGE - 1U)) !=
									(pmap_resolve(&sourceMap->pmap, sourceVaddr + ((mappedPages - 1U) * SIZE_PAGE)) & ~(SIZE_PAGE - 1U))))) {
				err = -EFAULT;
			}

			if (mappedPages != 0U) {
				(void)pmap_remove(&destinationMap->pmap, destinationVaddr, destinationVaddr + (mappedPages * SIZE_PAGE));
			}
			if (err < 0) {
				return err;
			}
		}

		vm_benchmarkPercentiles(vm_benchmarkCommon.memcpySamples, &memcpyP50, &memcpyP90, &memcpyP99);
		vm_benchmarkPercentiles(vm_benchmarkCommon.pageMapSamples, &pageMapP50, &pageMapP90, &pageMapP99);

		lib_printf("vm_page_transfer,%zu,%u,%llu,%llu,%llu,%llu,%llu,%llu\n",
				transferSize, pageCount,
				(unsigned long long)(memcpyP50 / iterations), (unsigned long long)(memcpyP90 / iterations),
				(unsigned long long)(memcpyP99 / iterations),
				(unsigned long long)(pageMapP50 / iterations), (unsigned long long)(pageMapP90 / iterations),
				(unsigned long long)(pageMapP99 / iterations));
	}

	return EOK;
}


void vm_pageBenchmark(vm_map_t *kmap)
{
	const vm_attr_t attr = PGHD_READ | PGHD_USER | PGHD_PRESENT;
	void *source = NULL, *destination = NULL;
	void *sourceVaddr = NULL, *destinationVaddr = NULL;
	kmsg_t *messages = NULL;
	addr_t paddr;
	cycles_t msgMapP50, msgMapP90, msgMapP99;
	size_t startOffset, transferSize;
	unsigned int testCaseIndex, allocationPages, fullPages;
	unsigned int iterations, repetition, i;
	int err = EOK;
	int sourceMapCreated = 0, destinationMapCreated = 0;

	source = vm_kmalloc(VM_BENCHMARK_BUFFER_SIZE);
	destination = vm_kmalloc(VM_BENCHMARK_BUFFER_SIZE);
	messages = vm_kmalloc(VM_BENCHMARK_BATCH_PAGES * sizeof(*messages));
	if ((source == NULL) || (destination == NULL) || (messages == NULL)) {
		err = -ENOMEM;
		goto cleanup;
	}

	if ((((ptr_t)source | (ptr_t)destination) & (SIZE_PAGE - 1U)) != 0U) {
		err = -EINVAL;
		goto cleanup;
	}

	err = vm_mapCreate(&vm_benchmarkCommon.sourceMap, (void *)(VADDR_MIN + SIZE_PAGE), (void *)(VADDR_USR_MAX - SIZE_PAGE));
	if (err < 0) {
		goto cleanup;
	}
	sourceMapCreated = 1;

	err = vm_mapCreate(&vm_benchmarkCommon.destinationMap, (void *)(VADDR_MIN + SIZE_PAGE), (void *)(VADDR_USR_MAX - SIZE_PAGE));
	if (err < 0) {
		goto cleanup;
	}
	destinationMapCreated = 1;

	sourceVaddr = vm_mapFind(&vm_benchmarkCommon.sourceMap, NULL, VM_BENCHMARK_BUFFER_SIZE, MAP_NOINHERIT, PROT_READ | PROT_USER);
	destinationVaddr = vm_mapFind(&vm_benchmarkCommon.destinationMap, NULL, VM_BENCHMARK_BUFFER_SIZE, MAP_NOINHERIT, PROT_READ | PROT_USER);
	if ((sourceVaddr == NULL) || (destinationVaddr == NULL)) {
		err = -ENOMEM;
		goto cleanup;
	}

	for (i = 0; i < VM_BENCHMARK_BATCH_PAGES; ++i) {
		paddr = pmap_resolve(&kmap->pmap, source + (i * SIZE_PAGE)) & ~(SIZE_PAGE - 1U);
		if ((paddr == 0U) || (page_map(&vm_benchmarkCommon.sourceMap.pmap, sourceVaddr + (i * SIZE_PAGE), paddr, attr) < 0)) {
			err = -EFAULT;
			goto cleanup;
		}
	}

	for (i = 0; i < VM_BENCHMARK_BATCH_PAGES; ++i) {
		paddr = pmap_resolve(&vm_benchmarkCommon.sourceMap.pmap, sourceVaddr + (i * SIZE_PAGE)) & ~(SIZE_PAGE - 1U);
		if ((paddr == 0U) || (page_map(&vm_benchmarkCommon.destinationMap.pmap, destinationVaddr + (i * SIZE_PAGE), paddr, attr) < 0)) {
			err = -EFAULT;
			goto cleanup;
		}
	}

	err = pmap_remove(&vm_benchmarkCommon.destinationMap.pmap, destinationVaddr, destinationVaddr + VM_BENCHMARK_BUFFER_SIZE);
	if (err < 0) {
		goto cleanup;
	}

	hal_memset(source, 0xa5, VM_BENCHMARK_BUFFER_SIZE);
	hal_memset(destination, 0, VM_BENCHMARK_BUFFER_SIZE);

	lib_printf("benchmark_config,page_size_bytes,step_size_bytes,max_size_bytes,batch_pages,repetitions\n");
	lib_printf("vm_benchmark,%u,%u,%u,%u,%u\n",
			(unsigned int)SIZE_PAGE, (unsigned int)VM_BENCHMARK_STEP_SIZE,
			(unsigned int)VM_BENCHMARK_MAX_SIZE, VM_BENCHMARK_BATCH_PAGES, VM_BENCHMARK_REPETITIONS);

	err = vm_benchmarkPageTransfer(&vm_benchmarkCommon.sourceMap, &vm_benchmarkCommon.destinationMap, sourceVaddr, destinationVaddr, source, destination);
	if (err < 0) {
		goto cleanup;
	}

	lib_printf("benchmark,layout,size_bytes,start_offset_bytes,mapped_pages,msg_map_p50_cycles_per_iteration,msg_map_p90_cycles_per_iteration,msg_map_p99_cycles_per_iteration\n");

	for (testCaseIndex = 0; testCaseIndex < sizeof(vm_benchmarkCases) / sizeof(*vm_benchmarkCases); ++testCaseIndex) {
		const vm_benchmark_case_t *testCase = &vm_benchmarkCases[testCaseIndex];

		for (transferSize = VM_BENCHMARK_STEP_SIZE; transferSize <= VM_BENCHMARK_MAX_SIZE; transferSize += VM_BENCHMARK_STEP_SIZE) {
			if (vm_benchmarkGeometry(testCase, transferSize, &startOffset, &allocationPages, &fullPages) == 0) {
				continue;
			}

			iterations = VM_BENCHMARK_BATCH_PAGES / allocationPages;

			if ((allocationPages == 0U) || (allocationPages > VM_BENCHMARK_BATCH_PAGES) || (iterations == 0U)) {
				err = -EINVAL;
				goto cleanup;
			}

			for (repetition = 0; repetition < VM_BENCHMARK_REPETITIONS; ++repetition) {
				err = vm_benchmarkMsgMapSample(kmap, &vm_benchmarkCommon.sourceMap, &vm_benchmarkCommon.destinationMap, sourceVaddr, source,
						testCase, startOffset, transferSize, allocationPages, iterations, messages, &vm_benchmarkCommon.msgMapSamples[repetition]);
				if (err < 0) {
					goto cleanup;
				}
			}

			vm_benchmarkPercentiles(vm_benchmarkCommon.msgMapSamples, &msgMapP50, &msgMapP90, &msgMapP99);

			lib_printf("vm_msg_map,%s,%zu,%zu,%u,%llu,%llu,%llu\n",
					testCase->name, transferSize, startOffset, fullPages,
					(unsigned long long)(msgMapP50 / iterations), (unsigned long long)(msgMapP90 / iterations),
					(unsigned long long)(msgMapP99 / iterations));
		}
	}

cleanup:
	if (destinationMapCreated != 0) {
		vm_mapDestroy(NULL, &vm_benchmarkCommon.destinationMap);
	}
	if (sourceMapCreated != 0) {
		vm_mapDestroy(NULL, &vm_benchmarkCommon.sourceMap);
	}
	vm_kfree(messages);
	vm_kfree(destination);
	vm_kfree(source);

	if (err < 0) {
		lib_printf("benchmark,error,%d\n", err);
	}
}

#else

void vm_pageBenchmark(vm_map_t *kmap)
{
	(void)kmap;
	lib_printf("benchmark,error,remapping-not-supported\n");
}

#endif
