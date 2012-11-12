
#if !defined(rl2_async_h)
#define rl2_async_h

#include <stdlib.h>

void start_async_file_dump(char const *dirname);
void stop_async_file_dump();
bool async_file_dump(char const *name, void const *data, size_t size);

#endif  //  rl2_async_h
