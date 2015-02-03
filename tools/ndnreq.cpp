#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/scheduler.hpp>

#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <atomic>
#include <string>
#include <random>
#include "../src/handles/ns.h"

namespace repo {

using std::shared_ptr;
using std::make_shared;
using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;

static const uint64_t DEFAULT_INTEREST_LIFETIME = 4000;
static const uint64_t DEFAULT_CHECK_PERIOD = 1000;
static const uint64_t DEFAULT_LIMIT = 10000;
static const uint64_t DEFAULT_PIPELINE = 20;

using namespace ndn::time;

static std::mt19937_64 m_rng(std::chrono::system_clock::now().time_since_epoch().count());

class NdnReq : ndn::noncopyable
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

  NdnReq(bool hasLimit, int limit, bool hasTimeout, milliseconds timeout, ndn::Name repoPrefix,
    std::ostream& os, int pipeline, uint64_t interestLifetime)
    : hasLimit(hasLimit)
    , limit(limit)
    , hasTimeout(hasTimeout)
    , timeout(timeout)
    , repoPrefix(repoPrefix)
    , os(os)
    , checkPeriod(DEFAULT_CHECK_PERIOD)
    , pipeline(DEFAULT_PIPELINE)
    , interestLifetime(DEFAULT_INTEREST_LIFETIME)

    , m_scheduler(m_face.getIoService())
    , m_sentCount(0)
    , m_recvCount(0)
    , m_timeoutCount(0)
  {
    if (!hasLimit)
      this->limit = DEFAULT_LIMIT;
    //m_rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
  }

  void
  run();

private:
  void
  stopProcess();

  void
  checkStatus();

  void
  start();

  void
  expressReqInterest();

  void
  onReqData(const ndn::Interest& interest, ndn::Data& data);

  void
  onReqTimeout(const ndn::Interest& interest);

public:
  bool hasLimit;
  int limit;
  bool hasTimeout;
  milliseconds timeout;
  ndn::Name repoPrefix;
  std::ostream& os;
  milliseconds checkPeriod;
  int pipeline;
  uint64_t interestLifetime;
  //static std::mt19937_64 m_rng;

private:
  ndn::Face m_face;
  ndn::Scheduler m_scheduler;
  std::atomic_int m_sentCount;
  std::atomic_int m_recvCount;
  std::atomic_int m_timeoutCount;
  std::chrono::system_clock::time_point m_start;
  boost::thread_group m_threads;
};

void
NdnReq::run()
{
  if (hasTimeout)
    m_scheduler.scheduleEvent(timeout, bind(&NdnReq::stopProcess, this));

  m_scheduler.scheduleEvent(milliseconds(checkPeriod),
                            bind(&NdnReq::checkStatus, this));

  m_start = std::chrono::system_clock::now();
  start();
  m_face.processEvents();
}

void
NdnReq::stopProcess()
{
  std::cout << "stopProcess" << std::endl;
  m_face.getIoService().stop();
  m_threads.join_all();
  checkStatus();
}

void
NdnReq::checkStatus()
{
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_start);
  os << duration.count() << " " << m_sentCount.load() << " " << m_recvCount.load() << " " << m_timeoutCount.load() << std::endl;
  m_scheduler.scheduleEvent(milliseconds(checkPeriod),
                          bind(&NdnReq::checkStatus, this));
}

//express a req interest with a random suffix
void
NdnReq::expressReqInterest()
{
  ndn::Name reqName(repoPrefix);
  reqName.append("req").append(std::to_string(m_rng()).c_str());
  ndn::Interest reqInterest(reqName);
  reqInterest.setInterestLifetime(milliseconds(interestLifetime));
  m_face.expressInterest(reqInterest,
                         bind(&NdnReq::onReqData, this, _1, _2),
                         bind(&NdnReq::onReqTimeout, this, _1));
  m_sentCount++;
}

void
NdnReq::onReqData(const ndn::Interest& interest, ndn::Data& data) {
  m_recvCount++;
  if (m_sentCount.load() < limit) {
    expressReqInterest();
  }
  else {
    std::cout << "exceed limit:" << limit << std::endl;
    stopProcess();
  }
}

void
NdnReq::onReqTimeout(const ndn::Interest& interest) {
  m_timeoutCount++;
  std::cout << "timeout" << std::endl;
  if (m_sentCount.load() < limit) {
    expressReqInterest();
  }
  else {
    std::cout << "exceed limit" << std::endl;
    stopProcess();
  }
}

void
NdnReq::start()
{
  for (int i = 0; i < pipeline; i++)
    m_threads.create_thread(boost::bind(&NdnReq::expressReqInterest, this));
}

static void
usage()
{
  fprintf(stderr,
          "ndnreq [-l] [-w] [-o] [-p] [-t] repo-prefix"
          "\n"
          " Write a file into a repo.\n"
          "  -l: InterestLifetime in milliseconds for each command\n"
          "  -w: total count for interest (default 10000)\n"
          "  -o: outputfile name (default std::out)\n"
          "  -p: pipline"
          "  -t: timeout"
          "  repo-prefix: repo command prefix\n"
          );
  exit(1);
}

int
main(int argc, char** argv)
{
  std::string name;
  const char* outputFile = 0;
  int interestLifetime;
  bool hasLimit = false;
  int limit = 0;
  int pipeline = 0;
  bool hasTimeout = false;
  milliseconds timeout;

  int opt;
  while ((opt = getopt(argc, argv, "l:w:o:t:p:")) != -1)
    {
      switch (opt) {
      case 'l':
        try
          {
            interestLifetime = boost::lexical_cast<int>(optarg);
          }
        catch (boost::bad_lexical_cast&)
          {
            std::cerr << "ERROR: -l option should be an integer." << std::endl;
            return 1;
          }
        break;
      case 'w':
        try
          {
            hasLimit = true;
            limit = boost::lexical_cast<int>(optarg);
          }
        catch (boost::bad_lexical_cast&)
          {
            std::cerr << "ERROR: -w option should be an integer." << std::endl;
            return 1;
          }
        break;
      case 'p':
        try
          {
            pipeline = boost::lexical_cast<int>(optarg);
          }
        catch (boost::bad_lexical_cast&)
          {
            std::cerr << "ERROR: -w option should be an integer." << std::endl;
            return 1;
          }
        break;

      case 't':
        try
          {
            hasTimeout = true;
            timeout = milliseconds(boost::lexical_cast<int>(optarg));
          }
        catch (boost::bad_lexical_cast&)
          {
            std::cerr << "ERROR: -w option should be an integer." << std::endl;
            return 1;
          }
        break;
      case 'o':
        outputFile = optarg;
        break;
      default:
        usage();
        return 0;
      }
    }

  argc -= optind;
  argv += optind;

  if (argc != 1) {
    usage();
    return 0;
  }

  ndn::Name repoPrefix(argv[0]);


  std::streambuf* buf;
  std::ofstream of;

  if (outputFile != 0)
    {
      of.open(outputFile, std::ios::out | std::ios::binary | std::ios::trunc);
      if (!of || !of.is_open()) {
        std::cerr << "ERROR: cannot open " << outputFile << std::endl;
        return 1;
      }
      buf = of.rdbuf();
    }
  else
    {
      buf = std::cout.rdbuf();
    }

  std::ostream os(buf);

  NdnReq ndnReq(hasLimit, limit, hasTimeout, timeout, repoPrefix, os, pipeline, interestLifetime);

  ndnReq.run();

  return 0;
}

}

int
main(int argc, char** argv)
{
  try {
    return repo::main(argc, argv);
  }
  catch (std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 2;
  }
}