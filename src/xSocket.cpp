#include "xSocket.h"
#include "xLog.h"

xSocket::xSocket(const char *ip,int16_t port)
{
	createTcpListenSocket(ip,port);
}

xSocket::xSocket()
{

}

xSocket::~xSocket()
{
	::close(listenSocketFd);
}

int32_t xSocket::getListenFd()
{
	return listenSocketFd;
}

struct sockaddr_in6 xSocket::getLocalAddr(int32_t sockfd)
{
	struct sockaddr_in6 localaddr;
	bzero(&localaddr, sizeof localaddr);
	socklen_t addrlen = static_cast<socklen_t>(sizeof localaddr);
	if (::getsockname(sockfd,(struct sockaddr*)&localaddr,&addrlen) < 0)
	{
		LOG_SYSERR << "sockets::getLocalAddr";
	}
	return localaddr;
}

struct sockaddr_in6 xSocket::getPeerAddr(int sockfd)
{
	struct sockaddr_in6 peeraddr;
	bzero(&peeraddr, sizeof peeraddr);
	socklen_t addrlen = static_cast<socklen_t>(sizeof peeraddr);
	if (::getpeername(sockfd,(struct sockaddr*)&peeraddr,&addrlen) < 0)
	{
		LOG_SYSERR << "sockets::getPeerAddr";
	}
	return peeraddr;
}

bool xSocket::isSelfConnect(int32_t sockfd)
{
	struct sockaddr_in6 localaddr = getLocalAddr(sockfd);
	struct sockaddr_in6 peeraddr = getPeerAddr(sockfd);
	if (localaddr.sin6_family == AF_INET)
	{
		const struct sockaddr_in* laddr4 = reinterpret_cast<struct sockaddr_in*>(&localaddr);
		const struct sockaddr_in* raddr4 = reinterpret_cast<struct sockaddr_in*>(&peeraddr);
		return laddr4->sin_port == raddr4->sin_port
			&& laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;
	}
	else if (localaddr.sin6_family == AF_INET6)
	{
		return localaddr.sin6_port == peeraddr.sin6_port
		&& memcmp(&localaddr.sin6_addr, &peeraddr.sin6_addr, sizeof localaddr.sin6_addr) == 0;
	}
	else
	{
	return false;
	}
}

int32_t xSocket::getSocketError(int32_t sockfd)
{
	int optval;
	socklen_t optlen = static_cast<socklen_t>(sizeof optval);

	if (::getsockopt(sockfd,SOL_SOCKET,SO_ERROR,&optval,&optlen) < 0)
	{
		return errno;
	}
	else
	{
		return optval;
	}
}

bool xSocket::getpeerName(int32_t fd,const char *ip,int16_t port)
{
	struct sockaddr_in sa;
	socklen_t len = sizeof(sa);
	if(!getpeername(fd,(struct sockaddr *)&sa,&len))
	{
		ip = inet_ntoa(sa.sin_addr);
		port = ntohs(sa.sin_port);
		return true;
	}
	else
	{
		return false;
	}
}

int32_t  xSocket::createSocket()
{
	return socket(AF_INET,SOCK_STREAM,0);
}


bool xSocket::connectWaitReady(int32_t fd,int32_t msec)
{
	struct pollfd   wfd[1];

	wfd[0].fd = fd;
	wfd[0].events = POLLOUT;


	if (errno == EINPROGRESS)
	{
		int res;
		 if ((res = ::poll(wfd,1,msec)) == -1)
		 {
			 return false;
		 }
		 else if(res == 0)
		 {
			 errno = ETIMEDOUT;
			 return false;
		 }
	}

	return true;
}

int32_t  xSocket::connect(int32_t sockfd,const char *ip,int16_t port)
{
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = inet_addr(ip);
	return  ::connect(sockfd,(struct sockaddr *)&sin,sizeof(sin));
}

int32_t xSocket::setFlag(int32_t fd,int32_t flag)
{
	int32_t ret = fcntl(fd, F_GETFD);
	return fcntl(fd, F_SETFD, ret | flag);
}

bool xSocket::setTimeOut(int32_t sockfd,const struct timeval tv)
{
    if (setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv)) == -1)
    {
        LOG_ERROR<<"setsockopt(SO_RCVTIMEO)";
        return false;
    }

    if (setsockopt(sockfd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv)) == -1)
    {
        LOG_ERROR<<"setsockopt(SO_SNDTIMEO)";
        return false;
    }
	
    return true;
}

