/*
 * Copyright 2003 PathScale, Inc.
 * Copyright (C) 2003 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 *
 * Licensed under the GPL
 */

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/regset.h>
#define __FRAME_OFFSETS
#include <asm/ptrace.h>
#include <linux/uaccess.h>
#include <registers.h>
#include <asm/ptrace-abi.h>

/*
 * determines which flags the user has access to.
 * 1 = access 0 = no access
 */
#define FLAG_MASK 0x44dd5UL

static const int reg_offsets[] =
{
	[R8 >> 3] = HOST_R8,
	[R9 >> 3] = HOST_R9,
	[R10 >> 3] = HOST_R10,
	[R11 >> 3] = HOST_R11,
	[R12 >> 3] = HOST_R12,
	[R13 >> 3] = HOST_R13,
	[R14 >> 3] = HOST_R14,
	[R15 >> 3] = HOST_R15,
	[RIP >> 3] = HOST_IP,
	[RSP >> 3] = HOST_SP,
	[RAX >> 3] = HOST_AX,
	[RBX >> 3] = HOST_BX,
	[RCX >> 3] = HOST_CX,
	[RDX >> 3] = HOST_DX,
	[RSI >> 3] = HOST_SI,
	[RDI >> 3] = HOST_DI,
	[RBP >> 3] = HOST_BP,
	[CS >> 3] = HOST_CS,
	[SS >> 3] = HOST_SS,
	[FS_BASE >> 3] = HOST_FS_BASE,
	[GS_BASE >> 3] = HOST_GS_BASE,
	[DS >> 3] = HOST_DS,
	[ES >> 3] = HOST_ES,
	[FS >> 3] = HOST_FS,
	[GS >> 3] = HOST_GS,
	[EFLAGS >> 3] = HOST_EFLAGS,
	[ORIG_RAX >> 3] = HOST_ORIG_AX,
};

int putreg(struct task_struct *child, int regno, unsigned long value)
{
	switch (regno) {
	case R8:
	case R9:
	case R10:
	case R11:
	case R12:
	case R13:
	case R14:
	case R15:
	case RIP:
	case RSP:
	case RAX:
	case RBX:
	case RCX:
	case RDX:
	case RSI:
	case RDI:
	case RBP:
		break;

	case ORIG_RAX:
		/* Update the syscall number. */
		UPT_SYSCALL_NR(&child->thread.regs.regs) = value;
		break;

	case FS:
	case GS:
	case DS:
	case ES:
	case SS:
	case CS:
		if (value && (value & 3) != 3)
			return -EIO;
		value &= 0xffff;
		break;

	case FS_BASE:
	case GS_BASE:
		if (!((value >> 48) == 0 || (value >> 48) == 0xffff))
			return -EIO;
		break;

	case EFLAGS:
		value &= FLAG_MASK;
		child->thread.regs.regs.gp[HOST_EFLAGS] |= value;
		return 0;

	default:
		panic("Bad register in putreg(): %d\n", regno);
	}

	child->thread.regs.regs.gp[reg_offsets[regno >> 3]] = value;
	return 0;
}

int poke_user(struct task_struct *child, long addr, long data)
{
	if ((addr & 3) || addr < 0)
		return -EIO;

	if (addr < MAX_REG_OFFSET)
		return putreg(child, addr, data);
	else if ((addr >= offsetof(struct user, u_debugreg[0])) &&
		(addr <= offsetof(struct user, u_debugreg[7]))) {
		addr -= offsetof(struct user, u_debugreg[0]);
		addr = addr >> 3;
		if ((addr == 4) || (addr == 5))
			return -EIO;
		child->thread.arch.debugregs[addr] = data;
		return 0;
	}
	return -EIO;
}

unsigned long getreg(struct task_struct *child, int regno)
{
	unsigned long mask = ~0UL;

	switch (regno) {
	case R8:
	case R9:
	case R10:
	case R11:
	case R12:
	case R13:
	case R14:
	case R15:
	case RIP:
	case RSP:
	case RAX:
	case RBX:
	case RCX:
	case RDX:
	case RSI:
	case RDI:
	case RBP:
	case ORIG_RAX:
	case EFLAGS:
	case FS_BASE:
	case GS_BASE:
		break;
	case FS:
	case GS:
	case DS:
	case ES:
	case SS:
	case CS:
		mask = 0xffff;
		break;
	default:
		panic("Bad register in getreg: %d\n", regno);
	}
	return mask & child->thread.regs.regs.gp[reg_offsets[regno >> 3]];
}

int peek_user(struct task_struct *child, long addr, long data)
{
	/* read the word at location addr in the USER area. */
	unsigned long tmp;

	if ((addr & 3) || addr < 0)
		return -EIO;

	tmp = 0;  /* Default return condition */
	if (addr < MAX_REG_OFFSET)
		tmp = getreg(child, addr);
	else if ((addr >= offsetof(struct user, u_debugreg[0])) &&
		(addr <= offsetof(struct user, u_debugreg[7]))) {
		addr -= offsetof(struct user, u_debugreg[0]);
		addr = addr >> 2;
		tmp = child->thread.arch.debugregs[addr];
	}
	return put_user(tmp, (unsigned long *) data);
}

long subarch_ptrace(struct task_struct *child, long request,
		    unsigned long addr, unsigned long data)
{
	int ret = -EIO;
	void __user *datap = (void __user *) data;

	switch (request) {
	case PTRACE_GETFPREGS: /* Get the child FPU state. */
		return copy_regset_to_user(child, task_user_regset_view(child),
					   REGSET_FP,
					   0, sizeof(struct user_i387_struct),
					   datap);
	case PTRACE_SETFPREGS: /* Set the child FPU state. */
		return copy_regset_from_user(child, task_user_regset_view(child),
					     REGSET_FP,
					     0, sizeof(struct user_i387_struct),
					     datap);
	case PTRACE_ARCH_PRCTL:
		/* XXX Calls ptrace on the host - needs some SMP thinking */
		ret = arch_prctl(child, data, (void __user *) addr);
		break;
	}

	return ret;
}
