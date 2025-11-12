#include "MotionGen/BVHFile.h"

#pragma warning( disable : 4996)
#pragma warning( disable : 4244)
#pragma warning( disable : 4018)

#include <fstream>
#include <cstring>
#include <string.h>
#include <sstream>

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "UObject/ObjectMacros.h"
#include "IMeshBuilderModule.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"

FBVHFile::FBVHFile(const char* file_name)
{
	bvh_file_name = file_name;
	motion = NULL;
}

FBVHFile::FBVHFile()
{
	bvh_file_name = "remote";
	motion = NULL;
}

FBVHFile::~FBVHFile()
{
	Clear();
}

void  FBVHFile::Clear()
{
	for (int i = 0; i < channels.size(); i++)
	{
		delete  channels[i];
	}
	for (int i = 0; i < joints.size(); i++)
	{
		delete  joints[i];
	}
	if (motion != NULL)
	{
		delete  motion;
	}

	is_load_success = false;
	num_channel = 0;
	channels.clear();
	joints.clear();
	joint_index.clear();

	num_frame = 0;
	interval = 0.0;
	motion = NULL;
}


void  FBVHFile::Init(const char* name,
	int n_joi, const Joint** a_joi, int n_chan, const Channel** a_chan,
	int n_frame, double inter, const double* mo)
{
	SetSkeleton(name, n_joi, a_joi, n_chan, a_chan);
	SetMotion(n_frame, inter, mo);
}


void  FBVHFile::SetSkeleton(const char* name,
	int n_joi, const Joint** a_joi, int n_chan, const Channel** a_chan)
{
	Clear();
	int  i, j;	

	if (name)
	{
		motion_name = name;
	}
	num_channel = n_chan;

	channels.resize(num_channel);
	for (i = 0; i < num_channel; i++)
	{
		channels[i] = new Channel();
	}
	joints.resize(n_joi);
	for (i = 0; i < n_joi; i++)
	{
		joints[i] = new Joint();
	}

	for (i = 0; i < num_channel; i++)
	{
		*channels[i] = *a_chan[i];
		channels[i]->joint = joints[a_chan[i]->joint->index];
	}
	for (i = 0; i < n_joi; i++)
	{
		*joints[i] = *a_joi[i];
		for (j = 0; j < joints[i]->channels.size(); j++)
		{
			joints[i]->channels[j] = channels[a_joi[i]->channels[j]->index];
		}
		if (a_joi[i]->parent == NULL)
		{
			joints[i]->parent = NULL;
		}
		else
		{
			joints[i]->parent = joints[a_joi[i]->parent->index];
		}
		for (j = 0; j < joints[i]->children.size(); j++)
		{
			joints[i]->children[j] = joints[a_joi[i]->children[j]->index];
		}

		if (joints[i]->name.size() > 0)
		{
			joint_index[joints[i]->name] = joints[i];
		}
	}
}


void  FBVHFile::SetMotion(int n_frame, double inter, const double* mo)
{
	num_frame = n_frame;
	interval = inter;
	motion = new double[num_frame * num_channel];
	if (mo != NULL)
	{
		memcpy(motion, mo, sizeof(double) * num_frame * num_channel);
	}
}


