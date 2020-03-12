#include "../Package/Package.h"
#include "../Utility/Config.h"
#include "../Utility/Log.h"

#include <fstream>
#include <iostream>
#include <queue>

using std::cout;
using std::endl;
using std::queue;
using std::string;

class WriteFile
{
private:
    int fifo_rtowf;
    int fifo_wftor;

    queue<UniformHeader> write_cmd;
public:
    WriteFile(/* args */);
    ~WriteFile();
};



WriteFile::WriteFile(/* args */)
{
}

WriteFile::~WriteFile()
{
}
