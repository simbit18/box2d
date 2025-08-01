// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "base.h"
#include "collision.h"
#include "id.h"
#include "math_functions.h"

#include <stdbool.h>
#include <stdint.h>

#define B2_DEFAULT_CATEGORY_BITS 1
#define B2_DEFAULT_MASK_BITS UINT64_MAX

/// Task interface
/// This is prototype for a Box2D task. Your task system is expected to invoke the Box2D task with these arguments.
/// The task spans a range of the parallel-for: [startIndex, endIndex)
/// The worker index must correctly identify each worker in the user thread pool, expected in [0, workerCount).
/// A worker must only exist on only one thread at a time and is analogous to the thread index.
/// The task context is the context pointer sent from Box2D when it is enqueued.
/// The startIndex and endIndex are expected in the range [0, itemCount) where itemCount is the argument to b2EnqueueTaskCallback
/// below. Box2D expects startIndex < endIndex and will execute a loop like this:
///
/// @code{.c}
/// for (int i = startIndex; i < endIndex; ++i)
/// {
/// 	DoWork();
/// }
/// @endcode
/// @ingroup world
typedef void b2TaskCallback( int startIndex, int endIndex, uint32_t workerIndex, void* taskContext );

/// These functions can be provided to Box2D to invoke a task system. These are designed to work well with enkiTS.
/// Returns a pointer to the user's task object. May be nullptr. A nullptr indicates to Box2D that the work was executed
/// serially within the callback and there is no need to call b2FinishTaskCallback.
/// The itemCount is the number of Box2D work items that are to be partitioned among workers by the user's task system.
/// This is essentially a parallel-for. The minRange parameter is a suggestion of the minimum number of items to assign
/// per worker to reduce overhead. For example, suppose the task is small and that itemCount is 16. A minRange of 8 suggests
/// that your task system should split the work items among just two workers, even if you have more available.
/// In general the range [startIndex, endIndex) send to b2TaskCallback should obey:
/// endIndex - startIndex >= minRange
/// The exception of course is when itemCount < minRange.
/// @ingroup world
typedef void* b2EnqueueTaskCallback( b2TaskCallback* task, int itemCount, int minRange, void* taskContext, void* userContext );

/// Finishes a user task object that wraps a Box2D task.
/// @ingroup world
typedef void b2FinishTaskCallback( void* userTask, void* userContext );

/// Optional friction mixing callback. This intentionally provides no context objects because this is called
/// from a worker thread.
/// @warning This function should not attempt to modify Box2D state or user application state.
/// @ingroup world
typedef float b2FrictionCallback( float frictionA, int userMaterialIdA, float frictionB, int userMaterialIdB );

/// Optional restitution mixing callback. This intentionally provides no context objects because this is called
/// from a worker thread.
/// @warning This function should not attempt to modify Box2D state or user application state.
/// @ingroup world
typedef float b2RestitutionCallback( float restitutionA, int userMaterialIdA, float restitutionB, int userMaterialIdB );

/// Result from b2World_RayCastClosest
/// If there is initial overlap the fraction and normal will be zero while the point is an arbitrary point in the overlap region.
/// @ingroup world
typedef struct b2RayResult
{
	b2ShapeId shapeId;
	b2Vec2 point;
	b2Vec2 normal;
	float fraction;
	int nodeVisits;
	int leafVisits;
	bool hit;
} b2RayResult;

/// World definition used to create a simulation world.
/// Must be initialized using b2DefaultWorldDef().
/// @ingroup world
typedef struct b2WorldDef
{
	/// Gravity vector. Box2D has no up-vector defined.
	b2Vec2 gravity;

	/// Restitution speed threshold, usually in m/s. Collisions above this
	/// speed have restitution applied (will bounce).
	float restitutionThreshold;

	/// Threshold speed for hit events. Usually meters per second.
	float hitEventThreshold;

	/// Contact stiffness. Cycles per second. Increasing this increases the speed of overlap recovery, but can introduce jitter.
	float contactHertz;

	/// Contact bounciness. Non-dimensional. You can speed up overlap recovery by decreasing this with
	/// the trade-off that overlap resolution becomes more energetic.
	float contactDampingRatio;

	/// This parameter controls how fast overlap is resolved and usually has units of meters per second. This only
	/// puts a cap on the resolution speed. The resolution speed is increased by increasing the hertz and/or
	/// decreasing the damping ratio.
	float contactSpeed;

	/// Maximum linear speed. Usually meters per second.
	float maximumLinearSpeed;

	/// Optional mixing callback for friction. The default uses sqrt(frictionA * frictionB).
	b2FrictionCallback* frictionCallback;

	/// Optional mixing callback for restitution. The default uses max(restitutionA, restitutionB).
	b2RestitutionCallback* restitutionCallback;

	/// Can bodies go to sleep to improve performance
	bool enableSleep;

	/// Enable continuous collision
	bool enableContinuous;

	/// Number of workers to use with the provided task system. Box2D performs best when using only
	/// performance cores and accessing a single L2 cache. Efficiency cores and hyper-threading provide
	/// little benefit and may even harm performance.
	/// @note Box2D does not create threads. This is the number of threads your applications has created
	/// that you are allocating to b2World_Step.
	/// @warning Do not modify the default value unless you are also providing a task system and providing
	/// task callbacks (enqueueTask and finishTask).
	int workerCount;

	/// Function to spawn tasks
	b2EnqueueTaskCallback* enqueueTask;

	/// Function to finish a task
	b2FinishTaskCallback* finishTask;

	/// User context that is provided to enqueueTask and finishTask
	void* userTaskContext;

	/// User data
	void* userData;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} b2WorldDef;

/// Use this to initialize your world definition
/// @ingroup world
B2_API b2WorldDef b2DefaultWorldDef( void );

/// The body simulation type.
/// Each body is one of these three types. The type determines how the body behaves in the simulation.
/// @ingroup body
typedef enum b2BodyType
{
	/// zero mass, zero velocity, may be manually moved
	b2_staticBody = 0,

	/// zero mass, velocity set by user, moved by solver
	b2_kinematicBody = 1,

	/// positive mass, velocity determined by forces, moved by solver
	b2_dynamicBody = 2,

	/// number of body types
	b2_bodyTypeCount,
} b2BodyType;

