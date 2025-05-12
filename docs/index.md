# Mulex Framework Brief

**Mul**tiple **Ex**periment **F**ramewor**k** (mulex-fk) is a realtime service manager for multiporpuse experiment control. It allows for TCP/IP communication
for multiple backends and frontends. As long as your machines have ethernet connection between one another they can seemlessly view each other's data in realtime.

## Main Features

- Platform native TCP/IP allowing for high speed and bandwith communication
- Backend isolation
- Webapp visualization frontend without any instalation required
- Realtime database that allows for a seemless interchange of data between backends and/or frontends
- Persistent sqlite database for configurations or other permanent data
- Data events
- Message logging
- Frontend data over websocket allows to transfer bulk data such as for visualizing realtime plots/images
- Backend API written in C++ but extensible for any language as long as the same protocol is supported
- Frontend API written in typescript but again extensible
- Out of the box support for USB/TCP/Serial drivers for backends
- Low CPU impact
- Backend remote start/stop

## Upcoming Features

- Event timeline view
- Alarms
