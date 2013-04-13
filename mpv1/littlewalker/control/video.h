
#if !defined(command_capture_h)
#define command_capture_h

#include <string>

extern void open_dev(std::string const &path, int width, int height);
extern void close_dev();
extern void start_capture();
extern void stop_capture();
extern void capture_frame(void *&out_ptr, size_t &out_size);

#endif  //  command_capture_h
