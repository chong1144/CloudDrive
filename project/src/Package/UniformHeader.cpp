#include <Package/UniformHeader.h>

UniformHeader::UniformHeader()
{
    len = 0;
}

UniformHeader::UniformHeader(const PackageType &pIn) : p(pIn)
{
    // 填充包的大小
    len = PackageSizeMap.at(p);
}
