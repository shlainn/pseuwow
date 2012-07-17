#ifndef __M2_MESH_H_INCLUDED__
#define __M2_MESH_H_INCLUDED__

#include "irrlicht/irrlicht.h"

namespace irr
{
namespace scene
{

    struct M2Animation
    {
        f32 probability;
        u32 begin;
        u32 end;
    };
	class IAnimatedMeshSceneNode;
	class IBoneSceneNode;

	class CM2Mesh: public ISkinnedMesh
	{
	public:

		//! constructor
		CM2Mesh();

		//! destructor
		virtual ~CM2Mesh();

		//! returns the amount of frames. If the amount is 1, it is a static (=non animated) mesh.
		virtual u32 getFrameCount() const;

        //! Gets the default animation speed of the animated mesh.
        /** \return Amount of frames per second. If the amount is 0, it is a static, non animated mesh. */
        virtual f32 getAnimationSpeed() const;

        //! Gets the frame count of the animated mesh.
        /** \param fps Frames per second to play the animation with. If the amount is 0, it is not animated.
        The actual speed is set in the scene node the mesh is instantiated in.*/
        virtual void setAnimationSpeed(f32 fps);

		//! returns the animated mesh based on a detail level (which is ignored)
		virtual IMesh* getMesh(s32 frame, s32 detailLevel=255, s32 startFrameLoop=-1, s32 endFrameLoop=-1);

		//! Animates this mesh's joints based on frame input
		//! blend: {0-old position, 1-New position}
		virtual void animateMesh(f32 frame, f32 blend);

		//! Preforms a software skin on this mesh based of joint positions
		virtual void skinMesh();

		//! returns amount of mesh buffers.
		virtual u32 getMeshBufferCount() const;

		//! returns pointer to a mesh buffer
		virtual IMeshBuffer* getMeshBuffer(u32 nr) const;

		//! Returns pointer to a mesh buffer which fits a material
 		/** \param material: material to search for
		\return Returns the pointer to the mesh buffer or
		NULL if there is no such mesh buffer. */
		virtual IMeshBuffer* getMeshBuffer( const video::SMaterial &material) const;

		//! returns an axis aligned bounding box
		virtual const core::aabbox3d<f32>& getBoundingBox() const;

		//! set user axis aligned bounding box
		virtual void setBoundingBox( const core::aabbox3df& box);

		//! sets a flag of all contained materials to a new value
		virtual void setMaterialFlag(video::E_MATERIAL_FLAG flag, bool newvalue);

		//! set the hardware mapping hint, for driver
		virtual void setHardwareMappingHint(E_HARDWARE_MAPPING newMappingHint, E_BUFFER_TYPE buffer=EBT_VERTEX_AND_INDEX);

		//! flags the meshbuffer as changed, reloads hardware buffers
		virtual void setDirty(E_BUFFER_TYPE buffer=EBT_VERTEX_AND_INDEX);

        //! updates the bounding box
        virtual void updateBoundingBox(void);

        //! Returns the type of the animated mesh.
		virtual E_ANIMATED_MESH_TYPE getMeshType() const;

		//! Gets joint count.
		virtual u32 getJointCount() const;

		//! Gets the name of a joint.
		virtual const c8* getJointName(u32 number) const;

		//! Gets a joint number from its name
		virtual s32 getJointNumber(const c8* name) const;

		//! uses animation from another mesh
		virtual bool useAnimationFrom(const ISkinnedMesh *mesh);

		//! Update Normals when Animating
		//! False= Don't (default)
		//! True = Update normals, slower
		virtual void updateNormalsWhenAnimating(bool on);

		//! Sets Interpolation Mode
		virtual void setInterpolationMode(E_INTERPOLATION_MODE mode);

		//! Recovers the joints from the mesh
		virtual void recoverJointsFromMesh(core::array<IBoneSceneNode*> &JointChildSceneNodes);

		//! Tranfers the joint data to the mesh
		virtual void transferJointsToMesh(const core::array<IBoneSceneNode*> &JointChildSceneNodes);