bool  FBVHFile::Import(const std::string& content)
{
#define  BUFFER_LENGTH  (1024*1024*4)
	Clear();

	std::istringstream   file(content);
	char           line[BUFFER_LENGTH];
	char* token;
	char           separater[] = " :,\t";

	std::vector< Joint* >   joint_stack;
	Joint* joint = NULL;
	Joint* new_joint = NULL;
	bool          is_site = false;
	double        x, y, z;
	int           i, j;

	const char* mn_first = bvh_file_name.c_str();
	const char* mn_last = bvh_file_name.c_str() + strlen(bvh_file_name.c_str());
	if (strrchr(bvh_file_name.c_str(), '\\') != NULL)
	{
		mn_first = strrchr(bvh_file_name.c_str(), '\\') + 1;
	}
	else if (strrchr(bvh_file_name.c_str(), '/') != NULL)
	{
		mn_first = strrchr(bvh_file_name.c_str(), '/') + 1;
	}
	if (strrchr(bvh_file_name.c_str(), '.') != NULL)
	{
		mn_last = strrchr(bvh_file_name.c_str(), '.');
	}
	if (mn_last < mn_first)
	{
		mn_last = bvh_file_name.c_str() + strlen(bvh_file_name.c_str());
	}
	motion_name.assign(mn_first, mn_last);


	while (!file.eof())
	{
		if (file.eof())  goto bvh_error;
		file.getline(line, BUFFER_LENGTH);
		token = strtok(line, separater);
		if (token == NULL)  continue;
		if (strcmp(token, "{") == 0)
		{
			joint_stack.push_back(joint);
			joint = new_joint;
			continue;
		}
		if (strcmp(token, "}") == 0)
		{
			joint = joint_stack.back();
			joint_stack.pop_back();
			is_site = false;
			continue;
		}

		if ((strcmp(token, "ROOT") == 0) || (strcmp(token, "JOINT") == 0))
		{
			new_joint = new Joint();
			new_joint->index = joints.size();
			new_joint->parent = joint;
			new_joint->has_site = false;
			new_joint->offset[0] = 0.0;  new_joint->offset[1] = 0.0;  new_joint->offset[2] = 0.0;
			new_joint->site[0] = 0.0;  new_joint->site[1] = 0.0;  new_joint->site[2] = 0.0;
			joints.push_back(new_joint);
			if (joint)
			{
				joint->children.push_back(new_joint);
			}

			token = strtok(NULL, "");
			while (*token == ' ')
			{
				token++;
			}
			new_joint->name = token;

			joint_index[new_joint->name] = new_joint;
			continue;
		}

		if ((strcmp(token, "End") == 0))
		{
			new_joint = joint;
			is_site = true;
			continue;
		}

		if (strcmp(token, "OFFSET") == 0)
		{
			token = strtok(NULL, separater);
			x = token ? atof(token) : 0.0;
			token = strtok(NULL, separater);
			y = token ? atof(token) : 0.0;
			token = strtok(NULL, separater);
			z = token ? atof(token) : 0.0;

			if (is_site)
			{
				joint->has_site = true;
				joint->site[0] = x;
				joint->site[1] = y;
				joint->site[2] = z;
			}
			else
			{
				joint->offset[0] = x;
				joint->offset[1] = y;
				joint->offset[2] = z;
			}
			continue;
		}

		if (strcmp(token, "CHANNELS") == 0)
		{
			token = strtok(NULL, separater);
			joint->channels.resize(token ? atoi(token) : 0);

			for (i = 0; i < joint->channels.size(); i++)
			{
				Channel* channel = new Channel();
				channel->joint = joint;
				channel->index = channels.size();
				channels.push_back(channel);
				joint->channels[i] = channel;

				token = strtok(NULL, separater);
				if (strcmp(token, "Xrotation") == 0)
				{
					channel->type = X_ROTATION;
				}
				else if (strcmp(token, "Yrotation") == 0)
				{
					channel->type = Y_ROTATION;
				}
				else if (strcmp(token, "Zrotation") == 0)
				{
					channel->type = Z_ROTATION;
				}
				else if (strcmp(token, "Xposition") == 0)
				{
					channel->type = X_POSITION;
				}
				else if (strcmp(token, "Yposition") == 0)
				{
					channel->type = Y_POSITION;
				}
				else if (strcmp(token, "Zposition") == 0)
				{
					channel->type = Z_POSITION;
				}
			}
		}

		if (strcmp(token, "MOTION") == 0)
		{
			break;
		}
	}

	while (!file.eof())
	{
		file.getline(line, BUFFER_LENGTH);
		token = strtok(line, separater);
		if (!token)
		{
			continue;
		}
		if (strcmp(token, "Frames") == 0)
		{
			break;
		}
	}
	if (file.eof())
	{
		goto bvh_error;
	}
	token = strtok(NULL, separater);
	if (token == NULL)
	{
		goto bvh_error;
	}
	num_frame = atoi(token);

	while (!file.eof())
	{
		file.getline(line, BUFFER_LENGTH);
		token = strtok(line, ":");
		if (!token)
		{
			continue;
		}
		if (strcmp(token, "Frame Time") == 0)
		{
			break;
		}
	}
	if (file.eof())
	{
		goto bvh_error;
	}
	token = strtok(NULL, separater);
	if (token == NULL)
	{
		goto bvh_error;
	}
	interval = atof(token);

	num_channel = channels.size();
	motion = new double[num_frame * num_channel];

	for (i = 0; i < num_frame; i++)
	{
		file.getline(line, BUFFER_LENGTH);
		token = strtok(line, separater);
		for (j = 0; j < num_channel; j++)
		{
			if (token == NULL)
			{
				goto bvh_error;
			}
			motion[i * num_channel + j] = atof(token);
			token = strtok(NULL, separater);
		}
	}
	is_load_success = true;

bvh_error:
	return is_load_success;
}


