
#include <string>

class Config {
    public:
        std::string dev_;

        Config();
        ~Config();

        int ParseOpts(int argc, char *argv[]);
};

