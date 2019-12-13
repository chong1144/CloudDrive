#include "Log.h"
#include <iostream>
#include <fstream>
#include <sys/timeb.h>
#include <ctime>
using std::endl;
using std::ios;
using std::to_string;
// #define DEBUG

Log::Log(const char *path) : logPosition(path)
{
    // clear log file
    std::fstream fout;
    //std::cout<<"path: "<<logPosition<<endl;
    fout.open(this->logPosition.c_str(),  ios::out | ios::binary);
    //fout.clear();
    //std::cout<<fout.is_open()<<endl;
    fout.close();
}

Log::Log (const string& path) : logPosition (path)
{ 
	// clear log file
	std::fstream fout;
	//std::cout<<"path: "<<logPosition<<endl;
	fout.open (this->logPosition, ios::out | ios::binary);
	//fout.clear();
	//std::cout<<fout.is_open()<<endl;
	fout.close ();
}

void Log::init (const string& path)
{
	logPosition = path;
	// clear log file
	std::fstream fout;
	//std::cout<<"path: "<<logPosition<<endl;
	fout.open (this->logPosition, ios::out | ios::binary);
	//fout.clear();
	//std::cout<<fout.is_open()<<endl;
	fout.close ();
}

Log::~Log()
{

}


void Log::writeLog(Log::LoggingLevel level, std::string Content)
{

#ifdef DEBUG
    std::ostream &out = std::cout;
#endif

#ifndef DEBUG
    std::fstream fout;
    fout.open(this->logPosition.c_str(), ios::out | ios::app | ios::binary);
    std::ostream &out = fout;
#endif

    time_t cur = time(0);
    std::string curtime(ctime(&cur));
    curtime.pop_back();
    curtime += " ";
    out << curtime;
    switch (level)
    {
    case Log::LoggingLevel::INFO:
        out << "[INFO]: ";
        break;
    case Log::LoggingLevel::WARNING:
        out << "[WARNING]: ";
        break;
    case Log::LoggingLevel::ERROR:
        out << "[ERROR]: ";
        break;
    default:
        break;
    }

	ftime (&tb);
	out << Content;
	out << ' ' << tb.time << '.' << tb.millitm << endl;

}