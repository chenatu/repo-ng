#ifndef REPO_HANDLES_SOAP_HANDLE_HPP
#define REPO_HANDLES_SOAP_HANDLE_HPP

#include <sys/socket.h>
#include "boost/thread.hpp"
#include <boost/scoped_ptr.hpp>
#include "common.hpp"

#include "storage/repo-storage.hpp"
#include "soapService.h"
//#include "ns.nsmap.h"

namespace repo {

class SoapHandle : noncopyable
{
public:
  class Error : std::runtime_error
  {
  public:
    explicit
    Error(const std::string& what)
      : std::runtime_error(what)
    {
    }
  };

public:
  SoapHandle(boost::asio::io_service& ioService, RepoStorage& storageHandle)
    : m_ioService(ioService),
      m_service(Service(storageHandle)),
      m_masterFd(-1),
      m_work(new boost::asio::io_service::work(m_ioService)),
      m_thread(boost::bind(&boost::asio::io_service::run, &m_ioService))
  {
  }

  ~SoapHandle()
  {
    std::cout << "call soap handle destructor" << std::endl;
    //m_work.reset();
    m_thread.join();
    //m_service.destroy();
  }

  void
  listen(const char* host, int port)
  {
    m_host = const_cast<char*>(host);
    m_port = port;
    m_masterFd = m_service.bind(m_host, m_port, 100);
    if (m_masterFd < 0) {
      throw Error("bind error");
      return;
    }
    asyncAccept();
  }

  void
  stop()
  {
    std::cout << "call stop" << std::endl;
    if (m_masterFd > 0) {
      if (close(m_masterFd) == -1) {
        std::cout << "close failed" <<std::endl;
      }
    }
    m_work.reset();
    //m_thread.join();
    m_service.destroy();
  }

  void
  asyncAccept()
  {
    std::cout << "async accept..." << std::endl;
    m_ioService.post(boost::bind(&SoapHandle::acceptOperation, this));
  }

  void
  acceptOperation()
  {
    std::cout << "start to accept" << std::endl;
    if (m_ioService.stopped()) {
      std::cout << "io_service stopped" << std::endl;
      return;
    }
    int acceptFd = m_service.accept();
    std::cout << "after accept" << std::endl;
    if (acceptFd > 0) {
      m_ioService.post(boost::bind(&SoapHandle::acceptHandle, this, m_service.copy()));
    }
    m_ioService.post(boost::bind(&SoapHandle::acceptOperation, this));
  }

  void
  acceptHandle(Service* service)
  {
    std::cout << "serve" << std::endl;
    service->serve();
    service->destroy();
  }

private:
  boost::asio::io_service& m_ioService;
  boost::asio::io_service m_asyncIoService;
  Service m_service;
  int m_masterFd;
  char* m_host;
  int m_port;
  boost::scoped_ptr<boost::asio::io_service::work> m_work;
  boost::thread m_thread;
};

} // namespace repo

#endif // REPO_HANDLES_BASE_HANDLE_HPP