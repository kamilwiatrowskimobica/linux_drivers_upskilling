#ifndef _MOBI_MODULE_H_
#define _MOBI_MODULE_H_

// #define MOB_DATA_MAX 255

struct mob_dev {
    char *msg;
    size_t msg_size;
    struct cdev cdev;
};


#endif /* _MOBI_MODULE_H_ */