#include <linux/fs.h>                                                                                                                                                  
#include <linux/init.h>                                                                                                                     
#include <linux/kernel.h>                                                                                                                   
#include <linux/proc_fs.h>                                                                                                                  
#include <linux/quicklist.h>                                                                                                                
#include <linux/seq_file.h>                                                                                                                 
#include <asm/atomic.h>                                                                                                                     

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include "coretftfb.h"


//

#define MAX_PROC_BUFFER_SIZE    16

//

#define LCD_SPI_FRAME_RATE_80HZ     0x0010
#define LCD_SPI_FRAME_RATE_90HZ     0x0000
#define LCD_SPI_FRAME_RATE_100HZ    0x0030
#define LCD_SPI_FRAME_RATE_110HZ    0x0020

#define LCD_PARALLEL_FRAME_RATE_30HZ 0b0000
#define LCD_PARALLEL_FRAME_RATE_40HZ 0b0101
#define LCD_PARALLEL_FRAME_RATE_51HZ 0b1000
#define LCD_PARALLEL_FRAME_RATE_62HZ 0b1010
#define LCD_PARALLEL_FRAME_RATE_70HZ 0b1011

#define LCD_FRAME_RATE_REG_DEFAULT 0x0000

//

#define DISPLAY_CONTROL_1_REG 0x07

#define DISPLAY_CONTROL_1_DTE 4
#define DISPLAY_CONTROL_1_GON 5
#define DISPLAY_CONTROL_1_D_0 0
#define DISPLAY_CONTROL_1_D_1 1


//

extern int power_state;

//

static long frame_rate_freq;
static long standby_mode;


static int set_frame_rate_freq(long freq) {

    long new_freq = LCD_FRAME_RATE_REG_DEFAULT;

    switch (freq) {
        
        // SPI LCD
        
        case 80:
            new_freq |= LCD_SPI_FRAME_RATE_80HZ;
            break;
        case 90:
            new_freq |= LCD_SPI_FRAME_RATE_90HZ;
            break;
        case 100:
            new_freq |= LCD_SPI_FRAME_RATE_100HZ;
            break;
        case 110:
            new_freq |= LCD_SPI_FRAME_RATE_110HZ;
            break;

        // Parallel LCD             

        case 30:
            new_freq |= LCD_PARALLEL_FRAME_RATE_30HZ;
            break;

        case 40:
            new_freq |= LCD_PARALLEL_FRAME_RATE_40HZ;
            break;

        case 51:
            new_freq |= LCD_PARALLEL_FRAME_RATE_51HZ;
            break;

        case 62:
            new_freq |= LCD_PARALLEL_FRAME_RATE_62HZ;
            break;

        case 70:
            new_freq |= LCD_PARALLEL_FRAME_RATE_70HZ;
            break;



        default:
            printk(KERN_INFO "Setting invalid frame rate frequency..");
            goto error;
    }

    LCD_REG = 0x2B;
    LCD_RAM = new_freq;

    frame_rate_freq = new_freq;

    return 0;

error:
    return -1;
}


#define SET_BIT(RANGE, VAL) \
            VAL |= 1 << RANGE;

#define CLR_BIT(RANGE, VAL) \
            VAL &= ~(1 << RANGE)


static int set_lcd_standby_mode(long std_mode) {

    if (std_mode) {
        LCD_REG = 0x07; LCD_RAM = 0x0173; 
    } else {
        LCD_REG = 0x07; LCD_RAM = 0;
    }
}



