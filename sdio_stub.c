/*
 * Copyright (c) 2013 Espressif System.
 *
 *  sdio stub code for RK
 */

//#include <mach/gpio.h>
//#include <mach/iomux.h>

#include <linux/mmc/host.h>

#define ESP8089_DRV_VERSION "1.9"

void sif_platform_rescan_card(unsigned insert)
{
}

void sif_platform_reset_target(void)
{
}

void sif_platform_target_poweroff(void)
{
}

void sif_platform_target_poweron(void)
{
}

void sif_platform_target_speed(int high_speed)
{
}

void sif_platform_check_r1_ready(struct esp_pub *epub)
{
}


late_initcall(esp_sdio_init);
module_exit(esp_sdio_exit);
