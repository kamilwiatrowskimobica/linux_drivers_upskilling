#include <linux/sched.h>
#include <linux/slab.h>
#include <mob_debug.h>
#include <mob_list.h>

void mob_list_init(struct mob_list *list)
{
    INIT_LIST_HEAD(&list->head);
    list->pid = 0;
}

void mob_list_add_item_to_back(struct mob_list *list)
{
    struct mob_list *new_node = kzalloc(sizeof(struct mob_list), GFP_KERNEL);
    MOB_PRINT("Process PID:%d - %s\n", current->pid, current->comm);
    new_node->pid = current->pid;
    strncpy(new_node->proc_name, current->comm, PROC_NAME-1);

    INIT_LIST_HEAD(&new_node->head);
    list_add_tail(&new_node->head, &list->head);
}

void mob_list_clear(struct mob_list *list)
{
    struct mob_list *tmp;
}

void mob_list_print(struct mob_list *list)
{
    struct mob_list *tmp;
    int cnt = 0;

    list_for_each_entry(tmp, &list->head, head){
        MOB_PRINT("Node:%d pid=%d/%s\n", cnt++, tmp->pid, tmp->proc_name);
    }
}