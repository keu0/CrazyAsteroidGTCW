#ifndef __BLACKHOLE_H__
#define __BLACKHOLE_H__

#include "GameUtil.h"
#include "GameObject.h"
#include <map>
#include <cmath>

class BlackHole : public GameObject
{
public:
	struct SpiralInfo {
		float startDist;
		float startAngle;
		int   elapsed_ms;
		float origScale;
	};

	BlackHole(GLVector3f pos)
		: GameObject("BlackHole"),
		  mLifetime(7000),
		  mOuterRadius(16.5f),
		  mSpiralRadius(7.0f),
		  mEventHorizonRadius(3.0f),
		  mPulseTimer(0),
		  mPulsePhase(0.0f),
		  mExpandTimer(0),
		  mDying(false),
		  mDeathTimer(0)
	{
		mPosition = pos;
	}

	void Update(int t) override
	{
		const int   EXPAND_DURATION = 2500;
		const int   DEATH_DURATION  = 900;
		const float PI2             = 2.0f * 3.14159265f;
		const float SPIRAL_DURATION = 1500.0f;
		const float SPIRAL_REVS     = 1.5f;

		float sizeScale = mOuterRadius / 16.5f;

		// Halved from previous values
		const float G_ASTEROID  = 11000.0f * sizeScale;
		const float G_SHIP      = 40000.0f * sizeScale;
		const float G_INNER     = 40000.0f * sizeScale;

		if (mExpandTimer < EXPAND_DURATION)
			mExpandTimer += t;

		float expandFrac = (float)mExpandTimer / EXPAND_DURATION;
		if (expandFrac > 1.0f) expandFrac = 1.0f;

		float visRadius = mOuterRadius * expandFrac;

		// Gravity zone is fixed at 50 units (75% of original 66), fully invisible,
		// decoupled from visual size so it stays wide even when the spiral is small
		float gravRadius = 50.0f * sizeScale;

		mPulseTimer += t;
		mPulsePhase = (float)(mPulseTimer % 1000) / 1000.0f;

		// --- Death / shrink phase ---
		if (mDying)
		{
			mDeathTimer += t;
			if ((float)mDeathTimer >= DEATH_DURATION)
			{
				if (mWorld)
				{
					GameObjectList all = mWorld->GetObjects();
					for (auto& obj : all)
					{
						auto it = mSpiralInfo.find(obj.get());
						if (it != mSpiralInfo.end())
						{
							obj->SetScale(it->second.origScale);
							obj->SetVelocity(GLVector3f(0, 0, 0));
						}
					}
				}
				mSpiralInfo.clear();
				mWorld->FlagForRemoval(GetThisPtr());
			}
			return;
		}

		mLifetime -= t;
		if (mLifetime <= 0)
		{
			mDying = true;
			mDeathTimer = 0;
			return;
		}

		if (!mWorld) return;

		float dt = t / 1000.0f;
		GameObjectList objects = mWorld->GetObjects();

		for (auto& obj : objects)
		{
			if (obj.get() == this) continue;

			GameObjectType type = obj->GetType();
			bool isAsteroid = (type == GameObjectType("Asteroid"));
			bool isShip     = (type == GameObjectType("Spaceship"));
			if (!isAsteroid && !isShip) continue;

			GLVector3f pos = obj->GetPosition();
			float dx = pos.x - mPosition.x;
			float dy = pos.y - mPosition.y;
			float dist = sqrtf(dx * dx + dy * dy);
			if (dist < 0.1f) dist = 0.1f;

			float nx = -dx / dist;
			float ny = -dy / dist;

			if (dist > gravRadius) continue;

			// Apply gravity
			float G;
			if (dist < mSpiralRadius)
				G = G_INNER;
			else
				G = isAsteroid ? G_ASTEROID : G_SHIP;

			float accel = G / (dist * dist);
			GLVector3f vel = obj->GetVelocity();
			vel.x += nx * accel * dt;
			vel.y += ny * accel * dt;
			obj->SetVelocity(vel);

			// Kill ship at event horizon
			if (isShip && dist < mEventHorizonRadius)
			{
				mWorld->FlagForRemoval(obj);
				continue;
			}

			// Asteroid spiral capture
			if (isAsteroid)
			{
				auto it = mSpiralInfo.find(obj.get());
				bool inSpiral = (it != mSpiralInfo.end());

				if (!inSpiral && dist <= mEventHorizonRadius) continue;

				if (inSpiral)
				{
					SpiralInfo& info = it->second;
					info.elapsed_ms += t;
					float tNorm = info.elapsed_ms / SPIRAL_DURATION;
					if (tNorm > 1.0f) tNorm = 1.0f;

					float curAngle = info.startAngle - SPIRAL_REVS * PI2 * tNorm;
					float curDist  = info.startDist * (1.0f - tNorm);

					GLVector3f newPos;
					newPos.x = mPosition.x + cosf(curAngle) * curDist;
					newPos.y = mPosition.y + sinf(curAngle) * curDist;
					newPos.z = mPosition.z;
					obj->SetPosition(newPos);
					obj->SetVelocity(GLVector3f(0, 0, 0));
					obj->SetScale(info.origScale * (1.0f - tNorm));

					if (tNorm >= 1.0f || curDist < mEventHorizonRadius)
					{
						mSpiralInfo.erase(it);
						mWorld->FlagForRemoval(obj);
						// Grow black hole
						mOuterRadius += 2.0f;
						if (mOuterRadius > 50.0f) mOuterRadius = 50.0f;
						mSpiralRadius       = mOuterRadius * 0.42f;
						mEventHorizonRadius = 3.0f + (mOuterRadius - 16.5f) * 0.05f;
						if (mEventHorizonRadius > 6.0f) mEventHorizonRadius = 6.0f;
						mLifetime += 400;
					}
				}
				else if (dist < mSpiralRadius)
				{
					SpiralInfo info;
					info.startDist  = dist;
					info.startAngle = atan2f(dy, dx);
					info.elapsed_ms = 0;
					info.origScale  = obj->GetScale();
					mSpiralInfo[obj.get()] = info;
				}
			}
		}
	}

