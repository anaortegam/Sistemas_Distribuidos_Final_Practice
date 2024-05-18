import time
import datetime
import logging
import argparse
from spyne import Application, ServiceBase, Integer, Unicode, rpc
from spyne.protocol.soap import Soap11
from spyne.server.wsgi import WsgiApplication
from wsgiref.simple_server import make_server

class Hora(ServiceBase):

    @rpc(_returns=Unicode)
    def get_current_time(ctx):
        current_time = datetime.datetime.now().strftime("%d/%m/%Y %H:%M:%S")
        return current_time

def main():
    parser = argparse.ArgumentParser(description="SOAP Server with dynamic IP binding")
    parser.add_argument('-i', '--ip', type=str, required=True, help='Server IP address')
    args = parser.parse_args()

    application = Application(
        services=[Hora],
        tns='http://tests.python-zeep.org/',
        in_protocol=Soap11(validator='lxml'),
        out_protocol=Soap11())

    application = WsgiApplication(application)

    server = make_server(args.ip, 8000, application)
    server.serve_forever()

if __name__ == '__main__':
    main()


