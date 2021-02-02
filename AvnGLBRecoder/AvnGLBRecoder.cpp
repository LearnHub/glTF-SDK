// AvnGLBRecoder.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLBResourceReader.h>
#include <GLTFSDK/Deserialize.h>

#include "GLBBufMapper.h"

using namespace Microsoft::glTF;

class StreamReader : public IStreamReader {
public:
    StreamReader(std::filesystem::path pathBase) : m_pathBase(std::move(pathBase)) {
        assert(m_pathBase.has_root_path());
    }

    // Resolves the relative URIs of any external resources declared in the glTF manifest
    std::shared_ptr<std::istream> GetInputStream(const std::string& filename) const override {
        // In order to construct a valid stream:
        // 1. The filename argument will be encoded as UTF-8 so use filesystem::u8path to
        //    correctly construct a path instance.
        // 2. Generate an absolute path by concatenating m_pathBase with the specified filename
        //    path. The filesystem::operator/ uses the platform's preferred directory separator
        //    if appropriate.
        // 3. Always open the file stream in binary mode. The glTF SDK will handle any text
        //    encoding issues for us.
        auto streamPath = m_pathBase / std::filesystem::path(filename);
        auto stream = std::make_shared<std::ifstream>(streamPath, std::ios_base::binary);

        // Check if the stream has no errors and is ready for I/O operations
        if (!stream || !(*stream)) {
            throw std::runtime_error("Unable to create a valid input stream for uri: " + filename);
        }

        return stream;
    }

private:
    std::filesystem::path m_pathBase;
};

void RecodeGLB(const std::filesystem::path& glbPath, const std::filesystem::path& glbNew) {
    std::cout << "Processing GLB at - " << glbPath << std::endl;

    auto streamReader = std::make_unique<StreamReader>(glbPath.parent_path());
    auto glbStream = streamReader->GetInputStream(glbPath.filename().string()); 
    auto glbResourceReader = std::make_shared<GLBResourceReader>(std::move(streamReader), std::move(glbStream));

    std::string manifest = glbResourceReader->GetJson(); // Get the manifest from the JSON chunk

    Document document;

    try {
        document = Deserialize(manifest);
    } catch (const GLTFException& ex) {
        std::stringstream ss;

        ss << "Microsoft::glTF::Deserialize failed: ";
        ss << ex.what();

        throw std::runtime_error(ss.str());
    }

    std::cout << "### GLB Info - " << glbPath.filename() << " ###" << std::endl << std::endl;
    std::cout << "Image count: " << document.images.Size() << ", texture count: " << document.textures.Size() << std::endl;
    std::cout << "Buffer count: " << document.buffers.Size() << ", Buffer views: " << document.bufferViews.Size() << std::endl;

    GLBBufMapper bufMapper(glbPath, glbResourceReader);
    bufMapper.LoadDocument(document);
    bufMapper.RecodeImages(document);

    bufMapper.SaveNewGLB(document, glbNew);

}

int main(int argc, char* argv[])
{
    try {
        std::cout << "Avantis GLB recoder utility..." << std::endl;

        if (argc != 3U) {
            std::cerr << "Usage: " << argv[0] << " <original glb> <new glb>" << std::endl;
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
            std::filesystem::path newGLB = argv[2U];
            if (newGLB.is_relative()) {
                auto pathCurrent = std::filesystem::current_path();

                // Convert the relative path into an absolute path by appending the command line argument to the current path
                pathCurrent /= newGLB;
                pathCurrent.swap(newGLB);
            }

            RecodeGLB(path, newGLB);
        } else {
            std::stringstream ss;
            ss << "Command line argument - " << argv[1U] << " - filename extension must be .glb";
            throw std::runtime_error(ss.str());
        }
    } catch (const std::runtime_error& ex) {
        std::cerr << "Error! - ";
        std::cerr << ex.what() << "\n";

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;

}

