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
	{ 0x9999af2c, "i2c_del_driver" },
	{ 0x1242b8d4, "i2c_register_driver" },
	{ 0x77338972, "misc_register" },
	{ 0xe346f67a, "__mutex_init" },
	{ 0xde10886d, "devm_kmalloc" },
	{ 0xf9a482f9, "msleep" },
	{ 0xec255e8a, "_dev_err" },
	{ 0x8384469f, "i2c_smbus_write_byte_data" },
	{ 0xdb7305a1, "__stack_chk_fail" },
	{ 0x528c709d, "simple_read_from_buffer" },
	{ 0xf9e73082, "scnprintf" },
	{ 0x67ea780, "mutex_unlock" },
	{ 0x5501fc44, "i2c_smbus_read_i2c_block_data" },
	{ 0xc271c3be, "mutex_lock" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0x559cff0c, "_dev_info" },
	{ 0xe5782892, "misc_deregister" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("i2c:ap3216c_raw");
MODULE_ALIAS("of:N*T*Crickyduran,i2c_ap3216c");
MODULE_ALIAS("of:N*T*Crickyduran,i2c_ap3216cC*");

MODULE_INFO(srcversion, "A70035CF5014268A78FE84A");