bool  FBVHFile::Open()
{
#define  BUFFER_LENGTH  (1024*1024*4)
	Clear();

	std::ifstream  file;
	char           line[BUFFER_LENGTH];
	char*          token;
	char           separater[] = " :,\t";
	
	std::vector< Joint* >   joint_stack;
	Joint*        joint = NULL;
	Joint*        new_joint = NULL;
	bool          is_site = false;
	double        x, y, z;
	int           i, j;

	const char* mn_first = bvh_file_name.c_str();
	const char* mn_last = bvh_file_name.c_str() + strlen(bvh_file_name.c_str());
	if (strrchr(bvh_file_name.c_str(), '\\') != NULL)
	{
		mn_first = strrchr(bvh_file_name.c_str(), '\\') + 1;
	}
	else if (strrchr(bvh_file_name.c_str(), '/') != NULL)
	{
		mn_first = strrchr(bvh_file_name.c_str(), '/') + 1;
	}
	if (strrchr(bvh_file_name.c_str(), '.') != NULL)
	{
		mn_last = strrchr(bvh_file_name.c_str(), '.');
	}
	if (mn_last < mn_first)
	{
		mn_last = bvh_file_name.c_str() + strlen(bvh_file_name.c_str());
	}
	motion_name.assign(mn_first, mn_last);

	file.open(bvh_file_name, std::ios::in);
	if (file.is_open() == 0)
	{
		goto bvh_error;
	}

	while (!file.eof())
	{
		if (file.eof())  goto bvh_error;
		file.getline(line, BUFFER_LENGTH);
		token = strtok(line, separater);
		if (token == NULL)  continue;
		if (strcmp(token, "{") == 0)
		{
			joint_stack.push_back(joint);
			joint = new_joint;
			continue;
		}
		if (strcmp(token, "}") == 0)
		{
			joint = joint_stack.back();
			joint_stack.pop_back();
			is_site = false;
			continue;
		}

		if ((strcmp(token, "ROOT") == 0) || (strcmp(token, "JOINT") == 0))
		{
			new_joint = new Joint();
			new_joint->index = joints.size();
			new_joint->parent = joint;
			new_joint->has_site = false;
			new_joint->offset[0] = 0.0;  new_joint->offset[1] = 0.0;  new_joint->offset[2] = 0.0;
			new_joint->site[0] = 0.0;  new_joint->site[1] = 0.0;  new_joint->site[2] = 0.0;
			joints.push_back(new_joint);
			if (joint)
			{
				joint->children.push_back(new_joint);
			}

			token = strtok(NULL, "");
			while (*token == ' ')
			{
				token++;
			}
			new_joint->name = token;

			joint_index[new_joint->name] = new_joint;
			continue;
		}

		if ((strcmp(token, "End") == 0))
		{
			new_joint = joint;
			is_site = true;
			continue;
		}

		if (strcmp(token, "OFFSET") == 0)
		{
			token = strtok(NULL, separater);
			x = token ? atof(token) : 0.0;
			token = strtok(NULL, separater);
			y = token ? atof(token) : 0.0;
			token = strtok(NULL, separater);
			z = token ? atof(token) : 0.0;

			if (is_site)
			{
				joint->has_site = true;
				joint->site[0] = x;
				joint->site[1] = y;
				joint->site[2] = z;
			}
			else
			{
				joint->offset[0] = x;
				joint->offset[1] = y;
				joint->offset[2] = z;
			}
			continue;
		}

		if (strcmp(token, "CHANNELS") == 0)
		{
			token = strtok(NULL, separater);
			joint->channels.resize(token ? atoi(token) : 0);

			for (i = 0; i < joint->channels.size(); i++)
			{
				Channel* channel = new Channel();
				channel->joint = joint;
				channel->index = channels.size();
				channels.push_back(channel);
				joint->channels[i] = channel;

				token = strtok(NULL, separater);
				if (strcmp(token, "Xrotation") == 0)
				{
					channel->type = X_ROTATION;
				}
				else if (strcmp(token, "Yrotation") == 0)
				{
					channel->type = Y_ROTATION;
				}
				else if (strcmp(token, "Zrotation") == 0)
				{
					channel->type = Z_ROTATION;
				}
				else if (strcmp(token, "Xposition") == 0)
				{
					channel->type = X_POSITION;
				}
				else if (strcmp(token, "Yposition") == 0)
				{
					channel->type = Y_POSITION;
				}
				else if (strcmp(token, "Zposition") == 0)
				{
					channel->type = Z_POSITION;
				}
			}
		}

		if (strcmp(token, "MOTION") == 0)
		{
			break;
		}
	}

	while (!file.eof())
	{
		file.getline(line, BUFFER_LENGTH);
		token = strtok(line, separater);
		if (!token)
		{
			continue;
		}
		if (strcmp(token, "Frames") == 0)
		{
			break;
		}
	}
	if (file.eof())
	{
		goto bvh_error;
	}
	token = strtok(NULL, separater);
	if (token == NULL)
	{
		goto bvh_error;
	}
	num_frame = atoi(token);

	while (!file.eof())
	{
		file.getline(line, BUFFER_LENGTH);
		token = strtok(line, ":");
		if (!token)
		{
			continue;
		}
		if (strcmp(token, "Frame Time") == 0)
		{
			break;
		}
	}
	if (file.eof())
	{
		goto bvh_error;
	}
	token = strtok(NULL, separater);
	if (token == NULL)
	{
		goto bvh_error;
	}
	interval = atof(token);

	num_channel = channels.size();
	motion = new double[num_frame * num_channel];

	for (i = 0; i < num_frame; i++)
	{
		file.getline(line, BUFFER_LENGTH);
		token = strtok(line, separater);
		for (j = 0; j < num_channel; j++)
		{
			if (token == NULL)
			{
				goto bvh_error;
			}
			motion[i * num_channel + j] = atof(token);
			token = strtok(NULL, separater);
		}
	}
	is_load_success = true;

bvh_error:
	file.close();
	return is_load_success;
}

