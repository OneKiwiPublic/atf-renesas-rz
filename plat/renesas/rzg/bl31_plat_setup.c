/*
 * Copyright (c) 2013-2014, ARM Limited and Contributors. All rights reserved.
 * Copyright (c) 2020, Renesas Electronics Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stddef.h>

#include <arch.h>
#include <arch_helpers.h>
#include <bl31/bl31.h>
#include <common/bl_common.h>
#include <common/debug.h>
#include <drivers/arm/cci.h>
#include <drivers/console.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>

#include "pwrc.h"
#include "rzg_def.h"
#include "rzg_private.h"
#include "rzg_version.h"

#define BL31_RO_BASE		BL_CODE_BASE
#define BL31_RO_LIMIT		BL_CODE_END

#if USE_COHERENT_MEM
#define BL31_COHERENT_RAM_BASE	BL_COHERENT_RAM_BASE
#define BL31_COHERENT_RAM_LIMIT	BL_COHERENT_RAM_END
#endif

extern void plat_rzg_gic_driver_init(void);
extern void plat_rzg_gic_init(void);

u_register_t rzg_boot_mpidr;

static int cci_map[] = {
	CCI500_CLUSTER0_SL_IFACE_IX_FOR_G2M,
	CCI500_CLUSTER1_SL_IFACE_IX_FOR_G2M
};

void plat_cci_init(void)
{
	cci_init(RZG_CCI_BASE, cci_map, ARRAY_SIZE(cci_map));
}

void plat_cci_enable(void)
{
	cci_enable_snoop_dvm_reqs(MPIDR_AFFLVL1_VAL(read_mpidr()));
}

void plat_cci_disable(void)
{
	cci_disable_snoop_dvm_reqs(MPIDR_AFFLVL1_VAL(read_mpidr()));
}

struct entry_point_info *bl31_plat_get_next_image_ep_info(uint32_t type)
{
	bl2_to_bl31_params_mem_t *from_bl2 = (bl2_to_bl31_params_mem_t *)
					     PARAMS_BASE;
	entry_point_info_t *next_image_info;

	next_image_info = (type == NON_SECURE) ?
		&from_bl2->bl33_ep_info : &from_bl2->bl32_ep_info;

	return next_image_info->pc ? next_image_info : NULL;
}

void bl31_early_platform_setup2(u_register_t arg0, u_register_t arg1,
				u_register_t arg2, u_register_t arg3)
{
	rzg_console_runtime_init();

	NOTICE("BL3-1 : Rev.%s\n", version_of_renesas);

	if (RZG_CLUSTER_A53A57 == rzg_pwrc_get_cluster()) {
		plat_cci_init();
		plat_cci_enable();
	}
}

void bl31_plat_arch_setup(void)
{
	rzg_configure_mmu_el3(BL31_BASE,
			       BL31_LIMIT - BL31_BASE,
			       BL31_RO_BASE, BL31_RO_LIMIT
#if USE_COHERENT_MEM
			       , BL31_COHERENT_RAM_BASE, BL31_COHERENT_RAM_LIMIT
#endif
	    );
	rzg_pwrc_code_copy_to_system_ram();
}

void bl31_platform_setup(void)
{
	plat_rzg_gic_driver_init();
	plat_rzg_gic_init();

	/* enable the system level generic timer */
	mmio_write_32(RZG_CNTC_BASE + CNTCR_OFF, CNTCR_FCREQ(U(0)) | CNTCR_EN);

	rzg_pwrc_setup();
#if 0
	/* TODO: there is a broad number of RZ/G2 SoC configurations; to
	   support all of them, Renesas use the pwrc driver to discover what
	   cores are on/off before announcing the topology.
	   This code hasnt been ported yet
	   */

	rzg_setup_topology();
#endif

	/* mask should match the kernel's MPIDR_HWID_BITMASK so the core can be
	   identified during cpuhotplug (check the kernel's psci migrate set of
	   functions */
	rzg_boot_mpidr = read_mpidr_el1() & 0x0000ffffU;
}