/// Motion locks to restrict the body movement
typedef struct b2MotionLocks
{
	/// Prevent translation along the x-axis
	bool linearX;

	/// Prevent translation along the y-axis
	bool linearY;

	/// Prevent rotation around the z-axis
	bool angularZ;
} b2MotionLocks;

/// A body definition holds all the data needed to construct a rigid body.
/// You can safely re-use body definitions. Shapes are added to a body after construction.
/// Body definitions are temporary objects used to bundle creation parameters.
/// Must be initialized using b2DefaultBodyDef().
/// @ingroup body
typedef struct b2BodyDef
{
	/// The body type: static, kinematic, or dynamic.
	b2BodyType type;

	/// The initial world position of the body. Bodies should be created with the desired position.
	/// @note Creating bodies at the origin and then moving them nearly doubles the cost of body creation, especially
	/// if the body is moved after shapes have been added.
	b2Vec2 position;

	/// The initial world rotation of the body. Use b2MakeRot() if you have an angle.
	b2Rot rotation;

	/// The initial linear velocity of the body's origin. Usually in meters per second.
	b2Vec2 linearVelocity;

	/// The initial angular velocity of the body. Radians per second.
	float angularVelocity;

	/// Linear damping is used to reduce the linear velocity. The damping parameter
	/// can be larger than 1 but the damping effect becomes sensitive to the
	/// time step when the damping parameter is large.
	/// Generally linear damping is undesirable because it makes objects move slowly
	/// as if they are floating.
	float linearDamping;

	/// Angular damping is used to reduce the angular velocity. The damping parameter
	/// can be larger than 1.0f but the damping effect becomes sensitive to the
	/// time step when the damping parameter is large.
	/// Angular damping can be use slow down rotating bodies.
	float angularDamping;

	/// Scale the gravity applied to this body. Non-dimensional.
	float gravityScale;

	/// Sleep speed threshold, default is 0.05 meters per second
	float sleepThreshold;

	/// Optional body name for debugging. Up to 31 characters (excluding null termination)
	const char* name;

	/// Use this to store application specific body data.
	void* userData;

	/// Motions locks to restrict linear and angular movement.
	/// Caution: may lead to softer constraints along the locked direction
	b2MotionLocks motionLocks;

	/// Set this flag to false if this body should never fall asleep.
	bool enableSleep;

	/// Is this body initially awake or sleeping?
	bool isAwake;

	/// Treat this body as high speed object that performs continuous collision detection
	/// against dynamic and kinematic bodies, but not other bullet bodies.
	/// @warning Bullets should be used sparingly. They are not a solution for general dynamic-versus-dynamic
	/// continuous collision.
	bool isBullet;

	/// Used to disable a body. A disabled body does not move or collide.
	bool isEnabled;

	/// This allows this body to bypass rotational speed limits. Should only be used
	/// for circular objects, like wheels.
	bool allowFastRotation;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} b2BodyDef;

/// Use this to initialize your body definition
/// @ingroup body
B2_API b2BodyDef b2DefaultBodyDef( void );

/// This is used to filter collision on shapes. It affects shape-vs-shape collision
/// and shape-versus-query collision (such as b2World_CastRay).
/// @ingroup shape
typedef struct b2Filter
{
	/// The collision category bits. Normally you would just set one bit. The category bits should
	/// represent your application object types. For example:
	/// @code{.cpp}
	/// enum MyCategories
	/// {
	///    Static  = 0x00000001,
	///    Dynamic = 0x00000002,
	///    Debris  = 0x00000004,
	///    Player  = 0x00000008,
	///    // etc
	/// };
	/// @endcode
	uint64_t categoryBits;

	/// The collision mask bits. This states the categories that this
	/// shape would accept for collision.
	/// For example, you may want your player to only collide with static objects
	/// and other players.
	/// @code{.c}
	/// maskBits = Static | Player;
	/// @endcode
	uint64_t maskBits;

	/// Collision groups allow a certain group of objects to never collide (negative)
	/// or always collide (positive). A group index of zero has no effect. Non-zero group filtering
	/// always wins against the mask bits.
	/// For example, you may want ragdolls to collide with other ragdolls but you don't want
	/// ragdoll self-collision. In this case you would give each ragdoll a unique negative group index
	/// and apply that group index to all shapes on the ragdoll.
	int groupIndex;
} b2Filter;

/// Use this to initialize your filter
/// @ingroup shape
B2_API b2Filter b2DefaultFilter( void );

/// The query filter is used to filter collisions between queries and shapes. For example,
/// you may want a ray-cast representing a projectile to hit players and the static environment
/// but not debris.
/// @ingroup shape
typedef struct b2QueryFilter
{
	/// The collision category bits of this query. Normally you would just set one bit.
	uint64_t categoryBits;

	/// The collision mask bits. This states the shape categories that this
	/// query would accept for collision.
	uint64_t maskBits;
} b2QueryFilter;

/// Use this to initialize your query filter
/// @ingroup shape
B2_API b2QueryFilter b2DefaultQueryFilter( void );

/// Shape type
/// @ingroup shape
typedef enum b2ShapeType
{
	/// A circle with an offset
	b2_circleShape,

	/// A capsule is an extruded circle
	b2_capsuleShape,

	/// A line segment
	b2_segmentShape,

	/// A convex polygon
	b2_polygonShape,

	/// A line segment owned by a chain shape
	b2_chainSegmentShape,

	/// The number of shape types
	b2_shapeTypeCount
} b2ShapeType;

/// Surface materials allow chain shapes to have per segment surface properties.
/// @ingroup shape
typedef struct b2SurfaceMaterial
{
	/// The Coulomb (dry) friction coefficient, usually in the range [0,1].
	float friction;

	/// The coefficient of restitution (bounce) usually in the range [0,1].
	/// https://en.wikipedia.org/wiki/Coefficient_of_restitution
	float restitution;

	/// The rolling resistance usually in the range [0,1].
	float rollingResistance;

	/// The tangent speed for conveyor belts
	float tangentSpeed;

	/// User material identifier. This is passed with query results and to friction and restitution
	/// combining functions. It is not used internally.
	int userMaterialId;

	/// Custom debug draw color.
	uint32_t customColor;
} b2SurfaceMaterial;

