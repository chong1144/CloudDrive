#include "Config.h"

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::ios;
using std::string;

Config::Config(const string &configFile)
{
    ifstream fin(configFile.c_str(), ios::in | ios::binary);
    if (!fin.is_open())
    {
        cerr << "Fail to open configuare file -- " << configFile << endl;
    }

    string strTemp;
    string levelAttribute("");
    string attribute;
    string value;
    mapValue mv;

    while (!fin.eof())
    {
        // read line from config file
        getline(fin, strTemp);

        // ignore blank line
        if (strTemp == "")
            continue;

        // remove all the space of the line
        strTemp.erase(remove(strTemp.begin(), strTemp.end(), ' '), strTemp.end());

        if (strTemp.front() == '[' && strTemp.back() == ']')
        {
            if (levelAttribute != "")
            {
                this->cv.insert(pair<string, mapValue>(levelAttribute, mv));
                mv.clear();
            }

            strTemp.erase(strTemp.begin());
            strTemp.pop_back();
            levelAttribute = strTemp;
            // cout << "Level: " << levelAttribute << endl;

            continue;
        }
        auto pos = strTemp.find_first_of('=');
        if (pos != string::npos)
        {
            value = strTemp.substr(pos + 1);
            strTemp.erase(strTemp.begin() + pos, strTemp.end());
            attribute = strTemp;
            mv.insert(pair<string, string>(attribute, value));
            // cout << attribute << ":" << value << endl;
            continue;
        }

        // for test
        // cout << strTemp << endl;

        strTemp.clear();
    }
    this->cv.insert(pair<string, mapValue>(levelAttribute, mv));

    fin.close();
}

Config::~Config()
{
}

string Config::getValue(const string &level, const string &attribute)
{
    return cv[level][attribute];
}