FTransform FBVHFile::GetTransform(int n_frame, int n_joint)
{
	double* p = &motion[n_frame * num_channel];
	Joint* j = joints[n_joint];

	FVector Offset = FVector::Zero();
	FVector Euler = FVector::Zero();

	Offset.X = j->offset[0];
	Offset.Y = -j->offset[1];
	Offset.Z = j->offset[2];

	for (int32 i = 0; i < j->channels.size(); ++i)
	{
		Channel* c = j->channels[i];
		if (c->type == ChannelEnum::X_POSITION)
		{
			Offset.X = p[c->index];
		}
		else if (c->type == ChannelEnum::Y_POSITION)
		{
			Offset.Y = -p[c->index];
		}
		else if (c->type == ChannelEnum::Z_POSITION)
		{
			Offset.Z = p[c->index];
		}
		else 
			
		if (c->type == ChannelEnum::Z_ROTATION)
		{
			Euler.Z = -p[c->index];
		}
		else if (c->type == ChannelEnum::Y_ROTATION)
		{
			Euler.Y = p[c->index];
		}
		else if (c->type == ChannelEnum::X_ROTATION)
		{
			Euler.X = -p[c->index];
		}
	}

	double RadX = FMath::DegreesToRadians(Euler.X);
	double RadY = FMath::DegreesToRadians(Euler.Y);
	double RadZ = FMath::DegreesToRadians(Euler.Z);

	FQuat RotationZ(FVector::UnitZ(), RadZ);
	FQuat RotationY(FVector::UnitY(), RadY);
	FQuat RotationX(FVector::UnitX(), RadX);
	FQuat Rotation = RotationZ * RotationY * RotationX;

	return FTransform(Rotation, Offset);
}