/// Use this to initialize your surface material
/// @ingroup shape
B2_API b2SurfaceMaterial b2DefaultSurfaceMaterial( void );

/// Used to create a shape.
/// This is a temporary object used to bundle shape creation parameters. You may use
/// the same shape definition to create multiple shapes.
/// Must be initialized using b2DefaultShapeDef().
/// @ingroup shape
typedef struct b2ShapeDef
{
	/// Use this to store application specific shape data.
	void* userData;

	/// The surface material for this shape.
	b2SurfaceMaterial material;

	/// The density, usually in kg/m^2.
	/// This is not part of the surface material because this is for the interior, which may have
	/// other considerations, such as being hollow. For example a wood barrel may be hollow or full of water.
	float density;

	/// Collision filtering data.
	b2Filter filter;

	/// Enable custom filtering. Only one of the two shapes needs to enable custom filtering. See b2WorldDef.
	bool enableCustomFiltering;

	/// A sensor shape generates overlap events but never generates a collision response.
	/// Sensors do not have continuous collision. Instead, use a ray or shape cast for those scenarios.
	/// Sensors still contribute to the body mass if they have non-zero density.
	/// @note Sensor events are disabled by default.
	/// @see enableSensorEvents
	bool isSensor;

	/// Enable sensor events for this shape. This applies to sensors and non-sensors. False by default, even for sensors.
	bool enableSensorEvents;

	/// Enable contact events for this shape. Only applies to kinematic and dynamic bodies. Ignored for sensors. False by default.
	bool enableContactEvents;

	/// Enable hit events for this shape. Only applies to kinematic and dynamic bodies. Ignored for sensors. False by default.
	bool enableHitEvents;

	/// Enable pre-solve contact events for this shape. Only applies to dynamic bodies. These are expensive
	/// and must be carefully handled due to multithreading. Ignored for sensors.
	bool enablePreSolveEvents;

	/// When shapes are created they will scan the environment for collision the next time step. This can significantly slow down
	/// static body creation when there are many static shapes.
	/// This is flag is ignored for dynamic and kinematic shapes which always invoke contact creation.
	bool invokeContactCreation;

	/// Should the body update the mass properties when this shape is created. Default is true.
	bool updateBodyMass;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} b2ShapeDef;

/// Use this to initialize your shape definition
/// @ingroup shape
B2_API b2ShapeDef b2DefaultShapeDef( void );

/// Used to create a chain of line segments. This is designed to eliminate ghost collisions with some limitations.
/// - chains are one-sided
/// - chains have no mass and should be used on static bodies
/// - chains have a counter-clockwise winding order (normal points right of segment direction)
/// - chains are either a loop or open
/// - a chain must have at least 4 points
/// - the distance between any two points must be greater than B2_LINEAR_SLOP
/// - a chain shape should not self intersect (this is not validated)
/// - an open chain shape has NO COLLISION on the first and final edge
/// - you may overlap two open chains on their first three and/or last three points to get smooth collision
/// - a chain shape creates multiple line segment shapes on the body
/// https://en.wikipedia.org/wiki/Polygonal_chain
/// Must be initialized using b2DefaultChainDef().
/// @warning Do not use chain shapes unless you understand the limitations. This is an advanced feature.
/// @ingroup shape
typedef struct b2ChainDef
{
	/// Use this to store application specific shape data.
	void* userData;

	/// An array of at least 4 points. These are cloned and may be temporary.
	const b2Vec2* points;

	/// The point count, must be 4 or more.
	int count;

	/// Surface materials for each segment. These are cloned.
	const b2SurfaceMaterial* materials;

	/// The material count. Must be 1 or count. This allows you to provide one
	/// material for all segments or a unique material per segment.
	int materialCount;

	/// Contact filtering data.
	b2Filter filter;

	/// Indicates a closed chain formed by connecting the first and last points
	bool isLoop;

	/// Enable sensors to detect this chain. False by default.
	bool enableSensorEvents;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} b2ChainDef;

/// Use this to initialize your chain definition
/// @ingroup shape
B2_API b2ChainDef b2DefaultChainDef( void );

//! @cond
/// Profiling data. Times are in milliseconds.
typedef struct b2Profile
{
	float step;
	float pairs;
	float collide;
	float solve;
	float prepareStages;
	float solveConstraints;
	float prepareConstraints;
	float integrateVelocities;
	float warmStart;
	float solveImpulses;
	float integratePositions;
	float relaxImpulses;
	float applyRestitution;
	float storeImpulses;
	float splitIslands;
	float transforms;
	float sensorHits;
	float jointEvents;
	float hitEvents;
	float refit;
	float bullets;
	float sleepIslands;
	float sensors;
} b2Profile;

/// Counters that give details of the simulation size.
typedef struct b2Counters
{
	int bodyCount;
	int shapeCount;
	int contactCount;
	int jointCount;
	int islandCount;
	int stackUsed;
	int staticTreeHeight;
	int treeHeight;
	int byteCount;
	int taskCount;
	int colorCounts[24];
} b2Counters;
//! @endcond

/// Joint type enumeration
///
/// This is useful because all joint types use b2JointId and sometimes you
/// want to get the type of a joint.
/// @ingroup joint
typedef enum b2JointType
{
	b2_distanceJoint,
	b2_filterJoint,
	b2_motorJoint,
	b2_mouseJoint,
	b2_prismaticJoint,
	b2_revoluteJoint,
	b2_weldJoint,
	b2_wheelJoint,
} b2JointType;

