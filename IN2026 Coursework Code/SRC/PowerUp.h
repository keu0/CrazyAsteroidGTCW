#ifndef __POWERUP_H__
#define __POWERUP_H__

#include "GameUtil.h"
#include "GameObject.h"
#include "BoundingShape.h"
#include <functional>
#include <cmath>

class PowerUp : public GameObject
{
public:
	enum Type { EXTRA_LIFE = 0, SHOOT_SPREAD = 1, RING_ATTACK = 2 };

	PowerUp(GLVector3f pos, Type type, std::function<void()> onCollected)
		: GameObject("PowerUp"),
		  mType(type),
		  mLifetime(8000),
		  mBlinkTimer(0),
		  mDrawVisible(true),
		  mOnCollected(onCollected)
	{
		mPosition = pos;
		SetRotation(50.0f); // spin at 50 deg/s via GameObject::Update
	}

	void Update(int t) override
	{
		mLifetime -= t;

		// Blink during final 2 seconds
		if (mLifetime <= 2000)
		{
			mBlinkTimer += t;
			if (mBlinkTimer >= 200)
			{
				mBlinkTimer = 0;
				mDrawVisible = !mDrawVisible;
			}
		}

		if (mLifetime <= 0)
		{
			mWorld->FlagForRemoval(GetThisPtr());
			return;
		}

		GameObject::Update(t);
	}

	void Render() override
	{
		if (!mDrawVisible) return;
		GameObject::Render(); // draws mShape (set per type in Asteroids.cpp)
	}

	bool CollisionTest(shared_ptr<GameObject> o) override
	{
		if (o->GetType() != GameObjectType("Spaceship")) return false;
		if (!mBoundingShape || !o->GetBoundingShape()) return false;
		return mBoundingShape->CollisionTest(o->GetBoundingShape());
	}

	void OnCollision(const GameObjectList& objects) override
	{
		for (auto& obj : objects)
		{
			if (obj->GetType() == GameObjectType("Spaceship"))
			{
				mOnCollected();
				mWorld->FlagForRemoval(GetThisPtr());
				return;
			}
		}
	}

private:
	Type  mType;
	int   mLifetime;
	int   mBlinkTimer;
	bool  mDrawVisible;
	std::function<void()> mOnCollected;

};

#endif
