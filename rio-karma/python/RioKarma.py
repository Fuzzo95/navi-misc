""" RioKarma

This module implements the 'Pearl' protocol used to communicate with
the Rio Karma portable audio player over TCP/IP, as well as higher-level
abstractions necessary to transfer files to and from the device.

This module wouldn't have been possible without the reverse-engineering
work done by the Karmalib and libkarma projects:

    http://karmalib.sourceforge.net/docs/protocol.php
    http://www.freakysoft.de/html/libkarma/

Requires Twisted and mmpython.

"""
#
# Copyright (C) 2005 Micah Dowty <micah@navi.cx>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

import struct, md5, os, random, time, sys
import shelve, cPickle
from cStringIO import StringIO
from twisted.internet import protocol, defer, reactor
from twisted.python import failure, log
import mmpython


############################################################################
############################################################  Protocols  ###
############################################################################

class ProtocolError(Exception):
    pass

class AuthenticationError(Exception):
    pass


# Most of these errors were guessed based on expected behavior, might be incorrect
statusExceptionMap = {
    0x8004005E: (ProtocolError,       "A lock required for this operation is not currently held"),
    0xC0040002: (ProtocolError,       "Nonexistant file ID"),
    0x8004005B: (AuthenticationError, "Password is incorrect"),
    }


class RequestResponseProtocol(protocol.Protocol):
    """The 'Pearl' protocol consists of client-initiated request/response
       pairs. Each request/response has a common header, but the data may be
       variable length and different requests may require different algorithms
       for buffering the received responses. We maintain a queue of outstanding
       requests, and the oldest one determines which read algorithm we use.

       The queue itself is unbounded, but bulk writes to it, for the file
       transfers themselves, are throttled such that the queue remains full
       enough to avoid wasting transmit or receive time, but that we don't
       waste all our CPU filling it. This ideal fill level is set in the
       constructor.
       """
    def __init__(self, preferredBacklog=10):
        self.buffer = ''

        # When the queue is less than preferredBacklog deep, we have
        # a list of clients waiting to be notified so they can add more.
        self.preferredBacklog = 4
        self.queueListeners = []

        # A list is really a suboptimal data type for this. If we feel like
        # requiring python 2.4, a collections.dequeue would be great.
        self.requestQueue = []

    def sendRequest(self, request):
        """Send a request, and add it to the queue of requests
           waiting for their response. Returns a Deferred that
           eventually yields the request's result.
           """
        self.requestQueue.append(request)
        request.sendTo(self.transport)
        return request.result

    def throttle(self, callable):
        """Add the provided callable to a list of clients that will be
           notified when the queue has fewer than the preferred number
           of requests in it. If the queue is already empty enough, the
           callable will be invoked immediately.
           """
        self.queueListeners.append(callable)
        self._checkQueueListeners()

    def _checkQueueListeners(self):
        """If the queue is empty enough, call everyone on the queueListeners list"""
        if len(self.requestQueue) < self.preferredBacklog:
            # Create the new listener list first, so it can be
            # safely written to by our callbacks.
            listeners = self.queueListeners
            self.queueListeners = []
            for callable in listeners:
                callable()

    def dataReceived(self, data):
        # Our new buffer goes into a StringIO, so individual requests
        # can read pieces of it efficiently.
        bufferFile = StringIO()
        bufferFile.write(self.buffer)
        bufferFile.write(data)
        bufferFileLength = bufferFile.tell()
        bufferFile.reset()

        while self.requestQueue:
            currentRequest = self.requestQueue[0]

            try:
                currentRequest.readResponse(bufferFile)
            except:
                # Any error in this handler belongs to the request, not us
                currentRequest.result.errback(failure.Failure())

            if currentRequest.result.called:
                # This request is done, on to the next
                del self.requestQueue[0]
                self._checkQueueListeners()
            else:
                # This request needs more data. If it did complete, we go around
                # and let the next request look at our buffer.
                break

        # All data remaining in our bufferFile after this round gets
        # stowed in self.buffer. The data we already read is discarded.
        self.buffer = bufferFile.read()


class AuthenticatedProtocol(RequestResponseProtocol):
    """This builds on the RequestResponseProtocol to include authentication,
       initiated when the protocol connects successfully. This reports
       authentication status via the factory's 'authResult' deferred, returning
       a reference to this Protocol on success.
       """
    def connectionMade(self):
        self.sendRequest(Request_GetAuthenticationSalt()).addCallback(
            self._sendAuthDigest).addErrback(self.factory.authResult.errback)

    def _sendAuthDigest(self, salt):
        self.sendRequest(Request_Authenticate(salt, self.factory.password)).addCallback(
            self._reportAuthResult).addErrback(self.factory.authResult.errback)

    def _reportAuthResult(self, rights):
        self.authRights = rights
        self.factory.authResult.callback(self)


def connect(host, port=8302, password='', cls=AuthenticatedProtocol):
    """Connect to a Rio Karma device. Returns a Deferred that results in
       an AuthenticatedProtocol instance on success.
       """
    factory = protocol.ClientFactory()
    factory.authResult = defer.Deferred()
    factory.protocol = cls
    factory.password = password
    factory.clientConnectionFailed = lambda connector, reason: factory.authResult.errback(reason)
    reactor.connectTCP(host, port, factory)
    return factory.authResult


############################################################################
#################################################  Request Base Classes  ###
############################################################################

