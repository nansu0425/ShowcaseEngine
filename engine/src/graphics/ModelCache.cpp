#include <showcase/graphics/ModelCache.h>

#include <showcase/core/FileSystem.h>
#include <showcase/core/Log.h>
#include <showcase/core/Profiler.h>
#include <showcase/graphics/RenderContext.h>

#include <cstdio>
#include <cstring>

namespace showcase {

// ── Cache file format constants ─────────────────────────────────
static constexpr uint32_t kCacheMagic = 0x53454341; // "SECA"
static constexpr uint32_t kCacheVersion = 4;

#pragma pack(push, 1)
struct CacheHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t sourceContentHash;
    uint32_t imageCount;
    uint32_t materialCount;
    uint32_t meshCount;
    uint32_t padding;
};

struct CacheImageEntry {
    uint32_t width;
    uint32_t height;
    uint64_t dataOffset;
    uint64_t dataSize;
    uint32_t uriLength;
};

struct CacheMaterialEntry {
    uint32_t nameLength;
    float baseColorFactor[4];
    int32_t baseColorTextureIndex;
    uint32_t alphaMode;
    float alphaCutoff;
    uint32_t doubleSided;
};

struct CachePrimitiveEntry {
    int32_t materialIndex;
    uint32_t vertexCount;
    uint32_t indexCount;
    uint64_t vertexDataOffset;
    uint64_t indexDataOffset;
    float aabbCenterX, aabbCenterY, aabbCenterZ;
    float aabbExtentsX, aabbExtentsY, aabbExtentsZ;
};

struct CacheMeshHeader {
    uint32_t nameLength;
    uint32_t primitiveCount;
};
#pragma pack(pop)

// ── Helpers ─────────────────────────────────────────────────────
std::string ModelCache::GetCachePath(const std::string& gltfPath) {
    auto dot = gltfPath.rfind('.');
    if (dot == std::string::npos)
        return gltfPath + ".secache";
    return gltfPath.substr(0, dot) + ".secache";
}

bool ModelCache::IsCacheValid(const std::string& gltfPath) {
    SE_ZONE_SCOPED_NC("ModelCache::IsCacheValid", profile::kColorAssetIO);

    std::string cachePath = GetCachePath(gltfPath);
    uint64_t sourceHash = HashFileContents(gltfPath);
    if (sourceHash == 0)
        return false;

    FILE* f = nullptr;
    if (fopen_s(&f, cachePath.c_str(), "rb") != 0 || !f)
        return false;

    CacheHeader header = {};
    bool valid = false;
    if (fread(&header, sizeof(header), 1, f) == 1) {
        valid =
            header.magic == kCacheMagic && header.version == kCacheVersion && header.sourceContentHash == sourceHash;
    }

    fclose(f);
    return valid;
}

