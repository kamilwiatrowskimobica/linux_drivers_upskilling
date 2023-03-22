#ifndef _MOB_LIST_H_
#define _MOB_LIST_H_

#define PROC_NAME (30u)

struct mob_list {
    struct list_head head;
    int pid;
    char proc_name[PROC_NAME];
};

void mob_list_init(struct mob_list *list);
void mob_list_clear(struct mob_list *list);
void mob_list_add_item_to_back(struct mob_list *list);
void mob_list_print(struct mob_list *list);

#endif // _MOB_LIST_H_