from enum import Enum
import argparse
import socket
import threading
import shutil
import threading
import os
import zeep

class client :

    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum) :
        OK = 0
        ERROR = 1
        USER_ERROR = 2

    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1
    _register_user = None
    _conected_user = None
    _conectado = False
    _hilo_clientes = None
    mutex_cliente = threading.Lock()
    # Obtener la ruta del directorio del script actual
    script_dir = os.path.dirname(os.path.abspath(__file__)) 

    # ******************** SERVIDOR WEB ********************
    wsdl_url = None
    soap = None
    # ******************** METHODS *******************


    @staticmethod
    def  register(user) :
        if(client._conected_user):
            print("c> REGISTER FAIL: TIENES QUE DESCONECTARTE PARA PODER REGISTRATE CON OTRO USUARIO")
            return client.RC.ERROR
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if client._server!=None and client._port != -1:
            server_address = (client._server, client._port)
            try:
                sock.connect(server_address)

                # Enviar la operación REGISTER al servidor
                operation = "REGISTER"
                sock.sendall(operation.encode() + b' ')
                current_time = client.soap.service.get_current_time()
                sock.sendall(current_time.encode()+ b' ')

                user = str(user)
                # Enviar el nombre del usuario.
                sock.sendall(user.encode() + b'\0')

                # Recibir el resultado de la operación por parte del servidor. Se recibe un número,4 bytes.
                msg = sock.recv(4)
                if len(msg) == 4:
                    value = int.from_bytes(msg, byteorder='big')  # Convertir los 4 bytes a un entero
                else:
                    value = 2

                # Interpretar el resultado 
                if value == 0:
                    #Si es cero OK.
                    client._register_user = str(user)
                    # Crear una carpeta en el directorio actual con el nombre del usuario
                    folder_name = client._register_user
                    os.makedirs(folder_name)
                    print("c> REGISTER OK")
                elif value == 1:
                    #Si es uno ya está registrado.
                    print("c> USERNAME IN USE")
                else:
                    #Caso de ERROR.
                    print("c> REGISTER FAIL")
            except Exception as e:
                print("c> Exception:", e)
            finally:
                sock.close()

        else:
            print("c> ERROR puerto o IP no definido.")
        return client.RC.ERROR

   
    @staticmethod
    def  unregister(user) :
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if client._server!=None and client._port != -1:
            server_address = (client._server, client._port)
            try:
                sock.connect(server_address)

                # Enviar la operación UNREGISTER al servidor
                operation = "UNREGISTER"
                sock.sendall(operation.encode()+ b' ')
                current_time = client.soap.service.get_current_time()
                sock.sendall(current_time.encode()+ b' ')
                user = str(user)
                # Enviar el nombre del usuario.
                sock.sendall(user.encode() + b'\0')
                # Recibir el resultado de la operación por parte del servidor.
                msg = sock.recv(4)
                if len(msg) == 4:
                    value = int.from_bytes(msg, byteorder='big')  # Convertir los 4 bytes a un entero
                else:
                    value = 2
                # Interpretar el resultado 
                if value == 0:
                    #Si es cero OK.
                    folder_path = os.path.join(client.script_dir, user)
                    # Eliminar el directorio folder_path
                    if os.path.exists(folder_path):
                        shutil.rmtree(folder_path)
                        client._register_user = None
                        client._conected_user = None
                        print("c> UNREGISTER OK")
                    else:
                        print("c> UNREGISTER FAIL: NO ESXITES ")
    
                elif value == 1:
                    #Si es uno ya está registrado.
                    print("c> USERNAME DOES NOT EXIST")
                else:
                    #Caso de ERROR.
                    print("c> UNREGISTER FAIL")
            except Exception as e:
                print("c> Exception:", e)
            finally:
                sock.close()

        else:
            print("c> ERROR puerto o IP no definido.")
        return client.RC.ERROR
    
    @staticmethod
    def atender_cliente(client_socket):
        #recibe el nombre del archivo
        archivo = b""
        while True:
            data = client_socket.recv(1)
            if data == b"\0":
                break
            archivo += data
        archivo = archivo.decode('utf-8')

        folder_path = os.path.join(client.script_dir, client._register_user)
        
        #Si el archivo existe lo manda
        if os.path.exists(os.path.join(folder_path, archivo)):
            lock = threading.Lock()
            lock.acquire()
            resultado = 0
            # Enviar el resultado al cliente
            result_bytes = resultado.to_bytes(4, byteorder='big')
            client_socket.sendall(result_bytes)
            # Enviar el tamaño del fichero
            size = os.path.getsize(os.path.join(folder_path, archivo))
            

            size_bytes = size.to_bytes(4, byteorder='big')
            client_socket.sendall(size_bytes)

            # Enviar el contenido del fichero
            with open(os.path.join(folder_path, archivo), 'rb') as f:
                data = f.read(1024)
                while data:
                    client_socket.sendall(data)
                    data = f.read(1024)
            lock.release()

        else:
            #si no existe devuelve 1
            resultado = 1
            # Enviar el resultado al cliente
            result_bytes = resultado.to_bytes(4, byteorder='big')
            client_socket.sendall(result_bytes)


    @staticmethod
    def atender_peticiones(ServerSocket):
        
        ServerSocket.listen(5)  # Esperar conexiones entrantes
        client.mutex_cliente.acquire()
        client._conectado = True
        client.mutex_cliente.release()

        while client._conectado:
            client_socket, addr = ServerSocket.accept()
            # Manejar la conexión en un hilo separado
            client_thread = threading.Thread(target=client.atender_cliente, args=(client_socket,))
            client_thread.daemon = True
            client_thread.start()    
        
        
    @staticmethod
    def  connect(user) :
        if(client._conected_user):
            print("c> CONNECT FAIL: TIENES QUE DESCONECTARTE PARA PODER CONECTARTE CON OTRO USUARIO")
            return client.RC.ERROR
        #Buscar un puerto libre. Para eso creamos un socket y lo aginamos a un puerto válido.
        ServerSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        ServerSocket.bind(('localhost', 0))  # Enlazar a cualquier dirección disponible
        addr, port = ServerSocket.getsockname()  # Obtener el puerto asignado

        #Enviar la solicitud de conexión al servidor
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if client._server!=None and client._port != -1:
            server_address = (client._server, client._port)
            try:
                sock.connect(server_address)

                # Enviar la operación CONNECT al servidor
                operation = "CONNECT"
                sock.sendall(operation.encode()+ b' ')
                current_time = client.soap.service.get_current_time()
                sock.sendall(current_time.encode()+ b' ')
                user = str(user)
                # Enviar el nombre del usuario.
                sock.sendall(user.encode()+ b' ')
                # Enviar la dirección IP del cliente.
                sock.sendall(addr.encode() + b' ')
                # Enviar el puerto asignado.
                puerto = str(port)
                sock.sendall(puerto.encode() + b'\0')
                # Recibir el resultado de la operación por parte del servidor.
                msg = sock.recv(4)
                if len(msg) == 4:
                    value = int.from_bytes(msg, byteorder='big')  # Convertir los 4 bytes a un entero
                else:
                    value = 3
                # Interpretar el resultado 
                if value == 0:
                    #Si es cero OK.
                    client._register_user = str(user)
                    client._conected_user = str(user)
                    print("c> CONNECT OK")
                     #Crear un hilo para atender peticiones
                    client._hilo_clientes = threading.Thread(target=client.atender_peticiones, args=(ServerSocket,))
                    client._hilo_clientes.daemon = True
                    client._hilo_clientes.start()
                    
                elif value == 1:
                    #Si es uno ya está registrado.
                    print("c> CONNECT FAIL: USERNAME DOES NOT EXIST")
                elif value == 2:
                    #CSi el usuario ya está conectado.
                    print("c> USER ALREADY CONNECTED")
                else:
                    #Caso de ERROR.
                    print("c> CONNECT FAIL")

            except Exception as e:
                print("c> Exception:", e)
            finally:
                sock.close()

        else:
            print("c> ERROR puerto o IP no definido.")
        return client.RC.ERROR

    
    @staticmethod
    def  disconnect(user) :
        if(client._conected_user == None):
            print("c> DISCONNECT FAIL, USER NOT CONNECTED")
            return client.RC.ERROR
        if(client._conected_user != user):
            print("c> DISCONNECT FAIL, USER NOT CONNECTED")
            return client.RC.ERROR
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if client._server!=None and client._port != -1:
            server_address = (client._server, client._port)
            try:
                sock.connect(server_address)

                # Enviar la operación DISCONNECT al servidor
                operation = "DISCONNECT"
                sock.sendall(operation.encode()+ b' ')
                current_time = client.soap.service.get_current_time()
                sock.sendall(current_time.encode()+ b' ')
                user = str(user)
                # Enviar el nombre del usuario.
                sock.sendall(user.encode()+ b'\0')
                msg = sock.recv(4)
                if len(msg) == 4:
                    value = int.from_bytes(msg, byteorder='big')  # Convertir los 4 bytes a un entero
                else:
                    value = 3
                # Interpretar el resultado 
                if value == 0:
                    client._conected_user = None
                    client.mutex_cliente.acquire()
                    client._conectado = False
                    client.mutex_cliente.release()
                    print("c> DISCONNECT OK")
                    
                elif value == 1:
                    #Si es uno ya está registrado.
                    print("c> DISCONNECT FAIL: USERNAME DOES NOT EXIST")
                elif value == 2:
                    #Si el usuario no está conectado.
                    print("c> DISCONNECT FAIL, USER NOT CONNECTED")
                else:
                    #Caso de ERROR.
                    print("c> DISCONNECT FAIL")
            except Exception as e:
                print("c> Exception:", e)
            finally:
                sock.close()
        else:
            print("c> ERROR puerto o IP no definido.")
        return client.RC.ERROR

    @staticmethod
    def  publish(fileName,  description) :
         # Construir la ruta relativa desde el directorio del script
        if(client._register_user == None):
            print("c> PUBLISH FAIL: NO SE HA IDENTIFICADO NINGÚN USUARIO.")
            return client.RC.ERROR
        folder_path = os.path.join(client.script_dir, client._register_user)
        
        if os.path.exists(os.path.join(folder_path, fileName)):
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            if client._server!=None and client._port != -1:
                server_address = (client._server, client._port)
                try:
                    sock.connect(server_address)

                    # Enviar la operación PUBLISH al servidor
                    operation = "PUBLISH"
                    sock.sendall(operation.encode()+ b' ')
                    current_time = client.soap.service.get_current_time()
                    sock.sendall(current_time.encode()+ b' ')
                    user = client._register_user 
                    # Enviar el nombre del usuario.
                    sock.sendall(user.encode()+ b' ')
                    # Enviar el nombre del fichero.
                    namefl = str(fileName)
                    sock.sendall(namefl.encode() + b' ')
                    # Enviar la descripción.
                    namefl = str(description)
                    sock.sendall(description.encode() + b'\0')

                    msg = sock.recv(4)
                    if len(msg) == 4:
                        value = int.from_bytes(msg, byteorder='big')  # Convertir los 4 bytes a un entero
                    else:
                        value = 4
                    # Interpretar el resultado 
                    if value == 0:
                        print("c> PUBLISH OK")
                    elif value == 1:
                        #Si es uno ya está registrado.
                        print("c> PUBLISH FAIL: USERNAME DOES NOT EXIST")
                    elif value == 2:
                        #Si el usuario no está conectado.
                        print("c> PUBLISH FAIL, USER NOT CONNECTED")
                    elif value == 3:
                        #Si el usuario no está conectado.
                        print("c> PUBLISH FAIL, CONTENT ALREADY PUBLISHED")
                    else:
                        #Caso de ERROR.
                        print("c> PUBLISH FAIL")
                except Exception as e:
                    print("c> Exception:", e)
                finally:
                    sock.close()
            else:
                print("c> ERROR puerto o IP no definido.")
        else:
            print("c> PUBLISH FAIL: FILE DOES NOT EXIST")
        return client.RC.ERROR

    @staticmethod
    def delete(fileName) :
        if(client._register_user == None):
            print("c> DELETE FAIL: NO SE HA IDENTIFICADO NINGÚN USUARIO.")
            return client.RC.ERROR
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if client._server!=None and client._port != -1:
            server_address = (client._server, client._port)
            
            try:
                sock.connect(server_address)

                # Enviar la operación DELETE al servidor
                operation = "DELETE"
                sock.sendall(operation.encode()+ b' ')
                current_time = client.soap.service.get_current_time()
                sock.sendall(current_time.encode()+ b' ')
                user = client._register_user 
                # Enviar el nombre del usuario.
                sock.sendall(user.encode()+ b' ')
                # Enviar el nombre del fichero.
                namefl = str(fileName)
                sock.sendall(namefl.encode() + b'\0')            

                msg = sock.recv(4)
                if len(msg) == 4:
                    value = int.from_bytes(msg, byteorder='big')  # Convertir los 4 bytes a un entero
                else:
                    value = 4
                # Interpretar el resultado 
                if value == 0:
                    print("c> DELETE OK")
                    # Obtener la ruta completa del archivo
                    folder_path = os.path.join(client.script_dir, client._register_user)
                    file_path = os.path.join(folder_path, fileName)

                    # Verificar si el archivo existe
                    if os.path.exists(file_path):
                        # Borrar el archivo
                        os.remove(file_path)
                    else:
                        print("c> DELETE FAIL")
                elif value == 1:
                    #Si es uno ya está registrado.
                    print("c> DELETE FAIL: USERNAME DOES NOT EXIST")
                elif value == 2:
                    #Si el usuario no está conectado.
                    print("c> DELETE FAIL, USER NOT CONNECTED")
                elif value == 3:
                    #Si el usuario no está conectado.
                    print("c> DELETE FAIL, CONTENT NOT PUBLISHED")
                else:
                    #Caso de ERROR.
                    print("c> DELETE FAIL")
            except Exception as e:
                print("c> Exception:", e)
            finally:
                sock.close()
        else:
            print("c> ERROR puerto o IP no definido.")
        return client.RC.ERROR

    @staticmethod
    def  listusers() :
        if(client._register_user == None):
            print("c> LIST_USER FAIL: NO SE HA IDENTIFICADO NINGÚN USUARIO.")
            return client.RC.ERROR
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if client._server!=None and client._port != -1:
            server_address = (client._server, client._port)
            
            try:
                sock.connect(server_address)

                # Enviar la operación LIST_USER al servidor
                operation = "LIST_USERS"
                sock.sendall(operation.encode()+ b' ')
                current_time = client.soap.service.get_current_time()
                sock.sendall(current_time.encode() + b' ')
                user = client._register_user 
                # Enviar el nombre del usuario.
                sock.sendall(user.encode()+ b'\0')         

                msg = sock.recv(4)
             
                if len(msg) == 4:
                    value = int.from_bytes(msg, byteorder='big')  # Convertir los 4 bytes a un entero
                else:
                    value = 3
                # Interpretar el resultado 
                if value == 0:
                   
                    # Recibir la cantidad de usuarios
                    num_users = sock.recv(4)
                    if len(num_users) == 4:
                        usuarios = int.from_bytes(num_users, byteorder='big')  # Convertir los 4 bytes a un entero
                        print("c > LIST_USERS OK")
                        # Recibir la información de cada usuario
                        for _ in range(usuarios):
                        # Recibir las cadenas, leyendo hasta el delimitador
                            username = b""
                            while True:
                                data = sock.recv(1)
                                if data == b"\0":
                                    break
                                username += data
                            username = username.decode('utf-8')

                            ip = b""
                            while True:
                                data = sock.recv(1)
                                if data == b"\0":
                                    break
                                ip += data
                            ip = ip.decode('utf-8')

                            port = b""
                            while True:
                                data = sock.recv(1)
                                if data == b"\0":
                                    break
                                port += data
                            port = port.decode('utf-8')
                            #imprime cada usuario
                            print("\t", username, ' ', ip, ' ', port, "\n")
                    else:
                        print("c> LIST_USERS FAIL")
                      
                elif value == 1:
                    #Si es uno ya está registrado.
                    print("c> LIST_USERS FAIL: USER DOES NOT EXIST")
                elif value == 2:
                    #Si el usuario no está conectado.
                    print("c> LIST_USERS FAIL: USER NOT CONNECTED")
                else:
                    #Caso de ERROR.
                    print("c> LIST_USERS FAIL")
            except Exception as e:
                print("c > Exception:", e)
            finally:
                sock.close()
        else:
            print("c> ERROR puerto o IP no definido.")
        return client.RC.ERROR

    @staticmethod
    def  listcontent(user) :
        if(client._register_user == None):
            print("c> LIST_USER FAIL: NO SE HA IDENTIFICADO NINGÚN USUARIO.")
            return client.RC.ERROR
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if client._server!=None and client._port != -1:
            server_address = (client._server, client._port)
            
            try:
                sock.connect(server_address)

                # Enviar la operación LIST_CONTENT al servidor
                operation = "LIST_CONTENT"
                sock.sendall(operation.encode()+ b' ')
                current_time = client.soap.service.get_current_time()
                sock.sendall(current_time.encode()+ b' ')
                user_actual = client._register_user 
                # Enviar el nombre del usuario.
                sock.sendall(user_actual.encode()+ b' ') 
                #Enviar el nombre del usuario al que se quiere ver el contenido.
                user = str(user)
                sock.sendall(user.encode() + b'\0')     

                msg = sock.recv(4)
            
                if len(msg) == 4:
                    value = int.from_bytes(msg, byteorder='big')  # Convertir los 4 bytes a un entero
                    
                else:
                    value = 4
                # Interpretar el resultado 
                if value == 0:
                    
                    # Recibir la cantidad de usuarios
                    num_files = sock.recv(4)
                    if len(num_files) == 4:
                        files = int.from_bytes(num_files, byteorder='big')  # Convertir los 4 bytes a un entero
                        print("c> LIST_CONTENT OK")
                        # Recibir la información de cada usuario
                        for _ in range(files):
                        # Recibir las cadenas reales, leyendo hasta el delimitador
                            namefile = b""
                            while True:
                                data = sock.recv(1)
                                if data == b"\0":
                                    break
                                namefile += data
                            namefile = namefile.decode('utf-8')

                            des = b""
                            while True:
                                data = sock.recv(1)
                                if data == b"\0":
                                    break
                                des += data
                            des = des.decode('utf-8')

                            print("\t", namefile, ' ', '"', des, '"', "\n")
                    else:
                        print("c> LIST_CONTENT FAIL")
                      
                elif value == 1:
                    #Si es uno ya está registrado.
                    print("c> LIST_CONTENT FAIL: USER DOES NOT EXIST")
                elif value == 2:
                    #Si el usuario no está conectado.
                    print("c> LIST_CONTENT FAIL, USER NOT CONNECTED")
                elif value == 3:
                    #Si el usuario no está conectado.
                    print("c> LIST_CONTENT FAIL, REMOTE USER DOES NOT EXIST")
                else:
                    #Caso de ERROR.
                    print("c> LIST_CONTENT FAIL")
            except Exception as e:
                print("c> Exception:", e)
            finally:
                sock.close()
        else:
            print("c> ERROR puerto o IP no definido.")
        return client.RC.ERROR

    @staticmethod
    def  getfile(user,  remote_FileName,  local_FileName) :
        if(client._conected_user == None):
            print("c> GET_FILE FAIL: NO SE HA CONECTADO NINGÚN USUARIO.")
            return client.RC.ERROR
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if client._server!=None and client._port != -1:
            server_address = (client._server, client._port)
            try:
                sock.connect(server_address)

                # Enviar la operación GET_FILE al servidor
                operation = "GET_FILE"
                sock.sendall(operation.encode()+ b' ')
                current_time = client.soap.service.get_current_time()
                sock.sendall(current_time.encode()+ b' ')
                user_actual = client._register_user 
                # Enviar el nombre del usuario.
                sock.sendall(user_actual.encode()+ b' ')
                #Enviar el nombre del usuario al que se quiere ver el contenido.
                user = str(user)
                sock.sendall(user.encode() + b'\0')       

                msg = sock.recv(4)

                if len(msg) == 4:
                    value = int.from_bytes(msg, byteorder='big')  # Convertir los 4 bytes a un entero
                    
                else:
                    value = 5
                # Interpretar el resultado 
                if value == 0:
                    # Recibir el tamaño del fichero
                    ip = b""
                    while True:
                        data = sock.recv(1)
                        if data == b"\0":
                            break
                        ip += data
                    ip = ip.decode('utf-8')

                    port = b""
                    while True:
                        data = sock.recv(1)
                        if data == b"\0":
                            break
                        port += data
                    port = port.decode('utf-8')
                    sock_cl = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    if ip!= None and port != None:
                        cl_address = (ip, int(port))
                        try:
                            sock_cl.connect(cl_address)

                            archivo = str(remote_FileName)
                            # Enviar el nombre del fichero.
                            sock_cl.sendall(archivo.encode() + b'\0')

                            msg = sock_cl.recv(4)
                            if len(msg) == 4:
                                value = int.from_bytes(msg, byteorder='big')  # Convertir los 4 bytes a un entero
                            else:
                                value = 3
                            # Interpretar el resultado
                            if value == 0:
                                # Recibir el tamaño del fichero
                                size = sock_cl.recv(4)
                                if len(size) == 4:
                                    size = int.from_bytes(size, byteorder='big')
                                    # Recibir el contenido del fichero
                                    folder_path = os.path.join(client.script_dir, client._register_user)
                                    file_path = os.path.join(folder_path, local_FileName)
                                    with open(file_path, 'wb') as f:
                                        while size > 0:
                                            data = sock_cl.recv(1024)
                                            f.write(data)
                                            size -= len(data)
                                        
                                    print("c> GET_FILE OK")
                                else:
                                    print("c> GET_FILE FAIL")                                
                            elif value == 1:
                                #Caso de ERROR.
                                print("c> GET_FILE FAIL: FILE NOT EXISTS")
                            else: 
                                #Caso de ERROR.
                                print("c> GET_FILE FAIL")
        

                        except Exception as e:
                            print("c> Exception:", e)
                        finally:
                            sock_cl.close()

                else:
                    #Caso de ERROR.
                    print("c> GET_FILE FAIL")
            except Exception as e:
                print("c> Exception:", e)
            finally:
                sock.close()
        else:
            print("c> ERROR puerto o IP no definido.")
        return client.RC.ERROR 

    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():

        while (True) :
            try :
                command = input("c> ")
                line = command.split(" ")
                if (len(line) > 0):

                    line[0] = line[0].upper()

                    if (line[0]=="REGISTER") :
                        if (len(line) == 2) :
                            client.register(line[1])
                        else :
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif(line[0]=="UNREGISTER") :
                        if (len(line) == 2) :
                            client.unregister(line[1])
                        else :
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif(line[0]=="CONNECT") :
                        if (len(line) == 2) :
                            client.connect(line[1])
                        else :
                            print("Syntax error. Usage: CONNECT <userName>")
                    
                    elif(line[0]=="PUBLISH") :
                        if (len(line) >= 3) :
                            #  Remove first two words
                            description = ' '.join(line[2:])
                            client.publish(line[1], description)
                        else :
                            print("Syntax error. Usage: PUBLISH <fileName> <description>")

                    elif(line[0]=="DELETE") :
                        if (len(line) == 2) :
                            client.delete(line[1])
                        else :
                            print("Syntax error. Usage: DELETE <fileName>")

                    elif(line[0]=="LIST_USERS") :
                        if (len(line) == 1) :
                            client.listusers()
                        else :
                            print("Syntax error. Use: LIST_USERS")

                    elif(line[0]=="LIST_CONTENT") :
                        if (len(line) == 2) :
                            client.listcontent(line[1])
                        else :
                            print("Syntax error. Usage: LIST_CONTENT <userName>")

                    elif(line[0]=="DISCONNECT") :
                        if (len(line) == 2) :
                            client.disconnect(line[1])
                        else :
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif(line[0]=="GET_FILE") :
                        if (len(line) == 4) :
                            client.getfile(line[1], line[2], line[3])
                        else :
                            print("Syntax error. Usage: GET_FILE <userName> <remote_fileName> <local_fileName>")

                    elif(line[0]=="QUIT") :
                        if (len(line) == 1) :
                            client.disconnect(client._register_user)
                            break
                        else :
                            print("Syntax error. Use: QUIT")
                    else :
                        print("Error: command " + line[0] + " not valid.")
            except Exception as e:
                print("Exception: " + str(e))

    # *
    # * @brief Prints program usage
    @staticmethod
    def usage() :
        print("Usage: python3 client.py -s <server> -p <port>")


    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def  parseArguments(argv) :
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        parser.add_argument('-w', type=str, required=True, help='Server Web IP')
        args = parser.parse_args()

        if (args.s is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if (args.w is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535");
            return False
        
        client._server = args.s
        client._port = args.p
        client.wsdl_url = "http://" + args.w + ":8000/?wsdl"
        client.soap = zeep.Client(wsdl=client.wsdl_url) 
        return True


    # ******************** MAIN *********************
    @staticmethod
    def main(argv) :
        if (not client.parseArguments(argv)) :
            print("Arguments Error")
            client.usage()
            return
        #  Write code here
        client.shell()
        print("+++ FINISHED +++")
    

if __name__=="__main__":
    client.main([])