class BaseRequest:
    """This is the abstract base class for requests in the Pearl protocol.
       Every request must know how to send itself, and know how to process
       received data. Every request signals its completion via a Deferred.
       """
    MAGIC = "Ri\xc5\x8d"
    id = None

    def __init__(self):
        self.result = defer.Deferred()

    def sendTo(self, fileObj):
        """Send this request to the provided file-like object"""
        raise NotImplementedError

    def readResponse(self, fileObj):
        """Read response data from the provided file-like object.
           This should read only the data that actually belongs to this
           request. If the request can be completed with the provided
           data, self.result.called must be True after this returns.
           """
        raise NotImplementedError

    def decodeStatus(self, status):
        """Decode the 'status' word common to many requests, generating
           an exception if appropriate.
           """
        # Get out fast if we were successful
        if status == 0:
            return
        try:
            exc = statusExceptionMap[status]
        except KeyError:
            raise ProtocolError("Unexpected status 0x%08X for request %r" %
                                (status, self))
        raise exc[0](*exc[1:])


class StatefulRequest(BaseRequest):
    """This class implements requests that have several different states
       in which received data means different things. State transitions can
       occur while reading, such that the new reader gets a chance to finish
       the current buffer, and it will be called later for future buffer
       updates.

       This implements default states to read the packet's magic and ID,
       validate them, and take appropriate action for NAK or Busy packets.
       """
    def __init__(self):
        BaseRequest.__init__(self)
        self.responseBuffer = ''
        self.stateTransition(self.state_beginResponse)

    def stateTransition(self, newReader, fileObj=None):
        self.readResponse = newReader
        if fileObj is not None:
            newReader(fileObj)

    def fillResponseBuffer(self, fileObj, size):
        """Read from fileObj, trying to fill self.responseBuffer
           up to exactly 'size' bytes. Returns True if the buffer is
           the right size, raises a ProtocolError if somehow it gets too big.
           """
        remaining = size - len(self.responseBuffer)
        if remaining > 0:
            self.responseBuffer += fileObj.read(remaining)

        responseLen = len(self.responseBuffer)
        if responseLen > size:
            raise ProtocolError("Request %r read too much data (have %d, need %d)" %
                                (self, responseLen, size))
        return responseLen == size

    def state_beginResponse(self, fileObj):
        """This is the initial state, as well as the state we return to after a NAK
           or Busy packet. Read the magic number and ID.
           """
        if self.fillResponseBuffer(fileObj, 8):
            # Check the magic
            if not self.responseBuffer.startswith(self.MAGIC):
                raise ProtocolError("Connection is out of sync, didn't receive header for %r" % self)

            # Decode the ID
            responseId = struct.unpack("<I", self.responseBuffer[4:])[0]
            self.responseBuffer = ''

            if responseId == self.id:
                # This is the reply we were looking for
                self.stateTransition(self.state_normalReply, fileObj)

            elif responseId == 1:
                # This is a NAK response, the device rejected our command
                raise ProtocolError("NAK response")

            elif responseId == 2:
                # This is a "Busy" response, we have a state for that
                self.stateTransition(self.state_busyReply, fileObj)

            else:
                # Something wonky...
                raise ProtocolError("Connection is out of sync, request #%d received a reply for #%d" %
                                    (self.id, responseId))

    def state_busyReply(self, fileObj):
        """We received a 'Busy' packet. Wait for its body, make note of it,
           then go back to waiting for our real packet.
           """
        if self.fillResponseBuffer(fileObj, 8):
            step, numSteps = struct.unpack("<II", self.responseBuffer)
            self.responseBuffer = ''

            # FIXME: Once we have progress reporting, this will go somewhere useful
            print "Busy: %d/%d" % (step, numSteps)

            self.stateTransition(self.state_beginResponse, fileObj)

    def state_normalReply(self, fileObj):
        """This state is implemented by subclasses to receive normal replies"""
        raise NotImplementedError


class StructRequest(StatefulRequest):
    """This is an abstract base class for requests where the paclet bodies
       can be described, at least in part, using the struct module.
       """
    requestFormat = ''
    responseFormat = ''
    parameters = ()

    def __init__(self):
        StatefulRequest.__init__(self)
        self._responseLength = struct.calcsize(self.responseFormat)

    def sendTo(self, fileObj):
        # Send the header, then our formatted parameters
        fileObj.write(self.MAGIC + struct.pack("<I", self.id) +
                      struct.pack(self.requestFormat, *self.parameters))

    def state_normalReply(self, fileObj):
        if self.fillResponseBuffer(fileObj, self._responseLength):
            response = struct.unpack(self.responseFormat, self.responseBuffer)
            self.responseBuffer = ''
            self.receivedResponse(fileObj, *response)

    def receivedResponse(self, source, *args):
        """This is called when a complete decoded response is received, with arguments
           as specified by the responseFormat. It is expected to call self.result with some
           sort of return value, None if necessary.

           The default implementation just calls the callback with all args we got- packed
           into a tuple if there are multiple arguments. This will work if the caller
           doesn't expect any special processing to happen on the response.
           """
        if len(args) > 1:
            self.result.callback(args)
        else:
            self.result.callback(args[0])


############################################################################
##############################################  Request Implementations  ###
############################################################################

class Request_GetProtocolVersion(StructRequest):
    """Return the major and minor version of this device's protocol"""
    id = 0
    parameters = (0,0)
    requestFormat = '<II'
    responseFormat = '<II'


class Request_GetAuthenticationSalt(StructRequest):
    """Returns a 16-byte random string that is to be prepended to the password when hashed"""
    id = 3
    responseFormat = '16s'


