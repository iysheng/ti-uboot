/*
 * Copyright 2016 Texas Instruments, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <malloc.h>

#include <asm/omap_common.h>
#include <asm/arch-omap5/sys_proto.h>

#ifdef CONFIG_TI_SECURE_DEVICE

/* Give zero values if not already defined */
#ifndef CONFIG_SECURE_BOOT_SRAM
#define CONFIG_SECURE_BOOT_SRAM (0)
#endif
#ifndef CONFIG_SECURE_RUN_SRAM
#define CONFIG_SECURE_RUN_SRAM (0)
#endif

static u32 hs_irq_skip[] = {
	8,	/* Secure violation reporting interrupt */
	15,	/* One interrupt for SDMA by secure world */
	118	/* One interrupt for Crypto DMA by secure world */
};

static int ft_hs_fixup_crossbar(void *fdt, bd_t *bd)
{
	const char *path;
	int offs;
	int ret;
	int len, i, old_cnt, new_cnt;
	u32 *temp;
	const u32 *p_data;

	/*
	 * Increase the size of the fdt
	 * so we have some breathing room
	 */
	ret = fdt_increase_size(fdt, 512);
	if (ret < 0) {
		printf("Could not increase size of device tree: %s\n",
		       fdt_strerror(ret));
		return ret;
	}

	/* Reserve IRQs that are used/needed by secure world */
	path = "/ocp/crossbar";
	offs = fdt_path_offset(fdt, path);
	if (offs < 0) {
		debug("Node %s not found.\n", path);
		return 0;
	}

	/* Get current entries */
	p_data = fdt_getprop(fdt, offs, "ti,irqs-skip", &len);
	if (p_data)
		old_cnt = len / sizeof(u32);
	else
		old_cnt = 0;

	new_cnt = sizeof(hs_irq_skip) /
				sizeof(hs_irq_skip[0]);

	/* Create new/updated skip list for HS parts */
	temp = malloc(sizeof(u32) * (old_cnt + new_cnt));
	for (i = 0; i < new_cnt; i++)
		temp[i] = cpu_to_fdt32(hs_irq_skip[i]);
	for (i = 0; i < old_cnt; i++)
		temp[i + new_cnt] = p_data[i];

	/* Blow away old data and set new data */
	fdt_delprop(fdt, offs, "ti,irqs-skip");
	ret = fdt_setprop(fdt, offs, "ti,irqs-skip",
			  temp,
			  (old_cnt + new_cnt) * sizeof(u32));
	free(temp);

	/* Check if the update worked */
	if (ret < 0) {
		printf("Could not add ti,irqs-skip property to node %s: %s\n",
		       path, fdt_strerror(ret));
		return ret;
	}

	return 0;
}

static int ft_hs_disable_rng(void *fdt, bd_t *bd)
{
	const char *path;
	int offs;
	int ret;

	/* Make HW RNG reserved for secure world use */
	path = "/ocp/rng";
	offs = fdt_path_offset(fdt, path);
	if (offs < 0) {
		debug("Node %s not found.\n", path);
		return 0;
	}
	ret = fdt_setprop_string(fdt, offs,
				 "status", "disabled");
	if (ret < 0) {
		printf("Could not add status property to node %s: %s\n",
		       path, fdt_strerror(ret));
		return ret;
	}
	return 0;
}

#if (CONFIG_SECURE_BOOT_SRAM != 0) || (CONFIG_SECURE_RUN_SRAM != 0)
static int ft_hs_fixup_sram(void *fdt, bd_t *bd)
{
	const char *path;
	int offs;
	int ret;
	u32 temp[2];

	/*
	 * Update SRAM reservations on secure devices. The OCMC RAM
	 * is always reserved for secure use from the start of that
	 * memory region
	 */
	path = "/ocp/ocmcram@40300000/sram-hs";
	offs = fdt_path_offset(fdt, path);
	if (offs < 0) {
		debug("Node %s not found.\n", path);
		return 0;
	}

	/* relative start offset */
	temp[0] = cpu_to_fdt32(0);
	/* reservation size */
	temp[1] = cpu_to_fdt32(max(CONFIG_SECURE_BOOT_SRAM,
				   CONFIG_SECURE_RUN_SRAM));
	fdt_delprop(fdt, offs, "reg");
	ret = fdt_setprop(fdt, offs, "reg", temp, 2 * sizeof(u32));
	if (ret < 0) {
		printf("Could not add reg property to node %s: %s\n",
		       path, fdt_strerror(ret));
		return ret;
	}

	return 0;
}
#else
static int ft_hs_fixup_sram(void *fdt, bd_t *bd) { return 0; }
#endif

static void ft_hs_fixups(void *fdt, bd_t *bd)
{
	/* Check we are running on an HS/EMU device type */
	if (GP_DEVICE != get_device_type()) {
		if ((ft_hs_fixup_crossbar(fdt, bd) == 0) &&
		    (ft_hs_disable_rng(fdt, bd) == 0) &&
		    (ft_hs_fixup_sram(fdt, bd) == 0))
			return;
	} else {
		printf("ERROR: Incorrect device type (GP) detected!");
	}
	/* Fixup failed or wrong device type */
	hang();
}
#else
static void ft_hs_fixups(void *fdt, bd_t *bd)
{
}
#endif

