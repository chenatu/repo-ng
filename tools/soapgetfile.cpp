#include "soapgetfile.hpp"
#include <boost/lexical_cast.hpp>
#include <fstream>

namespace repo {

void
SoapGetFile::fetchData(const Name& name)
{
  Interest interest(name);
  interest.setInterestLifetime(m_interestLifetime);
  //std::cout<<"interest name = "<<interest.getName()<<std::endl;
  if (m_hasVersion)
  {
    interest.setMustBeFresh(m_mustBeFresh);
  }
  else
  {
    interest.setMustBeFresh(true);
    interest.setChildSelector(1);
  }
  m_ioService.post(boost::bind(&SoapGetFile::expressInterest, this, interest));
}


void
SoapGetFile::expressInterest(const ndn::Interest interest)
{
  std::cout << "expressInterest" << std::endl;
  std::vector<unsigned char> i;
  const unsigned char* rawInterest = interest.wireEncode().wire();
  size_t rawInterestSize = interest.wireEncode().size();
  i.assign(rawInterest, rawInterest + rawInterestSize);

  std::vector<unsigned char> response;
  m_proxy.read(i, &response);
  if (m_hasVersion) {
    m_ioService.post(boost::bind(&SoapGetFile::onVersionedData, this, response));
  }
  else {
    m_ioService.post(boost::bind(&SoapGetFile::onUnversionedData, this, response));
  }
}

void
SoapGetFile::onVersionedData(std::vector<unsigned char> d)
{
  std::cout << "onVersionedData" << std::endl;
  ndn::Data data(ndn::Block(d.data(), d.size()));
  const Name& name = data.getName();

  // the received data name may have segment number or not
  if (name.size() == m_dataName.size()) {
    if (!m_isSingle) {
      Name fetchName = name;
      fetchName.appendSegment(0);
      fetchData(fetchName);
    }
  }
  else if (name.size() == m_dataName.size() + 1) {
    if (!m_isSingle) {
      if (m_isFirst) {
        uint64_t segment = name[-1].toSegment();
        if (segment != 0) {
          fetchData(Name(m_dataName).appendSegment(0));
          m_isFirst = false;
          return;
        }
        m_isFirst = false;
      }
      fetchNextData(name, data);
    }
    else {
      std::cerr << "ERROR: Data is not stored in a single packet" << std::endl;
      return;
    }
  }
  else {
    std::cerr << "ERROR: Name size does not match" << std::endl;
    return;
  }
  readData(data);
}

void
SoapGetFile::onUnversionedData(std::vector<unsigned char> d)
{
  std::cout << "onUnversionedData" << std::endl;
  ndn::Data data(ndn::Block(d.data(), d.size()));
  const Name& name = data.getName();
  //std::cout<<"recevied data name = "<<name<<std::endl;
  if (name.size() == m_dataName.size() + 1) {
    if (!m_isSingle) {
      Name fetchName = name;
      fetchName.append(name[-1]).appendSegment(0);
      fetchData(fetchName);
    }
  }
  else if (name.size() == m_dataName.size() + 2) {
    if (!m_isSingle) {
       if (m_isFirst) {
        uint64_t segment = name[-1].toSegment();
        if (segment != 0) {
          fetchData(Name(m_dataName).append(name[-2]).appendSegment(0));
          m_isFirst = false;
          return;
        }
        m_isFirst = false;
      }
      fetchNextData(name, data);
    }
    else {
      std::cerr << "ERROR: Data is not stored in a single packet" << std::endl;
      return;
    }
  }
  else {
    std::cerr << "ERROR: Name size does not match" << std::endl;
    return;
  }
  readData(data);
}

void
SoapGetFile::fetchNextData(const ndn::Name& name, const ndn::Data& data)
{
  uint64_t segment = name[-1].toSegment();
  BOOST_ASSERT(segment == (m_nextSegment - 1));
  const ndn::name::Component& finalBlockId = data.getMetaInfo().getFinalBlockId();
  if (finalBlockId == name[-1]) {
    m_isFinished = true;
  }
  else
  {
    // Reset retry counter
    m_retryCount = 0;
    if (m_hasVersion)
      fetchData(Name(m_dataName).appendSegment(m_nextSegment++));
    else
      fetchData(Name(m_dataName).append(name[-2]).appendSegment(m_nextSegment++));
  }
}

void
SoapGetFile::readData(const ndn::Data& data)
{
  const Block& content = data.getContent();
  m_os.write(reinterpret_cast<const char*>(content.value()), content.value_size());
  m_totalSize += content.value_size();
  if (m_verbose)
  {
    std::cerr << "LOG: received data = " << data.getName() << std::endl;
  }
  if (m_isFinished || m_isSingle) {
    std::cerr << "INFO: End of file is reached." << std::endl;
    std::cerr << "INFO: Total # of segments received: " << m_nextSegment  << std::endl;
    std::cerr << "INFO: Total # bytes of content received: " << m_totalSize << std::endl;
  }
}

void
SoapGetFile::run()
{
  std::cout << SOAP_BUFLEN << std::endl;
  Name name(m_dataName);
  m_nextSegment++;
  fetchData(name);
  m_ioService.run();
}

int
usage(const std::string& filename)
{
  std::cerr << "Usage: \n    "
  << filename << " [-v] [-s] [-u] [-l lifetime] [-w timeout] [-o filename] server ndn-name\n\n"
  << "-v: be verbose\n"
  << "-s: only get single data packet\n"
  << "-u: versioned: ndn-name contains version component\n"
  << "    if -u is not specified, this command will return the rightmost child for the prefix\n"
  << "-l: InterestLifetime in milliseconds\n"
  << "-w: timeout in milliseconds for whole process (default unlimited)\n"
  << "-o: write to local file name instead of stdout\n"
  << "ndn-name: NDN Name prefix for Data to be read\n";
  return 1;
}


int
main(int argc, char** argv)
{
  std::string name;
  const char* outputFile = 0;
  bool verbose = false, versioned = false, single = false;
  int interestLifetime = 4000;  // in milliseconds
  int timeout = 0;  // in milliseconds

  int opt;
  while ((opt = getopt(argc, argv, "vsul:w:o:")) != -1)
  {
    switch (opt) {
      case 'v':
      verbose = true;
      break;
      case 's':
      single = true;
      break;
      case 'u':
      versioned = true;
      break;
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
      interestLifetime = std::max(interestLifetime, 0);
      break;
      case 'w':
      try
      {
        timeout = boost::lexical_cast<int>(optarg);
      }
      catch (boost::bad_lexical_cast&)
      {
        std::cerr << "ERROR: -w option should be an integer." << std::endl;
        return 1;
      }
      timeout = std::max(timeout, 0);
      break;
      case 'o':
      outputFile = optarg;
      break;
      default:
      return usage(argv[0]);
    }
  }

  char* filename = argv[0];

  argc -= optind;
  argv += optind;

  if (argc != 2) {
    return usage(filename);
  }

  char* server = argv[0];

  name = argv[1];

  if (name.empty())
  {
    return usage(filename);
  }

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

  boost::asio::io_service ioService;

  SoapGetFile consumer(ioService, server, name, os, verbose, versioned, single,
    interestLifetime, timeout);

  try
  {
    consumer.run();
  }
  catch (const std::exception& e)
  {
    std::cerr << "ERROR: " << e.what() << std::endl;
  }

  return 0;
}

} // namespace repo


int
main(int argc, char** argv)
{
  return repo::main(argc, argv);
}