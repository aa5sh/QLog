#!/usr/bin/env python3

"""
TCI WebSocket simulator 

This script implements a minimal TCI-like WebSocket server that accepts multiple client connections
and broadcasts status updates to all connected clients.

How it works
- Starts a WebSocket server on TCP port 8000.
- When a client connects, the server immediately sends a set of capability/status messages
  (limits, device info, supported modulations) and a READY message.
- Incoming messages are parsed as simple "COMMAND:arg1,arg2,..." frames and mapped to handlers.
  Supported commands include: VFO, TRX, MODULATION, DRIVE, RIT_OFFSET, RIT_ENABLE, XIT_OFFSET,
  XIT_ENABLE, and CW_MACROS_SPEED.
- The server keeps an internal state (frequency, TX/RX, modulation, drive, RIT/XIT settings, CW speed).
  When a command updates the state, the server broadcasts the corresponding response/status frame
  to all connected clients.

Notes
- This is a simulator: it does not control real radio hardware; it only maintains and publishes state.
- Each connection creates its own SimpleTCI state instance; broadcasting still goes to all clients.
- Messages are terminated with ';' when sent to clients.
"""

import asyncio
import websockets

clients = set()

class SimpleTCI:
    def __init__(self):
        self.currFreq = 14000000
        self.currTrx = False
        self.currModulation = "USB"
        self.currDrive = 20
        self.currRITOffset = 0
        self.currRITEnable = False
        self.currCWSpeed = 20
        self.currXITOffset = 0
        self.currXITEnable = False

    async def sendTCIMessage(self, msg: str):
        print(f"Sending msg: {msg}")
        await asyncio.sleep(0.1)
        dead = set()
        for ws in clients:
            try:
                await ws.send(msg + ";")
            except Exception:
                dead.add(ws)
        for ws in dead:
            clients.discard(ws)

    async def VFOCommand(self, args, getNum):
        if len(args) > getNum:
            self.currFreq = args[2]
        await self.sendTCIMessage(f"vfo:0,0,{self.currFreq}")

    async def TRXCommand(self, args, getNum):
        if len(args) > getNum:
            self.currTrx = (args[1].lower() == "true")
        await self.sendTCIMessage(f"trx:0,{self.currTrx}")

    async def MODULATIONCommand(self, args, getNum):
        if len(args) > getNum:
            self.currModulation = args[1].upper()
        await self.sendTCIMessage(f"modulation:0,{self.currModulation}")

    async def DRIVECommand(self, args, getNum):
        if len(args) > getNum:
            self.currDrive = args[1]
        await self.sendTCIMessage(f"drive:0,{self.currDrive}")

    async def RITOFFSETCommand(self, args, getNum):
        if len(args) > getNum:
            self.currRITOffset = args[1]
        await self.sendTCIMessage(f"rit_offset:0,{self.currRITOffset}")

    async def RITENABLECommand(self, args, getNum):
        if len(args) > getNum:
            self.currRITEnable = (args[1].lower() == "true")
        await self.sendTCIMessage(f"rit_enable:0,{self.currRITEnable}")

    async def XITOFFSETCommand(self, args, getNum):
        if len(args) > getNum:
            self.currXITOffset = args[1]
        await self.sendTCIMessage(f"xit_offset:0,{self.currXITOffset}")

    async def XITENABLECommand(self, args, getNum):
        if len(args) > getNum:
            self.currXITEnable = (args[1].lower() == "true")
        await self.sendTCIMessage(f"xit_enable:0,{self.currXITEnable}")

    async def CWMACROSSPEEDCommand(self, args, getNum):
        if len(args) > getNum:
            self.currCWSpeed = args[0]
        await self.sendTCIMessage(f"cw_macros_speed:0,{self.currCWSpeed}")

    def _processCommand(self, argString, getArgsNum):
        stripArgs = argString.replace(";", "")
        args = stripArgs.split(",")
        if len(args) < getArgsNum:
            return None
        return args

    async def handleMessage(self, data: str):
        commandString = data.split(":")
        print(f"Received: {commandString}")

        if len(commandString) < 2:
            return

        command = commandString[0].upper()
        payload = commandString[1]

        if command == "VFO":
            args = self._processCommand(payload, 2)
            if args: await self.VFOCommand(args, 2)
        elif command == "TRX":
            args = self._processCommand(payload, 1)
            if args: await self.TRXCommand(args, 1)
        elif command == "MODULATION":
            args = self._processCommand(payload, 1)
            if args: await self.MODULATIONCommand(args, 1)
        elif command == "DRIVE":
            args = self._processCommand(payload, 1)
            if args: await self.DRIVECommand(args, 1)
        elif command == "RIT_OFFSET":
            args = self._processCommand(payload, 1)
            if args: await self.RITOFFSETCommand(args, 1)
        elif command == "RIT_ENABLE":
            args = self._processCommand(payload, 1)
            if args: await self.RITENABLECommand(args, 1)
        elif command == "XIT_OFFSET":
            args = self._processCommand(payload, 1)
            if args: await self.XITOFFSETCommand(args, 1)
        elif command == "XIT_ENABLE":
            args = self._processCommand(payload, 1)
            if args: await self.XITENABLECommand(args, 1)
        elif command == "CW_MACROS_SPEED":
            args = self._processCommand(payload, 0)
            if args is not None: await self.CWMACROSSPEEDCommand(args, 0)

async def handler(websocket, path=None):
    clients.add(websocket)
    tci = SimpleTCI()

    print(websocket.remote_address, "connected")

    await tci.sendTCIMessage("VFO_LIMITS:10000,30000000;TRX_COUNT:2")
    await tci.sendTCIMessage("DEVICE:SunSDR2DX;MODULATIONS_LIST:AM,SAM,LSB,USB,CW,NFM,WFM;PROTOCOL:ExpertSDR3,1.9")
    await tci.sendTCIMessage("VFO:0,0,14000000;TRX:0,false;MODULATION:0,USB;RIT_OFFSET:0,-50;RIT_ENABLE:0,false")
    await tci.sendTCIMessage("READY")

    try:
        async for message in websocket:
            await tci.handleMessage(message)
    finally:
        clients.discard(websocket)
        print(websocket.remote_address, "closed")

async def main():
    host = "0.0.0.0"   # nebo "127.0.0.1" jen pro localhost
    port = 8000

    print(f"TCI Server Simulator listening on ws://{host}:{port}")

    async with websockets.serve(handler, host, port):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    asyncio.run(main())