// ── Load from cache ─────────────────────────────────────────────
bool ModelCache::LoadFromCache(RenderContext& ctx, const std::string& cachePath, Model& outModel) {
    SE_ZONE_SCOPED_NC("ModelCache::LoadFromCache", profile::kColorAssetIO);

    FILE* f = nullptr;
    if (fopen_s(&f, cachePath.c_str(), "rb") != 0 || !f) {
        SE_LOG_ERROR("Failed to open cache file: {}", cachePath);
        return false;
    }

    // Read header
    CacheHeader header = {};
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return false;
    }

    auto* device = ctx.GetDevice().GetDevice();
    auto* allocator = ctx.GetDevice().GetAllocator();

    // Read image table (variable-length: entry + URI string)
    struct ImageReadInfo {
        CacheImageEntry entry;
        std::string uri;
    };
    std::vector<ImageReadInfo> imageTable(header.imageCount);
    for (uint32_t i = 0; i < header.imageCount; ++i) {
        if (fread(&imageTable[i].entry, sizeof(CacheImageEntry), 1, f) != 1) {
            fclose(f);
            return false;
        }
        if (imageTable[i].entry.uriLength > 0) {
            imageTable[i].uri.resize(imageTable[i].entry.uriLength);
            if (fread(imageTable[i].uri.data(), 1, imageTable[i].entry.uriLength, f) != imageTable[i].entry.uriLength) {
                fclose(f);
                return false;
            }
        }
    }

    // Read material table (variable-length: entry + name string)
    struct MaterialReadInfo {
        CacheMaterialEntry entry;
        std::string name;
    };
    std::vector<MaterialReadInfo> materialTable(header.materialCount);
    for (uint32_t i = 0; i < header.materialCount; ++i) {
        if (fread(&materialTable[i].entry, sizeof(CacheMaterialEntry), 1, f) != 1) {
            fclose(f);
            return false;
        }
        if (materialTable[i].entry.nameLength > 0) {
            materialTable[i].name.resize(materialTable[i].entry.nameLength);
            if (fread(materialTable[i].name.data(), 1, materialTable[i].entry.nameLength, f) !=
                materialTable[i].entry.nameLength) {
                fclose(f);
                return false;
            }
        }
    }

    // Read mesh headers and primitive tables
    struct MeshReadInfo {
        std::string name;
        std::vector<CachePrimitiveEntry> primitives;
    };
    std::vector<MeshReadInfo> meshReadInfos(header.meshCount);

    for (uint32_t mi = 0; mi < header.meshCount; ++mi) {
        CacheMeshHeader mh = {};
        if (fread(&mh, sizeof(mh), 1, f) != 1) {
            fclose(f);
            return false;
        }
        meshReadInfos[mi].name.resize(mh.nameLength);
        if (mh.nameLength > 0) {
            if (fread(meshReadInfos[mi].name.data(), 1, mh.nameLength, f) != mh.nameLength) {
                fclose(f);
                return false;
            }
        }
        meshReadInfos[mi].primitives.resize(mh.primitiveCount);
        if (mh.primitiveCount > 0) {
            if (fread(meshReadInfos[mi].primitives.data(), sizeof(CachePrimitiveEntry), mh.primitiveCount, f) !=
                mh.primitiveCount) {
                fclose(f);
                return false;
            }
        }
    }

    // Read model AABB
    float modelAABB[6] = {};
    if (fread(modelAABB, sizeof(float), 6, f) != 6) {
        fclose(f);
        return false;
    }

    // Load textures from bulk data
    std::vector<std::shared_ptr<Texture>> textures(header.imageCount);
    if (header.imageCount > 0) {
        CommandList cmdList;
        if (!cmdList.Init(device, D3D12_COMMAND_LIST_TYPE_DIRECT)) {
            fclose(f);
            return false;
        }
        cmdList.Reset();

        for (uint32_t i = 0; i < header.imageCount; ++i) {
            const auto& ie = imageTable[i].entry;
            std::vector<uint8_t> pixels(ie.dataSize);
            if (_fseeki64(f, static_cast<long long>(ie.dataOffset), SEEK_SET) != 0 ||
                fread(pixels.data(), 1, ie.dataSize, f) != ie.dataSize) {
                cmdList.Shutdown();
                fclose(f);
                return false;
            }

            textures[i] = std::make_shared<Texture>();
            TextureLoadDesc texDesc = {device,        allocator, cmdList.Get(), &ctx.GetSrvHeap(),
                                       pixels.data(), ie.width,  ie.height,     4};
            if (!textures[i]->InitFromMemory(texDesc)) {
                SE_LOG_ERROR("Cache: failed to create texture {}", i);
                cmdList.Shutdown();
                fclose(f);
                return false;
            }
            textures[i]->SetSourceURI(imageTable[i].uri);
        }

        cmdList.Close();
        ctx.GetDirectQueue().ExecuteCommandList(cmdList.Get());
        ctx.GetDirectQueue().Flush();

        for (auto& tex : textures) {
            if (tex)
                tex->ReleaseUploadResources();
        }
        cmdList.Shutdown();
    }

    // Reconstruct materials
    std::vector<std::shared_ptr<Material>> materials(header.materialCount);
    for (uint32_t i = 0; i < header.materialCount; ++i) {
        const auto& me = materialTable[i].entry;
        auto mat = std::make_shared<Material>();
        mat->name = materialTable[i].name;
        mat->baseColorFactor =
            Vector4(me.baseColorFactor[0], me.baseColorFactor[1], me.baseColorFactor[2], me.baseColorFactor[3]);
        if (me.baseColorTextureIndex >= 0 && me.baseColorTextureIndex < static_cast<int32_t>(textures.size())) {
            mat->baseColorTexture = textures[me.baseColorTextureIndex];
        }
        mat->alphaMode = static_cast<AlphaMode>(me.alphaMode);
        mat->alphaCutoff = me.alphaCutoff;
        mat->doubleSided = me.doubleSided != 0;
        materials[i] = std::move(mat);
    }

    // Reconstruct meshes
    outModel.meshes.resize(header.meshCount);
    for (uint32_t mi = 0; mi < header.meshCount; ++mi) {
        auto& mesh = outModel.meshes[mi];
        const auto& info = meshReadInfos[mi];
        mesh.name = info.name;
        mesh.primitives.resize(info.primitives.size());

        for (size_t pi = 0; pi < info.primitives.size(); ++pi) {
            const auto& pe = info.primitives[pi];
            auto& prim = mesh.primitives[pi];

            // Read vertex data
            if (pe.vertexCount > 0) {
                uint32_t vbSize = pe.vertexCount * sizeof(ModelVertex);
                std::vector<ModelVertex> vertices(pe.vertexCount);
                if (_fseeki64(f, static_cast<long long>(pe.vertexDataOffset), SEEK_SET) != 0 ||
                    fread(vertices.data(), sizeof(ModelVertex), pe.vertexCount, f) != pe.vertexCount) {
                    fclose(f);
                    return false;
                }

                if (!prim.vertexBuffer.InitAsVertexBuffer(
                        {device, allocator, vertices.data(), vbSize, sizeof(ModelVertex)})) {
                    fclose(f);
                    return false;
                }
            }

            // Read index data
            prim.indexCount = pe.indexCount;
            if (pe.indexCount > 0) {
                uint32_t ibSize = pe.indexCount * sizeof(uint32_t);
                std::vector<uint32_t> indices(pe.indexCount);
                if (_fseeki64(f, static_cast<long long>(pe.indexDataOffset), SEEK_SET) != 0 ||
                    fread(indices.data(), sizeof(uint32_t), pe.indexCount, f) != pe.indexCount) {
                    fclose(f);
                    return false;
                }

                if (!prim.indexBuffer.InitAsIndexBuffer({device, allocator, indices.data(), ibSize})) {
                    fclose(f);
                    return false;
                }
            }

            prim.materialIndex = pe.materialIndex;
            if (pe.materialIndex >= 0 && pe.materialIndex < static_cast<int32_t>(materials.size())) {
                prim.material = materials[pe.materialIndex];
            }
            prim.localAABB.Center = {pe.aabbCenterX, pe.aabbCenterY, pe.aabbCenterZ};
            prim.localAABB.Extents = {pe.aabbExtentsX, pe.aabbExtentsY, pe.aabbExtentsZ};
        }
    }

    outModel.localAABB.Center = {modelAABB[0], modelAABB[1], modelAABB[2]};
    outModel.localAABB.Extents = {modelAABB[3], modelAABB[4], modelAABB[5]};

    fclose(f);
    SE_LOG_INFO("Loaded model from cache: {}", cachePath);
    return true;
}

