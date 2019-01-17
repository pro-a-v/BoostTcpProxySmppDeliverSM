# BoostTcpProxySmppDeliverSM

# Based on boost asio TCP Proxy 

//https://github.com/ArashPartow/proxy/blob/master/tcpproxy_server.cpp<br/>
// Copyright (c) 2007 Arash Partow (http://www.partow.net)<br/>
// URL: http://www.partow.net/programming/tcpproxy/index.html<br/>
//<br/>
//<br/>
//                                    ---> upstream --->           +---------------+<br/>
//                                                     +---->------>               |<br/>
//                               +-----------+         |           | Remote Server |<br/>
//                     +--------->          [x]--->----+  +---<---[x]              |<br/>
//                     |         | TCP Proxy |            |        +---------------+<br/>
// +-----------+       |  +--<--[x] Server   <-----<------+<br/>
// |          [x]--->--+  |      +-----------+<br/>
// |  Client   |          |<br/>
// |           <-----<----+<br/>
// +-----------+<br/>
//                <--- downstream <---<br/>
//<br/>
//<br/>

