/*
 * Copyright (c) 2010 -2014 Espressif System.
 *
 *   file operation in kernel space
 *
 */

#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/firmware.h>
#include <linux/netdevice.h>
#include <linux/aio.h>

#include "esp_file.h"
#include "esp_debug.h"
#include "esp_sif.h"

static char *modparam_init_data_conf;
module_param_named(config, modparam_init_data_conf, charp, 0444);
MODULE_PARM_DESC(config, "Firmware init config string (format: key=value;)");

struct esp_init_table_elem esp_init_table[MAX_ATTR_NUM] = {
	{"crystal_26M_en", 48, 0},
	{"test_xtal", 49, 0},
	{"sdio_configure", 50, 2},
	{"bt_configure", 51, 0},
	{"bt_protocol", 52, 0},
	{"dual_ant_configure", 53, 0},
	{"test_uart_configure", 54, 2},
	{"share_xtal", 55, 0},
	{"gpio_wake", 56, 0},
	{"no_auto_sleep", 57, 0},
	{"speed_suspend", 58, 0},
	{"attr11", -1, -1},
	{"attr12", -1, -1},
	{"attr13", -1, -1},
	{"attr14", -1, -1},
	{"attr15", -1, -1},
	//attr that is not send to target
	{"ext_rst", -1, 0},
	{"wakeup_gpio", -1, 12},
	{"ate_test", -1, 0},
	{"attr19", -1, -1},
	{"attr20", -1, -1},
	{"attr21", -1, -1},
	{"attr22", -1, -1},
	{"attr23", -1, -1},

};

static void show_esp_init_table(struct esp_init_table_elem *econf)
{
	int i;
	for (i = 0; i < MAX_ATTR_NUM; i++)
		if (esp_init_table[i].offset > -1)
			esp_dbg(ESP_DBG_ERROR,
				"%s: esp_init_table[%d] attr[%s] offset[%d] value[%d]\n",
				__FUNCTION__, i, esp_init_table[i].attr,
				esp_init_table[i].offset,
				esp_init_table[i].value);
}

/* update init config table */
static int update_init_config_attr(const char *attr, int attr_len,
				   const char *val, int val_len)
{
	char digits[4];
	short value;
	int i;

	for (i = 0; i < sizeof(digits) - 1 && i < val_len; i++)
		digits[i] = val[i];
	digits[i] = 0;

	if (kstrtou16(digits, 10, &value) < 0) {
		esp_dbg(ESP_DBG_ERROR, "%s: invalid attribute value: %s",
			__func__, digits);
		return -1;
	}

	for (i = 0; i < MAX_ATTR_NUM; i++) {
		if (!memcmp(esp_init_table[i].attr, attr, attr_len)) {
			if (value < 0 || value > 255) {
				esp_dbg(ESP_DBG_ERROR, "%s: attribute value for %s is out of range",
					__func__, esp_init_table[i].attr);
				return -1;
			}
			esp_init_table[i].value = value;
			return 0;
		}
	}

	return -1;
}

/* export config table settings to SDIO driver */
static void record_init_config(void)
{
	int i;

	for (i = 0; i < MAX_ATTR_NUM; i++) {
		if (esp_init_table[i].value < 0)
			continue;

		if (!strcmp(esp_init_table[i].attr, "share_xtal"))
			sif_record_bt_config(esp_init_table[i].value);
		else if (!strcmp(esp_init_table[i].attr, "ext_rst"))
			sif_record_rst_config(esp_init_table[i].value);
		else if (!strcmp(esp_init_table[i].attr, "wakeup_gpio"))
			sif_record_wakeup_gpio_config(esp_init_table[i].value);
		else if (!strcmp(esp_init_table[i].attr, "ate_test"))
			sif_record_ate_config(esp_init_table[i].value);
	}
}

int request_init_conf(void)
{
	char *attr, *str, *p;
	int attr_len, str_len;
	int ret = 0;

	/* parse optional parameter in the form of key1=value,key2=value,.. */
	attr = NULL;
	attr_len = str_len = 0;
	for (p = str = modparam_init_data_conf; p && *p; p++) {
		if (*p == '=') {
			attr = str;
			attr_len = str_len;

			str = p + 1;
			str_len = 0;
		} else if (*p == ',' || *p == ';') {
			if (attr_len)
				ret |= update_init_config_attr(attr, attr_len,
							       str, str_len);

			str = p + 1;
			attr_len = str_len = 0;
		} else
			str_len++;
	}

	if (attr_len && str != attr)
		ret |= update_init_config_attr(attr, attr_len, str, str_len);

	/* show_esp_init_table(esp_init_table); */

	record_init_config();

	return ret;
}

void fix_init_data(u8 * init_data_buf, int buf_size)
{
	int i;

	for (i = 0; i < MAX_FIX_ATTR_NUM; i++) {
		if (esp_init_table[i].offset > -1
		    && esp_init_table[i].offset < buf_size
		    && esp_init_table[i].value > -1) {
			*(u8 *) (init_data_buf +
				 esp_init_table[i].offset) =
			    esp_init_table[i].value;
		} else if (esp_init_table[i].offset > buf_size) {
			esp_dbg(ESP_DBG_ERROR,
				"%s: offset[%d] longer than init_data_buf len[%d] Ignore\n",
				__FUNCTION__, esp_init_table[i].offset,
				buf_size);
		}
	}

}
