#include "Package.h"
#include <iostream>

using std::cout;
using std::endl;

int main(int argc, char const *argv[])
{
    UniformHeader u(PackageType::DOWNLOAD_PUSH);

    // PackageSizeMap.at(PackageType::UPLOAD);

    cout << u.len << " " << sizeof(u) << endl;

    return 0;
}
