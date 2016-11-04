#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <array.h>
#include <synch.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);
  
    /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);
  
#if OPT_A2
	lock_acquire(p->p_exit_lk);
	p->p_exitstatus=_MKWAIT_EXIT(exitcode);
	p->p_exited=true;
	cv_broadcast(p->p_exit_cv,p->p_exit_lk);
	lock_release(p->p_exit_lk);
#endif // OPT_A2

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
#if OPT_A2
	*retval = curproc->p_id;
#else
	/* for now, this is just a stub that always returns a PID of 1 */
	/* you need to fix this to make it work properly */
	*retval = 1;	
#endif /* OPT_A2 */
	return(0);

}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
#if OPT_A2
	struct proc * target=find_child((struct proc*)curproc,pid);
	if (target==NULL){
		return(ECHILD);
	}
	if (!pid_in_use(pid)){
		return(ESRCH);
	}
	if (status==NULL){
		return(EFAULT);
	}
	
	lock_acquire(target->p_exit_lk);
	if (!target->p_exited){
		cv_wait(target->p_exit_cv,target->p_exit_lk);
	}
	exitstatus=target->p_exitstatus;
	lock_release(target->p_exit_lk);
#else

  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
#endif
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2
int sys_fork(struct trapframe *tf, pid_t *retval){
	int err;
	struct proc *child = proc_create_runprogram(curproc->p_name);
	if (child==NULL){
		return(ENOMEM);
	}
	
	struct addrspace * childspace=NULL;
	err=as_copy(curproc->p_addrspace,&childspace);
	if (err!=0){
		proc_destroy(child);
		return(err);
	}
	if (childspace==NULL){
		proc_destroy(child);
		return(ENOMEM);
	}
	child->p_addrspace=childspace;
	
	child->p_parent=curproc;
	err=add_child(curproc,child);
	if (err!=0){
		kfree(childspace);
		proc_destroy(child);
		return(err);
	}
	struct trapframe * child_tf=kmalloc(sizeof(struct trapframe));
	memcpy(child_tf,tf,sizeof(struct trapframe));
	DEBUG(DB_SYSCALL, "Inside sys_fork:copied trapframe\n");
	err=thread_fork("child_thread",child,&enter_forked_process,(void *)child_tf,0);
	//err=thread_fork(curthread->t_name,child,&enter_forked_process,(void *)tf,0);
	if (err!=0){
		DEBUG(DB_SYSCALL, "Inside sys_fork:error occurs!\n");
		kfree(child_tf);
		kfree(childspace);
		proc_destroy(child);
		return(err);
	}
	//kprintf("fork: child pid is%d",child->p_id);
	*retval=child->p_id;
	return(0);
}
#endif

