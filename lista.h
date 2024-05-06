#ifndef _LISTA_H
#define _LISTA_H 1


#define MAX_NAME_LENGTH 256
#define MAX_FILE_NAME_LENGTH 256
#define MAX_DESCRIPTION_LENGTH 256
#define MAX_FILES 100
#define MAX_USERS 100

struct User {
    char name[MAX_NAME_LENGTH];
    char ip[16];    // Por ejemplo, asumiendo IPv4
    int port;
};

struct DictUserList {
    int num_users;
    struct User users[MAX_USERS];
};

void addUser(struct DictUserList *dict, const char *name, const char *ip, int port);

void printUsers(const struct DictUserList *dict);

struct file {
    char file_name[MAX_FILE_NAME_LENGTH];
    char description[MAX_DESCRIPTION_LENGTH];
};

struct file_dict2 {
    struct file files[MAX_FILES];
    int num_files; // Cantidad de archivos en este diccionario
};

// Definición de la estructura para un diccionario
struct file_dict {
    char file_name[MAX_FILE_NAME_LENGTH];
    char description[MAX_DESCRIPTION_LENGTH];
    int num_files;
};

// Definición del struct user
struct user {
    char user_name[MAX_NAME_LENGTH];
    char user_ip[MAX_NAME_LENGTH];
    char user_port[MAX_NAME_LENGTH];
    bool connected;
    struct file_dict public_files[MAX_FILES]; // file_name : description
    int num_files; 
};

// Definición de la estructura Node para la lista enlazada
struct Node {
    struct user userData;
    struct Node *next;
};

// Definición de un alias para el tipo de puntero a Node
typedef struct Node *List;

void saveListToFile(const char *filename, List list);
List loadListFromFile(const char *filename);

int init_servicio(List *l);

int register_(List *my_list, char *user_name, char *user_ip, char *user_port);
int unregister_(List *my_list, char *user_name);
int connect_(List my_list, char *user_name, char *user_ip, char *user_port);	
int disconnect_(List my_list, char *user_name);
int publish_(List my_list, char *user_name, char *file_name, char *description);
int delete_(List my_list, char *user_name, char *file_name);
int list_users_(List my_list, char *user_name, struct DictUserList *users_list);	
int list_contents_(List my_list, char *user_name1, char *user_name2, struct file_dict2 *files);

#endif