		//! Tranfers the joint hints to the mesh
		virtual void transferOnlyJointsHintsToMesh(const core::array<IBoneSceneNode*> &JointChildSceneNodes);

		//! Creates an array of joints from this mesh
		virtual void createJoints(core::array<IBoneSceneNode*> &JointChildSceneNodes,
				IAnimatedMeshSceneNode* AnimatedMeshSceneNode,
				ISceneManager* SceneManager);

		//! Convertes the mesh to contain tangent information
		virtual void convertMeshToTangents();

		//! Does the mesh have no animation
		virtual bool isStatic();

		//! (This feature is not implemented in irrlicht yet)
		virtual bool setHardwareSkinning(bool on);

		//Interface for the mesh loaders (finalize should lock these functions, and they should have some prefix like loader_

		//these functions will use the needed arrays, set vaules, etc to help the loaders

		//! exposed for loaders to add mesh buffers
		virtual core::array<SSkinMeshBuffer*> &getMeshBuffers();

		//! alternative method for adding joints
		virtual core::array<SJoint*> &getAllJoints();

		//! alternative method for adding joints
		virtual const core::array<SJoint*> &getAllJoints() const;

		//! loaders should call this after populating the mesh
		virtual void finalize();

        SSkinMeshBuffer *addMeshBuffer(u32 id)
        {
          GeoSetID.push_back(id);
          GeoSetRender.push_back((id==0?true:false));//This may be changed later on when we know more about the submesh switching business
          return addMeshBuffer();
        };
		virtual SSkinMeshBuffer *addMeshBuffer();

		virtual SJoint *addJoint(SJoint *parent=0);

		virtual SPositionKey *addPositionKey(SJoint *joint);
		virtual SRotationKey *addRotationKey(SJoint *joint);
		virtual SScaleKey *addScaleKey(SJoint *joint);

		virtual SWeight *addWeight(SJoint *joint);

        //Retrieve animation information
        void getFrameLoop(u32 animId, s32 &start, s32 &end);
        void newAnimation(u32 id, s32 start, s32 end, f32 probability);
        //Retrieve geoset rendering information
        void setGeoSetRender(u32 id, bool render);
        void setMBRender(u32 id, bool render);
        bool getGeoSetRender(u32 meshbufferNumber);
private:

		void checkForAnimation();

		void normalizeWeights();

		void buildAllAnimatedMatrices(SJoint *Joint=0, SJoint *ParentJoint=0); //public?

		void getFrameData(f32 frame, SJoint *Node,
				core::vector3df &position, s32 &positionHint,
				core::vector3df &scale, s32 &scaleHint,
				core::quaternion &rotation, s32 &rotationHint);

		void CalculateGlobalMatrices(SJoint *Joint,SJoint *ParentJoint);

		void SkinJoint(SJoint *Joint, SJoint *ParentJoint);

		void calculateTangents(core::vector3df& normal,
			core::vector3df& tangent, core::vector3df& binormal,
			core::vector3df& vt1, core::vector3df& vt2, core::vector3df& vt3,
			core::vector2df& tc1, core::vector2df& tc2, core::vector2df& tc3);


		core::array<SSkinMeshBuffer*> *SkinningBuffers; //Meshbuffer to skin, default is to skin localBuffers

		core::array<SSkinMeshBuffer*> LocalBuffers;
        core::array<u32> GeoSetID; //Array of Submesh Meshpart IDs used for switching Geosets on and off
        core::array<bool> GeoSetRender;

        core::array<SJoint*> AllJoints;
		core::array<SJoint*> RootJoints;

		bool HasAnimation;

		bool PreparedForSkinning;

		f32 AnimationFrames;
        f32 FramesPerSecond;

		f32 LastAnimatedFrame;
        bool SkinnedLastFrame;

		bool AnimateNormals;

		bool HardwareSkinning;


		E_INTERPOLATION_MODE InterpolationMode;

		core::aabbox3d<f32> BoundingBox;

		core::array< core::array<bool> > Vertices_Moved;

        core::array< M2Animation > Animations;
        core::map<u32, core::array<u32> > AnimationLookup;
	};

} // end namespace scene
} // end namespace irr

#endif



