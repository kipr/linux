/*
 *  linux/arch/arm/mach-mmp/aspenite.c
 *
 *  Support for the Marvell PXA168-based Aspenite and Zylonite2
 *  Development Platform.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/smc91x.h>
#include <linux/i2c/pca953x.h>
#include <linux/card.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <linux/usb/otg.h>
#include <linux/mfd/stmpe.h>
#include <linux/leds_pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/gpio_keys.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/addr-map.h>
#include <mach/mfp-pxa168.h>
#include <mach/pxa168.h>
#include <mach/gpio.h>
#include <mach/max8660.h>
#include <mach/pxa3xx_nand.h>
#include <mach/camera.h>
#include <mach/pxa168_eth.h>
#include <mach/pxa168_pcie.h>
#include <mach/cputype.h>

#include <plat/part_table.h>
#include <plat/generic.h>
#include <plat/pxa3xx_pmic.h>
#include <plat/pxa_u2o.h>
#include <plat/pxa3xx_otg.h>

#include "common.h"
#include <linux/mmc/sdhci.h>
#include <plat/pfn_cfg.h>

#define USB_WIFI_GPIO 101
#define KOVAN_GPIO_POWERKEY 89

static unsigned long kovan_pin_config[] __initdata = {

	/* MMC controller */
	GPIO0_MMC3_DAT7,
	GPIO1_MMC3_DAT6,
	GPIO2_MMC3_DAT5,
	GPIO3_MMC3_DAT4,
	GPIO4_MMC3_DAT3,
	GPIO5_MMC3_DAT2,
	GPIO6_MMC3_DAT1,
	GPIO7_MMC3_DAT0,
	GPIO8_MMC3_CLK,
	GPIO9_MMC3_CMD,
	GPIO16_SMC_nCS0_DIS,


	/* LCD */
	MFP_CFG_DRV_PULL(GPIO56, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO57, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO58, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO59, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO60, AF1, FAST, LOW),
	MFP_CFG_DRV_PULL(GPIO61, AF1, FAST, LOW),
	MFP_CFG_DRV_PULL(GPIO62, AF1, FAST, LOW),
	MFP_CFG_DRV_PULL(GPIO63, AF1, FAST, LOW),
	MFP_CFG_DRV_PULL(GPIO64, AF1, FAST, LOW),
	MFP_CFG_DRV_PULL(GPIO65, AF1, FAST, LOW),
	MFP_CFG_DRV_PULL(GPIO66, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO67, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO68, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO69, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO70, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO71, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO72, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO73, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO74, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO75, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO76, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO77, AF1, VERY_SLOW, LOW),
	MFP_CFG_DRV_PULL(GPIO78, AF1, FAST, LOW),
	MFP_CFG_DRV_PULL(GPIO79, AF1, FAST, LOW),
	MFP_CFG_DRV_PULL(GPIO80, AF1, FAST, LOW),
	MFP_CFG_DRV_PULL(GPIO81, AF1, FAST, LOW),
	MFP_CFG_DRV_PULL(GPIO82, AF1, FAST, LOW),
	MFP_CFG_DRV_PULL(GPIO83, AF1, FAST, LOW),


	/* I2C bus */
	GPIO105_CI2C_SDA,
	GPIO106_CI2C_SCL,


	/* UART1 input */
	MFP_CFG(GPIO109, AF0),


	/* UART3 */
	MFP_CFG(GPIO98, AF2), // txd (cpu to world)
	MFP_CFG(GPIO99, AF2), // rxd (world to cpu)


	/* Audio controller - I2S / SPI / SSP0 */
	GPIO113_I2S_MCLK, // I2S audio output to the FPGA
	MFP_CFG(GPIO114, AF1), /* SSP1 FRM */
	MFP_CFG(GPIO115, AF1), /* SSP1 CLK */
	MFP_CFG(GPIO116, AF2), /* SSP1 RXD */
	MFP_CFG(GPIO117, AF2), /* SSP1 TXD */


	/* Status LED */
	MFP_CFG(GPIO96, AF1),


	/* LCD backlight */
	GPIO84_PWM1_OUT,


	/* FPGA programming GPIOs */
	MFP_CFG(GPIO119, AF0), /* fpga_reset_n (output) */
	MFP_CFG(GPIO120, AF0), /* fpga_init_n (input, mostly) */
	MFP_CFG(GPIO97, AF0), /* fpga_done (input) */

	/* FPGA programming SSPs */
	MFP_CFG(GPIO118, AF1), /* ssp2_clk */
	MFP_CFG(GPIO121, AF1), /* ssp2_txd */
	MFP_CFG(GPIO90, AF3), /* ssp3_clk */

	/* FPGA PSP config */
	// GPIO90 ssp3_clk should be here
	MFP_CFG(GPIO91, AF3), /* ssp3_frm */
	MFP_CFG(GPIO92, AF3), /* ssp3_miso */
	MFP_CFG(GPIO93, AF3), /* ssp3_mosi */

	/* FPGA status GPIOs */
	//MFP_CFG(GPIO91, AF0), /* HPD report */
	//MFP_CFG(GPIO92, AF0), /* Key ready */
	//MFP_CFG(GPIO93, AF0), /* Low-voltage alarm */
	MFP_CFG(GPIO79, AF0), /* Battery / ADC8 selection switch */
	MFP_CFG(GPIO43, AF0), /* AC-present switch */

	/* FPGA JTAG lines */
	MFP_CFG(GPIO16, AF0), /* JTAG TDI */
	MFP_CFG(GPIO18, AF0), /* JTAG TMS */
	MFP_CFG(GPIO20, AF0), /* JTAG TCK */
	MFP_CFG(GPIO34, AF0), /* JTAG TDO */


	/* Recovery button */
	MFP_CFG(GPIO89, AF0),

	/* vsync input */
	MFP_CFG(GPIO49, AF0),

	/* Touchscreen IRQ */
	MFP_CFG(GPIO52, AF0),
};



