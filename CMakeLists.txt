cmake_minimum_required(VERSION 2.8)
project(BoostTcpProxySmppDeliverSM)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11 -fpermissive")


find_package( Boost COMPONENTS system thread REQUIRED )
include_directories( ${Boost_INCLUDE_DIRS} )
include_directories( ./ )







set(SRC_LIST
    main.cpp
    smpp_pdu/deliver_sm.cpp
    smpp_pdu/alert_notification.cpp
    smpp_pdu/aux_types.cpp
    smpp_pdu/buffer.cpp
    smpp_pdu/error.cpp
    smpp_pdu/header.cpp
    )

add_executable(${PROJECT_NAME} ${SRC_LIST})


target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} pthread m dl)