/// Base joint definition used by all joint types.
/// The local frames are measured from the body's origin rather than the center of mass because:
/// 1. you might not know where the center of mass will be
/// 2. if you add/remove shapes from a body and recompute the mass, the joints will be broken
typedef struct b2JointDef
{
	/// User data pointer
	void* userData;

	/// The first attached body
	b2BodyId bodyIdA;

	/// The second attached body
	b2BodyId bodyIdB;

	/// The first local joint frame
	b2Transform localFrameA;

	/// The second local joint frame
	b2Transform localFrameB;

	/// Force threshold for joint events
	float forceThreshold;

	/// Torque threshold for joint events
	float torqueThreshold;

	/// Constraint hertz (advanced feature)
	float constraintHertz;

	/// Constraint damping ratio (advanced feature)
	float constraintDampingRatio;

	/// Debug draw scale
	float drawScale;

	/// Set this flag to true if the attached bodies should collide
	bool collideConnected;

} b2JointDef;

/// Distance joint definition
/// Connects a point on body A with a point on body B by a segment.
/// Useful for ropes and springs.
/// @ingroup distance_joint
typedef struct b2DistanceJointDef
{
	/// Base joint definition
	b2JointDef base;

	/// The rest length of this joint. Clamped to a stable minimum value.
	float length;

	/// Enable the distance constraint to behave like a spring. If false
	/// then the distance joint will be rigid, overriding the limit and motor.
	bool enableSpring;

	/// The lower spring force controls how much tension it can sustain
	float lowerSpringForce;

	/// The upper spring force controls how much compression it an sustain
	float upperSpringForce;

	/// The spring linear stiffness Hertz, cycles per second
	float hertz;

	/// The spring linear damping ratio, non-dimensional
	float dampingRatio;

	/// Enable/disable the joint limit
	bool enableLimit;

	/// Minimum length. Clamped to a stable minimum value.
	float minLength;

	/// Maximum length. Must be greater than or equal to the minimum length.
	float maxLength;

	/// Enable/disable the joint motor
	bool enableMotor;

	/// The maximum motor force, usually in newtons
	float maxMotorForce;

	/// The desired motor speed, usually in meters per second
	float motorSpeed;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} b2DistanceJointDef;

/// Use this to initialize your joint definition
/// @ingroup distance_joint
B2_API b2DistanceJointDef b2DefaultDistanceJointDef( void );

/// A motor joint is used to control the relative velocity and or transform between two bodies.
/// With a velocity of zero this acts like top-down friction.
/// @ingroup motor_joint
typedef struct b2MotorJointDef
{
	/// Base joint definition
	b2JointDef base;

	/// The desired linear velocity
	b2Vec2 linearVelocity;

	/// The maximum motor force in newtons
	float maxVelocityForce;

	/// The desired angular velocity
	float angularVelocity;

	/// The maximum motor torque in newton-meters
	float maxVelocityTorque;

	/// Linear spring hertz for position control
	float linearHertz;

	/// Linear spring damping ratio
	float linearDampingRatio;

	/// Maximum spring force in newtons
	float maxSpringForce;

	/// Angular spring hertz for position control
	float angularHertz;

	/// Angular spring damping ratio
	float angularDampingRatio;

	/// Maximum spring torque in newton-meters
	float maxSpringTorque;

	/// The desired relative transform. Body B relative to bodyA.
	b2Transform relativeTransform;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} b2MotorJointDef;

/// Use this to initialize your joint definition
/// @ingroup motor_joint
B2_API b2MotorJointDef b2DefaultMotorJointDef( void );

/// A mouse joint is used to make a point on body B track a point on body A.
/// You may move local frame A to change the target point.
/// This a soft constraint and allows the constraint to stretch without
/// applying huge forces. This also applies rotation constraint heuristic to improve control.
/// @ingroup mouse_joint
typedef struct b2MouseJointDef
{
	/// Base joint definition
	b2JointDef base;

	/// Stiffness in hertz
	float hertz;

	/// Damping ratio, non-dimensional
	float dampingRatio;

	/// Maximum force, typically in newtons
	float maxForce;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} b2MouseJointDef;

/// Use this to initialize your joint definition
/// @ingroup mouse_joint
B2_API b2MouseJointDef b2DefaultMouseJointDef( void );

/// A filter joint is used to disable collision between two specific bodies.
///
/// @ingroup filter_joint
typedef struct b2FilterJointDef
{
	/// Base joint definition
	b2JointDef base;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} b2FilterJointDef;

/// Use this to initialize your joint definition
/// @ingroup filter_joint
B2_API b2FilterJointDef b2DefaultFilterJointDef( void );

/// Prismatic joint definition
/// Body B may slide along the x-axis in local frame A. Body B cannot rotate relative to body A.
/// The joint translation is zero when the local frame origins coincide in world space.
/// @ingroup prismatic_joint
typedef struct b2PrismaticJointDef
{
	/// Base joint definition
	b2JointDef base;

	/// Enable a linear spring along the prismatic joint axis
	bool enableSpring;

	/// The spring stiffness Hertz, cycles per second
	float hertz;

	/// The spring damping ratio, non-dimensional
	float dampingRatio;

	/// The target translation for the joint in meters. The spring-damper will drive
	/// to this translation.
	float targetTranslation;

	/// Enable/disable the joint limit
	bool enableLimit;

	/// The lower translation limit
	float lowerTranslation;

	/// The upper translation limit
	float upperTranslation;

	/// Enable/disable the joint motor
	bool enableMotor;

	/// The maximum motor force, typically in newtons
	float maxMotorForce;

	/// The desired motor speed, typically in meters per second
	float motorSpeed;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} b2PrismaticJointDef;

/// Use this to initialize your joint definition
/// @ingroupd prismatic_joint
B2_API b2PrismaticJointDef b2DefaultPrismaticJointDef( void );

