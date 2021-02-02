#pragma once

#include <fstream>
#include <filesystem>

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/Document.h>
#include <GLTFSDK/GLBResourceReader.h>

using namespace Microsoft::glTF;

class StreamWriter : public IStreamWriter {
public:
    StreamWriter(std::filesystem::path pathBase) : m_pathBase(std::move(pathBase)) {
        assert(m_pathBase.has_root_path());
    }

    // Resolves the relative URIs of any external resources declared in the glTF manifest
    std::shared_ptr<std::ostream> GetOutputStream(const std::string& filename) const override {
        // In order to construct a valid stream:
        // 1. The filename argument will be encoded as UTF-8 so use filesystem::u8path to
        //    correctly construct a path instance.
        // 2. Generate an absolute path by concatenating m_pathBase with the specified filename
        //    path. The filesystem::operator/ uses the platform's preferred directory separator
        //    if appropriate.
        // 3. Always open the file stream in binary mode. The glTF SDK will handle any text
        //    encoding issues for us.
        auto streamPath = m_pathBase / std::filesystem::path(filename);
        auto stream = std::make_shared<std::ofstream>(streamPath, std::ios_base::binary);

        // Check if the stream has no errors and is ready for I/O operations
        if (!stream || !(*stream)) {
            throw std::runtime_error("Unable to create a valid output stream for uri: " + filename);
        }

        return stream;
    }

private:
    std::filesystem::path m_pathBase;
};

class GLBBufMapper {
public:
	GLBBufMapper(const std::filesystem::path& glbPath, const std::shared_ptr<GLBResourceReader>& glbReader);
	~GLBBufMapper();

	void LoadDocument(const Document& doc);
	void RecodeImages(const Document& doc);

	void SaveNewGLB(Document& doc, std::filesystem::path glbNew);

private:

	struct BufferViewInfo {
		uint32_t bvId;
		size_t offset;
		size_t len;

		bool bv_updated;
		size_t new_offset;
		size_t new_len;

		std::string basisDataFile;

		BufferViewInfo() {
			bvId = 0;
			offset = len = 0;
			bv_updated = false;
			new_offset = new_len = 0;
		}
	};

	std::vector<BufferViewInfo> _bufferViews;
	std::vector<uint32_t> _bufOrder;
	std::shared_ptr<GLBResourceReader> _glbReader;

	void RecodeImage(const Document& doc, const Image& img, BufferViewInfo& bvi);

	std::filesystem::path _originalGLB;
	std::filesystem::path _imgFolder;
};

