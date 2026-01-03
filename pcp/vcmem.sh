#!/bin/sh
tce-load -il raspi-utils
vcgencmd get_mem gpu
vcgencmd get_mem malloc_total
vcgencmd get_mem malloc
vcgencmd get_mem reloc_total
vcgencmd get_mem reloc
