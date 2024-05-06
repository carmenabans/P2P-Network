// Librerías
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "lista.h"
#include "lines.h"
#include "rpc.h"

// Definiciones de constantes
#define MAX_THREADS 10
#define MAX_PETICIONES 256
#define MAX_NAME_LENGTH 256
#define MAX_FILE_NAME_LENGTH 256
#define MAX_DESCRIPTION_LENGTH 256
#define MAX_FILES 100
#define MAX_USERS 100


// Buffer
int buffer_peticiones[MAX_PETICIONES];
int n_elementos;
int pos_servicio = 0;


// mutex y variables condicionales para proteger la copia del mensaje
pthread_mutex_t mutex;
pthread_cond_t no_lleno;
pthread_cond_t no_vacio;
pthread_mutex_t mfin;
int fin = false;


// Funcion para llamadas RPC para mandar la fecha al servidor rpc
void registrar_1(char *message) {
    CLIENT *clnt;
    enum clnt_stat retval_1;
    int result_1;

#ifndef DEBUG
    clnt = clnt_create("localhost", REGISTRAR, REGISTRARVER, "udp");
    if (clnt == NULL) {
        clnt_pcreateerror("localhost");
        exit(1);
    }
#endif /* DEBUG */

    retval_1 = rpc_date_1(message, &result_1, clnt);
    if (retval_1 != RPC_SUCCESS) {
        clnt_perror(clnt, "call failed");
    }

#ifndef DEBUG
    clnt_destroy(clnt);
#endif /* DEBUG */
}


// Función para obtener la información de la dirección IP y el puerto del socket
void getSocketAddress(int sockfd, char *ip, int *port) {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    getpeername(sockfd, (struct sockaddr *)&addr, &addrlen);

    // Obtiene la dirección IP en formato legible
    strcpy(ip, inet_ntoa(addr.sin_addr));
    // Obtiene el puerto y lo convierte de network byte order a host byte order
    *port = ntohs(addr.sin_port);
}


