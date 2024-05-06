#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "lista.h"



// FUNCIONES PARA LISTA DE USUARIOS
// ====================================================

void addUser(struct DictUserList *dict, const char *name, const char *ip, int port) {
    if (dict->num_users < MAX_USERS) {
        strcpy(dict->users[dict->num_users].name, name);
        strcpy(dict->users[dict->num_users].ip, ip);
        dict->users[dict->num_users].port = port;
        dict->num_users++;
    } else {
        printf("No se pueden agregar más usuarios. La lista está llena.\n");
    }
}

void printUsers(const struct DictUserList *dict) {
    printf("Lista de usuarios:\n");
    for (int i = 0; i < dict->num_users; ++i) {
        printf("Usuario %d: Nombre: %s, IP: %s, Puerto: %d\n", i+1, dict->users[i].name, dict->users[i].ip, dict->users[i].port);
    }
}


// FUNCIONES PARA LA BASE DE DATOS
// ====================================================

// Función para guardar la lista en un archivo
void saveListToFile(const char *filename, List list) {
    FILE *file = fopen(filename, "wb");  // Se abre el archivo
    if (file == NULL) {
        printf("Error al abrir el archivo para escritura.\n");
        return;
    }

    List current = list;  // Se crea un puntero auxiliar para recorrer la lista
    while (current != NULL) {
        fwrite(current, sizeof(struct Node), 1, file);  // Se escribe cada nodo de la lista en el archivo
        current = current->next;  // Se avanza al siguiente nodo
    }

    fclose(file);  // Se cierra el archivo después de escribir la lista
}

// Función para cargar la lista desde un archivo
List loadListFromFile(const char *filename) {
  FILE *file = fopen(filename, "rb"); // Se abre el archivo para lectura binaria
  if (file == NULL) {
    printf("Error al abrir el archivo para lectura.\n"); // Se muestra un mensaje de error si no se puede abrir el archivo
    return NULL;
  }

  List list = NULL; // Se inicializa la lista como vacía
  struct Node node;

  while (fread(&node, sizeof(struct Node), 1, file)) { // Se lee cada nodo del archivo
    struct Node *newNode = malloc(sizeof(struct Node)); // Se asigna memoria para un nuevo nodo
    if (newNode == NULL) {
      printf("Error de memoria al cargar la lista desde el archivo.\n"); // Se muestra un mensaje de error si no se puede asignar memoria
      fclose(file);
      return list;
    }
    memcpy(newNode, &node, sizeof(struct Node)); // Se copia el contenido del nodo leído al nuevo nodo
    newNode->next = list; // Se enlaza el nuevo nodo al principio de la lista
    list = newNode; // Se actualiza la lista con el nuevo nodo
  }

  fclose(file); // Se cierra el archivo después de cargar la lista
  return list; // Se devuelve la lista cargada desde el archivo
}


// FUNCIONES PARA EL SERVICIO
// ====================================================

// Función para inicializar la lista
int init_servicio(List *l) {
    *l = NULL;  // Se asigna NULL a la lista para inicializarla como vacía
    return 0;
}


// Función para añadir un nuevo usuario a la lista enlazada
int register_(List *my_list, char *user_name, char *user_ip, char *user_port) {
    // Verificar si el usuario ya está en la lista
    struct Node *current = *my_list;
    while (current != NULL) {
        if (strcmp(current->userData.user_name, user_name) == 0) {
            return 1; // El usuario ya está en la lista
        }
        current = current->next;
    }

    // Crear un nuevo nodo para el usuario
    struct Node *newNode = malloc(sizeof(struct Node));
    if (newNode == NULL) {
        perror("Error al asignar memoria para el nuevo usuario");
        return 2;
    }

    // Llenar los datos del nuevo usuario
    strcpy(newNode->userData.user_name, user_name);
    strcpy(newNode->userData.user_ip, user_ip);
    strcpy(newNode->userData.user_port, user_port);
    newNode->userData.connected = false;
    newNode->userData.num_files = 0;  // Inicialmente no tiene archivos publicados

    // Establecer el siguiente nodo como NULL, ya que este será el último de la lista
    newNode->next = NULL;

    // Si la lista está vacía, el nuevo nodo será el primer nodo de la lista
    if (*my_list == NULL) {
        *my_list = newNode;
    } else {
        // Si la lista no está vacía, encontrar el último nodo y establecer el nuevo nodo como su siguiente
        current = *my_list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
    }

    return 0; // El usuario se añadió correctamente
}

