cmd_/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/drivers/i2c_sensors/mpu6050_raw.o := arm-linux-gnueabihf-gcc -Wp,-MD,/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/drivers/i2c_sensors/.mpu6050_raw.o.d  -nostdinc -isystem /usr/lib/gcc-cross/arm-linux-gnueabihf/7/include -I/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include -I./arch/arm/include/generated  -I/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include -I./include -I/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/uapi -I./arch/arm/include/generated/uapi -I/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi -I./include/generated/uapi -include /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/kconfig.h -include /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/compiler_types.h  -I/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/drivers/i2c_sensors -I/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/drivers/i2c_sensors -D__KERNEL__ -mlittle-endian -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -Werror-implicit-function-declaration -Wno-format-security -std=gnu89 -fno-PIE -DCC_HAVE_ASM_GOTO -fno-dwarf2-cfi-asm -fno-ipa-sra -mabi=aapcs-linux -mfpu=vfp -funwind-tables -marm -Wa,-mno-warn-deprecated -D__LINUX_ARM_ARCH__=7 -march=armv7-a -msoft-float -Uarm -fno-delete-null-pointer-checks -Wno-frame-address -Wno-format-truncation -Wno-format-overflow -Wno-int-in-bool-context -O2 --param=allow-store-data-races=0 -Wframe-larger-than=1024 -fstack-protector-strong -Wno-unused-but-set-variable -Wno-unused-const-variable -fomit-frame-pointer -fno-var-tracking-assignments -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fno-merge-all-constants -fmerge-constants -fno-stack-check -fconserve-stack -Werror=implicit-int -Werror=strict-prototypes -Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init  -DMODULE  -DKBUILD_BASENAME='"mpu6050_raw"' -DKBUILD_MODNAME='"mpu6050_raw"' -c -o /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/drivers/i2c_sensors/.tmp_mpu6050_raw.o /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/drivers/i2c_sensors/mpu6050_raw.c

source_/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/drivers/i2c_sensors/mpu6050_raw.o := /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/drivers/i2c_sensors/mpu6050_raw.c