int servicio(void) {

  // Inicializar variables
  // ======================================  

  // Variable socket
  int s_local;
  List my_list;
  int err;
  struct DictUserList users_list;
  char client_ip[INET_ADDRSTRLEN];
  int client_port;

  // Variables mensaje
  int res;
  char op[15];
  char date[256];
  char user_name[MAX_NAME_LENGTH];
  char user_name2[MAX_NAME_LENGTH];
  char file_name[MAX_FILE_NAME_LENGTH];
  char description[MAX_DESCRIPTION_LENGTH];
  struct file_dict2 files;

  // Variable buffer
  char buffer[256];


  // FOR Servicio
  // ======================================  

  for (;;) {

    // Coger el socket cliente del buffer de peticiones
    // ==================================================  
    pthread_mutex_lock( & mutex);
    while (n_elementos == 0) {
      if (fin == true) {
        fprintf(stderr, " s> Ending service\n");
        pthread_mutex_unlock( & mutex);
        pthread_exit(0);
      }
      pthread_cond_wait( & no_vacio, & mutex);
    }
    s_local = buffer_peticiones[pos_servicio];
    pos_servicio = (pos_servicio + 1) % MAX_PETICIONES;
    n_elementos--;
    pthread_cond_signal( & no_lleno);
    pthread_mutex_unlock( & mutex);

    // Obtener información de la dirección IP y el puerto del socket
    getSocketAddress(s_local, client_ip, &client_port);

    // Cargar la BBDD
    // ======================================  

    my_list = loadListFromFile("list_data.dat");

    // Recepción de mensajes del cliente
    // ======================================  

    // Recibir op -> Para todas las operaciones
    err = readLine(s_local, buffer, 256);
    if (err == -1) {
      printf("s> Error receiving petition op\n");
      close(s_local);
      pthread_exit(NULL);
    }
    strcpy(op, buffer);
    
    // Recibir date -> Para todas las operaciones
    err = readLine(s_local, buffer, 256);
    if (err == -1) {
      printf("s> Error receiving petition date and time\n");
      close(s_local);
      pthread_exit(NULL);
    }
    strcpy(date, buffer);
    
    // Recibir name -> Para todas las operaciones
    err = readLine(s_local, buffer, 256);
    if (err == -1) {
      printf("s> Error receiving petition name_user\n");
      close(s_local);
      pthread_exit(NULL);
    }
    strcpy(user_name, buffer);


    // Recibir name2 -> Para operacion get_file y list_content
    if ((strcmp(op, "GET_FILE") == 0) || (strcmp(op, "LIST_CONTENT") == 0)) {
        err = readLine(s_local, buffer, 256);
        if (err == -1) {
          printf("s> Error receiving petition name\n");
          close(s_local);
          pthread_exit(NULL);
        }
        strcpy(user_name2, buffer);
    }

    // Recibir file_name -> Para operaciones publish y delete
    if ((strcmp(op, "PUBLISH") == 0) || (strcmp(op, "DELETE") == 0)) {
        err = readLine(s_local, buffer, 256);
        if (err == -1) {
          printf("s> Error receiving petition file_name\n");
          close(s_local);
          pthread_exit(NULL);
        }
        strcpy(file_name, buffer);
    }

    // Recibir description -> Para operacion publish
    if (strcmp(op, "PUBLISH") == 0) {
        err = readLine(s_local, buffer, 256);
        if (err == -1) {
          printf("s> Error receiving petition file_description\n");
          close(s_local);
          pthread_exit(NULL);
        }
        strcpy(description, buffer);
    }


    // Mensaje recibido
    // printf("OPERATION FROM %s \n", user_name);
    if (strcmp(op, "PUBLISH") == 0 || strcmp(op, "DELETE") == 0) {
        // Imprimir mensaje con file_name si la operación es PUBLISH o DELETE
        printf("s> OPERATION FROM %s\n", user_name);
        snprintf(buffer, 1024, "%s\t%s %s\t%s", user_name, op, file_name, date);
        registrar_1(buffer);
    } else {
        // Imprimir mensaje sin file_name para otras operaciones
          printf("s> OPERATION FROM %s\n", user_name);
          snprintf(buffer, 1024, "%s\t%s\t%s", user_name, op, date);
          registrar_1(buffer);
    }

    // Procesar el mensaje
    // ======================================
    if (strcmp(op, "REGISTER") == 0) {   	
        // Convierto el puerto a entero
        char client_port_str[10];
        sprintf(client_port_str, "%d", client_port);
        res = register_(&my_list, user_name, client_ip, client_port_str);	
        // Actualizar BBDD
        pthread_mutex_lock( & mutex);
        saveListToFile("list_data.dat", my_list);
        pthread_mutex_unlock( & mutex);
    } 

    else if (strcmp(op, "UNREGISTER") == 0) {     
        res = unregister_(&my_list, user_name);	
        // Actualizar BBDD
        pthread_mutex_lock( & mutex);
        saveListToFile("list_data.dat", my_list);
        pthread_mutex_unlock( & mutex);

    }

    else if (strcmp(op, "CONNECT") == 0) {
        // Recibir puerto del cliente
        char client_port_str[10];
        err = readLine(s_local, buffer, 256);
        if (err == -1) {
          printf("s> Error receiving client_port\n");
          close(s_local);
          pthread_exit(NULL);
        }
        strcpy(client_port_str, buffer);
        res = connect_(my_list, user_name, client_ip, client_port_str);
        // Actualizar BBDD
        pthread_mutex_lock( & mutex);
        saveListToFile("list_data.dat", my_list);
        pthread_mutex_unlock( & mutex);

    }

    else if (strcmp(op, "DISCONNECT") == 0) {
        res = disconnect_(my_list, user_name);	
        // Actualizar BBDD
        pthread_mutex_lock( & mutex);
        saveListToFile("list_data.dat", my_list);
        pthread_mutex_unlock( & mutex);

    }

    else if (strcmp(op, "PUBLISH") == 0) {
        res = publish_(my_list, user_name, file_name, description);	
        // Actualizar BBDD
        pthread_mutex_lock( & mutex);
        saveListToFile("list_data.dat", my_list);
        pthread_mutex_unlock( & mutex);

    }

    else if (strcmp(op, "DELETE") == 0) {
        res = delete_(my_list, user_name, file_name);	
        // Actualizar BBDD
        pthread_mutex_lock( & mutex);
        saveListToFile("list_data.dat", my_list);
        pthread_mutex_unlock( & mutex);

    }

    else if (strcmp(op, "LIST_USERS") == 0) {
        res = list_users_(my_list, user_name, &users_list);	
        // Actualizar BBDD
        pthread_mutex_lock( & mutex);
        saveListToFile("list_data.dat", my_list);
        pthread_mutex_unlock( & mutex);

    }

    else if (strcmp(op, "LIST_CONTENT") == 0) {
        res = list_contents_(my_list, user_name, user_name2, &files);	
        // Actualizar BBDD
        pthread_mutex_lock( & mutex);
        saveListToFile("list_data.dat", my_list);
        pthread_mutex_unlock( & mutex);

    }

    else {
      res = -1;
    }


    // Enviar los mensajes al cliente
    // ======================================

    // Envío res -> Para todas las operaciones

    sprintf(buffer, "%d", res);
    err = sendMessage(s_local, buffer, strlen(buffer) + 1);
    if (err == -1) {
      printf("s> Error sending res\n");
      return -1;
    }

    // Para list_users enviar diccionario con: user ip port
    if (strcmp(op, "LIST_USERS") == 0) {

      // Envía el número de usuarios en el diccionario

      snprintf(buffer, sizeof(buffer), "%d", users_list.num_users);
      err = sendMessage(s_local, buffer, strlen(buffer) + 1);
      if (err == -1) {
          printf("s> Error sending num_users\n");
          return -1;
      }

      // Envía cada entrada del diccionario users_list
      for (int i = 0; i < users_list.num_users; ++i) {
        snprintf(buffer, sizeof(buffer), "%s", users_list.users[i].name);
        err = sendMessage(s_local, buffer, strlen(buffer) + 1);
        if (err == -1) {
            printf("s> Error sending user_name\n");
            return -1;
        }
        snprintf(buffer, sizeof(buffer), "%s", users_list.users[i].ip);
        err = sendMessage(s_local, buffer, strlen(buffer) + 1);
        if (err == -1) {
            printf("s> Error sending user_ip\n");
            return -1;
        }
        snprintf(buffer, sizeof(buffer), "%d", users_list.users[i].port);
        err = sendMessage(s_local, buffer, strlen(buffer) + 1);
        if (err == -1) {
            printf("s> Error sending user_port\n");
            return -1;
        }
        
      }
    }

    // Para listcontents enviar diccionario con: file_name description
    else if (strcmp(op, "LIST_CONTENT") == 0) {
        // Envía el número de archivos en el diccionario
        sprintf(buffer, "%d", files.num_files);
        err = sendMessage(s_local, buffer, strlen(buffer) + 1);
        if (err == -1) {
            printf("s> Error sending num_files\n");
            return -1;
        }

        // Envía cada entrada del diccionario files
        for (int i = 0; i < files.num_files; ++i) {
            // Envío file_name
            strcpy(buffer, files.files[i].file_name);
            err = sendMessage(s_local, buffer, strlen(buffer) + 1);
            if (err == -1) {
                printf("s> Error sending file_name\n");
                return -1;
            }

            // Envío description
            strcpy(buffer, files.files[i].description);
            err = sendMessage(s_local, buffer, strlen(buffer) + 1);
            if (err == -1) {
                printf("s> Error sending file_description\n");
                return -1;
            }
        }
    }


    // Mensaje enviado
    printf("s> \n");

    close(s_local);
  } // FOR

  pthread_exit(0);

} // servicio