void  FBVHFile::Save()
{
	int  i, j;
	std::ofstream  file;

	std::vector< int >   channel_order;

	file.open(bvh_file_name.c_str(), std::ios::out);
	if (file.is_open() == 0)
	{
		return;
	}

	file.flags(std::ios::showpoint | std::ios::fixed);
	file.precision(6);
	//	int  value_widht = 11;

	file << "HIERARCHY" << std::endl;
	OutputHierarchy(file, joints[0], 0, channel_order);

	file << "MOTION" << std::endl;
	file << "Frames: " << num_frame << std::endl;
	file << "Frame Time: " << interval << std::endl;
	for (i = 0; i < num_frame; i++)
	{
		for (j = 0; j < channel_order.size(); j++)
		{
//			if ( value_widht > 0 )
//				file.width( value_widht );

			file << GetMotion(i, channel_order[j]);
			if (j != channel_order.size() - 1)
			{
				file << "  ";
			}
			else
			{
				file << std::endl;
			}
		}
	}
	file.close();
}


USkeleton* FBVHFile::FindDefaultSkeleton() {

	FString SkeletonPath = TEXT("/XCharacter/Skeleton/Default_Skeleton");

	return LoadObject<USkeleton>(nullptr, *SkeletonPath);
}

USkeleton* FBVHFile::CreateSkeletonByBvhInfo(UObject* Outer, FString SkeletonName) {
	
	UPackage* SkeletonPackage = CreatePackage(*FString::Printf(TEXT("%s/%s"), *FPackageName::GetLongPackagePath(TEXT("/XCharacter/Skeleton/")), *SkeletonName));
	UPackage* SkeletalMeshPackage = CreatePackage(*FString::Printf(TEXT("%s/%s"), *FPackageName::GetLongPackagePath(TEXT("/XCharacter/SkeletonMesh/")), *SkeletonName));

	USkeletalMesh* BvhSkeletalMesh = NewObject<USkeletalMesh>(SkeletalMeshPackage, FName("Default_SkeletonMesh"), RF_Standalone | RF_Public);

	USkeleton* BvhSkeleton = NewObject<USkeleton>(SkeletonPackage, FName("Default_Skeleton"), RF_Standalone | RF_Public);

	// Start build skeletal mesh and skeleton
	FReferenceSkeleton& RefSkeleton = BvhSkeletalMesh->GetRefSkeleton();

	FReferenceSkeletonModifier RefSkelModifier(RefSkeleton,BvhSkeleton);

	for (int32 BoneIndex = 0; BoneIndex < GetNumJoint(); ++BoneIndex)
	{

		const Joint* joint = GetJoint(BoneIndex);
		int32 JointIdx = joint->index;
		FName BoneName(ANSI_TO_TCHAR(joint->name.c_str()));
		int32 ParentIdx = -1;

		if (joint->parent) {
			ParentIdx = joint->parent->index;
		}
		
		const FMeshBoneInfo BoneInfo(BoneName, joint->name.c_str(), ParentIdx);

		FVector Offset = FVector::Zero();
		FVector Euler = FVector::Zero();

		Offset.X = joint->offset[0];
		Offset.Y = joint->offset[1];
		Offset.Z = joint->offset[2];


		double RadX = FMath::DegreesToRadians(0);
		double RadY = FMath::DegreesToRadians(0);
		double RadZ = FMath::DegreesToRadians(0);

		FQuat RotationZ(FVector::UnitZ(), RadZ);
		FQuat RotationY(FVector::UnitY(), RadY);
		FQuat RotationX(FVector::UnitX(), RadX);
		FQuat Rotation = RotationZ * RotationY * RotationX;

		RefSkelModifier.Add(BoneInfo, FTransform(Rotation, Offset));
	}


	// SkeletalMesh render data initialize
	FSkeletalMeshModel* ImportedResource = BvhSkeletalMesh->GetImportedModel();

	ImportedResource->LODModels.Empty();
	ImportedResource->LODModels.Add(new FSkeletalMeshLODModel());
	const int32 ImportLODModelIndex = 0;
	FSkeletalMeshLODModel& LODModel = ImportedResource->LODModels[ImportLODModelIndex];

	BvhSkeletalMesh->ResetLODInfo();
	FSkeletalMeshLODInfo& NewLODInfo = BvhSkeletalMesh->AddLODInfo();
	NewLODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
	NewLODInfo.ReductionSettings.NumOfVertPercentage = 1.0f;
	NewLODInfo.ReductionSettings.MaxDeviationPercentage = 0.0f;
	NewLODInfo.LODHysteresis = 0.02f;

	TArray<FSkeletalMaterial>& Materials = BvhSkeletalMesh->GetMaterials();
	Materials.Empty();
	Materials.Add(FSkeletalMaterial(NULL, true, false, NAME_None, NAME_None));

	FSkeletalMeshBuildSettings BuildOptions;
	BvhSkeletalMesh->GetLODInfo(ImportLODModelIndex)->BuildSettings = BuildOptions;
	//New MeshDescription build process
	IMeshBuilderModule& MeshBuilderModule = IMeshBuilderModule::GetForRunningPlatform();
	//We must build the LODModel so we can restore properly the mesh, but we do not have to regenerate LODs
	const bool bRegenDepLODs = false;
	FSkeletalMeshBuildParameters SkeletalMeshBuildParameters(BvhSkeletalMesh, GetTargetPlatformManagerRef().GetRunningTargetPlatform(), ImportLODModelIndex, bRegenDepLODs);
	bool bBuildSuccess = MeshBuilderModule.BuildSkeletalMesh(SkeletalMeshBuildParameters);


	BvhSkeletalMesh->CalculateInvRefMatrices();
	BvhSkeletalMesh->Build();
	BvhSkeletalMesh->SetSkeleton(BvhSkeleton);

	RefSkeleton.RebuildRefSkeleton(BvhSkeleton, true);
	
	BvhSkeleton->MergeAllBonesToBoneTree(BvhSkeletalMesh);
	BvhSkeleton->SetPreviewMesh(BvhSkeletalMesh);


	/*FString PackageFileName = FPackageName::LongPackageNameToFilename(SkeletonPackage->GetPathName(), FPackageName::GetAssetPackageExtension());
	UPackage::SavePackage(SkeletonPackage, nullptr, RF_Standalone | RF_Public, *PackageFileName);*/

	FAssetRegistryModule::AssetCreated(BvhSkeletalMesh);
	FAssetRegistryModule::AssetCreated(BvhSkeleton);

	return BvhSkeleton;
}


