#include "Package/Package.h"
#include "Utility/Config.h"
#include "Utility/Log.h"

int main(int argc, char const *argv[])
{
    Config c("../config.conf");

    Log l("../Log/test.log");

    return 0;
}