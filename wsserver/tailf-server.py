#!/usr/bin/python -u
from twisted.internet import reactor, protocol
from autobahn.twisted.websocket import WebSocketServerFactory, WebSocketServerProtocol,listenWS
from datetime import datetime
import sys
#startLogging(sys.stdout) 

PORT='50000'

logs = ['/home/pi/rpilogger/log/ads.log',
        '/home/pi/rpilogger/checker.log']
        #'/var/log/syslog',
        #'/var/log/daemon.log',
        #'/var/log/auth.log',
        #'/var/log/fail2ban.log',
        #'/home/pi/tardir.log',
        #'/home/pi/wsserver/log/main/current']
logarr=[]
for log in logs:
    logarr.append('-f')
    logarr.append(log)


class ProcessProtocol(protocol.ProcessProtocol):
    """ I handle a child process launched via reactor.spawnProcess.
    I just buffer the output into a list and call WebSocketProcessOutputterThingFactory.broadcast when
    any new output is read
    """
    def __init__(self, websocket_factory):
        self.ws = websocket_factory
        self.buffer = []

    def outReceived(self, message):
        self.ws.broadcast(message)
        self.buffer.append(message)
        self.buffer = self.buffer[-20:] # Last 10 messages please

    def errReceived(self, data):
        print "Error: %s" % data


class WebSocketProcessOutputterThing(WebSocketServerProtocol):
    """ I handle a single connected client. We don't need to do much here, simply call the register and un-register
    functions when needed.
    """
    def onOpen(self):
        self.factory.register(self)
        for line in self.factory.process.buffer:
            self.sendMessage(line)

    def connectionLost(self, reason):
        WebSocketServerProtocol.connectionLost(self, reason)
        self.factory.unregister(self)


class WebSocketProcessOutputterThingFactory(WebSocketServerFactory):
    """ I maintain a list of connected clients and provide a method for pushing a single message to all of them.
    """
    protocol = WebSocketProcessOutputterThing

    def __init__(self, *args, **kwargs):
        WebSocketServerFactory.__init__(self, *args, **kwargs)
        self.clients = []
        self.numClients=0
        self.process = ProcessProtocol(self)
        reactor.spawnProcess(self.process, "tail", args=logarr, usePTY=True)
        

    def register(self, client):
        if not client in self.clients:
            self.clients.append(client)
            self.numClients+=1
            print str(datetime.now()) +" %d. registered client %s" % (self.numClients, client.peer)

    def unregister(self, client):
        if client in self.clients:
            self.clients.remove(client)
            self.numClients-=1
            print str(datetime.now()) + "  unregistered client %s (%d remains)" % (client.peer, self.numClients)

    def broadcast(self, message):
        for client in self.clients:
            client.sendMessage(message)
        #import pdb; pdb.set_trace()
        #print "broadcasted for %d clients" %(self.clients.count(self))


if __name__ == "__main__":
    print "Running process tail with multiple args"
    print "tail " + " ".join(logarr)
    factory = WebSocketProcessOutputterThingFactory("ws://localhost:%s" % (PORT), debug=False)
    listenWS(factory)
    reactor.run()
