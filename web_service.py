import time
from datetime import datetime

from spyne import Application, ServiceBase, Integer, Unicode, rpc
from spyne.protocol.soap import Soap11
from spyne.server.wsgi import WsgiApplication

# Definición de la clase Registro que hereda de ServiceBase
class Registro(ServiceBase):

    # Método RPC que devuelve la fecha y hora actual formateada como cadena
    @rpc(_returns=Unicode)
    def get_date(ctx):
        return datetime.now().strftime("%d/%m/%Y %H:%M:%S")

# Creación de una instancia de la aplicación Spyne
application = Application(
    services=[Registro],  # Se especifica que el servicio disponible es Registro
    tns='http://tests.python-zeep.org/',  # Espacio de nombres del servicio
    in_protocol=Soap11(validator='lxml'),  # Protocolo de entrada SOAP 1.1 con validación XML
    out_protocol=Soap11()  # Protocolo de salida SOAP 1.1
)

# Envoltura de la aplicación Spyne en una aplicación WSGI (Web Server Gateway Interface)
application = WsgiApplication(application)

if __name__ == '__main__':
    import logging

    from wsgiref.simple_server import make_server

    # Configuración del registro de eventos
    logging.basicConfig(level=logging.DEBUG)
    logging.getLogger('spyne.protocol.xml').setLevel(logging.DEBUG)

    # Mensajes informativos sobre la dirección y el WSDL de la aplicación
    logging.info("Escuchando en http://127.0.0.1:8000")
    logging.info("WSDL disponible en: http://localhost:8000/?wsdl")

    # Creación de un servidor WSGI
    server = make_server('127.0.0.1', 8000, application)
    # Inicio del servidor para atender solicitudes de manera indefinida
    server.serve_forever()

