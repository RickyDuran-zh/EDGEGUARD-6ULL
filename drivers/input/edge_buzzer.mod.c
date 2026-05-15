#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x10d06f3b, "module_layout" },
	{ 0xe5135656, "platform_driver_unregister" },
	{ 0x911060d2, "__platform_driver_register" },
	{ 0xec255e8a, "_dev_err" },
	{ 0x559cff0c, "_dev_info" },
	{ 0x77338972, "misc_register" },
	{ 0x6693d56a, "devm_gpiod_get" },
	{ 0xe346f67a, "__mutex_init" },
	{ 0xde10886d, "devm_kmalloc" },
	{ 0x7c32d0f0, "printk" },
	{ 0xdb7305a1, "__stack_chk_fail" },
	{ 0x67ea780, "mutex_unlock" },
	{ 0xf9a482f9, "msleep" },
	{ 0xe2d5255a, "strcmp" },
	{ 0xc271c3be, "mutex_lock" },
	{ 0x77bc13a0, "strim" },
	{ 0x28cc25db, "arm_copy_from_user" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0xe5782892, "misc_deregister" },
	{ 0x9c20da97, "gpiod_set_value_cansleep" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("of:N*T*Crickyduran,edge_buzzer");
MODULE_ALIAS("of:N*T*Crickyduran,edge_buzzerC*");

MODULE_INFO(srcversion, "1E6C3B464F2812B11F15180");
