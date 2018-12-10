#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h> /* copy_from_user, copy_to_user*/
#include <linux/vmalloc.h>
#include <linux/seq_file.h> /* For reading the ADT */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Carlos Bilbao");
MODULE_DESCRIPTION("Linux kernel module SMP-Safe for managing an efficient list (doubly-linked, with ghost node) of integers");

#define MAX_SIZE 	 500
#define MODULE_NAME 	"modlist"

/* Reader-Writer Spin Lock (SMP-Safe) */
DEFINE_RWLOCK(rwl);

/* My /proc file entry */
static struct proc_dir_entry *my_proc_entry;

#ifdef CHARS
typedef char* value;
#else
typedef int value;
#endif

static int elems = 0; //Elements read at the cat so far
static int numElems = 0; //Total number of elements;

/* Node */
struct list_item {
   value data;
   struct list_head links;
};

/* Linked list ghost node */
struct list_head ghost_node;

/* FUNCTIONS */

/* Auxiliar function */
struct list_head* findNode(int n, char *c, struct list_head* head){

	struct list_head* pos = NULL;
	struct list_head* aux = NULL;
	struct list_item* item = NULL;
	int find = 0;

	for (pos = (head)->next; pos != (head) && find == 0; pos = pos->next) {

		item = list_entry(pos, struct list_item, links);
#ifdef CHARS

		if(strcmp(c, item->data) == 0) find = 1;
#else
		if(item->data == n) find = 1;
#endif
		aux = pos;
	}

	if(find) return aux;

	return NULL;
}

/* Auxiliar function */
void removeList(void) {

        struct list_head* cur_node = NULL;
        struct list_head* aux = NULL;
        struct list_item* item = NULL;
	numElems = 0;

        list_for_each_safe(cur_node, aux, &ghost_node) {

                item = list_entry(cur_node, struct list_item, links);
                list_del(&item->links);
#ifdef CHARS
		vfree(item->data);
#endif
		vfree(item);
        }
}

/* Auxiliar function - return number of elements*/
int print_list(struct list_head* list, char members[]){

	struct list_item* item = NULL;
	struct list_head* cur_node = NULL;
	int read = 0;
	char* aux;

	list_for_each(cur_node, list) { /* while cur_node != list*/

		item = list_entry(cur_node, struct list_item, links);
		if(read < MAX_SIZE){
#ifdef CHARS
			aux = item->data;
			while((members[read++] = *aux) != '\n' && read < MAX_SIZE) {++aux;}
#else

			read += sprintf(&members[read],"%i\n",item->data);
                        members[read++] = '\n';
		}

#endif
	}
	return read;
}

/* Invoked with echo */
static ssize_t myproc_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {

	char kbuf[MAX_SIZE];
	char elem[MAX_SIZE];
	int n; //Memory address
	struct list_item *new_item = NULL;
	struct list_head *cur = NULL;

	if (len >= MAX_SIZE) return -EFBIG;
	if (copy_from_user(kbuf, buf, len) > 0) return -EINVAL;
	if((*off > 0)) return 0;

#ifdef CHARS

	if(sscanf(kbuf, "add %s", elem) == 1) {

	        new_item = vmalloc(sizeof(int) * 4 + strlen(elem) + 1);//vmalloc(sizeof(struct list_item));
		new_item->data = vmalloc(strlen(elem) + 1);
		strcpy(new_item->data, elem);
		new_item->data[strlen(new_item->data)] = '\n';
		write_lock(&rwl);
		list_add_tail(&new_item->links, &ghost_node);
		write_unlock(&rwl);
		numElems++;
	}
	else if (sscanf(kbuf, "remove %s", elem) == 1){

		if(numElems > 0){

          		elem[strlen(elem)] = '\n';
			if ((cur = findNode(0, elem, &ghost_node)) == NULL) return -EINVAL;
			write_lock(&rwl);
			new_item = list_entry(cur, struct list_item, links);
               		list_del(&new_item->links);
			write_unlock(&rwl);
			vfree(new_item->data);
        	        vfree(new_item);
			numElems--;
		}
	}
	else if (strcmp(kbuf, "cleanup\n") == 0) {
		write_lock(&rwl);	
		removeList();
		write_unlock(&rwl);
	}
	else return -EINVAL;

#else
	if(sscanf(kbuf, "add %d", &n) == 1){

		new_item = vmalloc(sizeof(int) * 4);
		new_item->data = n;
		write_lock(&rwl);
		list_add_tail(&new_item->links,&ghost_node);
		write_unlock(&rwl);
		numElems++;
	}
	else if (sscanf(kbuf, "remove %d", &n) == 1){

		if(numElems > 0){

			if ((cur = findNode(n, NULL, &ghost_node)) == NULL) return -EINVAL;
			write_lock(&rwl);
			new_item = list_entry(cur, struct list_item, links);
			list_del(&new_item->links);
			write_unlock(&rwl);
			vfree(new_item);
			numElems--;
		}
	}
	else if (strcmp(kbuf, "cleanup\n") == 0) {
		write_lock(&rwl);
		removeList();
		write_unlock(&rwl);
	}
	else return -EINVAL;
#endif

	(*off) += len;

 return len;
}

