#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <stdint.h>
#include <atomic>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>
#include <boost/iostreams/operations.hpp>
#include <boost/iostreams/read.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "../src/handles/soapProxy.h"
#include "../src/handles/ns.nsmap"

namespace repo {

std::atomic<bool> threadError (false);

class Error : public std::runtime_error
{
public:
  explicit
  Error(const std::string& what)
    : std::runtime_error(what)
  {
  }
};

class StreamEndError : public std::runtime_error
{
public:
  explicit
  StreamEndError(const std::string& what)
    : std::runtime_error(what)
  {
  }
};

class thread_pool
{
private:
  boost::asio::io_service io_service_;
  boost::asio::io_service::work work_;
  boost::thread_group threads_;
  std::size_t available_;
  boost::mutex mutex_;

public:

  /// @brief Constructor.
  thread_pool(std::size_t pool_size)
    : work_(io_service_),
      available_(pool_size)
  {
    for (std::size_t i = 0; i < pool_size; ++i)
    {
      threads_.create_thread(boost::bind(&boost::asio::io_service::run,
                                           &io_service_));
    }
  }

  /// @brief Destructor.
  ~thread_pool()
  {
    // Force all threads to return from io_service::run().
    io_service_.stop();

    // Suppress all exceptions.
    threads_.join_all();
  }

  void
  stop()
  {
    io_service_.stop();
    threads_.join_all();
  }

  /// @brief Adds a task to the thread pool if a thread is currently available.
  template <typename Task>
  bool run_task(Task task)
  {
    boost::unique_lock< boost::mutex > lock( mutex_ );

    // If no threads are available, then return.
    if (0 == available_) return false;

    // Decrement count, indicating thread is no longer available.
    --available_;

    // Post a wrapped task into the queue.
    io_service_.post(boost::bind(&thread_pool::wrap_task, this,
                                 boost::function<int()>(task)));
    return true;
  }

private:
  /// @brief Wrap a task so that the available count can be increased once
  ///        the user provided task has completed.
  void wrap_task(boost::function<int()>task)
  {
    // Run the user supplied task.
    int response = task();
    if (response == 200) {
      threadError = true;
    }
    // Task has finished, so increment count of available threads.
    boost::unique_lock< boost::mutex > lock( mutex_ );
    ++available_;
  }
};

using namespace ndn::time;

using std::shared_ptr;
using std::make_shared;
using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;

static const uint64_t DEFAULT_BLOCK_SIZE = 1000;
static const uint64_t DEFAULT_INTEREST_LIFETIME = 4000;
static const uint64_t DEFAULT_FRESHNESS_PERIOD = 10000;
static const uint64_t DEFAULT_CHECK_PERIOD = 1000;
static const size_t PRE_SIGN_DATA_COUNT = 10;

class SoapPutFile : ndn::noncopyable {
public:
  SoapPutFile()
    : isUnversioned(false)
    , isSingle(false)
    , useDigestSha256(false)
    , freshnessPeriod(DEFAULT_FRESHNESS_PERIOD)
    , hasTimeout(false)
    , timeout(0)
    , insertStream(0)
    , isVerbose(false)

    , m_timestampVersion(toUnixTimestamp(system_clock::now()).count())
    , m_currentSegmentNo(0)
    , m_isFinished(false)
    , m_thread_pool(PRE_SIGN_DATA_COUNT)
    {}

  void
  run();

private:
  void
  startSegmentInsert();

  void
  startSingleInsert();

  int
  putData(ndn::Data& data);

  void
  prepareNextData(uint64_t referenceSegmentNo);

  void
  signData(ndn::Data& data);

public:
  bool isUnversioned;
  bool isSingle;
  bool useDigestSha256;
  std::string identityForData;
  milliseconds freshnessPeriod;
  bool hasTimeout;
  milliseconds timeout;
  ndn::Name ndnName;
  std::istream* insertStream;
  bool isVerbose;
  char* server;
  Proxy proxy;

private:
  ndn::KeyChain m_keyChain;
  uint64_t m_timestampVersion;
  size_t m_currentSegmentNo;
  bool m_isFinished;
  ndn::Name m_dataPrefix;