class Request_Authenticate(StructRequest):
    """The MD5 hash of the salt+password is sent. On success, returns a list of access rights
       as a tuple of strings containing 'read' and/or 'write'. Raises an AuthenticationError
       if the password was incorrect.
       """
    id = 4
    requestFormat = '16s'
    responseFormat = '<II'

    def __init__(self, salt, password):
        StructRequest.__init__(self)
        self.parameters = (md5.md5(salt + password).digest(),)

    def receivedResponse(self, source, status, accessRights):
        self.decodeStatus(status)
        if accessRights == 0:
            self.result.callback(('read',))
        elif accessRights == 1:
            self.result.callback(('read', 'write'))
        else:
            raise ProtocolError("Unexpected access rights code: %d" % accessRights)


class Request_GetDeviceDetails(StructRequest):
    """Returns the device's name, version, and number of storage devices"""
    id = 5
    responseFormat = '<32s32sI'

    def receivedResponse(self, source, name, version, numStorageDevices):
        """Trim NULs from the received strings"""
        self.result.callback(( name.replace("\x00", ""),
                               version.replace("\x00", ""),
                               numStorageDevices ))


class Request_GetStorageDetails(StructRequest):
    """For any storage device, returns the number of files, storage size,
       free space, and the highest file ID.
       """
    id = 6
    requestFormat = '<I'
    responseFormat = '<IQQI'

    def __init__(self, storageId):
        self.parameters = (storageId,)
        StructRequest.__init__(self)


class Request_GetDeviceSettings(StructRequest):
    """Return a dictionary with the device's settings"""
    id = 7
    responseFormat = '<I'

    def receivedResponse(self, source, status):
        """After receiving the fixed portion of this reply, we swap
           over our readResponse() callback to readSettings, which
           buffers them into a PropertyFileAdaptor object.
           """
        self.decodeStatus(status)
        self.properties = PropertyFileAdaptor({})

        self.reader = AlignedStringReader()
        self.stateTransition(self.state_readSettings, source)

    def state_readSettings(self, fileObj):
        if not self.reader.next(fileObj, self.properties):
            self.result.callback(self.properties.db)


class Request_RequestIOLock(StructRequest):
    """Request an I/O lock. The lock type must be 'read' or 'write'.
       You must have at least read lock to retrieve information from the audio database,
       and at least a write lock to upload new data.
       """
    id = 9
    requestFormat = '<I'
    responseFormat = '<I'
    lockTypes = {
        'read': 0,
        'write': 1,
        }

    def __init__(self, lockType):
        try:
            self.parameters = (self.lockTypes[lockType],)
        except KeyError:
            raise ProtocolError("Unknown lock type %r" % lockType)
        StructRequest.__init__(self)

    def receivedResponse(self, source, status):
        self.decodeStatus(status)
        self.result.callback(None)


class Request_ReleaseIOLock(StructRequest):
    """Release any I/O locks that are currently held"""
    id = 10
    responseFormat = '<I'

    def receivedResponse(self, source, status):
        self.decodeStatus(status)
        self.result.callback(None)


class Request_WriteFileChunk(StructRequest):
    """Write a block of data to a file, identified by its number.
       The offset and size are 64-bit.
       """
    id = 12
    requestFormat = '<QQII'
    responseFormat = '<I'

    def __init__(self, fileID, offset, size, dataSource, storageID=0):
        self.dataSource = dataSource
        self.size = size
        self.parameters = (offset, size, fileID, storageID)
        StructRequest.__init__(self)

    def sendTo(self, fileObj):
        StructRequest.sendTo(self, fileObj)
        AlignedBlockWriter(self.size).next(self.dataSource, fileObj)

    def receivedResponse(self, source, status):
        self.decodeStatus(status)
        self.result.callback(None)


class Request_GetAllFileDetails(StructRequest):
    """Read details for all files, sending each file properties
       dictionary to a provided callback.
       """
    id = 13
    responseFormat = '<I'

    def __init__(self, callback):
        self.callback = callback
        StructRequest.__init__(self)

    def receivedResponse(self, source, status):
        self.decodeStatus(status)
        self.fileDatabase = FileDatabaseWriter(self.callback)

        self.reader = AlignedStringReader()
        self.stateTransition(self.state_readFiles, source)

    def state_readFiles(self, fileObj):
        if not self.reader.next(fileObj, self.fileDatabase):
            self.result.callback(None)


class Request_GetFileDetails(StructRequest):
    """Reads metadata for one file, by ID. Optionally a user-defined
       dictionary-like object may be provided as storage.
       """
    id = 14
    requestFormat = '<I'
    responseFormat = '<I'

    def __init__(self, fileID, storage=None):
        if storage is None:
            storage = {}
        self.storage = storage

        self.parameters = (fileID,)
        StructRequest.__init__(self)

    def receivedResponse(self, source, status):
        self.decodeStatus(status)
        self.properties = PropertyFileWriter(self.storage)

        self.reader = AlignedStringReader()
        self.stateTransition(self.state_readDetails, source)

    def state_readDetails(self, fileObj):
        if not self.reader.next(fileObj, self.properties):
            self.result.callback(self.storage)


class Request_UpdateFileDetails(StructRequest):
    """Update metadata for one file, by ID. The metadata is specified
       as a dictionary-like object.
       """
    id = 15
    requestFormat = '<I'
    responseFormat = '<I'

    def __init__(self, fileId, data):
        self.parameters = (fileId,)
        self.properties = PropertyFileReader(data)
        StructRequest.__init__(self)

    def receivedResponse(self, source, status):
        self.decodeStatus(status)
        self.result.callback(None)

    def sendTo(self, fileObj):
        StructRequest.sendTo(self, fileObj)
        AlignedStringWriter().next(self.properties, fileObj)