#if defined(CONFIG_TARGET_DRA7XX_EVM) || defined(CONFIG_TARGET_AM57XX_EVM)
#define OPP_DSP_CLK_NUM	3
#define OPP_IVA_CLK_NUM	2

const char *dra7_opp_dsp_clk_names[OPP_DSP_CLK_NUM] = {
	"/ocp/l4@4a000000/cm_core_aon@5000/clocks/dpll_dsp_ck",
	"/ocp/l4@4a000000/cm_core_aon@5000/clocks/dpll_dsp_m2_ck",
	"/ocp/l4@4a000000/cm_core_aon@5000/clocks/dpll_dsp_m3x2_ck",
};

const char *dra7_opp_iva_clk_names[OPP_DSP_CLK_NUM] = {
	"/ocp/l4@4a000000/cm_core_aon@5000/clocks/dpll_iva_ck",
	"/ocp/l4@4a000000/cm_core_aon@5000/clocks/dpll_iva_m2_ck",
};

/* DSPEVE voltage domain */
#if defined(CONFIG_DRA7_DSPEVE_OPP_HIGH) /* OPP_HIGH */
u32 dra752_opp_dsp_clk_rates[OPP_DSP_CLK_NUM] = {
	750000000, 750000000, 500000000,
};

u32 dra722_opp_dsp_clk_rates[OPP_DSP_CLK_NUM] = {
	700000000, 700000000, 466666667,
};
#elif defined(CONFIG_DRA7_DSPEVE_OPP_OD) /* OPP_OD */
u32 dra722_opp_dsp_clk_rates[OPP_DSP_CLK_NUM] = {
	700000000, 700000000, 466666667,
};

u32 dra752_opp_dsp_clk_rates[OPP_DSP_CLK_NUM] = {
	700000000, 700000000, 466666667,
};
#else /* OPP_NOM */
u32 dra752_opp_dsp_clk_rates[OPP_DSP_CLK_NUM] = {
	600000000, 600000000, 400000000,
};

u32 dra722_opp_dsp_clk_rates[OPP_DSP_CLK_NUM] = {
	600000000, 600000000, 400000000,
};
#endif

/* IVA voltage domain */
#if defined(CONFIG_DRA7_IVA_OPP_HIGH) /* OPP_HIGH */
u32 dra7_opp_iva_clk_rates[OPP_IVA_CLK_NUM] = {
	1064000000, 532000000,
};
#elif defined(CONFIG_DRA7_IVA_OPP_OD) /* OPP_OD */
u32 dra7_opp_iva_clk_rates[OPP_IVA_CLK_NUM] = {
	860000000, 430000000,
};
#else /* OPP_NOM */
u32 dra7_opp_iva_clk_rates[OPP_IVA_CLK_NUM] = {
	1165000000, 388333334,
};
#endif

static int ft_fixup_clocks(void *fdt, const char **paths, u32 *rates, int num)
{
	int offs, ret, i;

	for (i = 0; i < num; i++) {
		offs = fdt_path_offset(fdt, paths[i]);
		if (offs < 0) {
			debug("Could not find node path offset %s: %s\n",
			      paths[i], fdt_strerror(offs));
			return offs;
		}

		ret = fdt_setprop_u32(fdt, offs, "assigned-clock-rates",
				      rates[i]);
		if (ret < 0) {
			debug("Could not add reg property to node %s: %s\n",
			      paths[i], fdt_strerror(ret));
			return ret;
		}
	}

	return 0;
}

static void ft_opp_clock_fixups(void *fdt, bd_t *bd)
{
	const char **clk_names;
	u32 *clk_rates;
	int ret;

	if (!is_dra72x() && !is_dra7xx())
		return;

	/* fixup DSP clocks */
	clk_names = dra7_opp_dsp_clk_names;
	clk_rates = is_dra72x() ? dra722_opp_dsp_clk_rates :
				  dra752_opp_dsp_clk_rates;
	ret = ft_fixup_clocks(fdt, clk_names, clk_rates, OPP_DSP_CLK_NUM);
	if (ret) {
		printf("ft_fixup_clocks failed for DSP voltage domain: %s\n",
		       fdt_strerror(ret));
		return;
	}

	/* fixup IVA clocks */
	clk_names = dra7_opp_iva_clk_names;
	clk_rates = dra7_opp_iva_clk_rates;
	ret = ft_fixup_clocks(fdt, clk_names, clk_rates, OPP_IVA_CLK_NUM);
	if (ret) {
		printf("ft_fixup_clocks failed for IVA voltage domain: %s\n",
		       fdt_strerror(ret));
		return;
	}
}
#else
static void ft_opp_clock_fixups(void *fdt, bd_t *bd) { }
#endif /* CONFIG_TARGET_DRA7XX_EVM || CONFIG_TARGET_AM57XX_EVM */

/*
 * Place for general cpu/SoC FDT fixups. Board specific
 * fixups should remain in the board files which is where
 * this function should be called from.
 */
void ft_cpu_setup(void *fdt, bd_t *bd)
{
	ft_hs_fixups(fdt, bd);
	ft_opp_clock_fixups(fdt, bd);
}