  typedef std::map<uint64_t, shared_ptr<ndn::Data> > DataContainer;
  DataContainer m_data;

  thread_pool m_thread_pool;
  ndn::time::steady_clock::time_point startTime;
};

int
SoapPutFile::putData(ndn::Data& data)
{
  std::vector<unsigned char> d;
  const unsigned char* rawData = data.wireEncode().wire();
  size_t rawDataSize = data.wireEncode().size();
  d.assign(rawData, rawData + rawDataSize);
  int response;
  std::cout << "before insert" << std::endl;
  Proxy newProxy;
  newProxy.soap_endpoint =server;
  newProxy.insert(d, &response);
  std::cout << "after insert" << std::endl;
  return response;
}

void
SoapPutFile::prepareNextData(uint64_t referenceSegmentNo)
{
  // make sure m_data has [referenceSegmentNo, referenceSegmentNo + PRE_SIGN_DATA_COUNT] Data
  if (m_isFinished)
    return;

  size_t nDataToPrepare = PRE_SIGN_DATA_COUNT;

  if (!m_data.empty()) {
    uint64_t maxSegmentNo = m_data.rbegin()->first;

    if (maxSegmentNo - referenceSegmentNo >= nDataToPrepare) {
      // nothing to prepare
      return;
    }

    nDataToPrepare -= maxSegmentNo - referenceSegmentNo;
  }

  for (size_t i = 0; i < nDataToPrepare && !m_isFinished; ++i) {
    uint8_t buffer[DEFAULT_BLOCK_SIZE];

    std::streamsize readSize =
      boost::iostreams::read(*insertStream, reinterpret_cast<char*>(buffer), DEFAULT_BLOCK_SIZE);

    if (readSize <= 0) {
      throw Error("Error reading from the input stream");
    }

    shared_ptr<ndn::Data> data =
      make_shared<ndn::Data>(ndn::Name(m_dataPrefix)
                                    .appendSegment(m_currentSegmentNo));

    if (insertStream->peek() == std::istream::traits_type::eof()) {
      data->setFinalBlockId(ndn::name::Component::fromSegment(m_currentSegmentNo));
      m_isFinished = true;
    }

    data->setContent(buffer, readSize);
    data->setFreshnessPeriod(freshnessPeriod);
    signData(*data);

    m_data.insert(std::make_pair(m_currentSegmentNo, data));

    ++m_currentSegmentNo;
  }
}

void
SoapPutFile::signData(ndn::Data& data)
{
  if (useDigestSha256) {
    m_keyChain.signWithSha256(data);
  }
  else {
    if (identityForData.empty())
      m_keyChain.sign(data);
    else {
      ndn::Name keyName = m_keyChain.getDefaultKeyNameForIdentity(ndn::Name(identityForData));
      ndn::Name certName = m_keyChain.getDefaultCertificateNameForKey(keyName);
      m_keyChain.sign(data, certName);
    }
  }
}

void
SoapPutFile::startSegmentInsert()
{
  uint64_t segmentNo = 0;
  prepareNextData(0);
  while (true) {
    try {
      std::cout << "segment no: " << segmentNo << std::endl;
      DataContainer::iterator item = m_data.find(segmentNo);
      if (item == m_data.end()) {
        std::cerr << "data segment not prepared" << std::endl;
        ndn::time::steady_clock::time_point endTime = ndn::time::steady_clock::now();
        std::cout << "segment put last time: " << ndn::time::duration_cast<ndn::time::milliseconds>(endTime - startTime) << " ms" << std::endl;
        m_thread_pool.stop();
        return;
      }
      while (!m_thread_pool.run_task(boost::bind(&SoapPutFile::putData, this, *item->second)) && !threadError) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(1));
        if (threadError) {
          throw Error("put segment data error");
          m_thread_pool.stop();
          return;
        }
      }
      segmentNo++;
      prepareNextData(segmentNo);
    }
    catch (StreamEndError& e){
      std::cout << "input stream end" << std::endl;
      return;
    }
  }

}