class Request_ReadFileChunk(StructRequest):
    """Read a block of data from a file, identified by its number.
       This supports 64-bit offsets and lengths. This writes
       all received data to the 'destination' file-like object,
       then returns, via a deferred, the number of bytes
       actually read.
       """
    id = 16
    requestFormat = '<QQI'
    responseFormat = '<QI'

    def __init__(self, fileID, offset, size, destination):
        self.destination = destination
        self.parameters = (offset, size, fileID)
        StructRequest.__init__(self)

    def receivedResponse(self, source, size, status):
        self.decodeStatus(status)
        self.reader = AlignedBlockReader(size)
        self.stateTransition(self.state_readChunk, source)

    def state_readChunk(self, fileObj):
        """Wait for the bulk of the file to transfer"""
        if not self.reader.next(fileObj, self.destination):
            self.stateTransition(self.state_finalStatus, fileObj)

    def state_finalStatus(self, fileObj):
        """Wait for the final status code to arrive, after the file content"""
        if self.fillResponseBuffer(fileObj, 4):
            status = struct.unpack("<I", self.responseBuffer)[0]
            self.responseBuffer = ''
            self.decodeStatus(status)
            self.result.callback(self.reader.transferred)


class Request_DeleteFile(StructRequest):
    """Delete a file from storage, given its ID"""
    id = 17
    requestFormat = '<I'

    def __init__(self, fileId):
        self.parameters = (fileId,)
        StructRequest.__init__(self)


class Request_Hangup(StructRequest):
    """Indicates that the conversation is over"""
    id = 19
    responseFormat = '<I'

    def receivedResponse(self, source, status):
        self.decodeStatus(status)


############################################################################
#####################################################  Protocol Helpers  ###
############################################################################

class AlignedStringReader:
    """This object copies one 4-byte-aligned null terminated string
       from source to destination. The last read it performs is the
       4-byte block containing the null terminator, and the last write
       includes the character preceeding the null terminator but not
       the terminator itself.

       This returns True as long as it needs more data.
       """
    totalLength = 0
    foundNull = False

    def next(self, source, destination):
        while self.totalLength & 0x03 or not self.foundNull:
            # We can only read up until the end of the next 4-byte block,
            # to avoid overshooting the end of the string.
            block = source.read(4 - (self.totalLength & 0x03))
            if not block:
                return True
            self.totalLength += len(block)

            # Look for the null, and write everything before it but nothing
            # after it or including it.
            if not self.foundNull:
                i = block.find('\x00')
                if i < 0:
                    destination.write(block)
                else:
                    destination.write(block[:i])
                    self.foundNull = True
        return False


class AlignedStringWriter:
    """This object copies all data from the source into one
       4-byte-aligned null terminated string in the destination.
       """
    bufferSize = 4096

    def next(self, source, destination):
        totalLength = 0

        # Copy as many complete blocks as we can, tracking the total size
        while 1:
            block = source.read(self.bufferSize)
            totalLength += len(block)
            if len(block) == self.bufferSize:
                destination.write(block)
            else:
                break

        # Null-terminate the last block
        block += "\x00" * (4 - (totalLength & 0x03))
        destination.write(block)


class AlignedBlockReader:
    """This object copies one 4-byte-aligned block from source to
       destination, in which the length is known beforehand.
       This returns True as long as it needs more data.
       """
    def __init__(self, size, bufferSize=16384):
        self.size = size
        self.bufferSize = bufferSize
        self.paddedSize = size + ((4 - (size & 0x03)) & 0x03)
        self.transferred = 0

    def next(self, source, destination):
        while self.transferred < self.paddedSize:
            block = source.read(min(self.bufferSize, self.paddedSize - self.transferred))
            if not block:
                return True
            self.transferred += len(block)

            if self.transferred >= self.bufferSize:
                # We've transferred all the real data already, the rest is padding
                continue
            else:
                # Write up until we hit the end of the real data
                destination.write(block[:self.bufferSize - self.transferred])
        return False


class AlignedBlockWriter:
    """This object copies a fixed-size block of data from the
       source into one 4-byte-aligned block in the destination.
       """
    def __init__(self, size):
        self.size = size

    def next(self, source, destination):
        # This is probably the most speed-crucial part of the whole program-
        # so start of really simple. If we actually get oddly sized reads
        # here, we'll have to add a special case later.
        # buffering.
        block = source.read(self.size)
        if len(block) != self.size:
            raise IOError("Unexpected end of file")

        destination.write(block + "\x00" * ((4 - (len(block) & 0x03)) & 0x03))


class LineBufferedWriter:
    """Base class for file-like objects whose write() method must be line buffered"""
    _writeBuffer = ''

    def write(self, data):
        """Write an arbitrary amount of data, buffering any incomplete lines"""
        buffer = self._writeBuffer + data
        fragments = buffer.split("\n")
        self._writeBuffer = fragments[-1]
        for line in fragments[:-1]:
            self.writeLine(line)


class LineBufferedReader:
    """Base class for file-like objects whose read() method must be line buffered"""
    _readBuffer = ''

    def read(self, size=None):
        """Read up to 'size' bytes. If 'size' is None, this will read all
           available data.
           """
        buffer = self._readBuffer

        # Read all we can, within our size limits
        while (size is None) or (len(self._readBuffer) < size):
            line = self.readLine()
            if not line:
                break
            buffer += line

        # If we had no size limit, return the whole thing
        if size is None:
            self._readBuffer = ''
            return buffer

        # Otherwise, truncate it
        self._readBuffer = buffer[size:]
        return buffer[:size]


