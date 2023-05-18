
#include <iostream>

#include "config.h"
#include "cxxopts.h"

Config::Config()
{}

Config::~Config()
{}

int Config::ParseOpts(int argc, char *argv[])
{
    try {
        cxxopts::Options options(argv[0], " - frer daemon command line options");
        options
            .positional_help("[optional args]")
            .show_positional_help()
            .set_width(80)
            .set_tab_expansion()
            .add_options()
            ("h,help", "Print help")
            ("i,interface", "e.g.: eth0", cxxopts::value<std::string>(), "DEV")
            ("d,debug", "Enable debugging")
            ;

        auto result = options.parse(argc, argv);
        /* options.parse_positional({""}); */

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return true;
        }

        if (result.count("interface")) {
            dev_ = result["interface"].as<std::string>();
        }
    }
    catch (const cxxopts::exceptions::exception& e) {
        std::cout << "error parsing options: " << e.what() << std::endl;
        return false;
    }

    return true;
}