/// Revolute joint definition
/// A point on body B is fixed to a point on body A. Allows relative rotation.
/// @ingroup revolute_joint
typedef struct b2RevoluteJointDef
{
	/// Base joint definition
	b2JointDef base;

	/// The target angle for the joint in radians. The spring-damper will drive
	/// to this angle.
	float targetAngle;

	/// Enable a rotational spring on the revolute hinge axis
	bool enableSpring;

	/// The spring stiffness Hertz, cycles per second
	float hertz;

	/// The spring damping ratio, non-dimensional
	float dampingRatio;

	/// A flag to enable joint limits
	bool enableLimit;

	/// The lower angle for the joint limit in radians. Minimum of -0.99*pi radians.
	float lowerAngle;

	/// The upper angle for the joint limit in radians. Maximum of 0.99*pi radians.
	float upperAngle;

	/// A flag to enable the joint motor
	bool enableMotor;

	/// The maximum motor torque, typically in newton-meters
	float maxMotorTorque;

	/// The desired motor speed in radians per second
	float motorSpeed;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} b2RevoluteJointDef;

/// Use this to initialize your joint definition.
/// @ingroup revolute_joint
B2_API b2RevoluteJointDef b2DefaultRevoluteJointDef( void );

/// Weld joint definition
/// Connects two bodies together rigidly. This constraint provides springs to mimic
/// soft-body simulation.
/// @note The approximate solver in Box2D cannot hold many bodies together rigidly
/// @ingroup weld_joint
typedef struct b2WeldJointDef
{
	/// Base joint definition
	b2JointDef base;

	/// Linear stiffness expressed as Hertz (cycles per second). Use zero for maximum stiffness.
	float linearHertz;

	/// Angular stiffness as Hertz (cycles per second). Use zero for maximum stiffness.
	float angularHertz;

	/// Linear damping ratio, non-dimensional. Use 1 for critical damping.
	float linearDampingRatio;

	/// Linear damping ratio, non-dimensional. Use 1 for critical damping.
	float angularDampingRatio;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} b2WeldJointDef;

/// Use this to initialize your joint definition
/// @ingroup weld_joint
B2_API b2WeldJointDef b2DefaultWeldJointDef( void );

/// Wheel joint definition
/// Body B is a wheel that may rotate freely and slide along the local x-axis in frame A.
/// The joint translation is zero when the local frame origins coincide in world space.
/// @ingroup wheel_joint
typedef struct b2WheelJointDef
{
	/// Base joint definition
	b2JointDef base;

	/// Enable a linear spring along the local axis
	bool enableSpring;

	/// Spring stiffness in Hertz
	float hertz;

	/// Spring damping ratio, non-dimensional
	float dampingRatio;

	/// Enable/disable the joint linear limit
	bool enableLimit;

	/// The lower translation limit
	float lowerTranslation;

	/// The upper translation limit
	float upperTranslation;

	/// Enable/disable the joint rotational motor
	bool enableMotor;

	/// The maximum motor torque, typically in newton-meters
	float maxMotorTorque;

	/// The desired motor speed in radians per second
	float motorSpeed;

	/// Used internally to detect a valid definition. DO NOT SET.
	int internalValue;
} b2WheelJointDef;

/// Use this to initialize your joint definition
/// @ingroup wheel_joint
B2_API b2WheelJointDef b2DefaultWheelJointDef( void );

/// The explosion definition is used to configure options for explosions. Explosions
/// consider shape geometry when computing the impulse.
/// @ingroup world
typedef struct b2ExplosionDef
{
	/// Mask bits to filter shapes
	uint64_t maskBits;

	/// The center of the explosion in world space
	b2Vec2 position;

	/// The radius of the explosion
	float radius;

	/// The falloff distance beyond the radius. Impulse is reduced to zero at this distance.
	float falloff;

	/// Impulse per unit length. This applies an impulse according to the shape perimeter that
	/// is facing the explosion. Explosions only apply to circles, capsules, and polygons. This
	/// may be negative for implosions.
	float impulsePerLength;
} b2ExplosionDef;

/// Use this to initialize your explosion definition
/// @ingroup world
B2_API b2ExplosionDef b2DefaultExplosionDef( void );

/**
 * @defgroup events Events
 * World event types.
 *
 * Events are used to collect events that occur during the world time step. These events
 * are then available to query after the time step is complete. This is preferable to callbacks
 * because Box2D uses multithreaded simulation.
 *
 * Also when events occur in the simulation step it may be problematic to modify the world, which is
 * often what applications want to do when events occur.
 *
 * With event arrays, you can scan the events in a loop and modify the world. However, you need to be careful
 * that some event data may become invalid. There are several samples that show how to do this safely.
 *
 * @{
 */

/// A begin touch event is generated when a shape starts to overlap a sensor shape.
typedef struct b2SensorBeginTouchEvent
{
	/// The id of the sensor shape
	b2ShapeId sensorShapeId;

	/// The id of the shape that began touching the sensor shape
	b2ShapeId visitorShapeId;
} b2SensorBeginTouchEvent;

/// An end touch event is generated when a shape stops overlapping a sensor shape.
///	These include things like setting the transform, destroying a body or shape, or changing
///	a filter. You will also get an end event if the sensor or visitor are destroyed.
///	Therefore you should always confirm the shape id is valid using b2Shape_IsValid.
typedef struct b2SensorEndTouchEvent
{
	/// The id of the sensor shape
	///	@warning this shape may have been destroyed
	///	@see b2Shape_IsValid
	b2ShapeId sensorShapeId;

	/// The id of the shape that stopped touching the sensor shape
	///	@warning this shape may have been destroyed
	///	@see b2Shape_IsValid
	b2ShapeId visitorShapeId;

} b2SensorEndTouchEvent;

/// Sensor events are buffered in the world and are available
/// as begin/end overlap event arrays after the time step is complete.
/// Note: these may become invalid if bodies and/or shapes are destroyed
typedef struct b2SensorEvents
{
	/// Array of sensor begin touch events
	b2SensorBeginTouchEvent* beginEvents;

	/// Array of sensor end touch events
	b2SensorEndTouchEvent* endEvents;

	/// The number of begin touch events
	int beginCount;

	/// The number of end touch events
	int endCount;
} b2SensorEvents;

/// A begin touch event is generated when two shapes begin touching.
typedef struct b2ContactBeginTouchEvent
{
	/// Id of the first shape
	b2ShapeId shapeIdA;

	/// Id of the second shape
	b2ShapeId shapeIdB;

	/// The transient contact id. This contact maybe destroyed automatically when the world is modified or simulated.
	/// Used b2Contact_IsValid before using this id.
	b2ContactId contactId;
} b2ContactBeginTouchEvent;

