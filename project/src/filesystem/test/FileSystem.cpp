#include "FileOperations.h"

using std::cout;
using std::endl;

int main(int argc, char const *argv[])
{
    FileOperations f("../config.conf");

    MD5Code m = "HHH";
    cout << f.IsExists(m) << endl;

    UploadPushBody p;
    strcpy(p.content, "HHHHHH");
    // cout << p.content << endl;
    p.len = 6;
    p.id = 3;

    f.WriteFile(m, p);
    return 0;
}
