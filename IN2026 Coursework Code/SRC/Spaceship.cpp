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
	  mRotationSpeed(90.0f), mFriction(1.0f), mThrustPower(10.0f)
{
}

Spaceship::Spaceship(GLVector3f p, GLVector3f v, GLVector3f a, GLfloat h, GLfloat r)
	: GameObject("Spaceship", p, v, a, h, r), mThrust(0),
	  mInvulnerable(false), mInvulnerableTimer(0), mBlinkTimer(0), mBlinkVisible(true),
	  mRotationSpeed(90.0f), mFriction(1.0f), mThrustPower(10.0f)
{
}

Spaceship::Spaceship(const Spaceship& s)
	: GameObject(s), mThrust(0),
	  mInvulnerable(false), mInvulnerableTimer(0), mBlinkTimer(0), mBlinkVisible(true),
	  mRotationSpeed(s.mRotationSpeed), mFriction(s.mFriction), mThrustPower(s.mThrustPower)
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

	// --- Friction (control upgrade) ---
	// Only dampen velocity when not actively thrusting
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
	GLVector3f heading(cos(DEG2RAD * mAngle), sin(DEG2RAD * mAngle), 0);
	heading.normalize();
	GLVector3f bullet_position = mPosition + (heading * 4);
	float bullet_speed = 30;
	GLVector3f bullet_velocity = mVelocity + heading * bullet_speed;
	shared_ptr<GameObject> bullet(
		new Bullet(bullet_position, bullet_velocity, mAcceleration, mAngle, 0, 2000));
	bullet->SetBoundingShape(make_shared<BoundingSphere>(bullet->GetThisPtr(), 2.0f));
	bullet->SetShape(mBulletShape);
	mWorld->AddObject(bullet);
}

void Spaceship::StartInvulnerability(int duration_ms)
{
	mInvulnerable = true;
	mInvulnerableTimer = duration_ms;
	mBlinkTimer = 0;
	mBlinkVisible = true;
}

void Spaceship::SetControlLevel(int level)
{
	// Rotation: 90 deg/s base, +25 per level, cap at 215
	mRotationSpeed = min(90.0f + level * 25.0f, 215.0f);

	// Friction: per-second velocity multiplier — lower = stronger braking
	// Level 0: no friction; each level adds progressively more drag
	const float frictionTable[] = { 1.00f, 0.97f, 0.93f, 0.88f, 0.83f, 0.78f };
	int idx = min(level, 5);
	mFriction = frictionTable[idx];

	// Thrust power: increases from level 2 onward
	mThrustPower = 10.0f + max(0, level - 1) * 1.5f;
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
	mWorld->FlagForRemoval(GetThisPtr());
}
