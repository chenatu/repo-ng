#ifndef REPO_NG_TOOLS_SOAPGETFILE_HPP
#define REPO_NG_TOOLS_SOAPGETFILE_HPP

#include "../src/handles/soapProxy.h"
#include "../src/handles/ns.nsmap"
#include "../src/common.hpp"

namespace repo {

class SoapGetFile : ndn::noncopyable
{
public:
  class Error : public std::runtime_error
  {
  public:
    explicit
    Error(const std::string& what)
      : std::runtime_error(what)
    {
    }
  };

  SoapGetFile(boost::asio::io_service& ioService, char* server,
           const std::string& dataName, std::ostream& os,
           bool verbose, bool versioned, bool single,
           int interestLifetime, int timeout,
           bool mustBeFresh = false)
    : m_ioService(ioService)
    , m_dataName(dataName)
    , m_os(os)
    , m_verbose(verbose)
    , m_hasVersion(versioned)
    , m_isSingle(single)
    , m_isFinished(false)
    , m_isFirst(true)
    , m_interestLifetime(interestLifetime)
    , m_timeout(timeout)
    , m_nextSegment(0)
    , m_totalSize(0)
    , m_retryCount(0)
    , m_mustBeFresh(mustBeFresh)
    {
      m_proxy.soap_endpoint = server;
    }

  void
  run();

private:
  void
  fetchData(const ndn::Name& name);

  void
  onVersionedData(std::vector<unsigned char> data);

  void
  onUnversionedData(std::vector<unsigned char> data);

  void
  expressInterest(const ndn::Interest interest);

  void
  readData(const ndn::Data& data);

  void
  fetchNextData(const ndn::Name& name, const ndn::Data& data);

private:

  boost::asio::io_service& m_ioService;
  ndn::Name m_dataName;
  std::ostream& m_os;
  bool m_verbose;
  bool m_hasVersion;
  bool m_isSingle;
  bool m_isFinished;
  bool m_isFirst;
  ndn::time::milliseconds m_interestLifetime;
  ndn::time::milliseconds m_timeout;
  uint64_t m_nextSegment;
  int m_totalSize;
  int m_retryCount;
  bool m_mustBeFresh;
  Proxy m_proxy;
};

} // namespace repo

#endif // REPO_NG_TOOLS_SOAPGETFILE_HPP