/*
 * STMPE610
 */
static struct stmpe_ts_platform_data stmpe610_ts_data = {
	.sample_time = 4,	/* 80 clocks */
	.mod_12b = 0,		/* 10-bit mode */
	.ref_sel = 0,		/* Internal reference */
	.touch_det_delay = 3,	/* 500 uS */
	.settling = 2,		/* 500 uS settling time */
	.fraction_z = 7,	/* Length of the fractional part */
	.i_drive = 0,		/* 20 mA drive */
};

static struct stmpe_platform_data stmpe610_data = {
	.id		= 1,
	.blocks		= STMPE_BLOCK_TOUCHSCREEN,
	.irq_trigger	= IRQF_TRIGGER_FALLING,
	.irq_gpio	= 52,
	.irq_over_gpio	= true,
	.ts		= &stmpe610_ts_data,
};


/*
 * PWM green LED
 */
static struct led_pwm kovan_pwm_leds[] = {
	{
		.name			= "kovan:green:state",
		.default_trigger	= "heartbeat",
		.pwm_id			= 2,
		.max_brightness		= 100,
		.pwm_period_ns		= 10000,
	},
};

static struct led_pwm_platform_data kovan_pwm_leds_platform_data = {
	.leds		= kovan_pwm_leds,
	.num_leds	= ARRAY_SIZE(kovan_pwm_leds),
};

static struct platform_device kovan_leds_device = {
	.name	= "leds_pwm",
	.id	= -1,
	.dev	= {
		.platform_data	= &kovan_pwm_leds_platform_data,
	},
};



/*
 * GPIO keys
 */
static struct gpio_keys_button kovan_gpio_keys_buttons[] = {
	{
		.code		= KEY_POWER,
		.gpio		= KOVAN_GPIO_POWERKEY,
		.active_low	= 1,
		.desc		= "Power",
		.type		= EV_KEY,
		.wakeup		= 1,
	},
};