deps_/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/drivers/i2c_sensors/mpu6050_raw.o := \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/kconfig.h \
    $(wildcard include/config/cpu/big/endian.h) \
    $(wildcard include/config/booger.h) \
    $(wildcard include/config/foo.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/compiler_types.h \
    $(wildcard include/config/have/arch/compiler/h.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/arch/supports/optimized/inlining.h) \
    $(wildcard include/config/optimize/inlining.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/compiler-gcc.h \
    $(wildcard include/config/retpoline.h) \
    $(wildcard include/config/arch/use/builtin/bswap.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/module.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/sysfs.h) \
    $(wildcard include/config/modules/tree/lookup.h) \
    $(wildcard include/config/livepatch.h) \
    $(wildcard include/config/unused/symbols.h) \
    $(wildcard include/config/module/sig.h) \
    $(wildcard include/config/generic/bug.h) \
    $(wildcard include/config/kallsyms.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/tracepoints.h) \
    $(wildcard include/config/tracing.h) \
    $(wildcard include/config/event/tracing.h) \
    $(wildcard include/config/ftrace/mcount/record.h) \
    $(wildcard include/config/module/unload.h) \
    $(wildcard include/config/constructors.h) \
    $(wildcard include/config/function/error/injection.h) \
    $(wildcard include/config/strict/module/rwx.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/list.h \
    $(wildcard include/config/debug/list.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/types.h \
    $(wildcard include/config/have/uid16.h) \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/lbdaf.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/types.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/uapi/asm/types.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/int-ll64.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/int-ll64.h \
  arch/arm/include/generated/uapi/asm/bitsperlong.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitsperlong.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/bitsperlong.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/posix_types.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/stddef.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/stddef.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/compiler_types.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/uapi/asm/posix_types.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/posix_types.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/poison.h \
    $(wildcard include/config/illegal/pointer/value.h) \
    $(wildcard include/config/page/poisoning/zero.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/const.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/const.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/kernel.h \
    $(wildcard include/config/preempt/voluntary.h) \
    $(wildcard include/config/debug/atomic/sleep.h) \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/prove/locking.h) \
    $(wildcard include/config/arch/has/refcount.h) \
    $(wildcard include/config/panic/timeout.h) \
  /usr/lib/gcc-cross/arm-linux-gnueabihf/7/include/stdarg.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/linkage.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/stringify.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/export.h \
    $(wildcard include/config/modversions.h) \
    $(wildcard include/config/module/rel/crcs.h) \
    $(wildcard include/config/have/arch/prel32/relocations.h) \
    $(wildcard include/config/trim/unused/ksyms.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/linkage.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/stack/validation.h) \
    $(wildcard include/config/kasan.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/barrier.h \
    $(wildcard include/config/cpu/32v6k.h) \
    $(wildcard include/config/thumb2/kernel.h) \
    $(wildcard include/config/cpu/xsc3.h) \
    $(wildcard include/config/cpu/fa526.h) \
    $(wildcard include/config/arm/heavy/mb.h) \
    $(wildcard include/config/arm/dma/mem/bufferable.h) \
    $(wildcard include/config/cpu/spectre.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/barrier.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/kasan-checks.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/bitops.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/bits.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/bitops.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/irqflags.h \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/irqsoff/tracer.h) \
    $(wildcard include/config/preempt/tracer.h) \
    $(wildcard include/config/trace/irqflags/support.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/typecheck.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/irqflags.h \
    $(wildcard include/config/cpu/v7m.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/ptrace.h \
    $(wildcard include/config/arm/thumb.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/uapi/asm/ptrace.h \
    $(wildcard include/config/cpu/endian/be8.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/hwcap.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/uapi/asm/hwcap.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/irqflags.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/non-atomic.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/builtin-__fls.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/builtin-__ffs.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/builtin-fls.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/builtin-ffs.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/ffz.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/fls64.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/sched.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/hweight.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/arch_hweight.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/const_hweight.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/lock.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/atomic.h \
    $(wildcard include/config/generic/atomic64.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/atomic.h \
    $(wildcard include/config/arm/lpae.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/prefetch.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/processor.h \
    $(wildcard include/config/have/hw/breakpoint.h) \
    $(wildcard include/config/binfmt/elf/fdpic.h) \
    $(wildcard include/config/arm/errata/754327.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/hw_breakpoint.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/unified.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/cache.h \
    $(wildcard include/config/arm/l1/cache/shift.h) \
    $(wildcard include/config/aeabi.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/cmpxchg.h \
    $(wildcard include/config/cpu/sa1100.h) \
    $(wildcard include/config/cpu/sa110.h) \
    $(wildcard include/config/cpu/v6.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/cmpxchg-local.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/atomic-long.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/find.h \
    $(wildcard include/config/generic/find/first/bit.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/le.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/uapi/asm/byteorder.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/byteorder/little_endian.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/byteorder/little_endian.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/swab.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/swab.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/swab.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/uapi/asm/swab.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/byteorder/generic.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bitops/ext2-atomic-setbit.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/log2.h \
    $(wildcard include/config/arch/has/ilog2/u32.h) \
    $(wildcard include/config/arch/has/ilog2/u64.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/printk.h \
    $(wildcard include/config/message/loglevel/default.h) \
    $(wildcard include/config/console/loglevel/default.h) \
    $(wildcard include/config/console/loglevel/quiet.h) \
    $(wildcard include/config/early/printk.h) \
    $(wildcard include/config/printk/nmi.h) \
    $(wildcard include/config/printk.h) \
    $(wildcard include/config/dynamic/debug.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/init.h \
    $(wildcard include/config/strict/kernel/rwx.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/kern_levels.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/cache.h \
    $(wildcard include/config/arch/has/cache/line/size.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/kernel.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/sysinfo.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/dynamic_debug.h \
    $(wildcard include/config/jump/label.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/build_bug.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/div64.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/compiler.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/div64.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/stat.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/uapi/asm/stat.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/stat.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/time.h \
    $(wildcard include/config/arch/uses/gettimeoffset.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/seqlock.h \
    $(wildcard include/config/debug/lock/alloc.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/spinlock.h \
    $(wildcard include/config/debug/spinlock.h) \
    $(wildcard include/config/preempt.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/preempt.h \
    $(wildcard include/config/preempt/count.h) \
    $(wildcard include/config/debug/preempt.h) \
    $(wildcard include/config/trace/preempt/toggle.h) \
    $(wildcard include/config/preempt/notifiers.h) \
  arch/arm/include/generated/asm/preempt.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/preempt.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/thread_info.h \
    $(wildcard include/config/thread/info/in/task.h) \
    $(wildcard include/config/have/arch/within/stack/frames.h) \
    $(wildcard include/config/hardened/usercopy.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/bug.h \
    $(wildcard include/config/bug/on/data/corruption.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/bug.h \
    $(wildcard include/config/debug/bugverbose.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/opcodes.h \
    $(wildcard include/config/cpu/endian/be32.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/bug.h \
    $(wildcard include/config/bug.h) \
    $(wildcard include/config/generic/bug/relative/pointers.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/restart_block.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/time64.h \
    $(wildcard include/config/64bit/time.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/math64.h \
    $(wildcard include/config/arch/supports/int128.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/time.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/thread_info.h \
    $(wildcard include/config/crunch.h) \
    $(wildcard include/config/arm/thumbee.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/fpstate.h \
    $(wildcard include/config/vfpv3.h) \
    $(wildcard include/config/iwmmxt.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/page.h \
    $(wildcard include/config/cpu/copy/v4wt.h) \
    $(wildcard include/config/cpu/copy/v4wb.h) \
    $(wildcard include/config/cpu/copy/feroceon.h) \
    $(wildcard include/config/cpu/copy/fa.h) \
    $(wildcard include/config/cpu/xscale.h) \
    $(wildcard include/config/cpu/copy/v6.h) \
    $(wildcard include/config/kuser/helpers.h) \
    $(wildcard include/config/have/arch/pfn/valid.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/glue.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/pgtable-2level-types.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/memory.h \
    $(wildcard include/config/need/mach/memory/h.h) \
    $(wildcard include/config/page/offset.h) \
    $(wildcard include/config/highmem.h) \
    $(wildcard include/config/dram/base.h) \
    $(wildcard include/config/dram/size.h) \
    $(wildcard include/config/xip/kernel.h) \
    $(wildcard include/config/have/tcm.h) \
    $(wildcard include/config/arm/patch/phys/virt.h) \
    $(wildcard include/config/phys/offset.h) \
    $(wildcard include/config/xip/phys/addr.h) \
    $(wildcard include/config/debug/virtual.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/sizes.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/memory_model.h \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/discontigmem.h) \
    $(wildcard include/config/sparsemem/vmemmap.h) \
    $(wildcard include/config/sparsemem.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/pfn.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/getorder.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/bottom_half.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/spinlock_types.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/spinlock_types.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/lockdep.h \
    $(wildcard include/config/lockdep.h) \
    $(wildcard include/config/lock/stat.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rwlock_types.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/spinlock.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rwlock.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/spinlock_api_smp.h \
    $(wildcard include/config/inline/spin/lock.h) \
    $(wildcard include/config/inline/spin/lock/bh.h) \
    $(wildcard include/config/inline/spin/lock/irq.h) \
    $(wildcard include/config/inline/spin/lock/irqsave.h) \
    $(wildcard include/config/inline/spin/trylock.h) \
    $(wildcard include/config/inline/spin/trylock/bh.h) \
    $(wildcard include/config/uninline/spin/unlock.h) \
    $(wildcard include/config/inline/spin/unlock/bh.h) \
    $(wildcard include/config/inline/spin/unlock/irq.h) \
    $(wildcard include/config/inline/spin/unlock/irqrestore.h) \
    $(wildcard include/config/generic/lockbreak.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rwlock_api_smp.h \
    $(wildcard include/config/inline/read/lock.h) \
    $(wildcard include/config/inline/write/lock.h) \
    $(wildcard include/config/inline/read/lock/bh.h) \
    $(wildcard include/config/inline/write/lock/bh.h) \
    $(wildcard include/config/inline/read/lock/irq.h) \
    $(wildcard include/config/inline/write/lock/irq.h) \
    $(wildcard include/config/inline/read/lock/irqsave.h) \
    $(wildcard include/config/inline/write/lock/irqsave.h) \
    $(wildcard include/config/inline/read/trylock.h) \
    $(wildcard include/config/inline/write/trylock.h) \
    $(wildcard include/config/inline/read/unlock.h) \
    $(wildcard include/config/inline/write/unlock.h) \
    $(wildcard include/config/inline/read/unlock/bh.h) \
    $(wildcard include/config/inline/write/unlock/bh.h) \
    $(wildcard include/config/inline/read/unlock/irq.h) \
    $(wildcard include/config/inline/write/unlock/irq.h) \
    $(wildcard include/config/inline/read/unlock/irqrestore.h) \
    $(wildcard include/config/inline/write/unlock/irqrestore.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/time32.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/uidgid.h \
    $(wildcard include/config/multiuser.h) \
    $(wildcard include/config/user/ns.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/highuid.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/kmod.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/umh.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/gfp.h \
    $(wildcard include/config/zone/dma.h) \
    $(wildcard include/config/zone/dma32.h) \
    $(wildcard include/config/zone/device.h) \
    $(wildcard include/config/numa.h) \
    $(wildcard include/config/pm/sleep.h) \
    $(wildcard include/config/memory/isolation.h) \
    $(wildcard include/config/compaction.h) \
    $(wildcard include/config/cma.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/mmdebug.h \
    $(wildcard include/config/debug/vm.h) \
    $(wildcard include/config/debug/vm/pgflags.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/mmzone.h \
    $(wildcard include/config/force/max/zoneorder.h) \
    $(wildcard include/config/zsmalloc.h) \
    $(wildcard include/config/memcg.h) \
    $(wildcard include/config/memory/hotplug.h) \
    $(wildcard include/config/flat/node/mem/map.h) \
    $(wildcard include/config/page/extension.h) \
    $(wildcard include/config/no/bootmem.h) \
    $(wildcard include/config/deferred/struct/page/init.h) \
    $(wildcard include/config/transparent/hugepage.h) \
    $(wildcard include/config/have/memory/present.h) \
    $(wildcard include/config/have/memoryless/nodes.h) \
    $(wildcard include/config/have/memblock/node/map.h) \
    $(wildcard include/config/need/multiple/nodes.h) \
    $(wildcard include/config/have/arch/early/pfn/to/nid.h) \
    $(wildcard include/config/sparsemem/extreme.h) \
    $(wildcard include/config/memory/hotremove.h) \
    $(wildcard include/config/holes/in/zone.h) \
    $(wildcard include/config/arch/has/holes/memorymodel.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/wait.h \
  arch/arm/include/generated/asm/current.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/current.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/wait.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/threads.h \
    $(wildcard include/config/nr/cpus.h) \
    $(wildcard include/config/base/small.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/numa.h \
    $(wildcard include/config/nodes/shift.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/nodemask.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/bitmap.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/string.h \
    $(wildcard include/config/binary/printf.h) \
    $(wildcard include/config/fortify/source.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/string.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/string.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/pageblock-flags.h \
    $(wildcard include/config/hugetlb/page.h) \
    $(wildcard include/config/hugetlb/page/size/variable.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/page-flags-layout.h \
    $(wildcard include/config/numa/balancing.h) \
  include/generated/bounds.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/memory_hotplug.h \
    $(wildcard include/config/arch/has/add/pages.h) \
    $(wildcard include/config/have/arch/nodedata/extension.h) \
    $(wildcard include/config/have/bootmem/info/node.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/notifier.h \
    $(wildcard include/config/tree/srcu.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/errno.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/errno.h \
  arch/arm/include/generated/uapi/asm/errno.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/errno.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/errno-base.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/mutex.h \
    $(wildcard include/config/mutex/spin/on/owner.h) \
    $(wildcard include/config/debug/mutexes.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/osq_lock.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/debug_locks.h \
    $(wildcard include/config/debug/locking/api/selftests.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rwsem.h \
    $(wildcard include/config/rwsem/spin/on/owner.h) \
    $(wildcard include/config/rwsem/generic/spinlock.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/err.h \
  arch/arm/include/generated/asm/rwsem.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/rwsem.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/srcu.h \
    $(wildcard include/config/tiny/srcu.h) \
    $(wildcard include/config/srcu.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rcupdate.h \
    $(wildcard include/config/preempt/rcu.h) \
    $(wildcard include/config/rcu/stall/common.h) \
    $(wildcard include/config/no/hz/full.h) \
    $(wildcard include/config/rcu/nocb/cpu.h) \
    $(wildcard include/config/tasks/rcu.h) \
    $(wildcard include/config/tree/rcu.h) \
    $(wildcard include/config/tiny/rcu.h) \
    $(wildcard include/config/debug/objects/rcu/head.h) \
    $(wildcard include/config/hotplug/cpu.h) \
    $(wildcard include/config/prove/rcu.h) \
    $(wildcard include/config/rcu/boost.h) \
    $(wildcard include/config/arch/weak/release/acquire.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/cpumask.h \
    $(wildcard include/config/cpumask/offstack.h) \
    $(wildcard include/config/debug/per/cpu/maps.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rcutree.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/workqueue.h \
    $(wildcard include/config/debug/objects/work.h) \
    $(wildcard include/config/freezer.h) \
    $(wildcard include/config/wq/watchdog.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/timer.h \
    $(wildcard include/config/debug/objects/timers.h) \
    $(wildcard include/config/no/hz/common.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/ktime.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/jiffies.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/timex.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/timex.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/param.h \
  arch/arm/include/generated/uapi/asm/param.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/param.h \
    $(wildcard include/config/hz.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/param.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/timex.h \
  include/generated/timeconst.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/timekeeping.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/timekeeping32.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/debugobjects.h \
    $(wildcard include/config/debug/objects.h) \
    $(wildcard include/config/debug/objects/free.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rcu_segcblist.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/srcutree.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rcu_node_tree.h \
    $(wildcard include/config/rcu/fanout.h) \
    $(wildcard include/config/rcu/fanout/leaf.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/completion.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/topology.h \
    $(wildcard include/config/use/percpu/numa/node/id.h) \
    $(wildcard include/config/sched/smt.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/smp.h \
    $(wildcard include/config/up/late/init.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/llist.h \
    $(wildcard include/config/arch/have/nmi/safe/cmpxchg.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/smp.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/percpu.h \
    $(wildcard include/config/need/per/cpu/embed/first/chunk.h) \
    $(wildcard include/config/need/per/cpu/page/first/chunk.h) \
    $(wildcard include/config/have/setup/per/cpu/area.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/percpu.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/percpu.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/percpu-defs.h \
    $(wildcard include/config/debug/force/weak/per/cpu.h) \
    $(wildcard include/config/virtualization.h) \
    $(wildcard include/config/amd/mem/encrypt.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/topology.h \
    $(wildcard include/config/arm/cpu/topology.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/arch_topology.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/topology.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/sysctl.h \
    $(wildcard include/config/sysctl.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rbtree.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/sysctl.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/elf.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/elf.h \
    $(wildcard include/config/vdso.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/auxvec.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/uapi/asm/auxvec.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/vdso_datapage.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/user.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/elf.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/elf-em.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/kobject.h \
    $(wildcard include/config/uevent/helper.h) \
    $(wildcard include/config/debug/kobject/release.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/sysfs.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/kernfs.h \
    $(wildcard include/config/kernfs.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/idr.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/radix-tree.h \
    $(wildcard include/config/radix/tree/multiorder.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/kobject_ns.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/kref.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/refcount.h \
    $(wildcard include/config/refcount/full.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/moduleparam.h \
    $(wildcard include/config/alpha.h) \
    $(wildcard include/config/ia64.h) \
    $(wildcard include/config/ppc64.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/jump_label.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rbtree_latch.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/error-injection.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/error-injection.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/tracepoint-defs.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/static_key.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/module.h \
    $(wildcard include/config/arm/unwind.h) \
    $(wildcard include/config/arm/module/plts.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/module.h \
    $(wildcard include/config/have/mod/arch/specific.h) \
    $(wildcard include/config/modules/use/elf/rel.h) \
    $(wildcard include/config/modules/use/elf/rela.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/i2c.h \
    $(wildcard include/config/i2c.h) \
    $(wildcard include/config/i2c/slave.h) \
    $(wildcard include/config/i2c/boardinfo.h) \
    $(wildcard include/config/i2c/mux.h) \
    $(wildcard include/config/of.h) \
    $(wildcard include/config/acpi.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/mod_devicetable.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/uuid.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/uuid.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/device.h \
    $(wildcard include/config/debug/devres.h) \
    $(wildcard include/config/generic/msi/irq/domain.h) \
    $(wildcard include/config/pinctrl.h) \
    $(wildcard include/config/generic/msi/irq.h) \
    $(wildcard include/config/dma/cma.h) \
    $(wildcard include/config/devtmpfs.h) \
    $(wildcard include/config/sysfs/deprecated.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/ioport.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/klist.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/pm.h \
    $(wildcard include/config/vt/console/sleep.h) \
    $(wildcard include/config/pm.h) \
    $(wildcard include/config/pm/clk.h) \
    $(wildcard include/config/pm/generic/domains.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/ratelimit.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/sched.h \
    $(wildcard include/config/virt/cpu/accounting/native.h) \
    $(wildcard include/config/sched/info.h) \
    $(wildcard include/config/schedstats.h) \
    $(wildcard include/config/fair/group/sched.h) \
    $(wildcard include/config/rt/group/sched.h) \
    $(wildcard include/config/cgroup/sched.h) \
    $(wildcard include/config/blk/dev/io/trace.h) \
    $(wildcard include/config/memcg/kmem.h) \
    $(wildcard include/config/compat/brk.h) \
    $(wildcard include/config/cgroups.h) \
    $(wildcard include/config/blk/cgroup.h) \
    $(wildcard include/config/stackprotector.h) \
    $(wildcard include/config/arch/has/scaled/cputime.h) \
    $(wildcard include/config/virt/cpu/accounting/gen.h) \
    $(wildcard include/config/posix/timers.h) \
    $(wildcard include/config/sysvipc.h) \
    $(wildcard include/config/detect/hung/task.h) \
    $(wildcard include/config/auditsyscall.h) \
    $(wildcard include/config/rt/mutexes.h) \
    $(wildcard include/config/ubsan.h) \
    $(wildcard include/config/block.h) \
    $(wildcard include/config/task/xacct.h) \
    $(wildcard include/config/cpusets.h) \
    $(wildcard include/config/intel/rdt.h) \
    $(wildcard include/config/futex.h) \
    $(wildcard include/config/compat.h) \
    $(wildcard include/config/perf/events.h) \
    $(wildcard include/config/rseq.h) \
    $(wildcard include/config/task/delay/acct.h) \
    $(wildcard include/config/fault/injection.h) \
    $(wildcard include/config/latencytop.h) \
    $(wildcard include/config/function/graph/tracer.h) \
    $(wildcard include/config/kcov.h) \
    $(wildcard include/config/uprobes.h) \
    $(wildcard include/config/bcache.h) \
    $(wildcard include/config/vmap/stack.h) \
    $(wildcard include/config/security.h) \
    $(wildcard include/config/arch/task/struct/on/stack.h) \
    $(wildcard include/config/debug/rseq.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/sched.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/pid.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rculist.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/sem.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/sem.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/ipc.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rhashtable-types.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/ipc.h \
  arch/arm/include/generated/uapi/asm/ipcbuf.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/ipcbuf.h \
  arch/arm/include/generated/uapi/asm/sembuf.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/sembuf.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/shm.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/shm.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/hugetlb_encode.h \
  arch/arm/include/generated/uapi/asm/shmbuf.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/shmbuf.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/shmparam.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/kcov.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/kcov.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/plist.h \
    $(wildcard include/config/debug/pi/list.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/hrtimer.h \
    $(wildcard include/config/high/res/timers.h) \
    $(wildcard include/config/time/low/res.h) \
    $(wildcard include/config/timerfd.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/timerqueue.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/seccomp.h \
    $(wildcard include/config/seccomp.h) \
    $(wildcard include/config/have/arch/seccomp/filter.h) \
    $(wildcard include/config/seccomp/filter.h) \
    $(wildcard include/config/checkpoint/restore.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/seccomp.h \
  arch/arm/include/generated/asm/seccomp.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/seccomp.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/unistd.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/unistd.h \
    $(wildcard include/config/oabi/compat.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/uapi/asm/unistd.h \
  arch/arm/include/generated/uapi/asm/unistd-eabi.h \
  arch/arm/include/generated/uapi/asm/unistd-common.h \
  arch/arm/include/generated/asm/unistd-nr.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/resource.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/resource.h \
  arch/arm/include/generated/uapi/asm/resource.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/resource.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/resource.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/latencytop.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/sched/prio.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/signal_types.h \
    $(wildcard include/config/old/sigaction.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/signal.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/signal.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/uapi/asm/signal.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/signal-defs.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/uapi/asm/sigcontext.h \
  arch/arm/include/generated/uapi/asm/siginfo.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/siginfo.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/mm_types_task.h \
    $(wildcard include/config/arch/want/batched/unmap/tlb/flush.h) \
    $(wildcard include/config/split/ptlock/cpus.h) \
    $(wildcard include/config/arch/enable/split/pmd/ptlock.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/task_io_accounting.h \
    $(wildcard include/config/task/io/accounting.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/rseq.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/overflow.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/device.h \
    $(wildcard include/config/dmabounce.h) \
    $(wildcard include/config/iommu/api.h) \
    $(wildcard include/config/arm/dma/use/iommu.h) \
    $(wildcard include/config/xen.h) \
    $(wildcard include/config/arch/omap.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/pm_wakeup.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rtmutex.h \
    $(wildcard include/config/debug/rt/mutexes.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/irqdomain.h \
    $(wildcard include/config/irq/domain/hierarchy.h) \
    $(wildcard include/config/generic/irq/debugfs.h) \
    $(wildcard include/config/irq/domain.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/irqhandler.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/of.h \
    $(wildcard include/config/of/dynamic.h) \
    $(wildcard include/config/sparc.h) \
    $(wildcard include/config/of/promtree.h) \
    $(wildcard include/config/of/kobj.h) \
    $(wildcard include/config/of/numa.h) \
    $(wildcard include/config/of/overlay.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/property.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/fwnode.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/i2c.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/fs.h \
    $(wildcard include/config/fs/posix/acl.h) \
    $(wildcard include/config/cgroup/writeback.h) \
    $(wildcard include/config/ima.h) \
    $(wildcard include/config/fsnotify.h) \
    $(wildcard include/config/fs/encryption.h) \
    $(wildcard include/config/epoll.h) \
    $(wildcard include/config/file/locking.h) \
    $(wildcard include/config/quota.h) \
    $(wildcard include/config/fs/dax.h) \
    $(wildcard include/config/mandatory/file/locking.h) \
    $(wildcard include/config/migration.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/wait_bit.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/kdev_t.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/kdev_t.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/dcache.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rculist_bl.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/list_bl.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/bit_spinlock.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/lockref.h \
    $(wildcard include/config/arch/use/cmpxchg/lockref.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/stringhash.h \
    $(wildcard include/config/dcache/word/access.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/hash.h \
    $(wildcard include/config/have/arch/hash.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/path.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/list_lru.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/shrinker.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/xarray.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/mm_types.h \
    $(wildcard include/config/have/aligned/struct/page.h) \
    $(wildcard include/config/userfaultfd.h) \
    $(wildcard include/config/have/arch/compat/mmap/bases.h) \
    $(wildcard include/config/membarrier.h) \
    $(wildcard include/config/aio.h) \
    $(wildcard include/config/mmu/notifier.h) \
    $(wildcard include/config/hmm.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/auxvec.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/auxvec.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/uprobes.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/mmu.h \
    $(wildcard include/config/cpu/has/asid.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/capability.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/capability.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/semaphore.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/fcntl.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/fcntl.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/uapi/asm/fcntl.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/fcntl.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/fiemap.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/migrate_mode.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/percpu-rwsem.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rcuwait.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/rcu_sync.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/delayed_call.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/errseq.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/ioprio.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/sched/rt.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/iocontext.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/fs.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/limits.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/ioctl.h \
  arch/arm/include/generated/uapi/asm/ioctl.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/ioctl.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/asm-generic/ioctl.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/quota.h \
    $(wildcard include/config/quota/netlink/interface.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/percpu_counter.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/dqblk_xfs.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/dqblk_v1.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/dqblk_v2.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/dqblk_qtree.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/projid.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/quota.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/nfs_fs_i.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/miscdevice.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/uapi/linux/major.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/uaccess.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/uaccess.h \
    $(wildcard include/config/cpu/sw/domain/pan.h) \
    $(wildcard include/config/cpu/use/domains.h) \
    $(wildcard include/config/uaccess/with/memcpy.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/domain.h \
    $(wildcard include/config/io/36.h) \
    $(wildcard include/config/cpu/cp15/mmu.h) \
  arch/arm/include/generated/asm/extable.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/asm-generic/extable.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/slab.h \
    $(wildcard include/config/debug/slab.h) \
    $(wildcard include/config/failslab.h) \
    $(wildcard include/config/have/hardened/usercopy/allocator.h) \
    $(wildcard include/config/slab.h) \
    $(wildcard include/config/slub.h) \
    $(wildcard include/config/slob.h) \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/kasan.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/include/linux/delay.h \
  /home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/kernel/ebf_linux_kernel_6ull_depth1/arch/arm/include/asm/delay.h \

/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/drivers/i2c_sensors/mpu6050_raw.o: $(deps_/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/drivers/i2c_sensors/mpu6050_raw.o)

$(deps_/home/rickyduran/Desktop/EdgeGuard-6ULL/EdgeGard-6ULL/drivers/i2c_sensors/mpu6050_raw.o):
