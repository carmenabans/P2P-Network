from enum import Enum
import argparse
import socket
import sys
import os
import threading
import io
import select
import zeep

# Funcion para obtener la fecha y hora actual
def get_date():
    wsdl_url = "http://localhost:8000/?wsdl"
    soap = zeep.Client(wsdl=wsdl_url) 
    result = soap.service.get_date()
    return result


# Funcion para recibir numeros por sockets
def readNumber(sock):
    a = ""
    while True:
        msg = sock.recv(1)
        if msg == b"\0":
            break
        a += msg.decode()

    return int(a, 10)


def find_free_port():
    # Intentar encontrar un puerto libre
    for port in range(1024, 65536):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind(("localhost", port))
                return port
            except OSError:
                pass
    # Si no hay puertos disponibles llega aqui
    return -1


class client:

    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum):
        OK = 0
        ERROR = 1
        USER_ERROR = 2


    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1
    _user = None
    _connected = False
    _list_users = []


    # ****************** HILO DEMONIO ******************
    _server_thread = None  # Variable para almacenar el hilo del servidor
    _stop_flag = False  # Bandera para indicar si el hilo debe detenerse


    # Método para iniciar el hilo demonio
    @staticmethod
    def start_server_thread(port):
        client._server_thread = threading.Thread(
            target=client.worker, args=(port,), daemon=True
        )
        client._server_thread.start()


    # Método para detener el hilo demonio
    @staticmethod
    def stop_server_thread():
        client._stop_flag = True
        if client._server_thread is not None:
            client._server_thread.join()
            client._server_thread = None
            print('Fin hilo demonio')


    @staticmethod
    def worker(port):
        print('Inicia hilo demonio')
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        local_address = ("localhost", port)
        sock.bind(local_address)
        sock.listen(5)

        while not client._stop_flag:
            
            ready_to_read, _, _ = select.select([sock], [], [], 5)  # Esperar hasta 5 segundos

            if sock in ready_to_read:
                connection, client_address = sock.accept()

                # Procesar la conexión
                op = ""
                while True:
                    msg = connection.recv(1)
                    if msg == b"\0":
                        break
                    op += msg.decode()

                if op == "GET_FILE":
                    # Recibe nombre de archivo a enviar
                    file_name = ""
                    while True:
                        msg = connection.recv(1)
                        if msg == b"\0":
                            break
                        file_name += msg.decode()

                    # Verificar si el archivo existe
                    if not os.path.exists(file_name):
                        print(f"El archivo '{file_name}' no existe.")
                        res = "1\0"
                        connection.sendall(res.encode())
                        connection.close()
                    else:
                        # Abrir el archivo a enviar
                        with open(file_name, "rb") as file:
                            file_data = file.read()

                        # Enviar el archivo
                        res = "0\0"
                        connection.sendall(res.encode())
                        connection.sendall(file_data)
                        connection.close()
                else:
                    res = "2\0"
                    connection.sendall(res.encode())
                    connection.close()
            else:
                ...



    # ******************** METHODS *******************

    @staticmethod
    def register(user):
        # Metodo para registrar un usuario
        try:
            # Conectar con el servidor
            # =====================================================
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server_address = (client._server, int(client._port))
            print("connecting to {} port {}".format(*server_address))
            sock.connect(server_address)

            try:
                # Enviar peticiones
                # =====================================================
                
                # Enviar la cadena REGISTER\0
                op = b"REGISTER\0"
                sock.sendall(op)
                
                # Enviar fecha y hora
                date = get_date()
                op_date = (date + '\0').encode('utf-8')
                sock.sendall(op_date)
                
                # Enviar nombre de usuario (max 256)
                user_name = f"{user}\0".encode("utf-8")
                sock.sendall(user_name)

                # Recibir del servidor el int con la respuesta (0/1/2)
                # =====================================================
                res = readNumber(sock)

                # Respuesta
                if res == 0:
                    client._user = user
                    print("c> REGISTER OK")
                elif res == 1:
                    print("c> USERNAME IN USE")
                else:
                    print("c> REGISTER FAIL")

            finally:
                # Cerrar conexión
                # =====================================================
                sock.close()

        except Exception as e:
            print("c> REGISTER FAIL: ", e)
            # Cerrar conexión si se ha abierto
            if "sock" in locals():
                sock.close()

                
    @staticmethod
    def unregister(user):
        try:
            # Error si el usuario no se ha registrado y conectado
            # =====================================================
            if client._user != user or client._connected is False:
                print("c> UNREGISTER FAIL")
                return
                
            # Conectar con el servidor
            # =====================================================
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server_address = (client._server, int(client._port))
            print("connecting to {} port {}".format(*server_address))
            sock.connect(server_address)

            try:
                # Enviar peticiones
                # =====================================================
                # Enviar la cadena REGISTER\0
                op = b"UNREGISTER\0"
                sock.sendall(op)

                # Enviar fecha
                date = get_date()
                op_date = (date + '\0').encode('utf-8')
                sock.sendall(op_date)

                # Enviar nombre de usuario (max 256)
                user_name = f"{user}\0".encode("utf-8")
                sock.sendall(user_name)

                # Recibir del servidor el int (0/1/2)
                # =====================================================
                res = readNumber(sock)
                
                # Respuesta
                if res == 0:
                    # Desconectar al usuario
                    client._connected = False
                    client.stop_server_thread()
                    client._user = None
                    print("c> UNREGISTER OK")
                elif res == 1:
                    print("c> USER DOES NOT EXIST")
                else:
                    print("c> UNREGISTER FAIL")

            finally:
                # Cerrar conexión
                # =====================================================
                sock.close()

        except Exception as e:
            print("c> UNREGISTER FAIL: ", e)
            # Cerrar conexión si se ha abierto
            if "sock" in locals():
                sock.close()

                
    
    @staticmethod
    def connect(user):
        try:
            # Verificar que es el mismo usuario que esta registrado en esta terminal
            # ========================================================================
            if client._connected is True:
                print("c> CONNECT FAIL")
                return
            # Crear e iniciar el hilo para escuchar peticiones
            # =====================================================
            port = find_free_port()

            # Conectar con el servidor
            # =====================================================
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            server_address = (client._server, int(client._port))
            print("connecting to {} port {}".format(*server_address))
            sock.connect(server_address)

            try:
                # Enviar peticiones
                # =====================================================
                
                # Enviar la cadena REGISTER\0
                op = b"CONNECT\0"
                sock.sendall(op)

                # Enviar fecha
                date = get_date()
                op_date = (date + '\0').encode('utf-8')
                sock.sendall(op_date)

                # Enviar nombre de usuario (max 256)
                client._user = user
                user_name = f"{user}\0".encode("utf-8")
                sock.sendall(user_name)

                # Enviar puerto de usuario (max 256)
                user_name = f"{port}\0".encode("utf-8")
                sock.sendall(user_name)

                # Recibir del servidor el int (0/1/2)
                # =====================================================
                res = readNumber(sock)

                # Respuesta
                if res == 0:
                    client._connected = True
                    client.start_server_thread(port)
                    print("c> CONNECT OK")
                elif res == 1:
                    print("c> USERNAME NOT EXIST")
                elif res == 2:
                    print("c> USER is ALREADY CONNECTED")
                else:
                    print("c> CONNECT FAIL")

            finally:
                # Cerrar conexión
                # =====================================================
                sock.close()
        except Exception as e:
            print("c> CONNECT FAIL: ", e)
            # Cerrar conexión si se ha abierto
            if "sock" in locals():
                sock.close()


                
    @staticmethod
    def disconnect(user):
        try:
            # Error si el suario no se ha conectado o registrado
            # =====================================================
            if client._connected is False and client._user is None:  # QUIT
                print('c> NO USER TO DISCONNECT')
                return
            
            if client._user is None or client._user != user:
                print("c> DISCONNECT FAIL")
                return
            
            else:
                # Conectar con el servidor
                # =====================================================
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                server_address = (client._server, int(client._port))
                print("connecting to {} port {}".format(*server_address))
                sock.connect(server_address)

                try:
                    # Enviar peticiones
                    # =====================================================
                    
                    # Enviar la cadena DISCONNECT\0
                    op = b"DISCONNECT\0"
                    sock.sendall(op)

                    # Enviar fecha
                    date = get_date()
                    op_date = (date + '\0').encode('utf-8')
                    sock.sendall(op_date)

                    # Enviar nombre de usuario (max 256)
                    user_name = f"{user}\0".encode("utf-8")
                    sock.sendall(user_name)

                    # Recibir del servidor int (0/1/2)
                    # =====================================================
                    res = readNumber(sock)

                    # Respuesta
                    if res == 0:
                        client._connected = False
                        #client._stop_flag = True
                        client.stop_server_thread()
                        print("c> DISCONNECT OK")
                    elif res == 1:
                        print("c> DISCONNECT FAIL / USER DOES NOT EXIST")
                    elif res == 2:
                        print("c> DISCONNECT FAIL / USER IS NOT CONNECTED")
                    else:
                        print("c> DISCONNECT FAIL")

                finally:
                    # Cerrar conexión
                    # =====================================================
                    sock.close()

        except Exception as e:
            print("c> DISCONNECT FAIL: ", e)
            # Cerrar conexión si se ha abierto
            if "sock" in locals():
                sock.close()

    
    @staticmethod
    def publish(fileName, description):
        try:
            # Error si el usuario no se ha conectado o registrado
            # =====================================================
            if client._user is None:
                print("c> CONNECT FIRST BEFORE PUBLISHING")
            else:
                # Conectar con el servidor
                # =====================================================
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                server_address = (client._server, int(client._port))
                print("connecting to {} port {}".format(*server_address))
                sock.connect(server_address)

                try:
                    # Enviar peticiones
                    # =====================================================
                    
                    # Enviar la cadena PUBLISH\0
                    op = b"PUBLISH\0"
                    sock.sendall(op)

                    # Enviar fecha
                    date = get_date()
                    op_date = (date + '\0').encode('utf-8')
                    sock.sendall(op_date)

                    # Enviar nombre de usuario (max 256)
                    user_name = f"{client._user}\0".encode("utf-8")
                    sock.sendall(user_name)

                    # Enviar nombre del archivo (max 256)
                    file_name = f"{fileName}\0".encode("utf-8")
                    sock.sendall(file_name)

                    # Enviar descripcion del archivo (max 256)
                    description = f"{description}\0".encode("utf-8")
                    sock.sendall(description)

                    # Recibir del servidor int (0/1/2)
                    # =====================================================
                    res = readNumber(sock)

                    # Respuesta
                    if res == 0:
                        print("c> PUBLISH OK")
                    elif res == 1:
                        print("c> PUBLISH FAIL, USER DOES NOT EXIST")
                    elif res == 2:
                        print("c> PUBLISH FAIL , USER NOT CONNECTED")
                    elif res == 3:
                        print("c> PUBLISH FAIL , CONTENT ALREADY PUBLISHED")
                    else:
                        print("c> PUBLISH FAIL")

                finally:
                    # Cerrar conexión
                    # =====================================================
                    sock.close()

        except Exception as e:
            print("c> PUBLISH FAIL: ", e)
            # Cerrar conexión si se ha abierto
            if "sock" in locals():
                sock.close()

    
    @staticmethod
    def delete(fileName):
        try:
            # Error si el usuario no se ha conectado o registrado
            # =====================================================
            if client._user is None:
                print("c> CONNECT FIRST BEFORE DELETE")
            else:
                # Conectar con el servidor
                # =====================================================
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                server_address = (client._server, int(client._port))
                print("connecting to {} port {}".format(*server_address))
                sock.connect(server_address)

                try:
                    # Enviar peticiones
                    # =====================================================
                    
                    # Enviar la cadena DELETE\0
                    op = b"DELETE\0"
                    sock.sendall(op)

                    # Enviar fecha
                    date = get_date()
                    op_date = (date + '\0').encode('utf-8')
                    sock.sendall(op_date)

                    # Enviar nombre de usuario (max 256)
                    user_name = f"{client._user}\0".encode("utf-8")
                    sock.sendall(user_name)

                    # Enviar nombre del fichero
                    file_name = f"{fileName}\0".encode("utf-8")
                    sock.sendall(file_name)

                    # Recibir del servidor int (0/1/2)
                    # =====================================================
                    res = readNumber(sock)

                    # Respuesta
                    if res == 0:
                        print("c> DELETE OK")
                    elif res == 1:
                        print("c> DELETE FAIL, USER DOES NOT EXIST")
                    elif res == 2:
                        print("c> DELETE FAIL , USER NOT CONNECTED")
                    elif res == 3:
                        print("c> DELETE FAIL , CONTENT NOT PUBLISHED")
                    else:
                        print("c> DELETE FAIL")

                finally:
                    # Cerrar conexión
                    # =====================================================
                    sock.close()

        except Exception as e:
            print("c> DELETE FAIL: ", e)
            # Cerrar conexión si se ha abierto
            if "sock" in locals():
                sock.close()

                
    
    @staticmethod
    def listusers():
        try:
            # Error si el usuario no se ha conectado o registrado
            # =====================================================
            if client._user is None:
                print("c> CONNECT FIRST BEFORE UNREGISTER")
            else:
                # Conectar con el servidor
                # =====================================================
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                server_address = (client._server, int(client._port))
                print("connecting to {} port {}".format(*server_address))
                sock.connect(server_address)

                try:
                    # Enviar peticiones
                    # =====================================================
                    
                    # Enviar la cadena REGISTER\0
                    op = b"LIST_USERS\0"
                    sock.sendall(op)

                    # Enviar fecha
                    date = get_date()
                    op_date = (date + '\0').encode('utf-8')
                    sock.sendall(op_date)

                    # Enviar nombre de usuario (max 256)
                    user_name = f"{client._user}\0".encode("utf-8")
                    sock.sendall(user_name)

                    # Recibir del servidor int (0/1/2)
                    # =====================================================
                    res = readNumber(sock)

                    # respuesta
                    if res == 0:
                        print("LIST_USERS OK")

                        # Recibir numero de usuarios
                        num_users = readNumber(sock)

                        # Recibir informacion de cada usuario
                        for i in range(num_users):

                            # Recibir usuario
                            user = ""
                            while True:
                                msg = sock.recv(1)
                                if msg == b"\0":
                                    break
                                user += msg.decode()

                            # Recibir IP
                            ip = ""
                            while True:
                                msg = sock.recv(1)
                                if msg == b"\0":
                                    break
                                ip += msg.decode()

                            # Recibir puerto
                            port = ""
                            while True:
                                msg = sock.recv(1)
                                if msg == b"\0":
                                    break
                                port += msg.decode()

                            # Imprimir informacion del usuario
                            print(f"\t {user} {ip} {port}")
                            user_info = [user, ip, port]
                            client._list_users.append(user_info)

                    elif res == 1:
                        print("c> LIST_USERS FAIL, USER DOES NOT EXIST")
                    elif res == 2:
                        print("c> LIST_USERS FAIL , USER NOT CONNECTED")
                    else:
                        print("c> LIST_USERS FAIL")

                finally:
                    # Cerrar conexión
                    # =====================================================
                    sock.close()

        except Exception as e:
            print("c> LIST_USERS FAIL: ", e)
            # Cerrar conexión si se ha abierto
            if "sock" in locals():
                sock.close()

    
    @staticmethod
    def listcontent(user2):
        try:
            # Error si el usuario no se ha conectado o registrado
            # =====================================================
            if client._user is None:
                print("c> CONNECT FIRST BEFORE LIST_CONTENT")
            else:
                # Conectar con el servidor
                # =====================================================
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                server_address = (client._server, int(client._port))
                print("connecting to {} port {}".format(*server_address))
                sock.connect(server_address)

                try:
                    # Enviar peticiones
                    # =====================================================
                    
                    # Enviar la cadena LIST_CONTENT\0
                    op = b"LIST_CONTENT\0"
                    sock.sendall(op)

                    # Enviar fecha
                    date = get_date()
                    op_date = (date + '\0').encode('utf-8')
                    sock.sendall(op_date)

                    # Enviar nombre de usuario (max 256)
                    user_name = f"{client._user}\0".encode("utf-8")
                    sock.sendall(user_name)

                    # Enviar nombre de usuario 2 (max 256)
                    user_name2 = f"{user2}\0".encode("utf-8")
                    sock.sendall(user_name2)

                    # Recibir del servidor int (0/1/2/3/4)
                    # =====================================================
                    res = readNumber(sock)

                    # Respuesta
                    if res == 0:
                        print("c> LIST_CONTENT OK")

                        # Recibir numero de ficheros
                        num_files = readNumber(sock)

                        # Recibir informacion de cada usuario
                        for i in range(num_files):

                            # Recibir file_name
                            file_name = ""
                            while True:
                                msg = sock.recv(1)
                                if msg == b"\0":
                                    break
                                file_name += msg.decode()

                            # Recibir description
                            description = ""
                            while True:
                                msg = sock.recv(1)
                                if msg == b"\0":
                                    break
                                description += msg.decode()

                            # Imprimir informacion del usuario
                            print(f'\t {file_name} "{description}"')

                    elif res == 1:
                        print("c> LIST_CONTENT, USER DOES NOT EXIST")
                    elif res == 2:
                        print("c> LIST_CONTENT , USER NOT CONNECTED")
                    elif res == 3:
                        print("c> LIST_CONTENT, USER2 DOES NOT CONNECT")
                    else:
                        print("c> LIST_CONTENT FAIL")

                finally:
                    # Cerrar conexión
                    # =====================================================
                    sock.close()

        except Exception as e:
            print("c> LIST_CONTENT FAIL: ", e)
            # Cerrar conexión si se ha abierto
            if "sock" in locals():
                sock.close()

    
    @staticmethod
    def getfile(user, remote_FileName, local_FileName):
        try:
            if client._user is None:
                print("c> CONNECT FIRST BEFORE GET_FILE")
            else:
                if client._list_users == []:
                    print("c> LIST_USERS FIRST BEFORE GET_FILE")
                    return
                # Conectar con el cliente 2
                found = False
                for user_info in client._list_users:
                    if user_info[0] == user:
                        found = True
                        ip_client = user_info[1]
                        port_client = int(user_info[2])

                if found is False:
                    print("c> GET_FILE FAIL")
                    return
                    
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                server_address = (ip_client, int(port_client))
                print("connecting to {} port {}".format(*server_address))
                sock.connect(server_address)

                try:
                    # Enviar la cadena GET_FILE\0
                    op = b"GET_FILE\0"
                    sock.sendall(op)

                    # Enviar nombre fichero a descargar
                    file_name = f"{remote_FileName}\0".encode("utf-8")
                    sock.sendall(file_name)

                    # Recibir del cliente2 int (0/1/2)
                    res = readNumber(sock)

                    if res == 0:

                        # Recibir los datos del archivo
                        file_data = b""
                        while True:
                            data = sock.recv(1024)
                            if not data:
                                break
                            file_data += data

                        # Escribir los datos en el archivo de la copia
                        with open(local_FileName, "wb") as file:
                            file.write(file_data)

                        print("c> GET_FILE OK")

                    elif res == 1:
                        print("c> GET_FILE FAIL / FILE NOT EXIST")
                    else:
                        print("c> GET_FILE FAIL")
                finally:
                    # Cerrar conexión
                    sock.close()

        except Exception as e:
            print("c> GET_FILE FAIL: ", e)
            # Cerrar conexión si se ha abierto
            if "sock" in locals():
                sock.close()

    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():

        while True:
            try:
                command = input("c> ")
                line = command.split(" ")
                if len(line) > 0:

                    line[0] = line[0].upper()

                    if line[0] == "REGISTER":
                        if len(line) == 2:
                            client.register(line[1])
                        else:
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif line[0] == "UNREGISTER":
                        if len(line) == 2:
                            client.unregister(line[1])
                        else:
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif line[0] == "CONNECT":
                        if len(line) == 2:
                            client.connect(line[1])
                        else:
                            print("Syntax error. Usage: CONNECT <userName>")

                    elif line[0] == "PUBLISH":
                        if len(line) >= 2:
                            #  Remove first two words
                            description = " ".join(line[2:])
                            client.publish(line[1], description)

                        else:
                            print(
                                "Syntax error. Usage: PUBLISH <fileName> <description>"
                            )

                    elif line[0] == "DELETE":
                        if len(line) == 2:
                            client.delete(line[1])
                        else:
                            print("Syntax error. Usage: DELETE <fileName>")

                    elif line[0] == "LIST_USERS":
                        if len(line) == 1:
                            client.listusers()
                        else:
                            print("Syntax error. Use: LIST_USERS")

                    elif line[0] == "LIST_CONTENT":
                        if len(line) == 2:
                            client.listcontent(line[1])
                        else:
                            print("Syntax error. Usage: LIST_CONTENT <userName>")

                    elif line[0] == "DISCONNECT":
                        if len(line) == 2:
                            client.disconnect(line[1])
                        else:
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif line[0] == "GET_FILE":
                        if len(line) == 4:
                            client.getfile(line[1], line[2], line[3])
                        else:
                            print(
                                "Syntax error. Usage: GET_FILE <userName> <remote_fileName> <local_fileName>"
                            )

                    elif line[0] == "QUIT":
                        if len(line) == 1:
                            client.disconnect(client._user)
                            break
                        else:
                            print("Syntax error. Use: QUIT")
                    else:
                        print("Error: command " + line[0] + " not valid.")
            except Exception as e:
                print("Exception: " + str(e))

    # *
    # * @brief Prints program usage
    @staticmethod
    def usage():
        print("Usage: python3 client.py -s <server> -p <port>")

    
    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def parseArguments(argv):
        parser = argparse.ArgumentParser()
        parser.add_argument("-s", type=str, required=True, help="Server IP")
        parser.add_argument("-p", type=int, required=True, help="Server Port")
        args = parser.parse_args()

        if args.s is None:
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if (args.p < 1024) or (args.p > 65535):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535")
            return False

        client._server = args.s
        client._port = args.p
        return True

    
    # ******************** MAIN *********************
    @staticmethod
    def main(argv):
        if not client.parseArguments(argv):
            client.usage()
            return

        #  Write code here
        client.shell()
        print("+++ FINISHED +++")


if __name__ == "__main__":
    client.main([])
