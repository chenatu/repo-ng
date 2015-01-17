#import "stlvector.h"

int ns__read(std::vector<uint8_t> interest, std::vector<uint8_t>* response);
int ns__insert(std::vector<uint8_t> data, int* response);
int ns__remove(std::vector<uint8_t> interest, int* response);
