
#include "QueuedFile.h"
#include "FakeServices.h"
#include "testing.h"
#include <unistd.h>


static FakeServices fsvc;

int main() {
    boost::shared_ptr<QueuedFile> qf1(QueuedFile::create("file1.q"));
    boost::shared_ptr<QueuedFile> qf2(QueuedFile::create("file2.q"));
    qf1->write("hello, world!\n", 14);
    void *d = new char[1024*1024];
    memset(d, 0, 1024*1024);
    qf2->write(d, 1024*1024);
    qf1->write("and good-bye!\n", 15);
    ::usleep(100000);
    assert_equal((char const *)&fsvc.fakeFiles_["file1.q"][0], "hello, world!\nand good-bye!\n");
    assert_equal(fsvc.fakeFiles_["file1.q"].size(), (size_t)29);
    assert_equal(fsvc.fakeFiles_["file2.q"].size(), (size_t)1024*1024);
    return 0;
}

