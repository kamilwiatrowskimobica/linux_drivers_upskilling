#include <linux/sched.h>
#include <linux/slab.h>
#include <woab_debug.h>
#include <woab_list.h>

void woab_list_init(struct my_list *list)
{
	INIT_LIST_HEAD(&list->head);
	list->pid = 0;
	list->name[0] = '\0';
}

void woab_list_clear(struct my_list *list)
{
	struct my_list *cursor, *temp;
	list_for_each_entry_safe (cursor, temp, &list->head, head) {
		list_del(&cursor->head);
		kfree(cursor);
	}
}

void woab_list_add_item(struct my_list *list)
{
	struct my_list *temp_node = kzalloc(sizeof(struct my_list), GFP_KERNEL);
	WLISTDEBUG("Process %d named %s completed writting\n", current->pid,
		   current->comm);

	temp_node->pid = current->pid;
	strncpy(temp_node->name, current->comm,
		strlen(current->comm) < LIST_PROCESS_NAME_SIZE ?
			strlen(current->comm) :
			LIST_PROCESS_NAME_SIZE - 1);

	INIT_LIST_HEAD(&temp_node->head);
	list_add_tail(&temp_node->head, &list->head);
}

void woab_list_print(struct my_list *list)
{
	struct my_list *temp;
	int count = 0;

	/*Traversing Linked List and Print its Members*/
	list_for_each_entry (temp, &list->head, head) {
		WLISTDEBUG("Node %d pid = %d named %s\n", count++, temp->pid,
			   temp->name);
	}
	WLISTDEBUG("Total Nodes = %d\n", count);
}
