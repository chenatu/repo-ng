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