static struct gpio_keys_platform_data kovan_gpio_keys_data = {
	.buttons	= kovan_gpio_keys_buttons,
	.nbuttons	= ARRAY_SIZE(kovan_gpio_keys_buttons),
};

static struct platform_device kovan_keys_device = {
	.name	= "gpio-keys",
	.id	= -1,
	.dev	= {
		.platform_data = &kovan_gpio_keys_data,
	},
};



/*
 * Backlight data
 */
static struct platform_pwm_backlight_data kovan_backlight_data = {
	.pwm_id		= 0,
	.max_brightness	= 100,
	.dft_brightness	= 100,
	.pwm_period_ns	= 10000,
};

static struct platform_device kovan_backlight_device = {
	.name		= "pwm-backlight",
	.dev		= {
		.parent		= &pxa168_device_pwm0.dev,
		.platform_data	= &kovan_backlight_data,
	},
};


/*
 * Video interfaces
 */
static struct fb_videomode video_modes_aspen[] = {
	/* lpj032l001b HVGA mode info */
        [0] = {
                .pixclock       = 156000,
                .refresh        = 60,

                .hsync_len      = 30,
                .left_margin    = 38,
                .xres           = 320,
                .right_margin   = 20,

                .vsync_len      = 3,
                .upper_margin   = 15,
                .yres           = 240,
                .lower_margin   = 4,

                .sync           = 0, 
        },
};

struct pxa168fb_mach_info kovan_lcd_info __initdata = {
	.id                     = "Base-kovan",
	.modes                  = video_modes_aspen,
	.num_modes              = ARRAY_SIZE(video_modes_aspen),
	.pix_fmt                = PIX_FMT_RGB565,
	.max_fb_size		= 320 * 240 * 4 * 2,

	.active                 = 1,
	.enable_lcd             = 1,

        .io_pin_allocation_mode = PIN_MODE_DUMB_18_GPIO,

	.dumb_mode              = DUMB_MODE_RGB666,
	.gpio_output_mask	= 0xff,
	.gpio_output_data	= 0x10,

	.invert_pix_val_ena	= 1,
};

struct pxa168fb_mach_info kovan_lcd_ovly_info __initdata = {
        .id                     = "Ovly-kovan",
        .modes                  = video_modes_aspen,
        .num_modes              = ARRAY_SIZE(video_modes_aspen),
        .pix_fmt                = PIX_FMT_RGB565,
	.active                 = 1,
	.enable_lcd             = 1,

        .io_pin_allocation_mode = PIN_MODE_DUMB_18_GPIO,

	.dumb_mode              = DUMB_MODE_RGB666,
	.gpio_output_mask	= 0xff,
	.gpio_output_data	= 0x10,

	.invert_pix_val_ena	= 1,
};




/*
 * I2C devices
 */
static struct i2c_board_info kovan_i2c_board_info[] = {

};

static struct i2c_board_info pwr_i2c_board_info[] = {
	/* Touchscreen */
	{
		I2C_BOARD_INFO("stmpe610", 0x44),
		.platform_data = &stmpe610_data,
	},

	/* Audio codec */
	{
		I2C_BOARD_INFO("es8328", 0x11),
	},
};


static struct i2c_pxa_platform_data i2c_info __initdata = {
	.use_pio		= 1,
};



/*
 * MMC interface
 */

