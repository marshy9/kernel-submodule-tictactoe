#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/random.h>


#define DRIVER_AUTHOR "Marshall"
#define DRIVER_DESC "Tictactoe driver"

MODULE_LICENSE("GPL");

#define MODULE_NAME "tictactoe"


static int tictactoe_open(struct inode*, struct file*);
static int tictactoe_release(struct inode*, struct file*);
static ssize_t tictactoe_read(struct file*, char*, size_t, loff_t*);
static ssize_t tictactoe_write(struct file*, const char*, size_t, loff_t*);

static dev_t device_number;
static struct cdev the_cdev;
static struct class* the_class = NULL;
static char msg[256];
static int msg_len;
static char brd[10];
static int usr;
static int start = 0;
static int turn;


static struct file_operations fops = {//just contain pointers to functions
    .owner = THIS_MODULE,
    .read =  tictactoe_read,
    .write = tictactoe_write,
    .open = tictactoe_open,
    .release = tictactoe_release
};

static struct miscdevice tictactoe = {
    .name = "tictactoe",
    .mode = 0666,
    .fops = &fops
};

static int __init tictactoe_init(void){
    //Start game
    printk(KERN_INFO "Module Loaded\n");
    
    //This will show under cat /proc/devices
    if(alloc_chrdev_region(&device_number, 0, 1, MODULE_NAME) <0){
        goto r_return;
    }
    //This will show up under ls sys/class
    the_class = class_create(THIS_MODULE, MODULE_NAME);
    if( the_class == NULL){
        goto r_class;
    }

    //This will show up under /dev
    if(misc_register(&tictactoe)){
        goto r_device;
    }

    cdev_init(&the_cdev, &fops);

    if(cdev_add(&the_cdev, device_number, 1) <0) {
        goto r_cdev;
    }
   
    return 0;

    r_cdev:
        device_destroy(the_class, device_number);
        cdev_del(&the_cdev);
    r_device:
        misc_deregister(&tictactoe);
    r_class:
        unregister_chrdev_region(device_number, 1);
    r_return:
        return -1;

}

static void __exit tictactoe_exit(void){
    printk(KERN_INFO "Module Unloaded\n");
    misc_deregister(&tictactoe);
    cdev_del(&the_cdev);
    class_destroy(the_class);
    unregister_chrdev_region(device_number, 1);
}

static int tictactoe_open(struct inode *pinode, struct file *pfile) {
   printk(KERN_INFO "Tic-Tac-Toe device opened\n");
   return 0;
}

static ssize_t tictactoe_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset) {
   int errors;

   printk(KERN_INFO "writing\n");

/*   msg_len = length;*/
/*   memcpy(msg,buffer,length* sizeof(char));*/

   errors = copy_from_user(msg,buffer,length);

   printk(KERN_INFO "msg:%s\n", msg);
   if (errors < 0)
	return(-EINVAL);

   return length;
}