void
SoapPutFile::startSingleInsert()
{
  std::cout << "startSingleInsert" << std::endl;
  uint8_t buffer[DEFAULT_BLOCK_SIZE];
  std::streamsize readSize =
    boost::iostreams::read(*insertStream, reinterpret_cast<char*>(buffer), DEFAULT_BLOCK_SIZE);

  if (readSize <= 0) {
    throw Error("Error reading from the input stream");
  }

  if (insertStream->peek() != std::istream::traits_type::eof()) {
    throw Error("Input data does not fit into one Data packet");
  }

  shared_ptr<ndn::Data> data = make_shared<ndn::Data>(m_dataPrefix);
  data->setContent(buffer, readSize);
  data->setFreshnessPeriod(freshnessPeriod);
  signData(*data);
  if (!putData(*data)) {
    throw Error("put single data error");
  }
}

void
SoapPutFile::run()
{
  startTime = ndn::time::steady_clock::now();
  std::cout << "run" << std::endl;
  m_dataPrefix = ndnName;
  if (!isUnversioned)
    m_dataPrefix.appendVersion(m_timestampVersion);
  try {
    if (isSingle) {
      startSingleInsert();
    }
    else {
      startSegmentInsert();
    }
  }
  catch (Error& e) {
    std::cerr << e.what() << std::endl;
  }
}

static void
usage()
{
  fprintf(stderr,
          "ndnputfile [-u] [-s] [-D] [-i identity]"
          "  [-x freshness] [-w timeout] server ndn-name filename\n"
          "\n"
          " Write a file into a repo.\n"
          "  -u: unversioned: do not add a version component\n"
          "  -s: single: do not add version or segment component, implies -u\n"
          "  -D: use DigestSha256 signing method instead of SignatureSha256WithRsa\n"
          "  -i: specify identity used for signing Data\n"
          "  -x: FreshnessPeriod in milliseconds\n"
          "  -w: timeout in milliseconds for whole process (default unlimited)\n"
          "  -v: be verbose\n"
          "  server: host:port\n"
          "  ndn-name: NDN Name prefix for written Data\n"
          "  filename: local file name; \"-\" reads from stdin\n"
          );
  exit(1);
}

int
main(int argc, char** argv)
{
  SoapPutFile soapPutFile;
  int opt;
  while ((opt = getopt(argc, argv, "usDi:x:w:vh")) != -1) {
    switch (opt) {
    case 'u':
      soapPutFile.isUnversioned = true;
      break;
    case 's':
      soapPutFile.isSingle = true;
      break;
    case 'D':
      soapPutFile.useDigestSha256 = true;
      break;
    case 'i':
      soapPutFile.identityForData = std::string(optarg);
      break;
    case 'x':
      try {
        soapPutFile.freshnessPeriod = milliseconds(boost::lexical_cast<uint64_t>(optarg));
      }
      catch (boost::bad_lexical_cast&) {
        std::cerr << "-x option should be an integer.";
        return 1;
      }
      break;
    case 'w':
      soapPutFile.hasTimeout = true;
      try {
        soapPutFile.timeout = milliseconds(boost::lexical_cast<uint64_t>(optarg));
      }
      catch (boost::bad_lexical_cast&) {
        std::cerr << "-w option should be an integer.";
        return 1;
      }
      break;
    case 'v':
      soapPutFile.isVerbose = true;
      break;
    case 'h':
      usage();
      break;
    default:
      break;
    }
  }

  argc -= optind;
  argv += optind;

  if (argc != 3)
    usage();

  soapPutFile.server = argv[0];
  soapPutFile.proxy.soap_endpoint = argv[0];
  soapPutFile.ndnName = ndn::Name(argv[1]);
  if (strcmp(argv[2], "-") == 0) {

    soapPutFile.insertStream = &std::cin;
    soapPutFile.run();
  }
  else {
    std::ifstream inputFileStream(argv[2], std::ios::in | std::ios::binary);
    if (!inputFileStream.is_open()) {
      std::cerr << "ERROR: cannot open " << argv[2] << std::endl;
      return 1;
    }

    soapPutFile.insertStream = &inputFileStream;
    soapPutFile.run();
  }

  // ndnPutFile MUST NOT be used anymore because .insertStream is a dangling pointer

  return 0;
}

} // namespace repo

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