	void Render() override
	{
		const int   EXPAND_DURATION = 2500;
		const int   DEATH_DURATION  = 900;
		const float PI              = 3.14159265f;
		const float PI2             = 2.0f * PI;

		float expandFrac = (float)mExpandTimer / EXPAND_DURATION;
		if (expandFrac > 1.0f) expandFrac = 1.0f;

		float deathShrink = 1.0f;
		if (mDying)
		{
			deathShrink = 1.0f - (float)mDeathTimer / DEATH_DURATION;
			if (deathShrink < 0.0f) deathShrink = 0.0f;
		}

		float R     = mOuterRadius * expandFrac * deathShrink;
		float ehR   = mEventHorizonRadius * deathShrink;
		float pulse = 0.5f + 0.5f * sinf(mPulsePhase * PI2);
		float slowRot = (float)(mPulseTimer % 14000) / 14000.0f * PI2;

		if (R < 0.5f) return;

		glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT | GL_POINT_BIT);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LIGHTING);

		if (R > ehR)
		{
			struct Layer { int arms; float revs, rotMult, rotBias, bright; int seg, cid; };
			const Layer layers[] = {
				{ 4, 2.5f,  1.0f,  0.0f,       0.90f, 110, 0 }, // cyan → deep blue
				{ 3, 3.3f,  1.0f,  PI * 0.67f, 1.00f, 130, 1 }, // yellow → orange → red
				{ 2, 4.0f, -0.5f,  PI * 0.22f, 0.85f,  90, 2 }, // magenta → violet
			};

			glLineWidth(2.0f);
			for (const Layer& L : layers)
			{
				float rot = slowRot * L.rotMult + L.rotBias;
				for (int arm = 0; arm < L.arms; arm++)
				{
					float armOff = (float)arm / L.arms * PI2;
					glBegin(GL_LINE_STRIP);
					for (int i = 0; i <= L.seg; i++)
					{
						float t = (float)i / L.seg;
						float r = R + (ehR - R) * t;
						float a = rot + armOff + t * L.revs * PI2;

						float rc, gc, bc;
						if (L.cid == 0)
						{
							// white → cyan → deep blue → black
							if (t < 0.35f) {
								float s = t / 0.35f;
								rc = 1.0f - s; gc = 1.0f; bc = 1.0f;
							} else if (t < 0.70f) {
								float s = (t - 0.35f) / 0.35f;
								rc = 0.0f; gc = 1.0f - s * 0.6f; bc = 1.0f;
							} else {
								float s = (t - 0.70f) / 0.30f;
								rc = 0.0f; gc = 0.4f * (1.0f - s); bc = 1.0f * (1.0f - s);
							}
						}
						else if (L.cid == 1)
						{
							// white → yellow → orange → red → black
							if (t < 0.30f) {
								float s = t / 0.30f;
								rc = 1.0f; gc = 1.0f; bc = 1.0f - s;
							} else if (t < 0.60f) {
								float s = (t - 0.30f) / 0.30f;
								rc = 1.0f; gc = 1.0f - s * 0.6f; bc = 0.0f;
							} else if (t < 0.82f) {
								float s = (t - 0.60f) / 0.22f;
								rc = 1.0f; gc = 0.4f - s * 0.4f; bc = 0.0f;
							} else {
								float s = (t - 0.82f) / 0.18f;
								rc = 1.0f - s; gc = 0.0f; bc = 0.0f;
							}
						}
						else
						{
							// white → magenta → purple → black
							if (t < 0.35f) {
								float s = t / 0.35f;
								rc = 1.0f; gc = 1.0f - s; bc = 1.0f;
							} else if (t < 0.70f) {
								float s = (t - 0.35f) / 0.35f;
								rc = 1.0f - s * 0.3f; gc = 0.0f; bc = 1.0f - s * 0.35f;
							} else {
								float s = (t - 0.70f) / 0.30f;
								rc = 0.7f * (1.0f - s); gc = 0.0f; bc = 0.65f * (1.0f - s);
							}
						}

						float b = L.bright * expandFrac * deathShrink;
						glColor3f(rc * b, gc * b, bc * b);
						glVertex2f(cosf(a) * r, sinf(a) * r);
					}
					glEnd();
				}
			}
			glLineWidth(1.0f);
		}

		// Dark filled interior
		glColor3f(0.0f, 0.0f, 0.0f);
		glBegin(GL_TRIANGLE_FAN);
		glVertex2f(0.0f, 0.0f);
		for (int i = 0; i <= 48; i++) {
			float a = PI2 * i / 48;
			glVertex2f(cosf(a) * ehR, sinf(a) * ehR);
		}
		glEnd();

		// Event horizon: hot white core fading to orange, pulsing
		glLineWidth(2.5f);
		glBegin(GL_LINE_LOOP);
		for (int i = 0; i < 48; i++) {
			float a  = PI2 * i / 48;
			float sh = (i % 2 == 0) ? pulse : (1.0f - pulse);
			glColor3f(1.0f, 0.4f + 0.4f * sh, 0.1f * sh);
			glVertex2f(cosf(a) * ehR, sinf(a) * ehR);
		}
		glEnd();
		glLineWidth(1.0f);

		// Singularity
		glColor3f(1.0f, 0.8f, 0.3f);
		glPointSize(5.0f);
		glBegin(GL_POINTS);
		glVertex2f(0.0f, 0.0f);
		glEnd();

		glPopAttrib();
	}

	bool CollisionTest(shared_ptr<GameObject> o) override { return false; }
	void OnCollision(const GameObjectList& objects) override {}

private:
	int   mLifetime;
	float mOuterRadius;
	float mSpiralRadius;
	float mEventHorizonRadius;
	int   mPulseTimer;
	float mPulsePhase;
	int   mExpandTimer;
	bool  mDying;
	int   mDeathTimer;
	std::map<GameObject*, SpiralInfo> mSpiralInfo;
};

#endif