/// An end touch event is generated when two shapes stop touching.
///	You will get an end event if you do anything that destroys contacts previous to the last
///	world step. These include things like setting the transform, destroying a body
///	or shape, or changing a filter or body type.
typedef struct b2ContactEndTouchEvent
{
	/// Id of the first shape
	///	@warning this shape may have been destroyed
	///	@see b2Shape_IsValid
	b2ShapeId shapeIdA;

	/// Id of the second shape
	///	@warning this shape may have been destroyed
	///	@see b2Shape_IsValid
	b2ShapeId shapeIdB;

	/// Id of the contact.
	///	@warning this contact may have been destroyed
	///	@see b2Contact_IsValid
	b2ContactId contactId;
} b2ContactEndTouchEvent;

/// A hit touch event is generated when two shapes collide with a speed faster than the hit speed threshold.
/// This may be reported for speculative contacts that have a confirmed impulse.
typedef struct b2ContactHitEvent
{
	/// Id of the first shape
	b2ShapeId shapeIdA;

	/// Id of the second shape
	b2ShapeId shapeIdB;

	/// Point where the shapes hit at the beginning of the time step.
	/// This is a mid-point between the two surfaces. It could be at speculative
	/// point where the two shapes were not touching at the beginning of the time step.
	b2Vec2 point;

	/// Normal vector pointing from shape A to shape B
	b2Vec2 normal;

	/// The speed the shapes are approaching. Always positive. Typically in meters per second.
	float approachSpeed;
} b2ContactHitEvent;

/// Contact events are buffered in the Box2D world and are available
/// as event arrays after the time step is complete.
/// Note: these may become invalid if bodies and/or shapes are destroyed
typedef struct b2ContactEvents
{
	/// Array of begin touch events
	b2ContactBeginTouchEvent* beginEvents;

	/// Array of end touch events
	b2ContactEndTouchEvent* endEvents;

	/// Array of hit events
	b2ContactHitEvent* hitEvents;

	/// Number of begin touch events
	int beginCount;

	/// Number of end touch events
	int endCount;

	/// Number of hit events
	int hitCount;
} b2ContactEvents;

/// Body move events triggered when a body moves.
/// Triggered when a body moves due to simulation. Not reported for bodies moved by the user.
/// This also has a flag to indicate that the body went to sleep so the application can also
/// sleep that actor/entity/object associated with the body.
/// On the other hand if the flag does not indicate the body went to sleep then the application
/// can treat the actor/entity/object associated with the body as awake.
/// This is an efficient way for an application to update game object transforms rather than
/// calling functions such as b2Body_GetTransform() because this data is delivered as a contiguous array
/// and it is only populated with bodies that have moved.
/// @note If sleeping is disabled all dynamic and kinematic bodies will trigger move events.
typedef struct b2BodyMoveEvent
{
	void* userData;
	b2Transform transform;
	b2BodyId bodyId;
	bool fellAsleep;
} b2BodyMoveEvent;

/// Body events are buffered in the Box2D world and are available
/// as event arrays after the time step is complete.
/// Note: this data becomes invalid if bodies are destroyed
typedef struct b2BodyEvents
{
	/// Array of move events
	b2BodyMoveEvent* moveEvents;

	/// Number of move events
	int moveCount;
} b2BodyEvents;

/// Joint events report joints that are awake and have a force and/or torque exceeding the threshold
/// The observed forces and torques are not returned for efficiency reasons.
typedef struct b2JointEvent
{
	/// The joint id
	b2JointId jointId;

	/// The user data from the joint for convenience
	void* userData;
} b2JointEvent;

/// Joint events are buffered in the world and are available
/// as event arrays after the time step is complete.
/// Note: this data becomes invalid if joints are destroyed
typedef struct b2JointEvents
{
	/// Array of events
	b2JointEvent* jointEvents;

	/// Number of events
	int count;
} b2JointEvents;

/// The contact data for two shapes. By convention the manifold normal points
/// from shape A to shape B.
/// @see b2Shape_GetContactData() and b2Body_GetContactData()
typedef struct b2ContactData
{
	b2ContactId contactId;
	b2ShapeId shapeIdA;
	b2ShapeId shapeIdB;
	b2Manifold manifold;
} b2ContactData;

/**@}*/

/// Prototype for a contact filter callback.
/// This is called when a contact pair is considered for collision. This allows you to
/// perform custom logic to prevent collision between shapes. This is only called if
/// one of the two shapes has custom filtering enabled.
/// Notes:
/// - this function must be thread-safe
/// - this is only called if one of the two shapes has enabled custom filtering
/// - this may be called for awake dynamic bodies and sensors
/// Return false if you want to disable the collision
/// @see b2ShapeDef
/// @warning Do not attempt to modify the world inside this callback
/// @ingroup world
typedef bool b2CustomFilterFcn( b2ShapeId shapeIdA, b2ShapeId shapeIdB, void* context );

/// Prototype for a pre-solve callback.
/// This is called after a contact is updated. This allows you to inspect a
/// contact before it goes to the solver. If you are careful, you can modify the
/// contact manifold (e.g. modify the normal).
/// Notes:
/// - this function must be thread-safe
/// - this is only called if the shape has enabled pre-solve events
/// - this is called only for awake dynamic bodies
/// - this is not called for sensors
/// - the supplied manifold has impulse values from the previous step
/// Return false if you want to disable the contact this step
/// @warning Do not attempt to modify the world inside this callback
/// @ingroup world
typedef bool b2PreSolveFcn( b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2Vec2 point, b2Vec2 normal, void* context );

/// Prototype callback for overlap queries.
/// Called for each shape found in the query.
/// @see b2World_OverlapABB
/// @return false to terminate the query.
/// @ingroup world
typedef bool b2OverlapResultFcn( b2ShapeId shapeId, void* context );