void  xSocket::setkeepAlive(int32_t fd,int32_t idle)
{
#ifdef __linux__
	int32_t keepalive = 1;
	int32_t keepidle = idle;
	int32_t keepintvl = 2;
	int32_t keepcnt = 3;
	int32_t err = 0;

	if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,(char*)&keepalive,sizeof(keepalive)) < 0)
	{
		LOG_DEBUG<<"SOL_SOCKET";
	}

	if(setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,(char *)&keepidle,sizeof(keepidle)) < 0)
	{
		LOG_DEBUG<<"TCP_KEEPIDLE";
	}

	if(setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL,(char *)&keepintvl,sizeof(keepintvl)) < 0)
	{
		LOG_DEBUG<<"TCP_KEEPINTVL";
	}

	if(setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,(char *)&keepcnt,sizeof(keepcnt)) < 0)
	{
		LOG_DEBUG<<"TCP_KEEPCNT";
	}
#endif
}

bool xSocket::createTcpListenSocket(const char *ip,int16_t port)
{
    struct sockaddr_in serverAdress;
    serverAdress.sin_family = AF_INET;
    serverAdress.sin_port  = htons(port);

    if(ip)
    {
        serverAdress.sin_addr.s_addr = inet_addr(ip);
    }
    else
    {
        serverAdress.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    listenSocketFd = socket(AF_INET,SOCK_STREAM,0);

    if(listenSocketFd < 0)
    {
        LOG_WARN<<"Create Tcp Socket Failed! "<< strerror(errno);
        return false;
    }

    if(!setSocketNonBlock(listenSocketFd))
    {
		LOG_WARN<<"Set listen socket to non-block failed!";
		return false;
    }

    int32_t optval = 1;
	
	if(::setsockopt(listenSocketFd,SOL_SOCKET,SO_REUSEPORT,&optval,sizeof(optval)) < 0)
    {
		LOG_SYSERR<<"Set SO_REUSEPORT socket failed! error "<<strerror(errno);
        ::close(listenSocketFd);
        return false;
    }
	
  
    if(::setsockopt(listenSocketFd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval)) < 0)
    {
		LOG_SYSERR<<"Set SO_REUSEADDR socket  failed! error "<<strerror(errno);
		::close(listenSocketFd);
        return false;
    }

    if(::bind(listenSocketFd, (struct sockaddr*)&serverAdress,sizeof(serverAdress)) < 0 )
    {
        LOG_SYSERR<<"Bind bind socket failed! error "<<strerror(errno);
        ::close(listenSocketFd);
        return false;
    }

    if(::listen(listenSocketFd,SOMAXCONN))
    {
        LOG_SYSERR<<"Listen listen socket failed! error "<<strerror(errno);
        close(listenSocketFd);
        return false;
    }

#ifdef __linux__
    setsockopt(listenSocketFd,IPPROTO_TCP,TCP_NODELAY,&optval,static_cast<socklen_t>(sizeof optval));

    int32_t len = 65536;
    setsockopt(listenSocketFd,SOL_SOCKET,SO_RCVBUF,(void*)&len,sizeof(len));
    setsockopt(listenSocketFd,SOL_SOCKET,SO_SNDBUF,(void*)&len,sizeof(len));
#endif

    return true;
}

bool xSocket::setTcpNoDelay(int32_t socketFd,bool on)
{
#ifdef __linux__
	int32_t optval = on ? 1 : 0;
	::setsockopt(socketFd,IPPROTO_TCP,TCP_NODELAY,&optval,static_cast<socklen_t>(sizeof optval));
#endif
}

bool xSocket::setSocketBlock(int32_t socketFd)
{
    int32_t opt = fcntl(socketFd,F_GETFL);
    if (opt < 0)
    {
        LOG_WARN<<"fcntl F_GETFL) failed! error"<<strerror(errno);
        return false;
    }

    opt = opt &~ O_NONBLOCK;
    if (fcntl(socketFd,F_SETFL,opt) < 0)
    {
    	LOG_WARN<<"fcntl F_GETFL) failed! error"<<strerror(errno);
        return false;
    }

    return true;
}


bool xSocket::setSocketNonBlock(int32_t socketFd)
{
    int32_t opt = fcntl(socketFd,F_GETFL);
    if (opt < 0)
    {
        LOG_WARN<<"fcntl F_GETFL) failed! error"<<strerror(errno);
        return false;
    }

    opt = opt | O_NONBLOCK;
    if (fcntl(socketFd,F_SETFL,opt) < 0)
    {
    	LOG_WARN<<"fcntl F_GETFL) failed! error"<<strerror(errno);
        return false;
    }

    return true;
}