// Función para eliminar un usuario de la lista enlazada
int unregister_(List *my_list, char *user_name) {
    
    // Verificar si la lista está vacía
    if (*my_list == NULL) {
        return 1; // La lista está vacía, por lo tanto el usuario no puede ser eliminado
    }
    
    // Verificar si el primer nodo es el usuario a eliminar
    if (strcmp((*my_list)->userData.user_name, user_name) == 0) {
        
        struct Node *temp = *my_list;
        *my_list = (*my_list)->next;
        free(temp);
        return 0; // Usuario eliminado correctamente
    }
    
    // Buscar el usuario a eliminar en la lista
    struct Node *prev = *my_list;
    struct Node *current = (*my_list)->next;
    while (current != NULL) {
        
        if (strcmp(current->userData.user_name, user_name) == 0) {
            prev->next = current->next;
            free(current);
            return 0; // Usuario eliminado correctamente
        }
        
        prev = current;
        current = current->next;
    }
    

    return 1; // El usuario a eliminar no existe en la lista
}

// Función para conectar a un usuario
int connect_(List my_list, char *user_name, char *user_ip, char *user_port) {
    // Verificar si la lista está vacía
    if (my_list == NULL) {
        return 1; // El usuario no existe en la lista
    }

  // Buscar al usuario en la lista
    struct Node *current = my_list;
    while (current != NULL) {
        if (strcmp(current->userData.user_name, user_name) == 0) {
            // El usuario existe, ahora verificar si está conectado
            if (current->userData.connected) {              
                return 2; // El usuario ya está conectado
            }

            // Cambiar el estado de conexión del usuario a true
            current->userData.connected = true;
            strcpy(current->userData.user_ip, user_ip);
            strcpy(current->userData.user_port, user_port);
            return 0; // Exito           
        }                    
        current = current->next;
    }    
    return 1; // El usuario no está registrado en el sistema
}


// Función para desconectar a un usuario
int disconnect_(List my_list, char *user_name) {
  // Verificar si la lista está vacía
  if (my_list == NULL) {
      return 1; // El usuario no existe en la lista
  }

  // Buscar al usuario en la lista
  struct Node *current = my_list;
  while (current != NULL) {
      if (strcmp(current->userData.user_name, user_name) == 0) {
          // El usuario existe, ahora verificar si está conectado
          if (current->userData.connected  == false) {              
              return 2; // El usuario no está conectado
          }         
          // Cambiar el estado de conexión del usuario a false
          current->userData.connected = false;
          return 0; // Exito                   
      }                    
      current = current->next;
  }    
  return 1; // El usuario no está registrado en el sistema
}


// Función para publicar un archivo asociado a un usuario en la lista enlazada
int publish_(List my_list, char *user_name, char *file_name, char *description) {
    // Verificar si la lista está vacía
    if (my_list == NULL) {
        return 1; // El usuario no existe en la lista
    }

    // Buscar al usuario en la lista
    struct Node *current = my_list;
    while (current != NULL) {
        if (strcmp(current->userData.user_name, user_name) == 0) {
            // El usuario existe, ahora verificar si está conectado
            if (!current->userData.connected) {
                return 2; // El usuario no está conectado
            }

            // Verificar si el archivo ya está publicado
            for (int i = 0; i < current->userData.num_files; i++) {
                if (strcmp(current->userData.public_files[i].file_name, file_name) == 0) {
                    return 3; // El archivo ya está publicado
                }
            }

            // Si el archivo no está publicado, añadirlo al file_dict del usuario
            if (current->userData.num_files < MAX_FILES) {
                strcpy(current->userData.public_files[current->userData.num_files].file_name, file_name);
                strcpy(current->userData.public_files[current->userData.num_files].description, description);
                current->userData.num_files++;
                return 0; // Éxito
            } else {
                return 4; // La lista de archivos del usuario está llena
            }
        }
        current = current->next;
    }

    return 1; // El usuario no existe en la lista
}

