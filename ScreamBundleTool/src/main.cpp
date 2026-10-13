#include "ScreamBundleTool/ArchiveTool.hpp"

#include <exception>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Usage(std::ostream& output) {
    output
        << "ScreamBundleTool - Scream BNDL/FCL/LHD inspector and rebuilder\n\n"
        << "Usage:\n"
        << "  ScreamBundleTool info <archive> [--verbose]\n"
        << "  ScreamBundleTool list <archive> [--verbose]\n"
        << "  ScreamBundleTool extract <archive> <output-directory> [--verbose] [--force]\n"
        << "  ScreamBundleTool pack <input-directory> <output.bundle|output.fcl> [--force]\n"
        << "  ScreamBundleTool repack <original> <modified-directory> <output> [--verbose] [--force]\n"
        << "  ScreamBundleTool verify <original> <candidate> [--verbose] [--strict]\n\n"
        << "For logical FCL extraction, place the matching .lhd beside the .fcl.\n"
        << "Repacking an .lhd rebuilds its adjacent FCL set beside the output .lhd.\n";
}

std::vector<std::string> ParseArguments(int argc,
                                        char** argv,
                                        Scream::ArchiveTool::Options& options) {
    std::vector<std::string> positional;
    for (int index = 1; index < argc; ++index) {
        const std::string value = argv[index];
        if (value == "--verbose" || value == "-v") {
            options.verbose = true;
        } else if (value == "--force" || value == "-f") {
            options.force = true;
        } else if (value == "--strict") {
            options.strict = true;
        } else if (value == "--help" || value == "-h") {
            Usage(std::cout);
            std::exit(0);
        } else if (!value.empty() && value[0] == '-') {
            throw std::runtime_error("unknown option: " + value);
        } else {
            positional.push_back(value);
        }
    }
    return positional;
}

void RequireCount(const std::vector<std::string>& args,
                  std::size_t expected,
                  const char* command) {
    if (args.size() != expected) {
        throw std::runtime_error(std::string("invalid arguments for '") + command + "'");
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        Scream::ArchiveTool::Options options;
        const auto args = ParseArguments(argc, argv, options);
        if (args.empty()) {
            Usage(std::cerr);
            return 2;
        }

        const std::string& command = args[0];
        if (command == "info") {
            RequireCount(args, 2, "info");
            Scream::ArchiveTool::PrintInfo(args[1], options);
        } else if (command == "list") {
            RequireCount(args, 2, "list");
            Scream::ArchiveTool::PrintList(args[1], options);
        } else if (command == "extract") {
            RequireCount(args, 3, "extract");
            Scream::ArchiveTool::Extract(args[1], args[2], options);
        } else if (command == "pack") {
            RequireCount(args, 3, "pack");
            Scream::ArchiveTool::Pack(args[1], args[2], options);
        } else if (command == "repack") {
            RequireCount(args, 4, "repack");
            Scream::ArchiveTool::Repack(args[1], args[2], args[3], options);
        } else if (command == "verify") {
            RequireCount(args, 3, "verify");
            return Scream::ArchiveTool::Verify(args[1], args[2], options) ? 0 : 1;
        } else {
            throw std::runtime_error("unknown command: " + command);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
