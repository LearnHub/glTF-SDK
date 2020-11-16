// AvnGLBRecoder.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include <sstream>

#include <GLTFSDK/GLTF.h>

using namespace Microsoft::glTF;

void RecodeGLB(const std::filesystem::path& glbPath) {
    std::cout << "Processing GLB at - " << glbPath << std::endl;
}

int main(int argc, char* argv[])
{
    try {
        if (argc != 2U) {
            throw std::runtime_error("Unexpected number of command line arguments");
        }

        std::filesystem::path path = argv[1U];

        if (path.is_relative()) {
            auto pathCurrent = std::filesystem::current_path();

            // Convert the relative path into an absolute path by appending the command line argument to the current path
            pathCurrent /= path;
            pathCurrent.swap(path);
        }

        if (!path.has_filename()) {
            throw std::runtime_error("Command line argument path has no filename");
        }

        if (!path.has_extension()) {
            throw std::runtime_error("Command line argument path has no filename extension");
        }

        std::filesystem::path pathFile = path.filename();
        std::filesystem::path pathFileExt = pathFile.extension();

        auto MakePathExt = [](const std::string& ext) {
            return "." + ext;
        };

        if (pathFileExt == MakePathExt(GLB_EXTENSION)) {
            RecodeGLB(path);
        } else {
            std::stringstream ss;
            ss << "Command line argument - " << argv[1U] << " - filename extension must be .glb";
            throw std::runtime_error(ss.str());
        }

        char c;
        std::cin >> c;
    } catch (const std::runtime_error& ex) {
        std::cerr << "Error! - ";
        std::cerr << ex.what() << "\n";

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;

}