void  FBVHFile::OutputHierarchy(std::ofstream& file, const Joint* joint, int indent_level, std::vector< int >& channel_list)
{
	int  i;
	std::string  indent, space;
	Channel* channel;
	indent.assign(indent_level * 4, ' ');
	space.assign("  ");

	if (joint->parent)
	{
		file << indent << "JOINT" << space << joint->name << std::endl;
	}
	else
	{
		file << indent << "ROOT" << space << joint->name << std::endl;
	}

	file << indent << "{" << std::endl;
	indent_level++;
	indent.assign(indent_level * 4, ' ');

	file << indent << "OFFSET" << space;
	file << joint->offset[0] << space;
	file << joint->offset[1] << space;
	file << joint->offset[2] << std::endl;

	file << indent << "CHANNELS" << space << joint->channels.size() << space;
	for (i = 0; i < joint->channels.size(); i++)
	{
		channel = joint->channels[i];
		switch (channel->type)
		{
		case X_ROTATION:
			file << "Xrotation";  break;
		case Y_ROTATION:
			file << "Yrotation";  break;
		case Z_ROTATION:
			file << "Zrotation";  break;
		case X_POSITION:
			file << "Xposition";  break;
		case Y_POSITION:
			file << "Yposition";  break;
		case Z_POSITION:
			file << "Zposition";  break;
		}
		if (i != joint->channels.size() - 1)
		{
			file << space;
		}
		else
		{
			file << std::endl;
		}

		channel_list.push_back(channel->index);
	}

	if (joint->has_site)
	{
		file << indent << "End Site" << std::endl;
		file << indent << "{" << std::endl;

		indent_level++;
		indent.assign(indent_level * 4, ' ');

		file << indent << "OFFSET" << space;
		file << joint->site[0] << space;
		file << joint->site[1] << space;
		file << joint->site[2] << std::endl;

		indent_level--;
		indent.assign(indent_level * 4, ' ');

		file << indent << "}" << std::endl;
	}

	for (i = 0; i < joint->children.size(); i++)
	{
		OutputHierarchy(file, joint->children[i], indent_level, channel_list);
	}

	indent_level--;
	indent.assign(indent_level * 4, ' ');
	file << indent << "}" << std::endl;
}
// End of FBVHFile.cpp