static struct pfn_cfg mmc3_pfn_cfg[] = {
	PFN_CFG(PIN_MMC_DAT7, GPIO0_MMC3_DAT7, GPIO0_GPIO),
	PFN_CFG(PIN_MMC_DAT6, GPIO1_MMC3_DAT6, GPIO1_GPIO),
	PFN_CFG(PIN_MMC_DAT5, GPIO2_MMC3_DAT5, GPIO2_GPIO),
	PFN_CFG(PIN_MMC_DAT4, GPIO3_MMC3_DAT4, GPIO3_GPIO),
	PFN_CFG(PIN_MMC_DAT3, GPIO4_MMC3_DAT3, GPIO4_GPIO),
	PFN_CFG(PIN_MMC_DAT2, GPIO5_MMC3_DAT2, GPIO5_GPIO),
	PFN_CFG(PIN_MMC_DAT1, GPIO6_MMC3_DAT1, GPIO6_GPIO),
	PFN_CFG(PIN_MMC_DAT0, GPIO7_MMC3_DAT0, GPIO7_GPIO),
	PFN_CFG(PIN_MMC_CLK, GPIO8_MMC3_CLK, GPIO8_GPIO),
	PFN_CFG(PIN_MMC_CMD, GPIO9_MMC3_CMD, GPIO9_GPIO),
	PFN_CFG(PIN_MMC_CD, PFN_UNDEF, PFN_UNDEF),
	PFN_CFG(PIN_MMC_WP, PFN_UNDEF, PFN_UNDEF),
	PFN_CFG(PIN_MMC_END, PFN_TERM, PFN_TERM),
};


static struct pxasdh_platform_data kovan_sdh_platform_data_mmc3 = {
	.detect_delay	= 20,
	.ocr_mask	= MMC_VDD_29_30 | MMC_VDD_30_31,
	.bus_width	= 8,
	.quirks 	= SDHCI_QUIRK_BROKEN_CARD_DETECTION,
	.pfn_table	= mmc3_pfn_cfg,
};




/*
 * USB OTG interface
 */
static int kovan_u2o_vbus_status(unsigned base)
{
	int status = VBUS_LOW;

	mdelay(2);
	if (u2o_get(base, U2xOTGSC) & U2xOTGSC_BSV)
		status = VBUS_HIGH;
	else
		status = VBUS_LOW;

	return status;

}

static int kovan_u2o_vbus_set(int vbus_type)
{
	return 0;
}
static int kovan_otg_init(void)
{
	return 0;
}

static int kovan_u2o_vbus_set_ic(int function)
{
	return 0;
}

static struct otg_pmic_ops kovan_otg_ops = {
	.otg_vbus_init          = kovan_otg_init,
	.otg_set_vbus           = kovan_u2o_vbus_set,
	.otg_set_vbus_ic        = kovan_u2o_vbus_set_ic,
	.otg_get_vbus_state     = kovan_u2o_vbus_status,
};

struct otg_pmic_ops *init_kovan_otg_ops(void)
{
	return &kovan_otg_ops;
}

static int kovan_usbid_detect(struct otg_transceiver *otg)
{
	return 1; /* OTG_B_DEVICE */
}

static struct pxa_usb_plat_info kovan_u2o_info = {
	.phy_init	= pxa168_usb_phy_init,
	.phy_deinit	= pxa168_usb_phy_deinit,
	.vbus_set	= kovan_u2o_vbus_set,
	.vbus_status	= kovan_u2o_vbus_status,
	.init_pmic_ops	= (void *)init_kovan_otg_ops,
	.usbid_detect	= kovan_usbid_detect,
	.is_otg		= 1,
};


/* USB 2.0 Host Controller */
static struct pxa_usb_plat_info kovan_u2h_info = {
	.phy_init	= pxa168_usb_phy_init,
	.vbus_set	= kovan_u2o_vbus_set,
};



/*
 * FPGA interface
 */
static struct platform_device kovan_fpga_device = {
	.name		= "silvermoon-fpga",
};


/*
 * SPI and associated interfaces
 */

static struct pxa2xx_spi_master pxa_ssp3_spi_master_info = {
	.num_chipselect	= 1,
	.enable_dma	= 1,
};

static struct pxa2xx_spi_master pxa_ssp4_spi_master_info = {
	.num_chipselect	= 1,
	.enable_dma	= 1,
};

struct platform_device pxa_spi_ssp3 = {
	.name          = "pxa2xx-spi",
	.id            = 3,
	.dev           = {
		.platform_data = &pxa_ssp3_spi_master_info,
	},
};

