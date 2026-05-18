local MESH_LOD_COUNT_SKIPPED <const> = 1
local MIP_COUNT_SKIPPED <const> = 1

local writer = BinaryWriter.new()

local info = debug.getinfo(1, "S")
local scriptPath = info.source
if scriptPath:sub(1,1) == "@" then
    scriptPath = scriptPath:sub(2)
end
local scriptDir = scriptPath:match("(.*[/\\])") or "./"
local localPath = "../app/src/main/assets/data.bin"
local fullPath = scriptDir .. localPath

writer:open(fullPath)

local entityCount = getEntityCount()
Log.info("Found " .. entityCount .. " entities in " .. getSceneName())

local meshAssetMap = {}
local meshAssetIdOrder = {}
local materialAssetIdOrder = {}

local function getMaterialIndex(list, id)
    for i, v in ipairs(list) do
        if v == id then return i - 1 end
    end
    return nil
end

for i = 0, entityCount - 1 do
    local entity = getEntity(i)
    
    if (entity)
    then
        if (entity:hasStaticMeshComponent())
        then
            local name = entity:getName()
            local meshComp = entity:getStaticMeshComponent()
            local meshAssetId = meshComp:getMeshAssetId()
            local transform = meshComp:getTransform()
            local materialAssetId = meshComp:getMaterialAssetId()

            if (not meshAssetMap[meshAssetId])
            then
                meshAssetMap[meshAssetId] = {}
                table.insert(meshAssetIdOrder, meshAssetId)
            end

            local materialIdx = getMaterialIndex(materialAssetIdOrder, materialAssetId)
            if (not materialIdx)
            then
                table.insert(materialAssetIdOrder, materialAssetId)
                materialIdx = #materialAssetIdOrder - 1
            end

            table.insert(meshAssetMap[meshAssetId], { 
                trans = transform, 
                matIdx = materialIdx 
            })
            
            Log.info("Entity " .. name .. " added with Material Index: " .. materialIdx)
        end
    end
end

local INVALID_ASSET_ID = 4294967295

local uniqueMaterialCount = #materialAssetIdOrder
writer:writeUInt(uniqueMaterialCount)
for i = 1, uniqueMaterialCount do
    local materialAssetId = materialAssetIdOrder[i]
    local assetMaterial = getMaterialFromAssetId(materialAssetId)

    if (assetMaterial)
    then
        Log.info("Exporting material asset: " .. assetMaterial:getLoadingPath())

        local assetTextureSetCount = assetMaterial:getTextureSetCount()
        if assetTextureSetCount ~= 1
        then
            Log.error("Only 1 texture set per material is supported")
        end

        local textureSetAssetId = assetMaterial:getTextureSetAssetId(0)
        local assetTextureSet = getTextureSetFromAssetId(textureSetAssetId)

        Log.info("Exporting texture set asset: " .. assetTextureSet:getLoadingPath())

        local albedoAssetId = assetTextureSet:getAlbedoAssetId()
        if albedoAssetId ~= INVALID_ASSET_ID
        then
            local assetImage = getImageFromAssetId(albedoAssetId)

            Log.info("Exporting image asset: " .. assetImage:getLoadingPath())

            assetImage:loadImageAndDump(writer, MIP_COUNT_SKIPPED)
        else
            Log.info("Albedo image not found: " .. assetTextureSet:getLoadingPath())

            writer:writeUInt(INVALID_ASSET_ID)
        end
    else
        Log.error("Could not find material data for ID: " .. materialAssetId)
    end
end

local uniqueMeshCount = #meshAssetIdOrder
writer:writeUInt(uniqueMeshCount)

for i = 1, uniqueMeshCount do
    local meshId = meshAssetIdOrder[i]
    local instanceInfo = meshAssetMap[meshId]
    local assetMesh = getMeshFromAssetId(meshId)

    if (assetMesh)
    then
        Log.info("Exporting mesh asset: " .. assetMesh:getLoadingPath())

        local meshFormatter = assetMesh:computeMeshFormatter()
        local lodCount = meshFormatter:getLODCount();
        local lodSkipped = 0
        if lodCount > MESH_LOD_COUNT_SKIPPED + 1
        then
            lodSkipped = MESH_LOD_COUNT_SKIPPED
            lodCount = lodCount - MESH_LOD_COUNT_SKIPPED
        else
            lodSkipped = lodCount - 1
            lodCount = 1
        end

        local aabb = meshFormatter:getAABB()
        writer:writeAABB(aabb);

        Log.info("  -> " .. lodCount .. " lods")

        writer:writeUInt(lodCount)
        for lod = lodSkipped, meshFormatter:getLODCount() - 1 do
            local vertices = meshFormatter:getVertices(lod)
            local vertexCount = #vertices 
            Log.info("  -> LOD " .. lod .. " - " .. vertexCount .. " vertices")
            
            writer:writeUInt(vertexCount)
            for j = 1, vertexCount do
                local v = vertices[j]
                writer:writeVec3(v.position)
                writer:writeVec2(v.texCoords)
            end

            local indices = meshFormatter:getIndices(lod)
            local indexCount = #indices 
            Log.info("  -> LOD " .. lod .. " - " .. indexCount .. " indices")

            writer:writeUInt(indexCount)
            for j = 1, indexCount do
                writer:writeUInt(indices[j])
            end
        end

        local instanceInfoCount = #instanceInfo
        Log.info("  -> " .. instanceInfoCount .. " instances")
        
        writer:writeUInt(instanceInfoCount)
        for j = 1, instanceInfoCount do
            local info = instanceInfo[j]
            writer:writeMat4(info.trans)
            writer:writeUInt(info.matIdx)
        end

        deleteMeshFormatter(meshFormatter)
    else
        Log.error("Could not find mesh data for ID: " .. meshId)
    end
end

writer:close()
Log.info("Export finished")