// Función para eliminar un archivo asociado a un usuario en la lista enlazada
int delete_(List my_list, char *user_name, char *file_name) {
    // Verificar si la lista está vacía
    if (my_list == NULL) {
        return 1; // El usuario no existe en la lista
    }

    // Buscar al usuario en la lista
    struct Node *current = my_list;
    while (current != NULL) {
        if (strcmp(current->userData.user_name, user_name) == 0) {
            // El usuario existe, ahora verificar si está conectado
            if (!current->userData.connected) {
                return 2; // El usuario no está conectado
            }

            // Buscar el archivo en el file_dict del usuario
            int index = -1;
            for (int i = 0; i < current->userData.num_files; i++) {
                if (strcmp(current->userData.public_files[i].file_name, file_name) == 0) {
                    index = i;
                    break;
                }
            }

            // Verificar si el archivo no ha sido publicado previamente
            if (index == -1) {
                return 3; // El archivo no ha sido publicado previamente
            }

            // Eliminar el archivo del file_dict del usuario
            for (int i = index; i < current->userData.num_files - 1; i++) {
                strcpy(current->userData.public_files[i].file_name, current->userData.public_files[i + 1].file_name);
                strcpy(current->userData.public_files[i].description, current->userData.public_files[i + 1].description);
            }
            current->userData.num_files--;

            return 0; // Éxito
        }
        current = current->next;
    }

    return 1; // El usuario no existe en la lista
}

// Función para listar usuarios conectados y sus detalles en un diccionario
int list_users_(List my_list, char *user_name, struct DictUserList *users_list) {
    // Verificar si la lista está vacía
    if (my_list == NULL) {
        return 1; // La lista está vacía, el usuario no está en la lista
    }

    // Buscar al usuario en la lista
    struct Node *current = my_list;
    while (current != NULL) {
        if (strcmp(current->userData.user_name, user_name) == 0) {
            // El usuario existe, ahora verificar si está conectado
            if (!current->userData.connected) {
                return 2; // El usuario no está conectado
            }

            // Inicializar el contador de usuarios en el diccionario
            users_list->num_users = 0;

            // Recorrer la lista para encontrar usuarios conectados
            current = my_list;
            while (current != NULL) {
                if (current->userData.connected) {
                    // Agregar el usuario conectado al diccionario
                    strcpy(users_list->users[users_list->num_users].name, current->userData.user_name);
                    strcpy(users_list->users[users_list->num_users].ip, current->userData.user_ip);
                    users_list->users[users_list->num_users].port = atoi(current->userData.user_port);
                    users_list->num_users++;
                }
                current = current->next;
            }

            return 0; // Éxito
        }
        current = current->next;
    }

    return 1; // El usuario no está en la lista
}


// Función para listar los archivos publicados por un usuario en otro usuario
int list_contents_(List my_list, char *user_name1, char *user_name2, struct file_dict2 *files) {
    // Verificar si la lista está vacía
    if (my_list == NULL) {
        return 1; // La lista está vacía, el usuario user_name1 no está en la lista
    }

    // Buscar al usuario user_name1 en la lista
    struct Node *current = my_list;
    while (current != NULL) {
        if (strcmp(current->userData.user_name, user_name1) == 0) {
            // El usuario user_name1 existe, ahora verificar si está conectado
            if (!current->userData.connected) {
                return 2; // El usuario user_name1 no está conectado
            }

            // Buscar al usuario user_name2 en la lista
            current = my_list;
            while (current != NULL) {
                if (strcmp(current->userData.user_name, user_name2) == 0) {
                    // El usuario user_name2 existe, copiar sus archivos publicados a files
                    for (int i = 0; i < current->userData.num_files; i++) {
                        strcpy(files->files[i].file_name, current->userData.public_files[i].file_name);
                        strcpy(files->files[i].description, current->userData.public_files[i].description);
                    }
                    files->num_files = current->userData.num_files; // Actualizar num_files
                    return 0; // Éxito
                }
                current = current->next;
            }

            return 3; // El usuario user_name2 no existe en la lista
        }
        current = current->next;
    }

    return 4; // Otro problema no especificado
}