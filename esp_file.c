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

#include "esp_path.h"
#include "esp_conf.h"

struct esp_init_table_elem esp_init_table[MAX_ATTR_NUM] = {
	{"crystal_26M_en", 48, -1},
	{"test_xtal", 49, -1},
	{"sdio_configure", 50, -1},
	{"bt_configure", 51, -1},
	{"bt_protocol", 52, -1},
	{"dual_ant_configure", 53, -1},
	{"test_uart_configure", 54, -1},
	{"share_xtal", 55, -1},
	{"gpio_wake", 56, -1},
	{"no_auto_sleep", 57, -1},
	{"speed_suspend", 58, -1},
	{"attr11", -1, -1},
	{"attr12", -1, -1},
	{"attr13", -1, -1},
	{"attr14", -1, -1},
	{"attr15", -1, -1},
	//attr that is not send to target
	{"ext_rst", -1, -1},
	{"wakeup_gpio", -1, -1},
	{"ate_test", -1, -1},
	{"attr19", -1, -1},
	{"attr20", -1, -1},
	{"attr21", -1, -1},
	{"attr22", -1, -1},
	{"attr23", -1, -1},

};

int esp_atoi(char *str)
{
	int num = 0;
	int ng_flag = 0;

	if (*str == '-') {
		str++;
		ng_flag = 1;
	}

	while (*str != '\0') {
		num = num * 10 + *str++ - '0';
	}

	return ng_flag ? 0 - num : num;
}

void show_esp_init_table(struct esp_init_table_elem *econf)
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

int request_init_conf(void)
{

	u8 *conf_buf;
	u8 *pbuf;
	int flag;
	int str_len;
	int length;
	int ret;
	int i;
	char attr_name[CONF_ATTR_LEN];
	char num_buf[CONF_VAL_LEN];
	conf_buf = (u8 *) kmalloc(MAX_BUF_LEN, GFP_KERNEL);
	if (conf_buf == NULL) {
		esp_dbg(ESP_DBG_ERROR,
			"%s: failed kmalloc memory for read init_data_conf",
			__func__);
		return -ENOMEM;
	}

	length = strlen(INIT_DATA_CONF_BUF);
	strncpy(conf_buf, INIT_DATA_CONF_BUF, length);
	conf_buf[length] = '\0';

	flag = 0;
	str_len = 0;
	for (pbuf = conf_buf; *pbuf != '$' && *pbuf != '\n'; pbuf++) {
		if (*pbuf == '=') {
			flag = 1;
			*(attr_name + str_len) = '\0';
			str_len = 0;
			continue;
		}

		if (*pbuf == ';') {
			int value;
			flag = 0;
			*(num_buf + str_len) = '\0';
			if ((value = esp_atoi(num_buf)) > 255 || value < 0) {
				esp_dbg(ESP_DBG_ERROR,
					"%s: value is too big",
					__FUNCTION__);
				goto failed;
			}

			for (i = 0; i < MAX_ATTR_NUM; i++) {
				if (strcmp
				    (esp_init_table[i].attr,
				     attr_name) == 0) {
					esp_dbg(ESP_DBG_TRACE, "%s: attr_name[%s]", __FUNCTION__, attr_name);	/* add by th */
					esp_init_table[i].value = value;
				}

				if (esp_init_table[i].value < 0)
					continue;

				if (strcmp
				    (esp_init_table[i].attr,
				     "share_xtal") == 0) {
					sif_record_bt_config(esp_init_table
							     [i].value);
				}

				if (strcmp
				    (esp_init_table[i].attr,
				     "ext_rst") == 0) {
					sif_record_rst_config
					    (esp_init_table[i].value);
				}

				if (strcmp
				    (esp_init_table[i].attr,
				     "wakeup_gpio") == 0) {
					sif_record_wakeup_gpio_config
					    (esp_init_table[i].value);
				}

				if (strcmp
				    (esp_init_table[i].attr,
				     "ate_test") == 0) {
					sif_record_ate_config
					    (esp_init_table[i].value);
				}

			}
			str_len = 0;
			continue;
		}

		if (flag == 0) {
			*(attr_name + str_len) = *pbuf;
			if (++str_len > CONF_ATTR_LEN) {
				esp_dbg(ESP_DBG_ERROR,
					"%s: attr len is too long",
					__FUNCTION__);
				goto failed;
			}
		} else {
			*(num_buf + str_len) = *pbuf;
			if (++str_len > CONF_VAL_LEN) {
				esp_dbg(ESP_DBG_ERROR,
					"%s: value len is too long",
					__FUNCTION__);
				goto failed;
			}
		}
	}

	//show_esp_init_table(esp_init_table);

	ret = 0;
      failed:
	if (conf_buf)
		kfree(conf_buf);
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
