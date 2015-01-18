#include "soapService.h"

int Service::read(std::vector<unsigned char >interest, std::vector<unsigned char >*response) {
  std::cout << "read:" << std::endl;
  ndn::Interest i(ndn::Block(interest.data(), interest.size()));
  std::cout << "interest name: " << i << std::endl;
  ndn::shared_ptr<ndn::Data> data = m_storageHandle.readData(i);
  if (data == NULL) {
  	std::cout << "no data" << std::endl;
  	return SOAP_NO_DATA;
  }
  const unsigned char* rawData = data->wireEncode().wire();
  size_t rawDataSize = data->wireEncode().size();
  response->assign(rawData, rawData + rawDataSize);
  return SOAP_OK;
}

int Service::insert(std::vector<unsigned char >data, int*response) {
  std::cout << "write:" << std::endl;
  ndn::Data d(ndn::Block(data.data(), data.size()));
  std::cout << "data name: " << d.getName() << std::endl;
  std::cout << "segment: " << d.getName().get(-1).toSegment() << std::endl;
  int res = 200;
  if (m_storageHandle.insertData(d)) {
    response = &res;
    return SOAP_OK;
  }
  else {
    res = 404;
    response = &res;
    return SOAP_FATAL_ERROR;
  }
}

int Service::remove(std::vector<unsigned char >interest, int*response) {
  return SOAP_OK;
}