int main(int argc, char * argv[]) {

  if (argc < 3) {
    printf("Usage:\n");
    printf("To initialize the server: %s -p <port>\n", argv[0]);
    return -1;
  }

  // Inicializar mutex y variables condicionales
  pthread_attr_t t_attr;
  pthread_t thid[MAX_THREADS];
  int pos = 0;

  // Inicializar base de datos
  List my_list;
  init_servicio(&my_list);
  saveListToFile("list_data.dat", my_list);

  // Variable para almacenar códigos de error 
  int err;

  // Var sockets
  int sd, sc;

  // Estructuras para almacenar información del servidor y del cliente
  struct sockaddr_in server_addr, client_addr;

  // Representa el tamaño de una estructura de dirección
  socklen_t size;

  // Se crea el socket
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("s> Error socket");
    return (0);
  }

  // Se configura el socket para que la dirección del socket se reutilice después de que se cierre
  int val;
  val = 1;
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char * ) & val, sizeof(int));

  // Se inicializa a 0 todos los bytes de la estructura
  bzero((char * ) & server_addr, sizeof(server_addr));

  // Se establece el tipo de dirección IP que se está utilizando en la estructura
  server_addr.sin_family = AF_INET;
  // Se configura el campo de dirección IP de la estructura
  server_addr.sin_addr.s_addr = INADDR_ANY;
  // Se establece el puerto en el que se va a escuchar
  unsigned short port = atoi(argv[2]);
  server_addr.sin_port = htons(port);

  // Se asocia la dirección con el socket sd
  err = bind(sd, (const struct sockaddr * ) & server_addr, sizeof(server_addr));
  if (err == -1) {
    printf("s> Error bind\n");
    return -1;
  }

  // Se configura el socket para que se puedan recibir conexiones entrantes
  err = listen(sd, SOMAXCONN);
  if (err == -1) {
    printf("s> Error listen\n");
    return -1;
  }

  printf("s> init server %s: %d\ns>\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

  // Se establece size al tamaño de la estructura client_addr
  size = sizeof(client_addr);

  pthread_mutex_init( & mutex, NULL);
  pthread_cond_init( & no_lleno, NULL);
  pthread_cond_init( & no_vacio, NULL);
  pthread_mutex_init( & mfin, NULL);

  // Creación del pool de threads
  pthread_attr_init( & t_attr);
  for (int i = 0; i < MAX_THREADS; i++)
    if (pthread_create( & thid[i], NULL, (void * ) servicio, NULL) != 0) {
      perror("s> Error creating pool of threads\n");
      return 0;
    }

  while (true) {

    sc = accept(sd, (struct sockaddr * ) & client_addr, (socklen_t * ) & size);

    if (sc == -1) {
      printf("s> Error accept\n");
      return -1;
    }

    pthread_mutex_lock( & mutex);
    while (n_elementos == MAX_PETICIONES)
      pthread_cond_wait( & no_lleno, & mutex);
    buffer_peticiones[pos] = sc;
    pos = (pos + 1) % MAX_PETICIONES;
    n_elementos++;
    pthread_cond_signal( & no_vacio);
    pthread_mutex_unlock( & mutex);
  } /* FIN while */

  pthread_mutex_lock( & mfin);
  fin = true;
  pthread_mutex_unlock( & mfin);

  pthread_mutex_lock( & mutex);
  pthread_cond_broadcast( & no_vacio);
  pthread_mutex_unlock( & mutex);

  for (int i = 0; i < MAX_THREADS; i++)
    pthread_join(thid[i], NULL);

  pthread_mutex_destroy( & mutex);
  pthread_cond_destroy( & no_lleno);
  pthread_cond_destroy( & no_vacio);
  pthread_mutex_destroy( & mfin);

  return 0;
}
