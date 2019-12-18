#include "Package/Package.h"
#include "Config.h"
#include "Log.h"

#include "root_dir.h"


int main(int argc, char const *argv[])
{
    string configFile = string(root) + "/Config/config.conf";
    Config c(configFile);

    string logFile = string(root) + "/Log/test.log";
    Log l(logFile);

    l.writeLog(l.ERROR, "Nothing");
    return 0;
}
