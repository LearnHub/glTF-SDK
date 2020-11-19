#include "GLBBufMapper.h"

#include <iostream>
#include <sstream>

#include <GLTFSDK/Serialize.h>
#include <GLTFSDK/GLBResourceWriter.h>

GLBBufMapper::GLBBufMapper(const std::shared_ptr<GLBResourceReader>& glbReader) 
	: _glbReader(glbReader) {
	
}

GLBBufMapper::~GLBBufMapper() {

}

void GLBBufMapper::LoadDocument(const Microsoft::glTF::Document& doc) {
	_bufferViews.clear();
	_bufOrder.clear();
	
	// Until we know better, we'll assume that all GLBs only have one buffer for all BVs
	if (1 == doc.buffers.Size()) {
		// Create a local collection to represent the buffer views so that they can be remapped.
		_bufferViews.reserve(doc.bufferViews.Size());
		_bufOrder.reserve(doc.bufferViews.Size());

		for (int i = 0; i < doc.bufferViews.Size(); i++) {
			auto& bv = doc.bufferViews.Get(i);
			if (atoi(bv.id.c_str()) != i) {
				std::stringstream ss;
				ss << "GLB buffer view - id:" << bv.id << " - not ordered correctly - index = " << i;
				throw std::runtime_error(ss.str());
			}
			if (bv.bufferId != "0") {
				std::stringstream ss;
				ss << "GLB buffer view - id:" << bv.id << " - refers to buffer Id:" << bv.bufferId;
				throw std::runtime_error(ss.str());
			}

			std::cout << "BV #" << bv.id << " -> off:" << bv.byteOffset << ", len:" << bv.byteLength << std::endl;

			BufferViewInfo bvi;
			bvi.bvId = i;
			bvi.len = bv.byteLength;
			bvi.offset = bv.byteOffset;
			_bufferViews.emplace_back(bvi);

			// Now work out where this BufferView appears in the buffer
			auto itBuf = _bufOrder.begin();
			while ((itBuf != _bufOrder.end()) && (bv.byteOffset > _bufferViews[*itBuf].offset)) {
				itBuf++;
			}
			_bufOrder.insert(itBuf, i);
		}

		std::cout << std::endl << "Buffer View list in Buffer order --->" << std::endl;
		auto it = _bufOrder.begin();
		while (it != _bufOrder.end()) {
			auto& bvi = _bufferViews[*it];
			std::cout << "BV #" << bvi.bvId << " -> off:" << bvi.offset << ", len:" << bvi.len << 
				"    (off+len == " << bvi.offset+bvi.len << ")" << std::endl;
			if (it != _bufOrder.begin()) {
				auto& bviPrev = _bufferViews[*(it - 1)];
				auto next_offset = bviPrev.offset + bviPrev.len;
				if (bvi.offset != next_offset) {
					std::cout << "Gap of " << (bvi.offset - next_offset) << " bytes found -> calculated offset " << next_offset << std::endl;
				}
			}
			it++;
		}
	} else {
		std::stringstream ss;
		ss << "GLB contains " << doc.buffers.Size() << " buffers - only single buffer GLBs are supported at the moment";
		throw std::runtime_error(ss.str());
	}
}

void GLBBufMapper::RecodeImages(const Microsoft::glTF::Document& doc) {
	if (0 < _bufferViews.size()) {
		std::cout << std::endl << "Found " << doc.images.Size() << " images to re-encode..." << std::endl;
 		// Create a local collection to represent the buffer views so that they can be remapped.
		for (int i = 0; i < doc.images.Size(); i++) {
			auto& img = doc.images.Get(i);
			if (atoi(img.id.c_str()) != i) {
				std::stringstream ss;
				ss << "GLB image - id:" << img.id << " - not ordered correctly - index = " << i;
				throw std::runtime_error(ss.str());
			}

			auto& bvi = _bufferViews[atoi(img.bufferViewId.c_str())];
			std::cout << "Re-encoding image id:" << img.id << ", BV #" << img.bufferViewId << " -> off:"
				<< bvi.offset << ", len:" << bvi.len << std::endl;

			RecodeImage(doc, img, bvi);
		}
	} else {
		std::stringstream ss;
		ss << "GLB contains " << doc.buffers.Size() << " buffers - only single buffer GLBs are supported at the moment";
		throw std::runtime_error(ss.str());
	}
}