// ── Write cache ─────────────────────────────────────────────────
bool ModelCache::WriteCache(const std::string& cachePath, const ModelCacheWriteData& data) {
    SE_ZONE_SCOPED_NC("ModelCache::WriteCache", profile::kColorAssetIO);

    // Write to a temp file first, then rename to prevent corrupt partial caches
    std::string tempPath = cachePath + ".tmp";

    FILE* f = nullptr;
    if (fopen_s(&f, tempPath.c_str(), "wb") != 0 || !f) {
        SE_LOG_ERROR("Failed to create cache file: {}", tempPath);
        return false;
    }

    bool writeOk = true;
    auto writeBytes = [&](const void* ptr, size_t size, size_t count) {
        if (writeOk && fwrite(ptr, size, count, f) != count)
            writeOk = false;
    };

    uint64_t sourceHash = HashFileContents(*data.gltfPath);

    // Build header
    CacheHeader header = {};
    header.magic = kCacheMagic;
    header.version = kCacheVersion;
    header.sourceContentHash = sourceHash;
    header.imageCount = static_cast<uint32_t>(data.images.size());
    header.materialCount = static_cast<uint32_t>(data.materials.size());
    header.meshCount = static_cast<uint32_t>(data.meshes.size());

    // Calculate data offsets: header → image table → material table → mesh headers → model AABB → bulk data
    uint64_t offset = sizeof(CacheHeader);
    for (const auto& img : data.images)
        offset += sizeof(CacheImageEntry) + img.sourceURI.size();
    for (const auto& mat : data.materials)
        offset += sizeof(CacheMaterialEntry) + mat.name.size();

    // Mesh headers size
    for (const auto& mesh : data.meshes) {
        offset += sizeof(CacheMeshHeader);
        offset += mesh.name.size();
        offset += sizeof(CachePrimitiveEntry) * mesh.primitives.size();
    }

    // Model AABB
    offset += sizeof(float) * 6;

    // Assign image data offsets
    std::vector<CacheImageEntry> imageTable(data.images.size());
    for (size_t i = 0; i < data.images.size(); ++i) {
        imageTable[i].width = data.images[i].width;
        imageTable[i].height = data.images[i].height;
        imageTable[i].dataOffset = offset;
        imageTable[i].dataSize = data.images[i].dataSize;
        imageTable[i].uriLength = static_cast<uint32_t>(data.images[i].sourceURI.size());
        offset += data.images[i].dataSize;
    }

    // Build material table
    std::vector<CacheMaterialEntry> materialTable(data.materials.size());
    for (size_t i = 0; i < data.materials.size(); ++i) {
        const auto& src = data.materials[i];
        auto& dst = materialTable[i];
        dst.nameLength = static_cast<uint32_t>(src.name.size());
        memcpy(dst.baseColorFactor, src.baseColorFactor, sizeof(float) * 4);
        dst.baseColorTextureIndex = src.baseColorTextureIndex;
        dst.alphaMode = src.alphaMode;
        dst.alphaCutoff = src.alphaCutoff;
        dst.doubleSided = src.doubleSided;
    }

    // Assign vertex/index data offsets
    struct PrimOffsets {
        uint64_t vertexOffset;
        uint64_t indexOffset;
    };
    std::vector<std::vector<PrimOffsets>> allPrimOffsets(data.meshes.size());
    for (size_t mi = 0; mi < data.meshes.size(); ++mi) {
        const auto& mesh = data.meshes[mi];
        allPrimOffsets[mi].resize(mesh.primitives.size());
        for (size_t pi = 0; pi < mesh.primitives.size(); ++pi) {
            const auto& prim = mesh.primitives[pi];
            allPrimOffsets[mi][pi].vertexOffset = offset;
            offset += prim.vertices.size() * sizeof(ModelVertex);
            allPrimOffsets[mi][pi].indexOffset = offset;
            offset += prim.indices.size() * sizeof(uint32_t);
        }
    }

    // Write header
    writeBytes(&header, sizeof(header), 1);

    // Write image table (per-entry + URI string)
    for (size_t i = 0; i < imageTable.size(); ++i) {
        writeBytes(&imageTable[i], sizeof(CacheImageEntry), 1);
        if (!data.images[i].sourceURI.empty())
            writeBytes(data.images[i].sourceURI.data(), 1, data.images[i].sourceURI.size());
    }

    // Write material table (per-entry + name string)
    for (size_t i = 0; i < materialTable.size(); ++i) {
        writeBytes(&materialTable[i], sizeof(CacheMaterialEntry), 1);
        if (!data.materials[i].name.empty())
            writeBytes(data.materials[i].name.data(), 1, data.materials[i].name.size());
    }

    // Write mesh headers and primitive tables
    for (size_t mi = 0; mi < data.meshes.size(); ++mi) {
        const auto& mesh = data.meshes[mi];

        CacheMeshHeader mh = {};
        mh.nameLength = static_cast<uint32_t>(mesh.name.size());
        mh.primitiveCount = static_cast<uint32_t>(mesh.primitives.size());
        writeBytes(&mh, sizeof(mh), 1);

        if (!mesh.name.empty())
            writeBytes(mesh.name.data(), 1, mesh.name.size());

        for (size_t pi = 0; pi < mesh.primitives.size(); ++pi) {
            const auto& prim = mesh.primitives[pi];
            CachePrimitiveEntry pe = {};
            pe.materialIndex = prim.materialIndex;
            pe.vertexCount = static_cast<uint32_t>(prim.vertices.size());
            pe.indexCount = static_cast<uint32_t>(prim.indices.size());
            pe.vertexDataOffset = allPrimOffsets[mi][pi].vertexOffset;
            pe.indexDataOffset = allPrimOffsets[mi][pi].indexOffset;
            pe.aabbCenterX = prim.localAABB.Center.x;
            pe.aabbCenterY = prim.localAABB.Center.y;
            pe.aabbCenterZ = prim.localAABB.Center.z;
            pe.aabbExtentsX = prim.localAABB.Extents.x;
            pe.aabbExtentsY = prim.localAABB.Extents.y;
            pe.aabbExtentsZ = prim.localAABB.Extents.z;
            writeBytes(&pe, sizeof(pe), 1);
        }
    }

    // Write model AABB
    float modelAABB[6] = {data.modelAABB.Center.x,  data.modelAABB.Center.y,  data.modelAABB.Center.z,
                          data.modelAABB.Extents.x, data.modelAABB.Extents.y, data.modelAABB.Extents.z};
    writeBytes(modelAABB, sizeof(float), 6);

    // Write bulk image data
    for (const auto& img : data.images) {
        writeBytes(img.data, 1, img.dataSize);
    }

    // Write bulk vertex/index data
    for (const auto& mesh : data.meshes) {
        for (const auto& prim : mesh.primitives) {
            if (!prim.vertices.empty())
                writeBytes(prim.vertices.data(), sizeof(ModelVertex), prim.vertices.size());
            if (!prim.indices.empty())
                writeBytes(prim.indices.data(), sizeof(uint32_t), prim.indices.size());
        }
    }

    fclose(f);

    if (!writeOk) {
        std::remove(tempPath.c_str());
        SE_LOG_ERROR("Failed to write cache file: {}", cachePath);
        return false;
    }

    // Atomic rename: replace final cache only after successful write
    std::remove(cachePath.c_str());
    if (std::rename(tempPath.c_str(), cachePath.c_str()) != 0) {
        std::remove(tempPath.c_str());
        SE_LOG_ERROR("Failed to rename cache file: {}", cachePath);
        return false;
    }

    SE_LOG_INFO("Wrote model cache: {}", cachePath);
    return true;
}

} // namespace showcase
