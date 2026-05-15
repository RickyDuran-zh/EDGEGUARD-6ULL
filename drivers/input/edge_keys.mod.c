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
	{ 0xdb7305a1, "__stack_chk_fail" },
	{ 0xec255e8a, "_dev_err" },
	{ 0x559cff0c, "_dev_info" },
	{ 0x4b52df2c, "input_register_device" },
	{ 0x9887e3f, "devm_request_threaded_irq" },
	{ 0xfb129a51, "gpiod_to_irq" },
	{ 0x6693d56a, "devm_gpiod_get" },
	{ 0x2fea72c1, "input_set_capability" },
	{ 0xd09b5d64, "devm_input_allocate_device" },
	{ 0xd9edbeb1, "of_property_read_variable_u32_array" },
	{ 0xde10886d, "devm_kmalloc" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0x67b97045, "__dynamic_dev_dbg" },
	{ 0xf9a482f9, "msleep" },
	{ 0x288e0e0f, "input_event" },
	{ 0x3cb3a479, "gpiod_get_value_cansleep" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("of:N*T*Crickyduran,edge_keys");
MODULE_ALIAS("of:N*T*Crickyduran,edge_keysC*");

MODULE_INFO(srcversion, "106A31335AE1BDA44D9F90C");