class PropertyFileReader(LineBufferedReader):
    """This is a readable object that serializes a dictionary-like object
       using the 'properties file' key-value format.
       """
    _readIter = None

    def __init__(self, db):
        self.db = db

    def readLine(self):
        """Read exactly one key=value line, with \n termination. Returns the
           empty string if we have no more data.
           """
        if self._readIter is None:
            self._readIter = self.db.iteritems()
        try:
            key, value = self._readIter.next()
        except StopIteration:
            return ""

        # If we get a unicode value, encode it in UTF-8
        if type(value) is unicode:
            value = value.encode('utf-8')
        return "%s=%s\n" % (key, value)

    def rewind(self):
        """Seek back to the beginning of the buffer. This also revalidates
           a bad iterator if the dictionary was modified while reading.
           """
        self._readIter = None


class PropertyFileWriter(LineBufferedWriter):
    """This is a writeable object that populates a dictionary-like object
       using the 'properties file' key-value format.
       """

    # By default we decode as strings, but this table ensures
    # that our human-readable metadata is unicode and we get integers
    # where we should.
    keyTypes = {
        'artist':         unicode,
        'title':          unicode,
        'source':         unicode,
        'length':         int,
        'ctime':          int,
        'fid_generation': int,
        'fid':            int,
        'file_id':        int,
        'duration':       int,
        'samplerate':     int,
        'tracknr':        int,
        }

    def __init__(self, db):
        self.db = db

    def writeLine(self, line):
        """Write a complete key=value line"""
        key, value = line.strip().split("=", 1)

        keyType = self.keyTypes.get(key)
        if keyType is unicode:
            value = unicode(value, 'utf-8')
        elif keyType:
            value = keyType(value)
        self.db[key] = value


class FileDatabaseWriter(LineBufferedWriter):
    """This class implements a writeable object that parses a file database,
       sending each completed dictionary to a user-provided callback.
       """
    def __init__(self, callback):
        self.callback = callback
        self._current = PropertyFileWriter({})

    def writeLine(self, line):
        line = line.strip()
        if line:
            # Add a key-value pair to our current dict
            self._current.writeLine(line)
        else:
            # Commit this dict
            self.callback(self._current.db)
            self._current = PropertyFileWriter({})


############################################################################
#####################################################  Cache Management  ###
############################################################################

class AllocationTree:
    """This is a data structure for keeping track of unused areas of an
       abstract address space, marking areas as used, and looking for unused
       areas. It is a binary tree, with maximum depth equal to the number
       of bits in the address space. Every node (represented by a list) can
       either lead to another node or to a boolean indicating whether that
       space is used.

       If the tree were fully populated, it would be no better than a lookup
       table holding a flag for every address, scanned linearly every time we
       wanted to find an available one. To avoid this, the tree is optimized-
       a node with leaves for both children may only exist if the leaves have
       different values. Initially the tree's root node is a False leaf,
       indicating that the entire address space is empty. When we allocate
       space, leaves split into nodes as necessary. When traversing the tree
       looking for available space, this lets us jump over large blocks of
       unavailable space very quickly. This is very similar to the Karnaugh
       maps used to optimize simple digital logic circuits.
       """
    def __init__(self, bits):
        self.mask = 1 << (bits - 1)
        self.data = False

    def isAllocated(self, address):
        """Lookup the boolean value of a particular address"""
        data = self.data
        mask = self.mask

        while type(data) is list:
            data = data[bool(address & mask)]
            mask >>= 1

        return data

    def findFirst(self):
        """Find the first free location"""
        return self._findFirst(self.data, self.mask, 0)

    def _findFirst(self, data, mask, address):
        if type(data) is list:
            # A node- if the zero child isn't True, we can always follow
            # it and find a free address.
            if data[0] == True:
                return self._findFirst(data[1], mask >> 1, address | mask)
            else:
                return self._findFirst(data[0], mask >> 1, address)
        elif data:
            # Everything's allocated
            return None
        else:
            # Everything's free, return the first available
            # address by refraining from setting any additional
            # bits to the right of 'mask'
            return address

    def allocate(self, address):
        """Mark the given address as used"""
        mask = self.mask
        node = self.data
        getNode = lambda: self.data
        setNode = lambda x: setattr(self, 'data', x)
        stack = [(getNode, setNode)]

        while 1:
            if type(node) is not list:
                if node:
                    # Everything down this path is already allocated and we're done.
                    return
                else:
                    # Are we at the bottom of the tree yet?
                    if mask == 0:
                        # Yes. This leaf describes our address and only our address.
                        # We can safely mark it as True.
                        node = True
                        setNode(node)
                        break
                    else:
                        # Nope. We should split this leaf into a node with two
                        # False children. This isn't a legal condition for our
                        # tree, but it will be fixed up below.
                        node = [False, False]
                        setNode(node)

            # Traverse down a level
            i = bool(address & mask)
            getNode = lambda node=node, i=i: node.__getitem__(i)
            setNode = lambda x, node=node, i=i: node.__setitem__(i, x)
            node = node[i]
            mask >>= 1
            stack.append((getNode, setNode))

        # Now wander back up the tree, looking for nodes we can collapse into leaves
        stack.reverse()
        for getNode, setNode in stack:
            node = getNode()
            if node == [False, False]:
                setNode(False)
            elif node == [True, True]:
                setNode(True)


