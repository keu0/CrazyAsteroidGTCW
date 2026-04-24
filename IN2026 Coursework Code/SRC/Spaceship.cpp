#include "GameUtil.h"
#include "GameWorld.h"
#include "Bullet.h"
#include "Spaceship.h"
#include "BoundingSphere.h"
#include <cmath>

using namespace std;

// PUBLIC INSTANCE CONSTRUCTORS ///////////////////////////////////////////////

Spaceship::Spaceship()
	: GameObject("Spaceship"), mThrust(0),
	  mInvulnerable(false), mInvulnerableTimer(0), mBlinkTimer(0), mBlinkVisible(true),
	  mRotationSpeed(90.0f), mFriction(1.0f), mThrustPower(10.0f),
	  mShootCooldown(0), mCooldownDuration(0),
	  mCharging(false), mChargeTime(0),
	  mRingCooldown(0), mShootMode(SHOOT_NORMAL), mShootModeTimer(0), mDashCooldown(0),
	  mBraking(false), mBrakeHoldTime(0), mHasRingAttack(false),
	  mDashTrailTimer(0), mDashTrailPos(GLVector3f(0,0,0))
{
}

Spaceship::Spaceship(GLVector3f p, GLVector3f v, GLVector3f a, GLfloat h, GLfloat r)
	: GameObject("Spaceship", p, v, a, h, r), mThrust(0),
	  mInvulnerable(false), mInvulnerableTimer(0), mBlinkTimer(0), mBlinkVisible(true),
	  mRotationSpeed(90.0f), mFriction(1.0f), mThrustPower(10.0f),
	  mShootCooldown(0), mCooldownDuration(0),
	  mCharging(false), mChargeTime(0),
	  mRingCooldown(0), mShootMode(SHOOT_NORMAL), mShootModeTimer(0), mDashCooldown(0),
	  mBraking(false), mBrakeHoldTime(0), mHasRingAttack(false),
	  mDashTrailTimer(0), mDashTrailPos(GLVector3f(0,0,0))
{
}

Spaceship::Spaceship(const Spaceship& s)
	: GameObject(s), mThrust(0),
	  mInvulnerable(false), mInvulnerableTimer(0), mBlinkTimer(0), mBlinkVisible(true),
	  mRotationSpeed(s.mRotationSpeed), mFriction(s.mFriction), mThrustPower(s.mThrustPower),
	  mShootCooldown(0), mCooldownDuration(0),
	  mCharging(false), mChargeTime(0),
	  mRingCooldown(0), mShootMode(SHOOT_NORMAL), mShootModeTimer(0), mDashCooldown(0),
	  mBraking(false), mBrakeHoldTime(0), mHasRingAttack(false),
	  mDashTrailTimer(0), mDashTrailPos(GLVector3f(0,0,0))
{
}

Spaceship::~Spaceship(void)
{
}

// PUBLIC INSTANCE METHODS ////////////////////////////////////////////////////

void Spaceship::Reset(void)
{
	GameObject::Reset();
	mInvulnerable = false;
	mInvulnerableTimer = 0;
	mBlinkTimer = 0;
	mBlinkVisible = true;
	mShootCooldown = 0;
	mCharging = false;
	mChargeTime = 0;
	mRingCooldown = 0;
	mShootMode = SHOOT_NORMAL;
	mShootModeTimer = 0;
	mDashCooldown = 0;
	mBraking = false;
	mBrakeHoldTime = 0;
	mHasRingAttack = false;
	mDashTrailTimer = 0;
}

