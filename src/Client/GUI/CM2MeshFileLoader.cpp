#define _DEBUG 1
#include <iostream>
#include "MemoryDataHolder.h"
#include "MemoryInterface.h"
#include "CM2MeshFileLoader.h"
#include "common.h"

namespace irr
{
namespace scene
{

CM2MeshFileLoader::CM2MeshFileLoader(IrrlichtDevice* device):Device(device)
{
    Mesh = NULL;

}

CM2MeshFileLoader::~CM2MeshFileLoader()
{

}

core::vector3df fixCoordSystem(core::vector3df v)
{
        return core::vector3df(v.X, v.Z, v.Y);
}

core::quaternion fixQuaternion(core::quaternion q)
{
        return core::quaternion(q.X, q.Z, q.Y, q.W);
}
bool CM2MeshFileLoader::isALoadableFileExtension(const io::path& filename)const
{
    return core::hasFileExtension ( filename, "m2" );
}


//! creates/loads an animated mesh from the file.
//! \return Pointer to the created mesh. Returns 0 if loading failed.
//! If you no longer need the mesh, you should call IAnimatedMesh::drop().
//! See IUnknown::drop() for more information.
IAnimatedMesh* CM2MeshFileLoader::createMesh(io::IReadFile* file)
{
    if(!file)
        return 0;
    MeshFile = file;
    AnimatedMesh = new scene::CM2Mesh();

    if ( load() )
    {
        AnimatedMesh->finalize();
    }
    else
    {
        AnimatedMesh->drop();
        AnimatedMesh = 0;
    }

    return AnimatedMesh;
}

void CM2MeshFileLoader::ReadVertices()
{
    //Vertices.  Global data
    if(!M2MVertices.empty())
        M2MVertices.clear();

    ModelVertex tempM2MVert;
    MeshFile->seek(header.Vertices.ofs);

    for(u32 i =0;i<header.Vertices.num;i++)
    {
        MeshFile->read(&tempM2MVert,sizeof(ModelVertex));
        tempM2MVert.pos = fixCoordSystem(tempM2MVert.pos);
        tempM2MVert.normal = fixCoordSystem(tempM2MVert.normal);
        M2MVertices.push_back(tempM2MVert);
    }
    DEBUG(logdebug("Read %u/%u Vertices",M2MVertices.size(),header.Vertices.num));
}

void CM2MeshFileLoader::ReadViewData(io::IReadFile* file)
{
    //Vertex indices of a specific view.Local to View 0
    if(M2MIndices.size()>0)
        M2MIndices.clear();

    u16 tempM2Index;
    file->seek(currentView.Index.ofs);
    for(u32 i =0;i<currentView.Index.num;i++)
    {
        file->read(&tempM2Index,sizeof(u16));
        M2MIndices.push_back(tempM2Index);
    }
    DEBUG(logdebug("Read %u/%u Indices",M2MIndices.size(),currentView.Index.num));

    //Triangles. Data Points point to the Vertex Indices, not the vertices themself. 3 Points = 1 Triangle, Local to View 0
    if(M2MTriangles.size()>0)
        M2MTriangles.clear();

    u16 tempM2Triangle;
    file->seek(currentView.Triangle.ofs);
    for(u32 i =0;i<currentView.Triangle.num;i++)
    {
        file->read(&tempM2Triangle,sizeof(u16));
        M2MTriangles.push_back(tempM2Triangle);
    }
    DEBUG(logdebug("Read %u/%u Triangles",M2MTriangles.size(),currentView.Triangle.num));
    //Submeshes, Local to View 0
    if(M2MSubmeshes.size()>0)
        M2MSubmeshes.clear();

    ModelViewSubmesh tempM2Submesh;
    file->seek(currentView.Submesh.ofs);
    for(u32 i =0;i<currentView.Submesh.num;i++)
    {
        file->read(&tempM2Submesh,sizeof(ModelViewSubmesh)-(header.version==0x100?16:0));
        M2MSubmeshes.push_back(tempM2Submesh);
        DEBUG(logdebug("Submesh %u MeshPartID %u",i,tempM2Submesh.meshpartId));
    //    std::cout<< "Submesh " <<i<<" ID "<<tempM2Submesh.meshpartId<<" starts at V/T "<<tempM2Submesh.ofsVertex<<"/"<<tempM2Submesh.ofsTris<<" and has "<<tempM2Submesh.nVertex<<"/"<<tempM2Submesh.nTris<<" V/T\n";
    }
    DEBUG(logdebug("Read %u/%u Submeshes",M2MSubmeshes.size(),currentView.Submesh.num));

    //Texture units. Local to view 0
    TextureUnit tempM2TexUnit;
    if(!M2MTextureUnit.empty())
    {
        M2MTextureUnit.clear();
    }
    file->seek(currentView.Tex.ofs);
    for(u32 i=0;i<currentView.Tex.num;i++)
    {
        file->read(&tempM2TexUnit,sizeof(TextureUnit));
        M2MTextureUnit.push_back(tempM2TexUnit);
    DEBUG(logdebug(" TexUnit %u: Submesh: %u %u Render Flag: %u TextureUnitNumber: %u %u TTU: %u",i,tempM2TexUnit.submeshIndex1,tempM2TexUnit.submeshIndex2, tempM2TexUnit.renderFlagsIndex, tempM2TexUnit.TextureUnitNumber, tempM2TexUnit.TextureUnitNumber2 ,tempM2TexUnit.textureIndex));
    }
    DEBUG(logdebug("Read %u Texture Unit entries for View 0",M2MTextureUnit.size()));

}

void CM2MeshFileLoader::ReadABlock(AnimBlock &ABlock, u8 datatype, u8 datanum)
{
// datatypes: 0 = float, 1 = short
// datatype & datanum: datatype 0, datanum 3 --> read 3 floats
    MeshFile->read(&ABlock.header,4);
    if(header.version < 0x108)//Read/Skip Interpolation Ranges for preWOTLK
      MeshFile->read(&ABlock.header.InterpolationRanges,sizeof(numofs));
    MeshFile->read(&ABlock.header.TimeStamp,sizeof(numofs));
    MeshFile->read(&ABlock.header.Values,sizeof(numofs));
    u32 offset_next_entry = MeshFile->getPos();
//     logdebug("%u %u %u %u",datatype, datanum, ABlock.header.TimeStamp.num,ABlock.header.Values.num);

    core::array<numofs> data_offsets;
    numofs tempNumOfs;
    io::IReadFile* AnimFile;

    //Read Timestamps
    if(header.version < 0x108)
      data_offsets.push_back(ABlock.header.TimeStamp);
    else
    {
      MeshFile->seek(ABlock.header.TimeStamp.ofs);
      for(u32 i =0;i < ABlock.header.TimeStamp.num; i++)
      {
        MeshFile->read(&tempNumOfs,sizeof(numofs));
        data_offsets.push_back(tempNumOfs);
      }
    }
    for(u32 i = 0; i < data_offsets.size(); i++)
    {
      tempNumOfs = data_offsets[i];

      if((M2MAnimations[i].flags & 0x20) == 0 && (M2MAnimations[i].flags & 0x40))
        continue;

      u32 offset = 0;
      if(header.version >= 0x108)
        offset = M2MAnimations[i].start;//HACK: Squashing WoTLK multitimelines into one single timeline

      if(header.version >= 0x108 && M2MAnimfiles[i] != 0)
        AnimFile = M2MAnimfiles[i];
      else
        AnimFile = MeshFile;
      AnimFile->seek(tempNumOfs.ofs);
      u32 tempTS;
      for(u32 j = 0; j < tempNumOfs.num; j++)
      {
        AnimFile->read(&tempTS,sizeof(u32));
        ABlock.timestamps.push_back(tempTS+offset);
      }
    }

    data_offsets.clear();
    //Read Values
    if(header.version < 0x108)
      data_offsets.push_back(ABlock.header.Values);
    else
    {
      MeshFile->seek(ABlock.header.Values.ofs);
      for(u32 i =0;i < ABlock.header.Values.num; i++)
      {
        MeshFile->read(&tempNumOfs,sizeof(numofs));
        data_offsets.push_back(tempNumOfs);
      }
    }
    for(u32 i = 0; i < data_offsets.size(); i++)
    {
      tempNumOfs = data_offsets[i];
      if((M2MAnimations[i].flags & 0x20) == 0 && (M2MAnimations[i].flags & 0x40))
        continue;
      if(header.version >= 0x108 && M2MAnimfiles[i] != 0)
        AnimFile = M2MAnimfiles[i];
      else
        AnimFile = MeshFile;
      AnimFile->seek(tempNumOfs.ofs);
      s16 tempShort;
      s32 tempInt;
      f32 tempFloat;
      for(u32 j = 0; j < tempNumOfs.num * datanum; j++)
      {
        switch(datatype)
        {
          case ABDT_FLOAT:
            AnimFile->read(&tempFloat,sizeof(f32));
            break;
          case ABDT_SHORT:
            MeshFile->read(&tempShort, sizeof(s16));
            tempFloat=(tempShort>0?tempShort-32767:tempShort+32767)/32767.0f;
            break;
          case ABDT_INT:
            AnimFile->read(&tempInt,sizeof(s32));
            tempFloat=tempInt * 1.0f;
            break;
        }
        ABlock.values.push_back(tempFloat);
      }
    }

    MeshFile->seek(offset_next_entry);
}


void CM2MeshFileLoader::ReadBones()
{
    //Bones. This is global data
    if(!M2MBones.empty())
    {
        M2MBones.clear();
    }
    MeshFile->seek(header.Bones.ofs);
    for(u32 i=0;i<header.Bones.num;i++)
    {
        Bone tempBone;
        MeshFile->read(&tempBone,12+(header.version==0x100?0:4));

        ReadABlock(tempBone.translation,ABDT_FLOAT,3);
        ReadABlock(tempBone.rotation,(header.version>=0x104?ABDT_SHORT:ABDT_FLOAT),4);
        ReadABlock(tempBone.scaling,ABDT_FLOAT,3);

        MeshFile->read(&tempBone.PivotPoint,sizeof(core::vector3df));
        tempBone.PivotPoint=fixCoordSystem(tempBone.PivotPoint);
        M2MBones.push_back(tempBone);
        DEBUG(logdebug("Bone %u Parent %u PP %f %f %f Flags %X",i,tempBone.parentBone,tempBone.PivotPoint.X,tempBone.PivotPoint.Y,tempBone.PivotPoint.Z, tempBone.flags));
    }
    DEBUG(logdebug("Read %u Bones",M2MBones.size()));

}


void CM2MeshFileLoader::ReadColors()
{
    if (!M2MVertexColor.empty())
    {
        M2MVertexColor.clear();
    }
    //read vertex color and alpha offsets
    logdebug("Colors");
    MeshFile->seek(header.Colors.ofs);
    for(u32 i=0;i<header.Colors.num;i++)
    {
      VertexColor tempVC;
      logdebug("Color %u",i);
      ReadABlock(tempVC.Colors,ABDT_FLOAT,3);
      ReadABlock(tempVC.Alpha,ABDT_SHORT,1);
      M2MVertexColor.push_back(tempVC);
    }
}


void CM2MeshFileLoader::ReadLights()
{
    if (!M2MLights.empty())
    {
        M2MLights.clear();
    }
    //read Lights
    MeshFile->seek(header.Lights.ofs);

    for(u32 i=0;i<header.Lights.num;i++)
    {
        Light tempLight;
        MeshFile->read(&tempLight,16);//2xshort + vector3df
        ReadABlock(tempLight.AmbientColor,ABDT_FLOAT,3);
        ReadABlock(tempLight.AmbientIntensity,ABDT_FLOAT,1);
        ReadABlock(tempLight.DiffuseColor,ABDT_FLOAT,3);
        ReadABlock(tempLight.DiffuseIntensity,ABDT_FLOAT,1);
        ReadABlock(tempLight.AttenuationStart,ABDT_FLOAT,1);
        ReadABlock(tempLight.AttenuationEnd,ABDT_FLOAT,1);
        ReadABlock(tempLight.Unknown,ABDT_INT,1);
        M2MLights.push_back(tempLight);
    }
}


void CM2MeshFileLoader::ReadAnimationData()
{
//     //Global Sequences. This is global data
//     u32 tempGlobalSeq;
//     if(!M2MGlobalSequences.empty())
//     {
//         M2MGlobalSequences.clear();
//     }
//     MeshFile->seek(header.GlobalSequences.ofs);
//     for(u32 i=0;i<header.GlobalSequences.num;i++)
//     {
//         MeshFile->read(&tempGlobalSeq,sizeof(u32));
//         M2MGlobalSequences.push_back(tempGlobalSeq);
//         DEBUG(logdebug("Global Sequence %u End %u",i,tempGlobalSeq));
//     }
//     DEBUG(logdebug("Read %u Global Sequence entries",M2MGlobalSequences.size()));
//
//     //BoneLookupTable. This is global data
//     u16 tempBoneLookup;
//     if(!M2MBoneLookupTable.empty())
//     {
//         M2MBoneLookupTable.clear();
//     }
//     MeshFile->seek(header.BoneLookupTable.ofs);
//     for(u32 i=0;i<header.BoneLookupTable.num;i++)
//     {
//         MeshFile->read(&tempBoneLookup,sizeof(u16));
//         M2MBoneLookupTable.push_back(tempBoneLookup);
//         DEBUG(logdebug("BoneLookupTable %u Value %u",i,tempBoneLookup));
//     }
//     DEBUG(logdebug("Read %u BoneLookupTable entries",M2MBoneLookupTable.size()));
//
//     //SkeleBoneLookupTable. This is global data
//     u16 tempSkeleBoneLookup;
//     if(!M2MSkeleBoneLookupTable.empty())
//     {
//         M2MSkeleBoneLookupTable.clear();
//     }
//     MeshFile->seek(header.SkelBoneLookup.ofs);
//     for(u32 i=0;i<header.SkelBoneLookup.num;i++)
//     {
//         MeshFile->read(&tempSkeleBoneLookup,sizeof(u16));
//         M2MSkeleBoneLookupTable.push_back(tempSkeleBoneLookup);
//         DEBUG(logdebug("SkeleBoneLookupTable %u Value %u",i,tempSkeleBoneLookup));
//     }
//     DEBUG(logdebug("Read %u SkeleBoneLookupTable entries",M2MSkeleBoneLookupTable.size()));

    //Animations. This is global data
    u32 laststart = 0;
    if(!M2MAnimations.empty())
    {
        M2MAnimations.clear();
    }
    MeshFile->seek(header.Animations.ofs);
    for(u32 i=0;i<header.Animations.num;i++)
    {
        Animation tempAnimation;
        if(header.version < 0x108)
        {
          RawAnimation tempRaw;
          MeshFile->read(&tempRaw,sizeof(RawAnimation));

          tempAnimation.animationID = tempRaw.animationID;
          tempAnimation.probability = tempRaw.probability / 32767.0f;
          tempAnimation.start = tempRaw.start;
          tempAnimation.end = tempRaw.end;
        }
        else
        {
          RawAnimationWOTLK tempRaw;
          MeshFile->read(&tempRaw,sizeof(RawAnimationWOTLK));
          tempAnimation.animationID = tempRaw.animationID;
          tempAnimation.subanimationID = tempRaw.subanimationID;
          tempAnimation.flags = tempRaw.flags;
          tempAnimation.probability = tempRaw.probability / 32767.0f;
          tempAnimation.start = laststart;
          tempAnimation.end = laststart + tempRaw.length;
          laststart += tempRaw.length + 1000;
          if((tempAnimation.flags & 0x20) == 0)
          {
            std::string AnimName = MeshFile->getFileName().c_str();
            c8 ext[13];
            sprintf(ext,"%04d-%02d.anim",tempAnimation.animationID,tempAnimation.subanimationID);
            AnimName = AnimName.substr(0, AnimName.length()-3) + ext;
            io::IReadFile* AnimFile = io::IrrCreateIReadFileBasic(Device, AnimName.c_str());
            if (!AnimFile)
            {
                logerror("Error! Anim file not found: %s", AnimName.c_str());
                if(tempAnimation.flags & 0x40)
                {
                  logerror("We actually expected this error - Where is the data for this animation???");
                }
                AnimFile=0;
            }
            M2MAnimfiles.push_back(AnimFile);
          }
          else
            M2MAnimfiles.push_back(0);
        }
        M2MAnimations.push_back(tempAnimation);
        DEBUG(logdebug("Animation %u Id %u Start %u End %u",i,tempAnimation.animationID,tempAnimation.start,tempAnimation.end));
    }
    DEBUG(logdebug("Read %u Animations",M2MAnimations.size()));
//     //Animation Lookup. This is global data
//     s16 tempAniLookup;
//     if(!M2MAnimationLookup.empty())
//     {
//         M2MAnimationLookup.clear();
//     }
//     MeshFile->seek(header.AnimationLookup.ofs);
//     for(u32 i=0;i<header.AnimationLookup.num;i++)
//     {
//         MeshFile->read(&tempAniLookup,sizeof(s16));
//         M2MAnimationLookup.push_back(tempAniLookup);
//         DEBUG(logdebug("Animation Lookup %u Id %u",i,tempAniLookup));
//     }
//     DEBUG(logdebug("Read %u AnimationLookup",M2MAnimationLookup.size()));
}

void CM2MeshFileLoader::ReadTextureDefinitions()
{
    //Texture Lookup table. This is global data
    u16 tempM2TexLookup;
    if(!M2MTextureLookup.empty())
    {
        M2MTextureLookup.clear();
    }
    MeshFile->seek(header.TexLookup.ofs);
    for(u32 i=0;i<header.TexLookup.num;i++)
    {
        MeshFile->read(&tempM2TexLookup,sizeof(u16));
        M2MTextureLookup.push_back(tempM2TexLookup);
        DEBUG(logdebug("Texture %u Type %u",i,tempM2TexLookup));
    }
    DEBUG(logdebug("Read %u Texture lookup entries",M2MTextureLookup.size()));

    //Texture Definitions table. This is global data
    TextureDefinition tempM2TexDef;
    if(!M2MTextureDef.empty())
    {
        M2MTextureDef.clear();
    }
    MeshFile->seek(header.Textures.ofs);
    for(u32 i=0;i<header.Textures.num;i++)
    {
        MeshFile->read(&tempM2TexDef,sizeof(TextureDefinition));
        M2MTextureDef.push_back(tempM2TexDef);
        DEBUG(logdebug("Texture %u Type %u",i,tempM2TexDef.texType));
    }
    DEBUG(logdebug("Read %u Texture Definition entries",M2MTextureDef.size()));

    //Render Flags table. This is global data
    RenderFlags tempM2RF;
    if(!M2MRenderFlags.empty())
    {
        M2MRenderFlags.clear();
    }
    MeshFile->seek(header.TexFlags.ofs);
    for(u32 i=0;i<header.TexFlags.num;i++)
    {
        MeshFile->read(&tempM2RF,sizeof(RenderFlags));
        M2MRenderFlags.push_back(tempM2RF);
        DEBUG(logdebug("Flag %u: (%u, %u)",i,tempM2RF.blending,tempM2RF.flags));
    }
    DEBUG(logdebug("Read %u Renderflags",M2MRenderFlags.size()));

    if(!M2MTextureFiles.empty())
        M2MTextureFiles.clear();

    std::string tempTexFileName="";
    M2MTextureFiles.reallocate(M2MTextureDef.size());
    for(u32 i=0; i<M2MTextureDef.size(); i++)
    {
        tempTexFileName.resize(M2MTextureDef[i].texFileLen + 1);
        MeshFile->seek(M2MTextureDef[i].texFileOfs);
        MeshFile->read((void*)tempTexFileName.data(),M2MTextureDef[i].texFileLen);
        M2MTextureFiles.push_back("");
        M2MTextureFiles[i]=tempTexFileName.c_str();
        DEBUG(logdebug("Texture: %u %u (%s/%s) @ %u(%u)",i,M2MTextureFiles.size(),M2MTextureFiles[i].c_str(),tempTexFileName.c_str(),M2MTextureDef[i].texFileOfs,M2MTextureDef[i].texFileLen));
    }
}


bool CM2MeshFileLoader::load()
{
  DEBUG(logdebug("Trying to open file %s",MeshFile->getFileName().c_str()));

  MeshFile->read(&header,20);
  DEBUG(logdebug("M2 Version %X",header.version));

  switch(header.version)
  {
    case 0x100:
    case 0x104://NEED CHECK
    case 0x105://NEED CHECK
    case 0x106://NEED CHECK
    case 0x107://NEED CHECK
    {
      MeshFile->read((u8*)&header+20,sizeof(ModelHeader)-20);
      MeshFile->seek(header.Views.ofs);
      MeshFile->read(&currentView,sizeof(ModelView));
      ReadViewData(MeshFile);

      break;
    }
    case 0x108:
    {
      //This is pretty ugly... Any suggestions how to make this nicer?
      MeshFile->read((u8*)&header+0x14,24);//nGlobalSequences - ofsAnimationLookup
      MeshFile->read((u8*)&header+0x34,28);//nBones - nViews
      MeshFile->read((u8*)&header+0x54,24);//nColors - nTransparency
      MeshFile->read((u8*)&header+0x74,sizeof(ModelHeader)-0x74);//nTexAnims - END

      std::string SkinName = MeshFile->getFileName().c_str();
      SkinName = SkinName.substr(0, SkinName.length()-3) + "00.skin"; // FIX ME if we need more skins
      io::IReadFile* SkinFile = io::IrrCreateIReadFileBasic(Device, SkinName.c_str());
      if (!SkinFile)
      {
          logerror("Error! Skin file not found: %s", SkinName.c_str());
          return 0;
      }
      SkinFile->seek(4); // Header of Skin Files is always SKIN
      SkinFile->read(&currentView,sizeof(ModelView));
      ReadViewData(SkinFile);
      SkinFile->drop();

      break;
    }
    default:
    {
      logerror("M2: [%s] Wrong header %0X! File version doesn't match or file is not a M2 file.",MeshFile->getFileName().c_str(),header.version);
      return 0;
    }
  }
  ReadVertices();

  ReadTextureDefinitions();
  ReadAnimationData();

  ReadBones();
  ReadColors();

///////////////////////////
// EVERYTHING IS READ
///////////////////////////


///////////////////////////////////////
//      Animation related stuff      //
///////////////////////////////////////
for(u32 i=0;i<M2MAnimations.size();i++)
{
  AnimatedMesh->newAnimation(M2MAnimations[i].animationID,M2MAnimations[i].start,M2MAnimations[i].end,M2MAnimations[i].probability);
}


scene::CM2Mesh::SJoint* Joint;
for(u32 i=0;i<M2MBones.size();i++)
{
  if(M2MBones[i].parentBone == -1)
  {
      ParentJoint=(scene::CM2Mesh::SJoint*)0;
  }
  else
  {
      ParentJoint=AnimatedMesh->getAllJoints()[M2MBones[i].parentBone];
  }
  Joint=AnimatedMesh->addJoint(ParentJoint);

  if(M2MBones[i].translation.timestamps.size()>0)
  {
    for(u32 j=0;j<M2MBones[i].translation.timestamps.size();j++)
    {
      scene::CM2Mesh::SPositionKey* pos=AnimatedMesh->addPositionKey(Joint);
      pos->frame=M2MBones[i].translation.timestamps[j];
      pos->position=fixCoordSystem(core::vector3df(M2MBones[i].translation.values[j*3],M2MBones[i].translation.values[j*3+1],M2MBones[i].translation.values[j*3+2]));
    }
  }
  if(M2MBones[i].rotation.timestamps.size()>0)
  {
    for(u32 j=0;j<M2MBones[i].rotation.timestamps.size();j++)
    {
      scene::CM2Mesh::SRotationKey* rot=AnimatedMesh->addRotationKey(Joint);
      rot->frame=M2MBones[i].rotation.timestamps[j];
      core::quaternion tempQ=core::quaternion(M2MBones[i].rotation.values[j*4+0],M2MBones[i].rotation.values[j*4+1],M2MBones[i].rotation.values[j*4+2],M2MBones[i].rotation.values[j*4+3]);
      tempQ = fixQuaternion(tempQ);
      tempQ.normalize();
      rot->rotation=tempQ;
    }
  }

  if(M2MBones[i].scaling.timestamps.size()>0)
  {
    for(u32 j=0;j<M2MBones[i].scaling.timestamps.size();j++)
    {
      scene::CM2Mesh::SScaleKey* scale=AnimatedMesh->addScaleKey(Joint);
      scale->frame=M2MBones[i].scaling.timestamps[j];
      scale->scale=core::vector3df(M2MBones[i].scaling.values[j*3],M2MBones[i].scaling.values[j*3+1],M2MBones[i].scaling.values[j*3+2]);
    }
  }

  Joint->Animatedposition=M2MBones[i].PivotPoint;
  Joint->Animatedscale=core::vector3df(1.0f,1.0f,1.0f);
  Joint->Animatedrotation=core::quaternion(0.0f,0.0f,0.0f,1.0f);

  core::matrix4 positionMatrix;
  positionMatrix.setTranslation( Joint->Animatedposition );

  core::matrix4 rotationMatrix = Joint->Animatedrotation.getMatrix();

  core::matrix4 scaleMatrix;
  scaleMatrix.setScale( Joint->Animatedscale );

  Joint->GlobalMatrix = positionMatrix * rotationMatrix * scaleMatrix;
}





//And M2MVertices are not usable like this. Thus we put it into an irrlicht S3DVertex

if(M2Vertices.size()>0)
    M2Vertices.clear();

for(u32 i=0;i<M2MVertices.size();i++)
{
    M2Vertices.push_back(video::S3DVertex(core::vector3df(M2MVertices[i].pos.X,M2MVertices[i].pos.Y,M2MVertices[i].pos.Z),core::vector3df(M2MVertices[i].normal.X,M2MVertices[i].normal.Y,M2MVertices[i].normal.Z), video::SColor(255,100,100,100),M2MVertices[i].texcoords));
}
//Loop through the submeshes
for(u32 i=0; i < currentView.Submesh.num;i++)//
{
    //Now, M2MTriangles refers to M2MIndices and not to M2MVertices.
    scene::SSkinMeshBuffer *MeshBuffer = AnimatedMesh->addMeshBuffer(M2MSubmeshes[i].meshpartId);

    //Put the Indices and Vertices of the Submesh into a mesh buffer
    //Each Submesh contains only the Indices and Vertices that belong to it.
    //Because of this the Index values for the Submeshes must be corrected by the Vertex offset of the Submesh
    for(u32 j=M2MSubmeshes[i].ofsTris;j<M2MSubmeshes[i].ofsTris+M2MSubmeshes[i].nTris;j++)
    {
        MeshBuffer->Indices.push_back(M2MIndices[M2MTriangles[j]]-M2MSubmeshes[i].ofsVertex);
        if(M2MIndices[M2MTriangles[j]]<M2MSubmeshes[i].ofsVertex)
          logerror("Index %u < ofsVertex %u",M2MIndices[M2MTriangles[j]],M2MSubmeshes[i].ofsVertex);
    }

    for(u32 j=M2MSubmeshes[i].ofsVertex;j<M2MSubmeshes[i].ofsVertex+M2MSubmeshes[i].nVertex;j++)
    {
        MeshBuffer->Vertices_Standard.push_back(M2Vertices[j]);
        for(u32 k=0; k<4; k++)
        {
            if((M2MVertices[j].weights[k])>0)
            {
            u32 boneIndex = M2MVertices[j].bones[k];
            scene::CM2Mesh::SWeight* weight = AnimatedMesh->addWeight(AnimatedMesh->getAllJoints()[boneIndex]);
            weight->strength=M2MVertices[j].weights[k];
            weight->vertex_id=MeshBuffer->Vertices_Standard.size()-1;
            weight->buffer_id=i;
            }

        }
    }


    MeshBuffer->recalculateBoundingBox();
    for(u32 j=0;j<M2MTextureUnit.size();j++)//Loop through texture units
    {
        if(M2MTextureUnit[j].submeshIndex1==i && !M2MTextureFiles[M2MTextureLookup[M2MTextureUnit[j].textureIndex]].empty())//if a texture unit belongs to this submesh
        {
            //set vertex colors from colorblock
            //FIXME: Only the first color is used, no animation!!
            s16 vColIndex = M2MTextureUnit[j].colorIndex;
            if(vColIndex != -1)
            {
                DEBUG(logdebug("Applying color %f %f %f %f",M2MVertexColor[vColIndex].Colors.values[0],M2MVertexColor[vColIndex].Colors.values[1],M2MVertexColor[vColIndex].Colors.values[2],M2MVertexColor[vColIndex].Alpha.values[0]));
                video::SColor color = video::SColorf(M2MVertexColor[vColIndex].Colors.values[0],M2MVertexColor[vColIndex].Colors.values[1],M2MVertexColor[vColIndex].Colors.values[2],M2MVertexColor[vColIndex].Alpha.values[0]).toSColor();
//                 Device->getSceneManager()->getMeshManipulator()->apply(scene::SVertexColorSetManipulator(color), MeshBuffer);//
            //MeshBuffer->getMaterial().DiffuseColor = color; // if we want to set diffuse instead of vertex color
            }
            // get and set texture
            char buf[1000];
            MemoryDataHolder::MakeTextureFilename(buf,M2MTextureFiles[M2MTextureLookup[M2MTextureUnit[j].textureIndex]].c_str());

            video::ITexture* tex = Device->getVideoDriver()->findTexture(buf);
            if(!tex)
            {
              io::IReadFile* TexFile = io::IrrCreateIReadFileBasic(Device, buf);
              if (!TexFile)
              {
                  logerror("CM2MeshFileLoader: Texture file not found: %s", buf);
                  continue;
              }
            tex = Device->getVideoDriver()->getTexture(TexFile);
            TexFile->drop();
            }
            MeshBuffer->getMaterial().setTexture(M2MTextureUnit[j].TextureUnitNumber,tex);  // set a condition here to handle animated textures if they are used irrlicht.sourceforge.net/docu/example008.html

            DEBUG(logdebug("Render Flags: %u %u",M2MRenderFlags[M2MTextureUnit[j].renderFlagsIndex].flags,M2MRenderFlags[M2MTextureUnit[j].renderFlagsIndex].blending));
            u16 renderflags = M2MRenderFlags[M2MTextureUnit[j].renderFlagsIndex].flags;

            MeshBuffer->getMaterial().Lighting=(renderflags & 0x01)?false:true;
            MeshBuffer->getMaterial().FogEnable=(renderflags & 0x02)?false:true;
            MeshBuffer->getMaterial().BackfaceCulling=(renderflags & 0x04)?false:true;
            //We have a problem here
            //             MeshBuffer->getMaterial().ZBuffer=(renderflags & 0x10)?video::ECFN_LESS:video::ECFN_LESSEQUAL;

            switch(M2MRenderFlags[M2MTextureUnit[j].renderFlagsIndex].blending)
            {
              case 0:  //opaque
                MeshBuffer->getMaterial().MaterialType=video::EMT_SOLID;
                break;
              case 1:  //alpha ref
                MeshBuffer->getMaterial().MaterialType=video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;  // default REF = 127 maybe set a lower REF?
                break;
              case 2:  //alpha blend
                MeshBuffer->getMaterial().MaterialType=video::EMT_ONETEXTURE_BLEND;
                MeshBuffer->getMaterial().MaterialTypeParam = pack_textureBlendFunc(video::EBF_SRC_ALPHA, video::EBF_ONE_MINUS_SRC_ALPHA,  video::EMFN_MODULATE_1X, video::EAS_TEXTURE|video::EAS_VERTEX_COLOR);
                break;
              case 3:  //additive
                MeshBuffer->getMaterial().MaterialType=video::EMT_ONETEXTURE_BLEND;
                MeshBuffer->getMaterial().MaterialTypeParam = pack_textureBlendFunc(video::EBF_SRC_COLOR, video::EBF_DST_COLOR, video::EMFN_MODULATE_1X, video::EAS_TEXTURE|video::EAS_VERTEX_COLOR);  // | video::EAS_VERTEX_COLOR);
                break;
              case 4:  //additive alpha
                MeshBuffer->getMaterial().MaterialType=video::EMT_ONETEXTURE_BLEND;
                MeshBuffer->getMaterial().MaterialTypeParam = pack_textureBlendFunc(video::EBF_SRC_ALPHA, video::EBF_ONE, video::EMFN_MODULATE_2X, video::EAS_TEXTURE|video::EAS_VERTEX_COLOR);
                break;
              case 5:  //modulate blend
                MeshBuffer->getMaterial().MaterialType=video::EMT_ONETEXTURE_BLEND;
                MeshBuffer->getMaterial().MaterialTypeParam = pack_textureBlendFunc(video::EBF_SRC_COLOR, video::EBF_DST_COLOR, video::EMFN_MODULATE_1X, video::EAS_TEXTURE|video::EAS_VERTEX_COLOR);
                break;
              case 6:  //Lumirion: not sure exatly so I'm using modulate2x blend like wowmodelviewer or could use EMT_TRANSPARENT_ADD_COLOR
                MeshBuffer->getMaterial().MaterialType=video::EMT_ONETEXTURE_BLEND;
                MeshBuffer->getMaterial().MaterialTypeParam = pack_textureBlendFunc(video::EBF_DST_COLOR, video::EBF_SRC_COLOR, video::EMFN_MODULATE_2X, video::EAS_TEXTURE|video::EAS_VERTEX_COLOR);
                break;
            }
        }

    }

    MeshBuffer->recalculateBoundingBox();
    if(header.Animations.num==0 && header.GlobalSequences.num == 0)
      MeshBuffer->setHardwareMappingHint(EHM_STATIC);
    else
      MeshBuffer->setHardwareMappingHint(EHM_STREAM);

}
Device->getSceneManager()->getMeshManipulator()->flipSurfaces(AnimatedMesh);

for(u32 i=0; i< M2MAnimfiles.size();i++)
{
  if(M2MAnimfiles[i]!=0)
    M2MAnimfiles[i]->drop();
}

M2MAnimations.clear();
M2MAnimfiles.clear();
M2MTriangles.clear();
M2Vertices.clear();
M2Indices.clear();
M2MIndices.clear();
M2MVertices.clear();
M2MRenderFlags.clear();
M2MTextureUnit.clear();
M2MTextureDef.clear();
M2MSubmeshes.clear();
M2MTextureFiles.clear();
M2MTextureLookup.clear();
M2MVertexColor.clear();
M2MLights.clear();
return true;
}

}
}