class FileIdAllocator:
    """File IDs, from experimentation, seem to be multiples of 0x10 between 0x110 and
       0xFFFF0. This uses a 16-bit-deep AllocationTree, with bit shifting to hide the
       requirement that IDs be multiples of 0x10. Additionally, we pre-mark IDs below
       0x110 as unusable.
       """
    def __init__(self):
        self.tree = AllocationTree(16)
        for i in xrange(0x11):
            self.tree.allocate(i)

    def isAllocated(self, address):
        return self.tree.isAllocated(address >> 4)

    def allocate(self, address):
        return self.tree.allocate(address >> 4)

    def findFirst(self):
        return self.tree.findFirst() << 4

    def next(self):
        i = self.findFirst()
        if i is not None:
            self.allocate(i)
        return i


class Cache:
    """This is a container for the several individual databases that make up
       our cache of the Rio's file database. It is responsible for opening,
       closing, synchronizing, and resetting all caches in unison.
       """
    def __init__(self, path):
        self.path = os.path.expanduser(path)

    def _openShelve(self, name, mode):
        """This is a convenience function for opening shelve databases using
           a common naming convention and parent directory.
           """
        return shelve.open(os.path.join(self.path, name + '.db'), mode)

    def _openPickle(self, name, mode, factory, *args, **kwargs):
        """This is a convenience function for opening pickled objects in the same
           fashion as an anydbm database.
           """
        filename = os.path.join(self.path, name + '.p')
        if mode == 'n' or not os.path.isfile(filename):
            return factory(*args, **kwargs)
        else:
            return cPickle.load(open(filename, "rb"))

    def _savePickle(self, obj, name):
        """A convenience function for saving pickled objects using
           our common naming convention
           """
        filename = os.path.join(self.path, name + '.p')
        cPickle.dump(obj, open(filename, "wb"), cPickle.HIGHEST_PROTOCOL)

    def open(self, mode='c'):
        """Open all cache components, creating them if necessary"""
        if not os.path.isdir(self.path):
            os.makedirs(self.path)

        self.fileDetails     = self._openShelve('file-details', mode)
        self.fileIdAllocator = self._openPickle('file-id-allocator', mode, FileIdAllocator)

    def close(self):
        self.sync()
        self.quickClose()

    def sync(self):
        """Synchronize in-memory parts of the cache with disk"""
        self.fileDetails.sync()
        self._savePickle(self.fileIdAllocator, 'file-id-allocator')

    def quickClose(self):
        """Close without regard to data integrity! This will close our
           databases, but does not flush in-memory caches to disk.
           This should only be used when we're throwing the cache
           away soon anyway.
           """
        self.fileDetails.close()

    def empty(self):
        """Discard the contents of the cache"""
        self.quickClose()
        self.open('n')


############################################################################
##################################################  Metadata Collection  ###
############################################################################

class RidCalculator:
    """This object calculates the RID of a file- a sparse digest used by Rio Karma.
       For files <= 64K, this is the file's md5sum. For larger files, this is the XOR
       of three md5sums, from 64k blocks in the beginning, middle, and end.
       """
    def fromSection(self, fileObj, start, end, blockSize=0x10000):
        """This needs a file-like object, as well as the offset and length of the portion
           the RID is generated from. Beware that there is a special case for MP3 files.
           """
        # It's a short file, compute only one digest
        if end-start <= blockSize:
            fileObj.seek(start)
            return md5.md5(fileObj.read(length)).hexdigest()

        # Three digests for longer files
        fileObj.seek(start)
        a = md5.md5(fileObj.read(blockSize)).digest()

        fileObj.seek(end - blockSize)
        b = md5.md5(fileObj.read(blockSize)).digest()

        fileObj.seek((start + end - blockSize) / 2)
        c = md5.md5(fileObj.read(blockSize)).digest()

        # Combine the three digests
        return ''.join(["%02x" % (ord(a[i]) ^ ord(b[i]) ^ ord(c[i])) for i in range(16)])

    def fromFile(self, filename, length=None, mminfo=None):
        """Calculate the RID from a file, given its name. The file's length and
           mmpython results may be provided if they're known, to avoid duplicating work.
           """
        if mminfo is None:
            mminfo = mmpython.parse(filename)

        f = open(filename, "rb")

        if length is None:
            f.seek(0, 2)
            length = f.tell()
            f.seek(0)

        # Is this an MP3 file? For some silliness we have to skip the header
        # and the last 128 bytes of the file. mmpython can tell us where the
        # header starts, but only in a somewhat ugly way.
        if isinstance(mminfo, mmpython.audio.eyed3info.eyeD3Info):
            offset = mminfo._find_header(f)[0]
            f.seek(0)
            return self.fromSection(f, offset, length-128)

        # Otherwise, use the whole file
        else:
            return self.fromSection(f, 0, length)