void Spaceship::Update(int t)
{
	// --- Invulnerability blink ---
	if (mInvulnerable)
	{
		mInvulnerableTimer -= t;
		mBlinkTimer += t;
		if (mBlinkTimer >= 150)
		{
			mBlinkTimer = 0;
			mBlinkVisible = !mBlinkVisible;
		}
		if (mInvulnerableTimer <= 0)
		{
			mInvulnerable = false;
			mBlinkVisible = true;
		}
	}

	// --- Shoot cooldown ---
	if (mShootCooldown > 0)
	{
		mShootCooldown -= t;
		if (mShootCooldown < 0) mShootCooldown = 0;
	}

	// --- Shoot mode timer (-1 = permanent until game over) ---
	if (mShootModeTimer > 0)
	{
		mShootModeTimer -= t;
		if (mShootModeTimer <= 0)
		{
			mShootModeTimer = 0;
			mShootMode = SHOOT_NORMAL;
		}
	}

	// --- Ring-attack cooldown ---
	if (mRingCooldown > 0)
	{
		mRingCooldown -= t;
		if (mRingCooldown < 0) mRingCooldown = 0;
	}

	// --- Dash cooldown and trail ---
	if (mDashCooldown > 0)
	{
		mDashCooldown -= t;
		if (mDashCooldown < 0) mDashCooldown = 0;
	}
	if (mDashTrailTimer > 0)
	{
		mDashTrailTimer -= t;
		if (mDashTrailTimer < 0) mDashTrailTimer = 0;
	}

	// --- Charge timer ---
	if (mCharging)
		mChargeTime += t;

	// --- Brake: exponential decay, tap=gentle, hold=near-instant stop ---
	if (mBraking)
	{
		mBrakeHoldTime += t;
		float dt = t / 1000.0f;
		float holdFrac = (float)mBrakeHoldTime / 150.0f; // full power in 150ms
		if (holdFrac > 1.0f) holdFrac = 1.0f;
		float decayRate = 3.0f + holdFrac * 27.0f; // 3 → 30 per second
		float factor = expf(-decayRate * dt);
		mVelocity.x *= factor;
		mVelocity.y *= factor;
	}

	// --- Friction (control upgrade) ---
	if (mThrust == 0.0f && mFriction < 1.0f)
	{
		float dt = t / 1000.0f;
		float factor = (float)pow((double)mFriction, (double)dt);
		mVelocity.x *= factor;
		mVelocity.y *= factor;
	}

	GameObject::Update(t);
}

void Spaceship::Render(void)
{
	if (!mBlinkVisible) return;

	// Dash trail: faint streak from origin to current pos (in local space)
	if (mDashTrailTimer > 0)
	{
		float alpha = (float)mDashTrailTimer / 140.0f;
		float dx = mDashTrailPos.x - mPosition.x;
		float dy = mDashTrailPos.y - mPosition.y;
		glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT);
		glColor3f(0.4f * alpha, 0.8f * alpha, 1.0f * alpha);
		glLineWidth(2.5f);
		glBegin(GL_LINES);
		glVertex2f(dx, dy);   // where we came from
		glVertex2f(0.0f, 0.0f); // current position (local origin)
		glEnd();
		glLineWidth(1.0f);
		glPopAttrib();
	}

	if (mSpaceshipShape.get() != NULL) mSpaceshipShape->Render();

	if ((mThrust > 0) && (mThrusterShape.get() != NULL))
		mThrusterShape->Render();

	GameObject::Render();
}

void Spaceship::Thrust(float t)
{
	mThrust = t;
	mAcceleration.x = mThrust * cos(DEG2RAD * mAngle);
	mAcceleration.y = mThrust * sin(DEG2RAD * mAngle);
}

void Spaceship::Rotate(float r)
{
	mRotation = r;
}

void Spaceship::Shoot(void)
{
	if (!mWorld) return;
	if (mShootCooldown > 0) return;

	auto fireBullet = [this](float angle, float speed, int lifetime)
	{
		GLVector3f dir(cos(DEG2RAD * angle), sin(DEG2RAD * angle), 0);
		dir.normalize();
		GLVector3f bpos = mPosition + dir * 4.0f;
		GLVector3f bvel = mVelocity + dir * speed;
		shared_ptr<GameObject> b(new Bullet(bpos, bvel, GLVector3f(0,0,0), angle, 0, lifetime));
		b->SetBoundingShape(make_shared<BoundingSphere>(b->GetThisPtr(), 2.0f));
		b->SetShape(mBulletShape);
		mWorld->AddObject(b);
	};

	switch (mShootMode)
	{
	case SHOOT_SPREAD:
		fireBullet(mAngle - 20.0f, 30.0f, 2000);
		fireBullet(mAngle,          30.0f, 2000);
		fireBullet(mAngle + 20.0f, 30.0f, 2000);
		break;
	case SHOOT_LASER:
		fireBullet(mAngle, 65.0f, 5500); // fast, long range
		break;
	default:
		fireBullet(mAngle, 30.0f, 2000);
		break;
	}

	mShootCooldown = mCooldownDuration;
}