static int set_lcd_standby_mode_obselete(long std_mode) {

    if (std_mode) {
        
        /* Set standby mode :
         * 1) GON = 1, DTE = 1, D[1:0] = 01
         * 2) Waiting at least two frames
         * 3) GON = 1, DTE = 1, D[1:0] = 00
         * 4) Waiting at least two frames
         * 5) GON = 0, DTE = 0, D[1:0] = 00
         * 6) SAP = 0, AP[2:0] = 000, PON = 0
         * 7) STB = 0
         */

        LCD_REG = 0x07;

        CLR_BIT(1, LCD_RAM); 
        CLR_BIT(0, LCD_RAM);

        CLR_BIT(5, LCD_RAM); 

        CLR_BIT(4, LCD_RAM); 
    
        LCD_REG = 0x10;
        
        CLR_BIT(12, LCD_RAM); 
        CLR_BIT(6, LCD_RAM); 
        CLR_BIT(5, LCD_RAM); 
        CLR_BIT(4, LCD_RAM); 
        CLR_BIT(7, LCD_RAM);
        
        LCD_REG = 0x12;
        CLR_BIT(4, LCD_RAM);
    
        LCD_REG = 0x10;
        
        SET_BIT(0, LCD_RAM);

    } else {

        /* Set Standby Off
         * 1) STB = 0
         * 2) R10 <- 0190h
         * 3) Waiting at least 80 ms
         * 4) SAP = 1
         * 5) GON = 0, DTE = 0, D[1:0] = 01
         * 6) Waiting at least 2 frames
         * 7) GON = 1, DTE = 0, D[1:0] = 01
         * 8) GON = 1, DTE = 0, D[1:0] = 11
         * 8) Waiting at least two frames
         * 9) GON = 1, DTE = 1, D[1:0] = 11
         */

        LCD_REG = 0x10;
        CLR_BIT(0, LCD_RAM);
        
        LCD_REG = 0x10;
        LCD_RAM = 0x0190;

        msleep(100);
        
        SET_BIT(12, LCD_RAM);

        LCD_REG = 0x07;

        SET_BIT(0, LCD_RAM);
        msleep(70);
        SET_BIT(5, LCD_RAM);
        SET_BIT(1, LCD_RAM);
        msleep(70);
        SET_BIT(4, LCD_RAM);



    }
    
    return 0;
}


static struct proc_dir_entry *proc_frame_rate_entry;

static int read_frame_rate_proc(char *buf,char **start,off_t offset,int count,int *eof,void *data )
{
    int len=0;
    len = sprintf(buf,"%ld", frame_rate_freq);

    return len;
}

//

static int write_frame_rate_proc(struct file *file,const char *buf,int count,void *data )
{

    char proc_data[MAX_PROC_BUFFER_SIZE];
    long cnv;

    if(count > MAX_PROC_BUFFER_SIZE)
            count = MAX_PROC_BUFFER_SIZE;

    if(copy_from_user(proc_data, buf, count))
            return -EFAULT;

    if ( ! strict_strtol(proc_data, 10, &cnv) )
        set_frame_rate_freq(cnv);

    return count;
}

static int create_frame_rate_freq_proc_entry()
{
    proc_frame_rate_entry = create_proc_entry("frame_rate_freq",0666,NULL);

    if(!proc_frame_rate_entry)
    {
        printk(KERN_INFO "Error creating proc entry");
        return -ENOMEM;
    }
    proc_frame_rate_entry->read_proc = read_frame_rate_proc ;
    proc_frame_rate_entry->write_proc = write_frame_rate_proc;

    return 0;
}


//

static struct proc_dir_entry *proc_standby_mode_entry;

static int read_standby_mode_proc(char *buf,char **start,off_t offset,int count,int *eof,void *data )
{
    int len=0;
    len = sprintf(buf,"%ld", standby_mode);

    return len;
}

//

static int write_standby_mode_proc(struct file *file,const char *buf,int count,void *data )
{

    char proc_data[MAX_PROC_BUFFER_SIZE];
    long cnv;

    if(count > MAX_PROC_BUFFER_SIZE)
            count = MAX_PROC_BUFFER_SIZE;

    if(copy_from_user(proc_data, buf, count))
            return -EFAULT;

    if ( ! strict_strtol(proc_data, 10, &cnv) )
        set_lcd_standby_mode(cnv);

    return count;
}

static int create_standby_mode_proc_entry()
{
    proc_standby_mode_entry = create_proc_entry("standby_mode",0666,NULL);

    if(!proc_standby_mode_entry)
    {
        printk(KERN_INFO "Error creating proc entry");
        return -ENOMEM;
    }
    proc_standby_mode_entry->read_proc = read_standby_mode_proc ;
    proc_standby_mode_entry->write_proc = write_standby_mode_proc;

    return 0;

}


//

static int __init proc_init_module(void)
{
    
    create_frame_rate_freq_proc_entry();
    create_standby_mode_proc_entry();

    return 0;
}

//

static void __exit proc_cleanup_module(void)
{
    printk(KERN_INFO "Exiting cpumem_usage module..\n");
}

//

module_init(proc_init_module);
module_exit(proc_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Doceapower, doceapower@doceapower.com");
MODULE_DESCRIPTION("Adjust LCD screen frame rate");

