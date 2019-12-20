#include <control.h>
#include <Log.h>
#include <Config.h>
#include <root_dir.h>
#include <Database.h>

int main(int argc, char const *argv[])
{
    string configFile = string(root) + "/Config/config.conf";
    string logFile = string(root) + "/Log/test.log";

    Config c(configFile);
    Log l(logFile);

    int forkres = 0;
    control *con;
    Database *db;
    for (size_t i = 0; i < 2; i++)
    {
        forkres = fork();

        switch (forkres)
        {
            case -1:
                l.writeLog(Log::ERROR, "Fail to fork");
                exit(-1);
            case 0:
                switch (i)
                {
                case 0:
                    con = new control(configFile);
                    l.writeLog(Log::WARNING, "control process out!");
                    exit(0);
                    break;
                case 1:
                    db = new Database(configFile, string(root) + "/Log/database.log");
                    db->Run();
                    l.writeLog(Log::WARNING, "database process out!");
                    exit(0);
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
        }
    }

    delete con;
    delete db;

        return 0;
}