void Spaceship::SetShootMode(ShootMode mode, int duration_ms)
{
	mShootMode = mode;
	mShootModeTimer = duration_ms; // -1 = permanent until Reset()
}

void Spaceship::ShootRing(void)
{
	if (!mWorld) return;
	if (!mHasRingAttack) return;
	if (mRingCooldown > 0) return;
	const int   NUM_BULLETS = 24;
	const float STEP        = 360.0f / NUM_BULLETS;
	const float SPEED       = 28.0f;

	for (int i = 0; i < NUM_BULLETS; i++)
	{
		float angle = i * STEP;
		GLVector3f dir(cos(DEG2RAD * angle), sin(DEG2RAD * angle), 0);
		dir.normalize();
		GLVector3f bpos = mPosition + dir * 5.0f;
		GLVector3f bvel = dir * SPEED;
		shared_ptr<GameObject> bullet(
			new Bullet(bpos, bvel, GLVector3f(0, 0, 0), angle, 0, 1500));
		bullet->SetBoundingShape(make_shared<BoundingSphere>(bullet->GetThisPtr(), 2.0f));
		bullet->SetShape(mBulletShape);
		mWorld->AddObject(bullet);
	}
	mRingCooldown = 5000;
}

void Spaceship::StartCharge()
{
	mCharging  = true;
	mChargeTime = 0;
}

void Spaceship::StopCharge()
{
	mCharging = false;
}

void Spaceship::Dash(float worldAngle)
{
	if (mDashCooldown > 0) return;
	const float DASH_DIST = 18.0f;
	mDashTrailPos  = mPosition; // record origin for trail
	mDashTrailTimer = 140;      // trail fades over 140 ms
	mPosition.x += cosf(DEG2RAD * worldAngle) * DASH_DIST;
	mPosition.y += sinf(DEG2RAD * worldAngle) * DASH_DIST;
	mVelocity.x = 0.0f;
	mVelocity.y = 0.0f;
	mDashCooldown = 2500;
	StartInvulnerability(200);
}

void Spaceship::SetBraking(bool braking)
{
	mBraking = braking;
	if (!braking)
		mBrakeHoldTime = 0;
}

void Spaceship::StartInvulnerability(int duration_ms)
{
	mInvulnerable      = true;
	mInvulnerableTimer = duration_ms;
	mBlinkTimer        = 0;
	mBlinkVisible      = true;
}

void Spaceship::SetControlLevel(int level)
{
	// Rotation: 90 deg/s base, +25 per level, cap at 215
	mRotationSpeed = min(90.0f + level * 25.0f, 215.0f);

	// Friction per-second multiplier
	const float frictionTable[] = { 1.00f, 0.97f, 0.93f, 0.88f, 0.83f, 0.78f };
	mFriction = frictionTable[min(level, 5)];

	// Thrust power: increases from level 2 onward
	mThrustPower = 20.0f + max(0, level - 1) * 2.5f;

	// No cooldown at any level
}

bool Spaceship::CollisionTest(shared_ptr<GameObject> o)
{
	if (mInvulnerable) return false;
	if (o->GetType() != GameObjectType("Asteroid")) return false;
	if (mBoundingShape.get() == NULL) return false;
	if (o->GetBoundingShape().get() == NULL) return false;
	return mBoundingShape->CollisionTest(o->GetBoundingShape());
}

void Spaceship::OnCollision(const GameObjectList &objects)
{
	for (auto& obj : objects)
	{
		if (obj->GetType() == GameObjectType("Asteroid"))
		{
			mWorld->FlagForRemoval(GetThisPtr());
			return;
		}
	}
}