struct platform_device pxa_spi_ssp4 = {
	.name	= "pxa2xx-spi",
	.id	= 4,
	.dev	= {
		.platform_data = &pxa_ssp4_spi_master_info,
	},
};

static struct spi_board_info kovan_spi_board_info[] __initdata = {
	{
		.modalias	= "spidev",
		.max_speed_hz	= 26000000,
		.bus_num	= 3,
		.chip_select	= 0,
	},
	{
		.modalias	= "spidev",
		.max_speed_hz	= 26000000,
		.bus_num	= 4,
		.chip_select	= 0,
	},
};




static void kovan_power_off(void)
{
	while(1);
}


static void __init kovan_init(void)
{
	mfp_config(ARRAY_AND_SIZE(kovan_pin_config));
        pxa168_set_vdd_iox(VDD_IO0, VDD_IO_3P3V);
	pxa168_set_vdd_iox(VDD_IO1, VDD_IO_3P3V);
	pxa168_set_vdd_iox(VDD_IO2, VDD_IO_3P3V);
	pxa168_set_vdd_iox(VDD_IO3, VDD_IO_3P3V);
	pxa168_set_vdd_iox(VDD_IO4, VDD_IO_3P3V);
	pxa168_mfp_set_fastio_drive(MFP_DS02X);

	/* on-chip devices */
	pxa168_add_uart(1);
	pxa168_add_uart(3);

	pxa168_add_freq();

	if(pxa168_add_ssp(0))
		printk("Unable to add SSP0\n");
	if(pxa168_add_ssp(1))
		printk("Unable to add SSP1\n");
	if(pxa168_add_ssp(2))
		printk("Unable to add SSP2\n");
	if(pxa168_add_ssp(3))
		printk("Unable to add SSP3\n");
	if(pxa168_add_ssp(4))
		printk("Unable to add SSP4\n");

	//printk("Registering SPI drivers for SSP3 and SSP4\n");
	//spi_register_board_info(kovan_spi_board_info,
	//	ARRAY_SIZE(kovan_spi_board_info));
    //    platform_device_register(&pxa_spi_ssp3);
    //    platform_device_register(&pxa_spi_ssp4);

	pxa168_add_twsi(0, &i2c_info, ARRAY_AND_SIZE(kovan_i2c_board_info));
	pxa168_add_twsi(1, &i2c_info, ARRAY_AND_SIZE(pwr_i2c_board_info));

 	pxa168_add_u2o(&kovan_u2o_info);

#ifdef CONFIG_USB_OTG
	pxa168_add_u2ootg(&kovan_u2o_info);
	pxa168_add_u2oehci(&kovan_u2o_info);
#endif

 	pxa168_add_u2h(&kovan_u2h_info);

	pxa168_add_sdh(2, &kovan_sdh_platform_data_mmc3);

	pxa168_cir_init();


	pxa168_add_fb(&kovan_lcd_info);
	pxa168_add_fb_ovly(&kovan_lcd_ovly_info);

	/* Add FPGA device interface */
	platform_device_register(&kovan_fpga_device);

	pxa168_add_rtc(&pxa910_device_rtc);

	platform_device_register(&kovan_keys_device);

	/* Add the power state LED */
	platform_device_register(&pxa168_device_pwm2);
	platform_device_register(&kovan_leds_device);

	/* Add the LCD backlight */
	platform_device_register(&pxa168_device_pwm0);
	platform_device_register(&kovan_backlight_device);


	pm_power_off = kovan_power_off;
}



MACHINE_START(KOVAN, "PXA168-based Kovan Platform")
	.phys_io        = APB_PHYS_BASE,
	.boot_params    = 0x00000100,
	.io_pg_offst    = (APB_VIRT_BASE >> 18) & 0xfffc,
	.map_io		= pxa_map_io,
	.init_irq       = pxa168_init_irq,
	.timer          = &pxa168_timer,
	.init_machine   = kovan_init,
MACHINE_END
