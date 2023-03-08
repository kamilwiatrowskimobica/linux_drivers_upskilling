#ifndef __WOAB_LIST_H__
#define __WOAB_LIST_H__

#define LIST_PROCESS_NAME_SIZE 20

struct my_list {
	struct list_head head;
	int pid;
	char name[LIST_PROCESS_NAME_SIZE];
};

void woab_list_init(struct my_list *list);
void woab_list_clear(struct my_list *list);
void woab_list_add_item(struct my_list *list);
void woab_list_print(struct my_list *list);

#endif /* __WOAB_LIST_H__ */