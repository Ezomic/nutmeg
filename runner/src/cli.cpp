#include "nutmeg/runner/cli.h"

#include "nutmeg/core/file_io.h"
#include "nutmeg/runner/match_input.h"
#include "nutmeg/runner/stub_sim.h"
#include "nutmeg/runner/version.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <ostream>

namespace nutmeg::runner {

namespace {

constexpr std::uintmax_t kMaxInputBytes = std::uintmax_t{16} << 20;

void usage(std::ostream& out)
{
    out << "usage: nutmeg-run <match.json> [-o <result.json>]\n"
           "       nutmeg-run --version\n"
           "\n"
           "Reads a match file, writes the result as JSON to the -o file (or stdout).\n"
           "Problems with the input go to stderr, one per line.\n"
           "\n"
           "exit codes:\n"
           "  0  ok\n"
           "  1  usage error, or the input could not be read or the output written\n"
           "  2  invalid input\n"
           "  3  unsupported schema or engine version\n";
}

int reject(std::ostream& err, int code, const char* what, const std::vector<Problem>& problems)
{
    for (const auto& problem : problems) {
        err << "nutmeg-run: " << what << ": " << (problem.path.empty() ? "/" : problem.path) << ": " << problem.message << "\n";
    }
    return code;
}

}

int runCli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)
{
    std::string inputPath;
    std::string outputPath;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--help" || arg == "-h") {
            usage(out);
            return kExitOk;
        }
        if (arg == "--version") {
            out << "nutmeg-run engine " << kEngineVersion << " schema " << kSchemaVersion << " (stub simulation)\n";
            return kExitOk;
        }
        if (arg == "-o") {
            if (i + 1 >= args.size()) {
                err << "nutmeg-run: -o needs a file name\n";
                return kExitUsage;
            }
            outputPath = args[++i];
        } else if (!arg.empty() && arg[0] == '-') {
            err << "nutmeg-run: unknown option " << arg << "\n";
            return kExitUsage;
        } else if (inputPath.empty()) {
            inputPath = arg;
        } else {
            err << "nutmeg-run: only one input file is accepted\n";
            return kExitUsage;
        }
    }
    if (inputPath.empty()) {
        usage(err);
        return kExitUsage;
    }

    std::vector<std::uint8_t> bytes;
    if (!readFileBytes(inputPath, bytes, kMaxInputBytes)) {
        err << "nutmeg-run: cannot read " << inputPath << "\n";
        return kExitUsage;
    }

    const auto doc = nlohmann::json::parse(bytes.begin(), bytes.end(), nullptr, false);
    if (doc.is_discarded()) {
        return reject(err, kExitInvalidInput, "invalid input", {{"", "is not valid JSON"}});
    }

    if (const auto versions = checkVersions(doc); !versions.empty()) {
        return reject(err, kExitUnsupportedVersion, "unsupported version", versions);
    }

    MatchInput input;
    if (const auto problems = parseMatchInput(doc, input); !problems.empty()) {
        return reject(err, kExitInvalidInput, "invalid input", problems);
    }

    const std::string text = runStubSimulation(input).dump(2) + "\n";
    if (outputPath.empty()) {
        out << text;
        return kExitOk;
    }
    if (!writeFileAtomically(outputPath, std::vector<std::uint8_t>(text.begin(), text.end()))) {
        err << "nutmeg-run: cannot write " << outputPath << "\n";
        return kExitUsage;
    }
    return kExitOk;
}

}
