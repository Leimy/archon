#ifndef __DAEMON_H__
#define __DAEMON_H__

#include <concepts>
#include <string>

const string CTRL_SOCKET_PATH;

template <typename LOGGER>
class daemon {
	LOGGER m_l;
	int m_socket = -1;
	
};

#endif // 