static ssize_t tictactoe_read(struct file *pfile, char __user *buffer, size_t length, loff_t *offset) {
   int errors, row = 0, col = 0, winval = 0, i, j;
   unsigned int rand;


   if(msg[0] == '0' && msg[1] == '0' && msg[3] == 'X'){ //New game
	printk(KERN_INFO "selected X\n");
	usr = 1; //sets usr to X
	turn = 1;
	start = 1;
	memcpy(brd,"*********\n",10* sizeof(char));
	errors = copy_to_user(buffer,"OK\n",3);
	if (errors < 0)
		return(-EINVAL);
	return 3;
   }
   else if(msg[0] == '0' && msg[1] == '0' && msg[3] == 'O'){ //New game
	printk(KERN_INFO "selected O\n");
	usr = 0; //sets usr to O
	turn = 0;
	start = 1;
	memcpy(brd,"*********\n",10* sizeof(char));
	errors = copy_to_user(buffer,"OK\n",3);
	if (errors < 0)
		return(-EINVAL);
	return 3;
   }

   if(msg[0] == '0' && msg[1] == '1'){ //Display
        printk(KERN_INFO "reading display\n");
	errors = copy_to_user(buffer, brd, 10);
	if (errors < 0)
		return(-EINVAL);
	return 10;
   }


   else if(msg[0] == '0' && msg[1] == '2'){

	col = msg[3] - '0';//convert to int
	row = msg[5] - '0';
	printk(KERN_INFO "|USER MOVE| col: %d, row: %d\n", col, row);
	if (start == 0){
		errors = copy_to_user(buffer,"NOGAME\n",7);
   		if (errors < 0)
			return(-EINVAL);
		return 7;
	}
	if (turn == 0){
		errors = copy_to_user(buffer,"OOT\n",4);
   		if (errors < 0)
			return(-EINVAL);
		return 4;
	}

	else if (row > 2 || row < 0 || col > 2 || col < 0){
		errors = copy_to_user(buffer,"INVFMT\n",7);
   		if (errors < 0)
			return(-EINVAL);
		return 7;
	}

	else if (brd[3*row + col] != '*' ){
		errors = copy_to_user(buffer,"ILLMOVE\n",8);
   		if (errors < 0)
			return(-EINVAL);
		return 8;
	}
	

	printk(KERN_INFO "executing move on this brd: %s\n", brd);
	if(usr == 1)
		brd[3 * row + col] = 'X';
	else
		brd[3 * row + col] = 'O';

	winval = 0;
	turn = 0; //set back to computer turn

	//check tie
	if (brd[3 * 0 + 0] != '*' && brd[3 * 1 + 0] != '*' && 
	    brd[3 * 2 + 0] != '*' && brd[3 * 0 + 1] != '*' &&
	    brd[3 * 1 + 1] != '*' && brd[3 * 2 + 1] != '*' &&
	    brd[3 * 0 + 2] != '*' && brd[3 * 1 + 2] != '*' &&
	    brd[3 * 2 + 2] != '*')
		winval = 2;

	//check rows
	for (i = 0; i < 3; i++){
		if (brd[3 * 0 + i] == brd[3 * 1 + i] &&
		    brd[3 * 1 + i] == brd[3 * 2 + i] &&
		    brd[3 * 0 + i] != '*')
			winval = 1;

	}
	//check cols
	for (i = 0; i < 3; i++){
		if (brd[3 * i + 0] == brd[3 * i + 1] &&
		    brd[3 * i + 1] == brd[3 * i + 2] &&
		    brd[3 * i + 0] != '*')
			winval = 1;
	}
	//check diagonals
	for (i = 0; i < 3; i++){
		if (brd[3 * 0 + 0] == brd[3 * 1 + 1] &&
		    brd[3 * 1 + 1] == brd[3 * 2 + 2] &&
		    brd[3 * 0 + 0] != '*')
			winval = 1;
	}
	for (i = 0; i < 3; i++){
		if (brd[3 * 0 + 2] == brd[3 * 1 + 1] &&
		    brd[3 * 1 + 1] == brd[3 * 2 + 0] &&
		    brd[3 * 0 + 2] != '*')
			winval = 1;
	}
	if (winval == 1){
		errors = copy_to_user(buffer,"WIN\n",4);
   		if (errors < 0)
			return(-EINVAL);
		return 4;
	}
	if (winval == 2){
		errors = copy_to_user(buffer,"TIE\n",4);
   		if (errors < 0)
			return(-EINVAL);
		return 4;
	}
	else{
		errors = copy_to_user(buffer,"OK\n",3);
   		if (errors < 0)
			return(-EINVAL);
		return 3;	
	}
   }

   else if(msg[0] == '0' && msg[1] == '3'){

	if (turn == 1){
		errors = copy_to_user(buffer,"OOT\n",4);
   		if (errors < 0)
			return(-EINVAL);
		return 4;
	}

//this code used to generate a random cpu move but was removed due to 
//some errors that were unresolved

	get_random_bytes(&rand,sizeof(rand));
	rand%= 2;
	col = rand;//convert to int
	get_random_bytes(&rand,sizeof(rand));
	rand%= 2;
	row = rand;

/*	if(brd[3 * row + col] != '*'){*/
/*		for (i = 0; i < 3; i++){*/
/*			for (j = 0; j < 3; j++){*/
/*				if (brd[3 * i + j] == '*'){*/
/*					col = i;*/
/*					row = j;*/
/*					printk(KERN_INFO "|log| col: %d, row: %d\n", col, row);*/
/*					break;*/
/*				}*/
/*			}*/
/*		}*/
/*	}*/

//replacement code
	j = 0;
	for(i = 0; i<10; i++){
		if (brd[i] == '*'){
			if(usr == 1)
				brd[i] = 'O';
			else
				brd[i] = 'X';
			j++;
		}
		if (j>0)
			break;
	}

	printk(KERN_INFO "|COMP MOVE| col: %d, row: %d\n", col, row);

	printk(KERN_INFO "executing move on this brd: %s\n", brd);

/*	if(usr == 1)*/
/*		brd[3 * row + col] = 'O';*/
/*	else*/
/*		brd[3 * row + col] = 'X';*/

	winval = 0;
	turn = 1; //set back to user turn


	//check tie
	if (brd[3 * 0 + 0] != '*' && brd[3 * 1 + 0] != '*' && 
	    brd[3 * 2 + 0] != '*' && brd[3 * 0 + 1] != '*' &&
	    brd[3 * 1 + 1] != '*' && brd[3 * 2 + 1] != '*' &&
	    brd[3 * 0 + 2] != '*' && brd[3 * 1 + 2] != '*' &&
	    brd[3 * 2 + 2] != '*')
		winval = 2;

	//check rows
	for (i = 0; i < 3; i++){
		if (brd[3 * 0 + i] == brd[3 * 1 + i] &&
		    brd[3 * 1 + i] == brd[3 * 2 + i] &&
		    brd[3 * 0 + i] != '*')
			winval = 1;

	}
	//check cols
	for (i = 0; i < 3; i++){
		if (brd[3 * i + 0] == brd[3 * i + 1] &&
		    brd[3 * i + 1] == brd[3 * i + 2] &&
		    brd[3 * i + 0] != '*')
			winval = 1;
	}
	//check diagonals
	for (i = 0; i < 3; i++){
		if (brd[3 * 0 + 0] == brd[3 * 1 + 1] &&
		    brd[3 * 1 + 1] == brd[3 * 2 + 2] &&
		    brd[3 * 0 + 0] != '*')
			winval = 1;
	}
	for (i = 0; i < 3; i++){
		if (brd[3 * 0 + 2] == brd[3 * 1 + 1] &&
		    brd[3 * 1 + 1] == brd[3 * 2 + 0] &&
		    brd[3 * 0 + 2] != '*')
			winval = 1;
	}
	if (winval == 1){
		errors = copy_to_user(buffer,"WIN\n",4);
   		if (errors < 0)
			return(-EINVAL);
		return 4;
	}
	else if (winval == 2){
		errors = copy_to_user(buffer,"TIE\n",4);
   		if (errors < 0)
			return(-EINVAL);
		return 4;
	}
	else{
		errors = copy_to_user(buffer,"OK\n",3);
   		if (errors < 0)
			return(-EINVAL);
		return 3;	
	}
   }


   errors = copy_to_user(buffer,"UNKCMD\n",7);
   if (errors < 0)
	return(-EINVAL);
   return 7;
}

static int tictactoe_release(struct inode *pinode, struct file *pfile) {
   printk(KERN_INFO "Tic-Tac-Toe device closed\n");
   return 0;
}

module_init(tictactoe_init);
module_exit(tictactoe_exit);
MODULE_AUTHOR(DRIVER_AUTHOR);//have to capitalize
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_SUPPORTED_DEVICE("Tic-Tac-Toe");
