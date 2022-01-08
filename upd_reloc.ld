/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Linker command/script file
 *
 * Linker script for the Cortex-M platforms.
 */

#include <autoconf.h>
#include <linker/sections.h>
#include <devicetree.h>

#include <linker/linker-defs.h>
#include <linker/linker-tool.h>

#define FWUPD_ADDR 0x80D000
#define UPDROM_ADDR 0x30000

#define _SRAM2_DATA_SECTION_NAME  .sram2_data
#define _SRAM2_BSS_SECTION_NAME   .sram2_bss
#define _SRAM2_TEXT_SECTION_NAME  .sram2_text
#define SRAM2_ADDR  (CONFIG_SRAM_BASE_ADDRESS + RAM_SIZE2)

#define RAM_SIZE2 (CONFIG_SRAM_SIZE * 512)
MEMORY
{
	SRAM2                  (wx) : ORIGIN = (CONFIG_SRAM_BASE_ADDRESS + RAM_SIZE2), LENGTH = RAM_SIZE2
}

#include <arch/arm/aarch32/cortex_m/scripts/linker.ld>
