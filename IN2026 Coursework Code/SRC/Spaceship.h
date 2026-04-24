#ifndef __SPACESHIP_H__
#define __SPACESHIP_H__

#include "GameUtil.h"
#include "GameObject.h"
#include "Shape.h"

class Spaceship : public GameObject
{
public:
	Spaceship();
	Spaceship(GLVector3f p, GLVector3f v, GLVector3f a, GLfloat h, GLfloat r);
	Spaceship(const Spaceship& s);
	virtual ~Spaceship(void);

	virtual void Update(int t);
	virtual void Render(void);
	virtual void Reset(void);

	virtual void Thrust(float t);
	virtual void Rotate(float r);
	virtual void Shoot(void);
	virtual void ShootRing(void);

	void SetSpaceshipShape(shared_ptr<Shape> spaceship_shape) { mSpaceshipShape = spaceship_shape; }
	void SetThrusterShape(shared_ptr<Shape> thruster_shape)   { mThrusterShape = thruster_shape; }
	void SetBulletShape(shared_ptr<Shape> bullet_shape)       { mBulletShape = bullet_shape; }

	// Invulnerability (power-up: awarded on respawn)
	void StartInvulnerability(int duration_ms);
	bool IsInvulnerable() const { return mInvulnerable; }

	// Control upgrades (power-up: awarded at score milestones)
	void SetControlLevel(int level);
	float GetRotationSpeed() const { return mRotationSpeed; }
	float GetThrustPower()   const { return mThrustPower; }

	// Charge-shot tracking
	void StartCharge();
	void StopCharge();
	int  GetChargeTime() const { return mChargeTime; }
	bool IsCharging()    const { return mCharging; }

	// Ring-attack cooldown
	bool IsRingReady()       const { return mRingCooldown <= 0; }
	int  GetRingCooldown()   const { return mRingCooldown; }

	// Shoot mode (power-up)
	enum ShootMode { SHOOT_NORMAL = 0, SHOOT_SPREAD = 1, SHOOT_LASER = 2 };
	void      SetShootMode(ShootMode mode, int duration_ms = 15000);
	ShootMode GetShootMode() const { return mShootMode; }

	// Dash (worldAngle = world-space cardinal direction, degrees)
	void Dash(float worldAngle);
	bool CanDash()        const { return mDashCooldown <= 0; }
	int  GetDashCooldown() const { return mDashCooldown; }

	// Brake
	void SetBraking(bool braking);
	bool IsBraking() const { return mBraking; }

	// Ring attack power-up
	void SetHasRingAttack(bool has) { mHasRingAttack = has; }
	bool HasRingAttack() const { return mHasRingAttack; }

	bool CollisionTest(shared_ptr<GameObject> o);
	void OnCollision(const GameObjectList &objects);

private:
	float mThrust;

	// Invulnerability state
	bool  mInvulnerable;
	int   mInvulnerableTimer;
	int   mBlinkTimer;
	bool  mBlinkVisible;

	// Control-upgrade stats
	float mRotationSpeed;
	float mFriction;
	float mThrustPower;

	// Shoot cooldown
	int   mShootCooldown;     // ms remaining until next normal shot
	int   mCooldownDuration;  // ms between shots (0 = instant)

	// Charged ring-attack state
	bool  mCharging;
	int   mChargeTime;        // ms held so far

	// Ring-attack cooldown
	int       mRingCooldown;

	// Shoot mode
	ShootMode mShootMode;
	int       mShootModeTimer;

	// Dash state
	int        mDashCooldown;
	int        mDashTrailTimer;
	GLVector3f mDashTrailPos;

	// Brake state
	bool  mBraking;
	int   mBrakeHoldTime;

	// Ring attack power-up state
	bool  mHasRingAttack;

	shared_ptr<Shape> mSpaceshipShape;
	shared_ptr<Shape> mThrusterShape;
	shared_ptr<Shape> mBulletShape;
};

#endif