#ifndef CHARS
/* Invoked with cat -> Print the List (Alternative way to seq_file) */
static ssize_t myproc_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {

	/* Implementation without seq_files */

	char kbuf[MAX_SIZE];
	int read = 0;

	if ((*off) > 0) return 0; //Previously invoked!

	read_lock(&rwl);
	read = print_list(&ghost_node, kbuf);
	read_unlock(&rwl);

	kbuf[read++] = '\0';

	if (copy_to_user(buf, kbuf, read) > 0) return -EFAULT;
	(*off) += read;

 return read;
}
#else

/* Invoked by seq_read at the beginning */
static void *l_seq_start(struct seq_file *s, loff_t *pos) {

	//Not previously invoked
	if(*pos != 0){
		elems = 0;
		*pos = 0;
		return NULL;
	}
	else {
             	 *pos = 1;
		 return pos;
	}
}

/* Invoked by seq_read after start() */
static void *l_seq_next(struct seq_file *s, void *v, loff_t *pos){

	if(elems < numElems) ++elems;
	else return NULL;

        return pos;
}

/*Invoked by seq_read when next() is not NULL */
static int l_seq_show(struct seq_file *s, void *v){

	struct list_item *new_item;
	struct list_head *pos = &ghost_node;
	int i = 0;
	while(i < elems && i < numElems){ 
		++i;
		pos = pos->next;
	}
        new_item = list_entry(pos, struct list_item, links);

#ifdef CHARS

	if(elems){
		seq_printf(s, "%s\n", "ERROR -> Check dmesg (Can not use seq_files for chars)");
		printk(KERN_INFO "The seq_files implementation is only available for integers \n");
		printk(KERN_INFO "Please, comment the optional flag if you want to use it \n");
	}
#else
	seq_printf(s, "%i\n", new_item->data);
#endif
	return 0;
}

/* Invoked by seq_read when seq_next is NULL */
static void l_seq_stop(struct seq_file *s, void *v) {

	printk(KERN_INFO "seq_file has finished \n");
}

/* For seq_file at cat */
const struct seq_operations my_seq_ops = {

	.start = l_seq_start,
	.next = l_seq_next,
	.stop = l_seq_stop,
	.show = l_seq_show
};


/* Initializing seq_files */
static int l_seq_open(struct inode *inode, struct file *file){

	return seq_open(file, &my_seq_ops);
}

#endif

/* Operations that the module can handle */
const struct file_operations fops = {

#ifndef CHARS
	.read = myproc_read,
#else
	.read = seq_read, //Using seq_files
	.open = l_seq_open,
#endif
	.write = myproc_write,
};

int module_linit(void){

	int ret = 0;

	my_proc_entry = proc_create("modlist", 0666, NULL, &fops); //Create proc entry

	if(my_proc_entry == NULL) {
		ret = -ENOMEM;
		printk(KERN_INFO "Couldn't create the /proc entry \n");
	}
	else {
		INIT_LIST_HEAD(&ghost_node);
#ifdef CHARS
		printk( KERN_INFO "Module %s succesfully charged for chars. \n", MODULE_NAME);
#else
		printk( KERN_INFO "Module %s succesfully charged for integers. \n", MODULE_NAME);
#endif

	}

	return ret;
}

void module_lclean(void) {

	//Free the memory
	removeList();
	remove_proc_entry("modlist", NULL);
        printk(KERN_INFO "Module %s disconnected \n", MODULE_NAME);
}

module_init(module_linit);
module_exit(module_lclean);
