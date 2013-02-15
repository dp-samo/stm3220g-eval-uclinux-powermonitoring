#include <linux/fs.h>                                                                                                                                                  
#include <linux/hugetlb.h>                                                                                                                  
#include <linux/init.h>                                                                                                                     
#include <linux/kernel.h>                                                                                                                   
#include <linux/mm.h>                                                                                                                       
#include <linux/mman.h>                                                                                                                     
#include <linux/mmzone.h>                                                                                                                   
#include <linux/proc_fs.h>                                                                                                                  
#include <linux/quicklist.h>                                                                                                                
#include <linux/seq_file.h>                                                                                                                 
#include <linux/swap.h>                                                                                                                     
#include <linux/vmstat.h>                                                                                                                   
#include <asm/atomic.h>                                                                                                                     
#include <asm/page.h>                                                                                                                       
#include <asm/pgtable.h>  

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/kernel_stat.h>  
#include <asm/cputime.h> 

#include <linux/uaccess.h>
#include <linux/delay.h>

#ifndef arch_idle_time                                                                                                                      
#define arch_idle_time(cpu) 0                                                                                                               
#endif    

#define MAX_PROC_SIZE 16
#define CM_FREQ 1

typedef unsigned long long ULL;

typedef enum { USER, NICE, SYSTEM, IDLE } Actor ;

long memcpu_freq;

//

static ULL actor_usage(Actor actor, int cpu) {

    ULL res;

    cputime64_t clk_actor_usage = cputime64_zero;

    switch (actor) {

        case USER :
            clk_actor_usage = cputime64_add(clk_actor_usage, kstat_cpu(cpu).cpustat.user);
            break;

        case NICE :
            clk_actor_usage = cputime64_add(clk_actor_usage, kstat_cpu(cpu).cpustat.nice);
            break;

        case SYSTEM :
            clk_actor_usage = cputime64_add(clk_actor_usage, kstat_cpu(cpu).cpustat.system);
            break;

        case IDLE :
            clk_actor_usage = cputime64_add(clk_actor_usage, kstat_cpu(cpu).cpustat.idle);
            clk_actor_usage = cputime64_add(clk_actor_usage, arch_idle_time(i));
            break;
    }
    
    return (ULL) cputime64_to_clock_t(clk_actor_usage);
            
}

#define K(x) ((x) << (PAGE_SHIFT - 10))

static int printing_mem_usage(void* data) {

    struct sysinfo i;
    si_meminfo(&i);

    printk("MemFree : %5lu Kb", K(i.freeram));
                                    
    return 0;
}

//

static int printing_cpu_usage(void *data) {

   ULL cpu_usage = 0;

   ULL user_1, nice_1, system_1, idle_1;
   ULL user_2, nice_2, system_2, idle_2;

   user_1 = actor_usage(USER, 0) ; nice_1 = actor_usage(NICE, 0) ; 
   system_1 = actor_usage(SYSTEM, 0); idle_1 = actor_usage(IDLE, 0); 

   schedule_timeout_interruptible(HZ / memcpu_freq);

   user_2 = actor_usage(USER, 0) ; nice_2 = actor_usage(NICE, 0) ; 
   system_2 = actor_usage(SYSTEM, 0); idle_2 = actor_usage(IDLE, 0); 

   ULL dividend = user_2 + nice_2 + system_2 - user_1 - nice_1 - system_1;
   ULL divisor  = user_2 + system_2 + nice_2 + idle_2 - user_1 - nice_1 - system_1 - idle_1;

   cpu_usage = div64_u64(100 * dividend, divisor);

   printk("CPU: %%%llu", cpu_usage);

   return 0;
}

//

int cpumem_print_usage(void * data) {

    printing_cpu_usage(data);
//    printk(" | ");
//    printing_mem_usage(data);
    printk("\n");
 
}

EXPORT_GPL(cpumem_print_usage);


//

static int printing_usage_task(void *data) {

    while (!kthread_should_stop()) 
        cpumem_print_usage(data);


    return 0;
}

//

static struct proc_dir_entry *proc_write_entry;

static int read_proc(char *buf,char **start,off_t offset,int count,int *eof,void *data )
{
    int len=0;
    len = sprintf(buf,"%d", memcpu_freq);

    return len;
}

//

static int write_proc(struct file *file,const char *buf,int count,void *data )
{

    char proc_data[MAX_PROC_SIZE];
    long cnv;

    if(count > MAX_PROC_SIZE)
            count = MAX_PROC_SIZE;

    if(copy_from_user(proc_data, buf, count))
            return -EFAULT;

    if ( ! strict_strtol(proc_data, 10, &cnv) )
        memcpu_freq = cnv;

    return count;
}

//

static void create_new_proc_entry()
{
    proc_write_entry = create_proc_entry("cpumem_freq",0666,NULL);

    if(!proc_write_entry)
    {
        printk(KERN_INFO "Error creating proc entry");
        return -ENOMEM;
    }
    proc_write_entry->read_proc = read_proc ;
    proc_write_entry->write_proc = write_proc;

}


//

static int __init cpumem_usage_init_module(void)
{
    printk(KERN_INFO "Entering cpumem_usage module\n");

    memcpu_freq = CM_FREQ;
    create_new_proc_entry();

    kthread_run(printing_usage_task, NULL, "usage_task");
    return 0;
}

//

static void __exit cpumem_usage_cleanup_module(void)
{
    printk(KERN_INFO "Exiting cpumem_usage module..\n");
}

//

module_init(cpumem_usage_init_module);
module_exit(cpumem_usage_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A. ELLEUCH (DOCEA)");
MODULE_DESCRIPTION("Printing usage of CPU/MEM in kernel log");

