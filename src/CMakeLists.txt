# src/CMakeLists.txt

find_package(Qt5Core REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(KF5Archive REQUIRED NO_MODULE)
find_package(KF5DNSSD NO_MODULE)
find_package(GIF)
find_package(Miniupnpc)

if(LIBMINIUPNPC_FOUND)
	add_definitions(-DHAVE_UPNP)
endif()

if(KF5DNSSD_FOUND)
	add_definitions(-DHAVE_DNSSD)
endif()

add_subdirectory(libshared)
add_subdirectory(libserver)

if(CLIENT OR THICKSRV)
	add_subdirectory(libclient)
	add_subdirectory(libthicksrv)
endif()

if(CLIENT)
	add_subdirectory(desktop)
endif()

if(SERVER)
	add_subdirectory (thinsrv)
endif()

if (THICKSRV)
	add_subdirectory(thicksrv)
endif()

if(TOOLS)
	add_subdirectory(tools)
endif()