void GLBBufMapper::RecodeImage(const Document& doc, const Image& img, BufferViewInfo& bvi) {
	try {
		auto data = _glbReader->ReadBinaryData(doc, img);

		std::string filenameBase = "image_" + img.id + "_" + "BV" + img.bufferViewId;

		auto mime = img.mimeType;
		std::string orgImgFile = filenameBase + "." + mime.substr(mime.find_last_of('/') + 1);

		std::cout << "Image ID:" << img.id << ", mime: " << img.mimeType << " -> Saved " << data.size() << " bytes to " << orgImgFile << std::endl;
		
		std::ofstream imgFile(orgImgFile, std::ios::out | std::ios::binary);
		if (imgFile.is_open()) {
			imgFile.write(reinterpret_cast<const char*>(data.data()), data.size());
			imgFile.close();
		}
 
		std::string basisFile = filenameBase + ".basis";
		std::string cmd = "basisu.exe -mipmap -comp_level 1 -q 192 -file " + orgImgFile + " -output_file " + basisFile;

		auto rvSystem = std::system(cmd.c_str());
		std::cout << "RV: " << rvSystem << " COMMAND :- " << cmd << std::endl;

		if (std::filesystem::exists(basisFile)) {
			auto bf = std::filesystem::path(basisFile);
			bvi.basisDataFile = basisFile;
			bvi.new_len = std::filesystem::file_size(bf);
			bvi.bv_updated = true;

			std::cout << "Basis encoded file generated - " << basisFile << " - " << bvi.new_len << " bytes" << std::endl;

			// Adjust the Mime type...
			const_cast<Image&>(img).mimeType = "image/basis";
		} else {
			std::stringstream ss;
			ss << "Re-encoded Basis file wasn't created - " << basisFile;
			throw std::runtime_error(ss.str());
		}
	} catch (const std::exception& ex) {
		std::stringstream ss;
		ss << "Exception caught while recoding image - " << ex.what();
		throw std::runtime_error(ss.str());
	} catch (...) {
		throw std::runtime_error("Unrecognised exception caught while recoding image...");
	}
}

void GLBBufMapper::SaveNewGLB(Document& doc, std::filesystem::path glbNew) {
	if (std::filesystem::exists(glbNew)) {
		std::filesystem::remove(glbNew);
	}
	
	std::cout << std::endl << "Constructing new GLB -> " << glbNew << std::endl;

	auto streamWriter = std::make_unique<StreamWriter>(glbNew.parent_path());
	auto resWriter = std::make_unique<GLBResourceWriter>(std::move(streamWriter));
	auto os = resWriter->GetBufferStream(GLB_BUFFER_ID);

	long offset_adjustment = 0;
	auto it = _bufOrder.begin();
	while (it != _bufOrder.end()) {
		auto& bvi = _bufferViews[*it];
		auto& bv = const_cast<BufferView&>(doc.bufferViews.Get(bvi.bvId));

		if (bvi.bv_updated) {
			// This buffer view has been re-encoded so read the new data from the basis file
			std::ifstream strmBasis(bvi.basisDataFile, std::ios::in | std::ios::binary);
			if (strmBasis.is_open()) {
				auto basisData = StreamUtils::ReadBinaryFull<char>(strmBasis);
				 
				StreamUtils::WriteBinary(*os, basisData.data(), basisData.size());
				std::cout << "BV #" << bv.id << " - read " << basisData.size() << " bytes from re-encoded basis data - write_ptr=" << os->tellp() << std::endl;

				bv.byteOffset += offset_adjustment;

				// Update the length of the buffer view.
				bv.byteLength = bvi.new_len;
				offset_adjustment += -((long)bvi.len - (long)bvi.new_len);
			}
		} else {
			auto bvData = _glbReader->ReadBinaryData<uint8_t>(doc, bv);
			StreamUtils::WriteBinary(*os, bvData.data(), bvData.size());
			bv.byteOffset += offset_adjustment;
			std::cout << "BV #" << bv.id << " - read " << bvData.size() << " bytes from existing GLB - write_ptr=" << os->tellp() << std::endl;
		}

		it++;
	}

	std::cout << "Extensions -> used:" << doc.extensionsUsed.size() << ", reqd: " << doc.extensionsRequired.size() << std::endl;
	const_cast<std::unordered_set<std::string>&>(doc.extensionsUsed).insert("MOZ_HUBS_texture_basis");
	const_cast<std::unordered_set<std::string>&>(doc.extensionsRequired).insert("MOZ_HUBS_texture_basis");

	//auto& texture = doc.textures.Get(0);
	//const_cast<std::unordered_map<std::string, std::string>&>(texture.extensions).insert(std::make_pair("MOZ_HUBS_texture_basis", ""));
	//std::cout << "Textures (" << doc.textures.Size() << "): " << std::endl;

	os->flush();

	std::string manifest;
	try {
		// Serialize the glTF Document into a JSON manifest
		manifest = Serialize(doc, SerializeFlags::None);

		auto texturesPos = manifest.find("\"textures\":");
		//"textures":[{"sampler":0,"source":0
		std::cout << "Found [textures] entry at offset : " << texturesPos << std::endl;
		manifest = manifest.insert(texturesPos + 35, ",\"extensions\":{\"MOZ_HUBS_texture_basis\":{\"source\":0}}");

	} catch (const GLTFException& ex) {
		std::stringstream ss;

		ss << "Microsoft::glTF::Serialize failed: ";
		ss << ex.what();

		throw std::runtime_error(ss.str());
	}
	resWriter->Flush(manifest, glbNew.string());
}