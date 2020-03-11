#pragma once

#include <map>
#include <string>
#include <vector>

using std::map;
using std::pair;
using std::string;
using std::vector;

using mapValue = map<string, string>;
using configValue = map<string, mapValue>;

class Config
{
private:
    configValue cv;

public:
    Config(const string &configFile);
    ~Config();
    string getValue(const string &level, const string &attribute);
};