/// Prototype callback for ray and shape casts.
/// Called for each shape found in the query. You control how the ray cast
/// proceeds by returning a float:
/// return -1: ignore this shape and continue
/// return 0: terminate the ray cast
/// return fraction: clip the ray to this point
/// return 1: don't clip the ray and continue
/// A cast with initial overlap will return a zero fraction and a zero normal.
/// @param shapeId the shape hit by the ray
/// @param point the point of initial intersection
/// @param normal the normal vector at the point of intersection, zero for a shape cast with initial overlap
/// @param fraction the fraction along the ray at the point of intersection, zero for a shape cast with initial overlap
/// @param context the user context
/// @return -1 to filter, 0 to terminate, fraction to clip the ray for closest hit, 1 to continue
/// @see b2World_CastRay
/// @ingroup world
typedef float b2CastResultFcn( b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context );

// Used to collect collision planes for character movers.
// Return true to continue gathering planes.
typedef bool b2PlaneResultFcn( b2ShapeId shapeId, const b2PlaneResult* plane, void* context );

/// These colors are used for debug draw and mostly match the named SVG colors.
/// See https://www.rapidtables.com/web/color/index.html
/// https://johndecember.com/html/spec/colorsvg.html
/// https://upload.wikimedia.org/wikipedia/commons/2/2b/SVG_Recognized_color_keyword_names.svg
typedef enum b2HexColor
{
	b2_colorAliceBlue = 0xF0F8FF,
	b2_colorAntiqueWhite = 0xFAEBD7,
	b2_colorAqua = 0x00FFFF,
	b2_colorAquamarine = 0x7FFFD4,
	b2_colorAzure = 0xF0FFFF,
	b2_colorBeige = 0xF5F5DC,
	b2_colorBisque = 0xFFE4C4,
	b2_colorBlack = 0x000000,
	b2_colorBlanchedAlmond = 0xFFEBCD,
	b2_colorBlue = 0x0000FF,
	b2_colorBlueViolet = 0x8A2BE2,
	b2_colorBrown = 0xA52A2A,
	b2_colorBurlywood = 0xDEB887,
	b2_colorCadetBlue = 0x5F9EA0,
	b2_colorChartreuse = 0x7FFF00,
	b2_colorChocolate = 0xD2691E,
	b2_colorCoral = 0xFF7F50,
	b2_colorCornflowerBlue = 0x6495ED,
	b2_colorCornsilk = 0xFFF8DC,
	b2_colorCrimson = 0xDC143C,
	b2_colorCyan = 0x00FFFF,
	b2_colorDarkBlue = 0x00008B,
	b2_colorDarkCyan = 0x008B8B,
	b2_colorDarkGoldenRod = 0xB8860B,
	b2_colorDarkGray = 0xA9A9A9,
	b2_colorDarkGreen = 0x006400,
	b2_colorDarkKhaki = 0xBDB76B,
	b2_colorDarkMagenta = 0x8B008B,
	b2_colorDarkOliveGreen = 0x556B2F,
	b2_colorDarkOrange = 0xFF8C00,
	b2_colorDarkOrchid = 0x9932CC,
	b2_colorDarkRed = 0x8B0000,
	b2_colorDarkSalmon = 0xE9967A,
	b2_colorDarkSeaGreen = 0x8FBC8F,
	b2_colorDarkSlateBlue = 0x483D8B,
	b2_colorDarkSlateGray = 0x2F4F4F,
	b2_colorDarkTurquoise = 0x00CED1,
	b2_colorDarkViolet = 0x9400D3,
	b2_colorDeepPink = 0xFF1493,
	b2_colorDeepSkyBlue = 0x00BFFF,
	b2_colorDimGray = 0x696969,
	b2_colorDodgerBlue = 0x1E90FF,
	b2_colorFireBrick = 0xB22222,
	b2_colorFloralWhite = 0xFFFAF0,
	b2_colorForestGreen = 0x228B22,
	b2_colorFuchsia = 0xFF00FF,
	b2_colorGainsboro = 0xDCDCDC,
	b2_colorGhostWhite = 0xF8F8FF,
	b2_colorGold = 0xFFD700,
	b2_colorGoldenRod = 0xDAA520,
	b2_colorGray = 0x808080,
	b2_colorGreen = 0x008000,
	b2_colorGreenYellow = 0xADFF2F,
	b2_colorHoneyDew = 0xF0FFF0,
	b2_colorHotPink = 0xFF69B4,
	b2_colorIndianRed = 0xCD5C5C,
	b2_colorIndigo = 0x4B0082,
	b2_colorIvory = 0xFFFFF0,
	b2_colorKhaki = 0xF0E68C,
	b2_colorLavender = 0xE6E6FA,
	b2_colorLavenderBlush = 0xFFF0F5,
	b2_colorLawnGreen = 0x7CFC00,
	b2_colorLemonChiffon = 0xFFFACD,
	b2_colorLightBlue = 0xADD8E6,
	b2_colorLightCoral = 0xF08080,
	b2_colorLightCyan = 0xE0FFFF,
	b2_colorLightGoldenRodYellow = 0xFAFAD2,
	b2_colorLightGray = 0xD3D3D3,
	b2_colorLightGreen = 0x90EE90,
	b2_colorLightPink = 0xFFB6C1,
	b2_colorLightSalmon = 0xFFA07A,
	b2_colorLightSeaGreen = 0x20B2AA,
	b2_colorLightSkyBlue = 0x87CEFA,
	b2_colorLightSlateGray = 0x778899,
	b2_colorLightSteelBlue = 0xB0C4DE,
	b2_colorLightYellow = 0xFFFFE0,
	b2_colorLime = 0x00FF00,
	b2_colorLimeGreen = 0x32CD32,
	b2_colorLinen = 0xFAF0E6,
	b2_colorMagenta = 0xFF00FF,
	b2_colorMaroon = 0x800000,
	b2_colorMediumAquaMarine = 0x66CDAA,
	b2_colorMediumBlue = 0x0000CD,
	b2_colorMediumOrchid = 0xBA55D3,
	b2_colorMediumPurple = 0x9370DB,
	b2_colorMediumSeaGreen = 0x3CB371,
	b2_colorMediumSlateBlue = 0x7B68EE,
	b2_colorMediumSpringGreen = 0x00FA9A,
	b2_colorMediumTurquoise = 0x48D1CC,
	b2_colorMediumVioletRed = 0xC71585,
	b2_colorMidnightBlue = 0x191970,
	b2_colorMintCream = 0xF5FFFA,
	b2_colorMistyRose = 0xFFE4E1,
	b2_colorMoccasin = 0xFFE4B5,
	b2_colorNavajoWhite = 0xFFDEAD,
	b2_colorNavy = 0x000080,
	b2_colorOldLace = 0xFDF5E6,
	b2_colorOlive = 0x808000,
	b2_colorOliveDrab = 0x6B8E23,
	b2_colorOrange = 0xFFA500,
	b2_colorOrangeRed = 0xFF4500,
	b2_colorOrchid = 0xDA70D6,
	b2_colorPaleGoldenRod = 0xEEE8AA,
	b2_colorPaleGreen = 0x98FB98,
	b2_colorPaleTurquoise = 0xAFEEEE,
	b2_colorPaleVioletRed = 0xDB7093,
	b2_colorPapayaWhip = 0xFFEFD5,
	b2_colorPeachPuff = 0xFFDAB9,
	b2_colorPeru = 0xCD853F,
	b2_colorPink = 0xFFC0CB,
	b2_colorPlum = 0xDDA0DD,
	b2_colorPowderBlue = 0xB0E0E6,
	b2_colorPurple = 0x800080,
	b2_colorRebeccaPurple = 0x663399,
	b2_colorRed = 0xFF0000,
	b2_colorRosyBrown = 0xBC8F8F,
	b2_colorRoyalBlue = 0x4169E1,
	b2_colorSaddleBrown = 0x8B4513,
	b2_colorSalmon = 0xFA8072,
	b2_colorSandyBrown = 0xF4A460,
	b2_colorSeaGreen = 0x2E8B57,
	b2_colorSeaShell = 0xFFF5EE,
	b2_colorSienna = 0xA0522D,
	b2_colorSilver = 0xC0C0C0,
	b2_colorSkyBlue = 0x87CEEB,
	b2_colorSlateBlue = 0x6A5ACD,
	b2_colorSlateGray = 0x708090,
	b2_colorSnow = 0xFFFAFA,
	b2_colorSpringGreen = 0x00FF7F,
	b2_colorSteelBlue = 0x4682B4,
	b2_colorTan = 0xD2B48C,
	b2_colorTeal = 0x008080,
	b2_colorThistle = 0xD8BFD8,
	b2_colorTomato = 0xFF6347,
	b2_colorTurquoise = 0x40E0D0,
	b2_colorViolet = 0xEE82EE,
	b2_colorWheat = 0xF5DEB3,
	b2_colorWhite = 0xFFFFFF,
	b2_colorWhiteSmoke = 0xF5F5F5,
	b2_colorYellow = 0xFFFF00,
	b2_colorYellowGreen = 0x9ACD32,

	b2_colorBox2DRed = 0xDC3132,
	b2_colorBox2DBlue = 0x30AEBF,
	b2_colorBox2DGreen = 0x8CC924,
	b2_colorBox2DYellow = 0xFFEE8C
} b2HexColor;

