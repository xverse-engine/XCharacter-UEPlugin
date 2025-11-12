#ifndef  _BVH_H_
#define  _BVH_H_

#include <vector>
#include <map>
#include <string>

enum  ChannelEnum
{
	X_ROTATION, Y_ROTATION, Z_ROTATION,
	X_POSITION, Y_POSITION, Z_POSITION
};
struct  Joint;

struct  Channel
{
	Joint* joint;
	ChannelEnum             type;
	int                     index;
};

struct  Joint
{
	std::string              name;
	int                      index;
	Joint* parent;
	std::vector< Joint* >    children;
	double                   offset[3];
	bool                     has_site;
	double                   site[3];
	std::vector< Channel* >  channels;
};

class FBVHFile
{
private:
	bool                             is_load_success;
	std::string                      bvh_file_name;
	std::string                      motion_name;
	int                              num_channel;
	std::vector< Channel* >          channels;
	std::vector< Joint* >            joints;
	std::map< std::string, Joint* >  joint_index;


	int                      num_frame;
	double                   interval;
	double*                  motion;


public:
	FBVHFile(const char* bvh_file_name);
	FBVHFile();
	~FBVHFile();

	bool Open();
	bool Import(const std::string& content);

	void Clear();

	

	void Init(const char* name,
		int n_joi, const Joint** a_joi, int n_chan, const Channel** a_chan,
		int n_frame, double interval, const double* mo);

	void SetSkeleton(const char* name,
		int n_joi, const Joint** a_joi, int n_chan, const Channel** a_chan);

	void SetMotion(int n_frame, double interval, const double* mo = NULL);

	void Save();

	FTransform GetTransform(int n_frame, int n_joint);

public:
	bool  IsLoadSuccess() const { return is_load_success; }
	const std::string& GetMotionName() const { return motion_name; }

	const int       GetNumJoint() const { return  joints.size(); }
	const Joint*    GetJoint(int no) const { return  joints[no]; }
	const int       GetNumChannel() const { return  channels.size(); }
	const Channel*  GetChannel(int no) const { return  channels[no]; }

	const Joint* GetJoint(const std::string& j) const {
		std::map< std::string, Joint* >::const_iterator  i = joint_index.find(j);
		return  (i != joint_index.end()) ? (*i).second : NULL;
	}

	int     GetNumFrame() const { return  num_frame; }
	double  GetInterval() const { return  interval; }
	double  GetMotion(int f, int c) const { return  motion[f * num_channel + c]; }

	void  SetMotion(int f, int c, double v) { motion[f * num_channel + c] = v; }

	USkeleton* CreateSkeletonByBvhInfo(UObject* Outer, FString SkeletonName);
	USkeleton* FindDefaultSkeleton();

protected:
	void  OutputHierarchy(std::ofstream& file, const Joint* joint, int indent_level,
		std::vector< int >& channel_list);
};

#endif // _BVH_H_