class MetadataConverter:
    """This object manages the connection between different kinds of
       metadata- the data stored within a file on disk, mmpython attributes,
       Rio attributes, and file extensions.
       """
    # Maps mmpython classes to codec names for all formats the player
    # hardware supports.
    codecNames = {
        mmpython.audio.eyed3info.eyeD3Info: 'mp3',
        mmpython.audio.flacinfo.FlacInfo:   'flac',
        mmpython.audio.pcminfo.PCMInfo:     'wave',
        mmpython.video.asfinfo.AsfInfo:     'wma',
        mmpython.audio.ogginfo.OggInfo:     'vorbis',
        }

    # Maps codec names to extensions. Identity mappings are the
    # default, so they are omitted.
    codecExtensions = {
        'wave': 'wav',
        'vorbis': 'ogg',
        }

    def filenameFromDetails(self, details,
                            replaceSpaces   = True,
                            lowercase       = True,
                            unicodeEncoding = 'utf-8'):
        """Determine a good filename to use for a file with the given metadata
           in the Rio 'details' format. If it's a data file, this will use the
           original file as stored in 'title'. Otherwise, it cleans up the filename
           (always removing slashes, optionally replacing spaces with underscores)
           and adds an extension.
           """
        if details.get('type') == 'taxi':
            return details['title']

        name = details['title']
        name = name.replace(os.sep, "")
        if replaceSpaces:
            name = name.replace(" ", "_")
        if lowercase:
            name = name.lower()

        codec = details.get('codec')
        extension = self.codecExtensions.get(codec, codec)
        if extension:
            name += '.' + extension
        return unicode(name).encode(unicodeEncoding, 'replace')

    def detailsFromDisk(self, filename, details):
        """Automagically load media metadata out of the provided filename,
           adding entries to details. This works on any file type
           mmpython recognizes, and other files should be tagged
           appropriately for Rio Taxi.
           """
        info = mmpython.parse(filename)
        st = os.stat(filename)

        # Generic details for any file. Note that we start out assuming
        # all files are unreadable, and label everything for Rio Taxi.
        # Later we'll mark supported formats as music.
        details['length'] = st.st_size
        details['type'] = 'taxi'

        # We get the bulk of our metadata via mmpython if possible
        if info:
            self.detailsFromMM(info, details)

        if details['type'] == 'taxi':
            # All taxi files get their filename as their title, regardless of what mmpython said
            details['title'] = os.path.basename(filename)

            # Taxi files also always get a codec of 'taxi'
            details['codec'] = 'taxi'

        else:
            # All non-taxi files...

            # Music files that still don't get a title get their filename minus the extension
            if not details.get('title'):
                details['title'] = os.path.splitext(os.path.basename(filename))[0]

            # Music files get an 'RID' message digest
            details['rid'] = RidCalculator().fromFile(filename, st.st_size, info)

    def detailsFromMM(self, info, details):
        """Update Rio-style 'details' metadata from MMPython info"""
        # Mime types aren't implemented consistently in mmpython, but
        # we can look at the type of the returned object to decide
        # whether this is a format that the Rio probably supports.
        # This dictionary maps mmpython clases to Rio codec names.
        for cls, codec in self.codecNames.iteritems():
            if isinstance(info, cls):
                details['type'] = 'tune'
                details['codec'] = codec
                break

        # Map simple keys that don't require and hackery
        for fromKey, toKey in (
            ('artist', 'artist'),
            ('title', 'title'),
            ('album', 'source'),
            ('date', 'year'),
            ('samplerate', 'samplerate'),
            ):
            v = info[fromKey]
            if v is not None:
                details[toKey] = v

        # The rio uses a two-letter prefix on bit rates- the first letter
        # is 'f' or 'v', presumably for fixed or variable. The second is
        # 'm' for mono or 's' for stereo. There doesn't seem to be a good
        # way to get VBR info out of mmpython, so currently this always
        # reports a fixed bit rate. We also have to kludge a bit because
        # some metdata sources give us bits/second while some give us
        # kilobits/second. And of course, there are multiple ways of
        # reporting stereo...
        kbps = info['bitrate']
        if kbps and kbps > 0:
            stereo = bool( (info['channels'] and info['channels'] >= 2) or
                           (info['mode'] and info['mode'].find('stereo') >= 0) )
            if kbps > 8000:
                kbps = kbps // 1000
            details['bitrate'] = ('fm', 'fs')[stereo] + str(kbps)

        # If mmpython gives us a length it seems to always be in seconds,
        # whereas the Rio expects milliseconds.
        length = info['length']
        if length:
            details['duration'] = int(length * 1000)

        # mmpython often gives track numbers as a fraction- current/total.
        # The Rio only wants the current track, and we might as well also
        # strip off leading zeros and such.
        trackNo = info['trackno']
        if trackNo:
            details['tracknr'] = int(trackNo.split("/", 1)[0])


############################################################################
######################################################  File Management  ###
############################################################################