/// This struct holds callbacks you can implement to draw a Box2D world.
/// This structure should be zero initialized.
/// @ingroup world
typedef struct b2DebugDraw
{
	/// Draw a closed polygon provided in CCW order.
	void ( *DrawPolygonFcn )( const b2Vec2* vertices, int vertexCount, b2HexColor color, void* context );

	/// Draw a solid closed polygon provided in CCW order.
	void ( *DrawSolidPolygonFcn )( b2Transform transform, const b2Vec2* vertices, int vertexCount, float radius, b2HexColor color,
								void* context );

	/// Draw a circle.
	void ( *DrawCircleFcn )( b2Vec2 center, float radius, b2HexColor color, void* context );

	/// Draw a solid circle.
	void ( *DrawSolidCircleFcn )( b2Transform transform, float radius, b2HexColor color, void* context );

	/// Draw a solid capsule.
	void ( *DrawSolidCapsuleFcn )( b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context );

	/// Draw a line segment.
	void ( *DrawSegmentFcn )( b2Vec2 p1, b2Vec2 p2, b2HexColor color, void* context );

	/// Draw a transform. Choose your own length scale.
	void ( *DrawTransformFcn )( b2Transform transform, void* context );

	/// Draw a point.
	void ( *DrawPointFcn )( b2Vec2 p, float size, b2HexColor color, void* context );

	/// Draw a string in world space
	void ( *DrawStringFcn )( b2Vec2 p, const char* s, b2HexColor color, void* context );

	/// Bounds to use if restricting drawing to a rectangular region
	b2AABB drawingBounds;

	/// Option to draw shapes
	bool drawShapes;

	/// Option to draw joints
	bool drawJoints;

	/// Option to draw additional information for joints
	bool drawJointExtras;

	/// Option to draw the bounding boxes for shapes
	bool drawBounds;

	/// Option to draw the mass and center of mass of dynamic bodies
	bool drawMass;

	/// Option to draw body names
	bool drawBodyNames;

	/// Option to draw contact points
	bool drawContacts;

	/// Option to visualize the graph coloring used for contacts and joints
	bool drawGraphColors;

	/// Option to draw contact normals
	bool drawContactNormals;

	/// Option to draw contact normal impulses
	bool drawContactImpulses;

	/// Option to draw contact feature ids
	bool drawContactFeatures;

	/// Option to draw contact friction impulses
	bool drawFrictionImpulses;

	/// Option to draw islands as bounding boxes
	bool drawIslands;

	/// User context that is passed as an argument to drawing callback functions
	void* context;
} b2DebugDraw;

/// Use this to initialize your drawing interface. This allows you to implement a sub-set
/// of the drawing functions.
B2_API b2DebugDraw b2DefaultDebugDraw( void );