class Filesystem:
    """The Filesystem coordinates the activity of several File objects
       through one Protocol, and provides ways of discovering Files.
       It owns a local cache of the rio's database, which it can synchronize.

       The Filesystem is responsible for organizing metadata, but not for
       collecting it, or for dealing with file contents.
       """
    def __init__(self, protocol, cacheDir="~/.riokarma-py/cache"):
        self.protocol = protocol
        self.cache = Cache(cacheDir)
        self.cache.open()

    def readLock(self):
        """Acquire a read-only lock on this filesystem- necessary for most
           operations, but also automatic on syncrhonization. The device
           is still usable as normal when a read lock is active.
           """
        return self.protocol.sendRequest(Request_RequestIOLock('read'))

    def writeLock(self):
        """Acquire a read-write lock on this filesystem. This is necessary
           to modify anything, but it puts the device into an unusable
           state. Release this lock as soon as possible after you're done.
           """
        return self.protocol.sendRequest(Request_RequestIOLock('write'))

    def unlock(self):
        """Release any locks currently held"""
        return self.protocol.sendRequest(Request_ReleaseIOLock())

    def synchronize(self):
        """Update our local database if necessary. Returns a Deferred
           signalling completion. This has the side-effect of acquiring
           a read lock.
           """
        d = defer.Deferred()
        self.readLock().addCallback(self._startSynchronize, d).addErrback(d.errback)
        return d

    def _startSynchronize(self, retval, d):
        if self.isCacheDirty():
            # Empty and rebuild all database files
            self.cache.empty()
            self.protocol.sendRequest(Request_GetAllFileDetails(
                self._cacheFileDetails)).addCallback(
                self._finishSynchronize, d).addErrback(d.errback)
        else:
            d.callback(None)

    def _finishSynchronize(self, retval, d):
        # Before signalling completion, sync the cache
        self.cache.sync()
        d.callback(None)

    def isCacheDirty(self):
        """This function determines whether our local cache of the database
           is still valid. If not, we should retrieve a new copy.
           """
        # FIXME: this should compare datestamps, or free space on the disk, to
        #        be able to invalidate the cache when the rio is externally modified.
        if len(self.cache.fileDetails) == 0:
            return True
        else:
            return False

    def _cacheFileDetails(self, details):
        """Add a file details dictionary to our database cache. This is used to populate
           the database when synchronizing from the device, and it's used internally
           by updateFileDetails().
           """
        self.cache.fileIdAllocator.allocate(int(details['fid']))
        self.cache.fileDetails[str(details['fid'])] = details

        # FIXME: Report progress near here, at least for synchronization
        print "FID: 0x%05X" % details['fid']

    def updateFileDetails(self, f):
        """Update our cache and the device itself with the latest details dictionary
           from this file object. Returns a Deferred signalling completion.
           """
        self._cacheFileDetails(f.details)
        self.cache.sync()
        return self.protocol.sendRequest(Request_UpdateFileDetails(f.fileID, f.details))


class File:
    """A File represents one media or data file corresponding to an entry in
       the device's database and our copy of that database. File objects
       have lifetimes disjoint from that of the actual files on disk- if a
       file ID is provided, existing metadata is looked up. If an ID isn't
       provided, we generate a new ID and metadata will be uploaded.

       Files are responsible for holding metadata, extracting that metadata
       from real files on disk, and for transferring a file's content to and
       from disk. Since the real work of metadata extraction is done by
       mmpython, this mostly concerns translating mmpython's metadata into
       the Rio's metadata format.
       """
    def __init__(self, filesystem, fileID=None):
        self.filesystem = filesystem

        if fileID is None:
            # New file, start out with barebones metadata
            now = int(time.time())
            self.fileID = self.filesystem.cache.fileIdAllocator.next()
            self.details = {
                'ctime': now,
                'fid_generation': now,
                'fid': self.fileID,
                }
        else:
            # Load this file from the cache
            self.fileID = int(fileID)
            self.details = self.filesystem.cache.fileDetails[str(fileID)]

    def loadFromDisk(self, filename):
        """Load this file's metadata and content from a file on disk.
           Returns a Deferred signalling completion.
           """
        MetadataConverter().detailsFromDisk(filename, self.details)
        result = defer.Deferred()
        self.loadContentFrom(open(filename, "rb")).addCallback(
            self._updateDetailsAfterLoad, result).addErrback(result.errback)
        return result

    def _updateDetailsAfterLoad(self, retval, result):
        self.filesystem.updateFileDetails(self).chainDeferred(result)

    def saveToDisk(self, filename):
        """Save this file's content to disk. Returns a deferred signalling completion."""
        # We make sure to explicitly close it, so that
        # when this returns you're sure the file is complete.
        result = defer.Deferred()
        dest = open(filename, "wb")
        self.saveContentTo(dest).addCallback(
            self._finishSaveToDisk, dest, result.callback).addErrback(
            self._finishSaveToDisk, dest, result.errback)
        return result

    def _finishSaveToDisk(self, retval, dest, chainTo):
        dest.close()
        chainTo(retval)

    def saveContentTo(self, fileObj, blockSize=8192):
        """Save this file's content to a file-like object.
           Returns a deferred signalling completion.
           """
        return ContentTransfer(Request_ReadFileChunk,
                               self, fileObj, blockSize).result

    def loadContentFrom(self, fileObj, blockSize=8192):
        """Load this file's content from a file-like object.
           Returns a Deferred signalling completion.
           """
        return ContentTransfer(Request_WriteFileChunk,
                               self, fileObj, blockSize).result


class ContentTransfer:
    """This is an object that transfers content, in either direction, between the
       device and a file-like object. The transfer is buffered by Protocol, but
       also throttled so we don't dump the entire file into the request queue
       right away.
       """
    def __init__(self, request, rioFile, fileObj, blockSize):
        self.rioFile = rioFile
        self.request = request
        self.fileObj = fileObj
        self.blockSize = blockSize
        self.offset = 0
        self.remaining = self.rioFile.details['length']
        self.result = defer.Deferred()

        self.next()

    def next(self):
        """Queue up another transfer block"""

        if self.remaining <= 0:
            return

        chunkLen = min(self.remaining, self.blockSize)
        d = self.rioFile.filesystem.protocol.sendRequest(self.request(
            self.rioFile.fileID, self.offset, chunkLen, self.fileObj))
        self.remaining -= chunkLen
        self.offset += chunkLen

        if self.remaining <= 0:
            # This is the last one, chain to our deferred and
            # stop setting up transfers.
            d.addCallback(self.result.callback)
            return

        # FIXME: real progress updates should be triggered here
        d.addCallback(lambda x: sys.stderr.write("."))
        d.addErrback(self.result.errback)

        # Queue up the next block once Protocol thinks we should
        self.rioFile.filesystem.protocol.throttle(self.next)

### The End ###
