#include "Asteroid.h"
#include "Asteroids.h"
#include "Animation.h"
#include "AnimationManager.h"
#include "GameUtil.h"
#include "GameWindow.h"
#include "GameWorld.h"
#include "GameDisplay.h"
#include "Spaceship.h"
#include "BoundingShape.h"
#include "BoundingSphere.h"
#include "GUILabel.h"
#include "Explosion.h"
#include <algorithm>

// PUBLIC INSTANCE CONSTRUCTORS ///////////////////////////////////////////////

Asteroids::Asteroids(int argc, char *argv[])
	: GameSession(argc, argv)
{
	mLevel = 0;
	mAsteroidCount = 0;
	mGameStarted = false;
	mMenuState = MENU_MAIN;
	mDifficultyEasy = true;
	mFinalScore = 0;
	mNextMilestone = 250;
	mControlLevel = 0;
}

Asteroids::~Asteroids(void)
{
}

// PUBLIC INSTANCE METHODS ////////////////////////////////////////////////////

void Asteroids::Start()
{
	shared_ptr<Asteroids> thisPtr = shared_ptr<Asteroids>(this);

	mGameWorld->AddListener(thisPtr.get());
	mGameWindow->AddKeyboardListener(thisPtr);
	mGameWorld->AddListener(&mScoreKeeper);
	mScoreKeeper.AddListener(thisPtr);

	GLfloat ambient_light[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	GLfloat diffuse_light[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient_light);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse_light);
	glEnable(GL_LIGHT0);

	AnimationManager::GetInstance().CreateAnimationFromFile("explosion", 64, 1024, 64, 64, "explosion_fs.png");
	AnimationManager::GetInstance().CreateAnimationFromFile("asteroid1", 128, 8192, 128, 128, "asteroid1_fs.png");
	AnimationManager::GetInstance().CreateAnimationFromFile("spaceship", 128, 128, 128, 128, "spaceship_fs.png");

	CreateGUI();

	mGameWorld->AddListener(&mPlayer);
	mPlayer.AddListener(thisPtr);

	// Floating background asteroids for the start screen
	CreateBackgroundAsteroids(8);

	// Show main menu
	ShowMenuState(MENU_MAIN);

	GameSession::Start();
}

void Asteroids::Stop()
{
	GameSession::Stop();
}

// KEYBOARD LISTENER //////////////////////////////////////////////////////////

void Asteroids::OnKeyPressed(uchar key, int x, int y)
{
	// ESC navigates back to main menu from sub-menus
	if (key == 27)
	{
		if (!mGameStarted)
		{
			if (mMenuState == MENU_DIFFICULTY || mMenuState == MENU_INSTRUCTIONS)
			{
				ShowMenuState(MENU_MAIN);
			}
			else if (mMenuState == MENU_HIGHSCORES)
			{
				ResetGame();
			}
		}
		return;
	}

	if (!mGameStarted)
	{
		switch (mMenuState)
		{
		case MENU_MAIN:
			if      (key == '1') StartGame();
			else if (key == '2') ShowMenuState(MENU_DIFFICULTY);
			else if (key == '3') ShowMenuState(MENU_INSTRUCTIONS);
			else if (key == '4') ShowMenuState(MENU_HIGHSCORES);
			break;

		case MENU_DIFFICULTY:
			if (key == '1')      { mDifficultyEasy = true;  ShowMenuState(MENU_MAIN); }
			else if (key == '2') { mDifficultyEasy = false; ShowMenuState(MENU_MAIN); }
			break;

		case MENU_INSTRUCTIONS:
			ShowMenuState(MENU_MAIN);
			break;

		case MENU_HIGHSCORES:
			ResetGame();
			break;

		case MENU_ENTER_TAG:
			if (key == 13) // Enter — confirm name
			{
				if (mCurrentInput.empty()) mCurrentInput = "PLAYER";
				AddHighScore(mCurrentInput, mFinalScore);
				UpdateHighScoreLabels();
				ShowMenuState(MENU_HIGHSCORES);
			}
			else if (key == 8) // Backspace
			{
				if (!mCurrentInput.empty())
				{
					mCurrentInput.pop_back();
					mEnterTagInputLabel->SetText("> " + mCurrentInput + "_");
				}
			}
			else if (mCurrentInput.length() < 12 && key >= 32 && key < 127)
			{
				mCurrentInput += (char)key;
				mEnterTagInputLabel->SetText("> " + mCurrentInput + "_");
			}
			break;

		default:
			break;
		}
		return;
	}

	// In-game controls
	switch (key)
	{
	case ' ':
		mSpaceship->Shoot();
		break;
	default:
		break;
	}
}

void Asteroids::OnKeyReleased(uchar key, int x, int y) {}

void Asteroids::OnSpecialKeyPressed(int key, int x, int y)
{
	if (!mGameStarted) return;
	switch (key)
	{
	case GLUT_KEY_UP:    mSpaceship->Thrust(mSpaceship->GetThrustPower());   break;
	case GLUT_KEY_LEFT:  mSpaceship->Rotate(mSpaceship->GetRotationSpeed());  break;
	case GLUT_KEY_RIGHT: mSpaceship->Rotate(-mSpaceship->GetRotationSpeed()); break;
	default: break;
	}
}

void Asteroids::OnSpecialKeyReleased(int key, int x, int y)
{
	if (!mGameStarted) return;
	switch (key)
	{
	case GLUT_KEY_UP:    mSpaceship->Thrust(0);  break;
	case GLUT_KEY_LEFT:  mSpaceship->Rotate(0);  break;
	case GLUT_KEY_RIGHT: mSpaceship->Rotate(0);  break;
	default: break;
	}
}

// GAME WORLD LISTENER ////////////////////////////////////////////////////////

void Asteroids::OnObjectRemoved(GameWorld* world, shared_ptr<GameObject> object)
{
	if (!mGameStarted) return;
	if (object->GetType() == GameObjectType("Asteroid"))
	{
		shared_ptr<GameObject> explosion = CreateExplosion();
		explosion->SetPosition(object->GetPosition());
		explosion->SetRotation(object->GetRotation());
		mGameWorld->AddObject(explosion);
		mAsteroidCount--;
		if (mAsteroidCount <= 0)
		{
			SetTimer(500, START_NEXT_LEVEL);
		}
	}
}

// TIMER LISTENER /////////////////////////////////////////////////////////////

void Asteroids::OnTimer(int value)
{
	if (value == CREATE_NEW_PLAYER)
	{
		if (!mGameStarted) return;
		mSpaceship->Reset();
		mGameWorld->AddObject(mSpaceship);
		// Power-up: invulnerability on respawn (Easy mode only)
		if (mDifficultyEasy)
			mSpaceship->StartInvulnerability(3000);
	}

	if (value == START_NEXT_LEVEL)
	{
		if (!mGameStarted) return;
		mLevel++;
		int num_asteroids = 10 + 2 * mLevel;
		CreateAsteroids(num_asteroids);
	}

	if (value == HIDE_NOTIFICATION)
	{
		mNotificationLabel->SetVisible(false);
	}

	if (value == SHOW_GAME_OVER)
	{
		mFinalScore = mScoreKeeper.GetScore();
		mCurrentInput = "";

		std::ostringstream ss;
		ss << "Final Score: " << mFinalScore;
		mEnterTagScoreLabel->SetText(ss.str());
		mEnterTagInputLabel->SetText("> _");

		mGameStarted = false;
		ShowMenuState(MENU_ENTER_TAG);
	}
}

// PRIVATE METHODS ////////////////////////////////////////////////////////////

void Asteroids::StartGame()
{
	// Silence listeners while removing background asteroids so they don't score
	mGameWorld->RemoveListener(&mScoreKeeper);
	mGameWorld->RemoveListener(&mPlayer);
	for (auto& obj : mBackgroundAsteroids)
		mGameWorld->RemoveObject(obj);
	mBackgroundAsteroids.clear();
	mGameWorld->AddListener(&mScoreKeeper);
	mGameWorld->AddListener(&mPlayer);

	// Reset state for a clean game
	mScoreKeeper.Reset();
	mPlayer.Reset();
	mLevel = 0;
	mAsteroidCount = 0;
	mNextMilestone = 250;
	mControlLevel = 0;
	mScoreLabel->SetText("Score: 0");
	mLivesLabel->SetText("Lives: 3");

	ShowMenuState(STATE_PLAYING);

	mGameStarted = true;
	mGameWorld->AddObject(CreateSpaceship());
	mSpaceship->SetControlLevel(0);
	CreateAsteroids(10);
}

void Asteroids::ResetGame()
{
	mGameStarted = false;
	mLevel = 0;
	mAsteroidCount = 0;
	mNextMilestone = 250;
	mControlLevel = 0;

	// Remove listeners before clearing to prevent side effects
	mGameWorld->RemoveListener(&mScoreKeeper);
	mGameWorld->RemoveListener(&mPlayer);

	mGameWorld->Clear();
	mBackgroundAsteroids.clear();

	mGameWorld->AddListener(&mScoreKeeper);
	mGameWorld->AddListener(&mPlayer);

	mScoreKeeper.Reset();
	mPlayer.Reset();

	mScoreLabel->SetText("Score: 0");
	mLivesLabel->SetText("Lives: 3");

	CreateBackgroundAsteroids(8);
	ShowMenuState(MENU_MAIN);
}

shared_ptr<GameObject> Asteroids::CreateSpaceship()
{
	mSpaceship = make_shared<Spaceship>();
	mSpaceship->SetBoundingShape(make_shared<BoundingSphere>(mSpaceship->GetThisPtr(), 4.0f));
	shared_ptr<Shape> bullet_shape = make_shared<Shape>("bullet.shape");
	mSpaceship->SetBulletShape(bullet_shape);
	Animation *anim_ptr = AnimationManager::GetInstance().GetAnimationByName("spaceship");
	shared_ptr<Sprite> spaceship_sprite =
		make_shared<Sprite>(anim_ptr->GetWidth(), anim_ptr->GetHeight(), anim_ptr);
	mSpaceship->SetSprite(spaceship_sprite);
	mSpaceship->SetScale(0.1f);
	mSpaceship->Reset();
	return mSpaceship;
}

void Asteroids::CreateAsteroids(const uint num_asteroids)
{
	mAsteroidCount = num_asteroids;
	for (uint i = 0; i < num_asteroids; i++)
	{
		Animation *anim_ptr = AnimationManager::GetInstance().GetAnimationByName("asteroid1");
		shared_ptr<Sprite> asteroid_sprite =
			make_shared<Sprite>(anim_ptr->GetWidth(), anim_ptr->GetHeight(), anim_ptr);
		asteroid_sprite->SetLoopAnimation(true);
		shared_ptr<GameObject> asteroid = make_shared<Asteroid>();
		asteroid->SetBoundingShape(make_shared<BoundingSphere>(asteroid->GetThisPtr(), 10.0f));
		asteroid->SetSprite(asteroid_sprite);
		asteroid->SetScale(0.2f);
		mGameWorld->AddObject(asteroid);
	}
}

void Asteroids::CreateBackgroundAsteroids(const uint num_asteroids)
{
	for (uint i = 0; i < num_asteroids; i++)
	{
		Animation *anim_ptr = AnimationManager::GetInstance().GetAnimationByName("asteroid1");
		shared_ptr<Sprite> asteroid_sprite =
			make_shared<Sprite>(anim_ptr->GetWidth(), anim_ptr->GetHeight(), anim_ptr);
		asteroid_sprite->SetLoopAnimation(true);
		shared_ptr<GameObject> asteroid = make_shared<Asteroid>();
		// No bounding shape — will not collide with anything
		asteroid->SetSprite(asteroid_sprite);
		asteroid->SetScale(0.2f);
		mGameWorld->AddObject(asteroid);
		mBackgroundAsteroids.push_back(asteroid);
	}
}

void Asteroids::CreateGUI()
{
	mGameDisplay->GetContainer()->SetBorder(GLVector2i(10, 10));

	// ---- IN-GAME LABELS ----

	mScoreLabel = make_shared<GUILabel>("Score: 0");
	mScoreLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_TOP);
	mScoreLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mScoreLabel), GLVector2f(0.0f, 1.0f));

	mLivesLabel = make_shared<GUILabel>("Lives: 3");
	mLivesLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_BOTTOM);
	mLivesLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mLivesLabel), GLVector2f(0.0f, 0.0f));

	mGameOverLabel = make_shared<GUILabel>("GAME OVER");
	mGameOverLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mGameOverLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mGameOverLabel->SetColor(GLVector3f(1.0f, 0.2f, 0.2f));
	mGameOverLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mGameOverLabel), GLVector2f(0.5f, 0.5f));

	// ---- MAIN MENU LABELS ----

	mMenuTitleLabel = make_shared<GUILabel>("* ASTEROIDS *");
	mMenuTitleLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mMenuTitleLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mMenuTitleLabel->SetColor(GLVector3f(1.0f, 1.0f, 0.0f));
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mMenuTitleLabel), GLVector2f(0.5f, 0.85f));

	mMenuItem1Label = make_shared<GUILabel>("1 - Start Game");
	mMenuItem1Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mMenuItem1Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mMenuItem1Label), GLVector2f(0.5f, 0.68f));

	mMenuItem2Label = make_shared<GUILabel>("2 - Difficulty  [EASY]");
	mMenuItem2Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mMenuItem2Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mMenuItem2Label), GLVector2f(0.5f, 0.58f));

	mMenuItem3Label = make_shared<GUILabel>("3 - Instructions");
	mMenuItem3Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mMenuItem3Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mMenuItem3Label), GLVector2f(0.5f, 0.48f));

	mMenuItem4Label = make_shared<GUILabel>("4 - High Scores");
	mMenuItem4Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mMenuItem4Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mMenuItem4Label), GLVector2f(0.5f, 0.38f));

	// ---- DIFFICULTY MENU LABELS ----

	mDiffTitleLabel = make_shared<GUILabel>("DIFFICULTY SETTINGS");
	mDiffTitleLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mDiffTitleLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mDiffTitleLabel->SetColor(GLVector3f(1.0f, 1.0f, 0.0f));
	mDiffTitleLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mDiffTitleLabel), GLVector2f(0.5f, 0.85f));

	mDiffItem1Label = make_shared<GUILabel>("1 - Easy  (Powerups Enabled)");
	mDiffItem1Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mDiffItem1Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mDiffItem1Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mDiffItem1Label), GLVector2f(0.5f, 0.65f));

	mDiffItem2Label = make_shared<GUILabel>("2 - Hard  (No Powerups)");
	mDiffItem2Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mDiffItem2Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mDiffItem2Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mDiffItem2Label), GLVector2f(0.5f, 0.55f));

	mDiffBackLabel = make_shared<GUILabel>("ESC - Back to Menu");
	mDiffBackLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mDiffBackLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mDiffBackLabel->SetColor(GLVector3f(0.7f, 0.7f, 0.7f));
	mDiffBackLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mDiffBackLabel), GLVector2f(0.5f, 0.30f));

	// ---- INSTRUCTIONS LABELS ----

	mInstrTitleLabel = make_shared<GUILabel>("INSTRUCTIONS");
	mInstrTitleLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrTitleLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrTitleLabel->SetColor(GLVector3f(1.0f, 1.0f, 0.0f));
	mInstrTitleLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrTitleLabel), GLVector2f(0.5f, 0.88f));

	mInstrLine1Label = make_shared<GUILabel>("Up Arrow  -  Thrust");
	mInstrLine1Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine1Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine1Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine1Label), GLVector2f(0.5f, 0.75f));

	mInstrLine2Label = make_shared<GUILabel>("Left/Right Arrow  -  Rotate");
	mInstrLine2Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine2Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine2Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine2Label), GLVector2f(0.5f, 0.65f));

	mInstrLine3Label = make_shared<GUILabel>("Space  -  Shoot");
	mInstrLine3Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine3Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine3Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine3Label), GLVector2f(0.5f, 0.55f));

	mInstrLine4Label = make_shared<GUILabel>("Destroy all asteroids to advance");
	mInstrLine4Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine4Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine4Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine4Label), GLVector2f(0.5f, 0.43f));

	mInstrLine5Label = make_shared<GUILabel>("Each asteroid scores 10 points");
	mInstrLine5Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine5Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine5Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine5Label), GLVector2f(0.5f, 0.33f));

	mInstrLine6Label = make_shared<GUILabel>("3 lives - avoid collisions!");
	mInstrLine6Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine6Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine6Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine6Label), GLVector2f(0.5f, 0.25f));

	mInstrLine7Label = make_shared<GUILabel>("Easy: every 250 pts = life + control boost");
	mInstrLine7Label->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrLine7Label->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrLine7Label->SetColor(GLVector3f(0.4f, 0.9f, 0.4f));
	mInstrLine7Label->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrLine7Label), GLVector2f(0.5f, 0.15f));

	mInstrBackLabel = make_shared<GUILabel>("Press any key to return");
	mInstrBackLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mInstrBackLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mInstrBackLabel->SetColor(GLVector3f(0.7f, 0.7f, 0.7f));
	mInstrBackLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mInstrBackLabel), GLVector2f(0.5f, 0.05f));

	// ---- IN-GAME NOTIFICATION LABEL ----

	mNotificationLabel = make_shared<GUILabel>("");
	mNotificationLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mNotificationLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mNotificationLabel->SetColor(GLVector3f(0.3f, 1.0f, 0.3f));
	mNotificationLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mNotificationLabel), GLVector2f(0.5f, 0.25f));

	// ---- HIGH SCORE LABELS ----

	mHSTitleLabel = make_shared<GUILabel>("HIGH SCORES");
	mHSTitleLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mHSTitleLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mHSTitleLabel->SetColor(GLVector3f(1.0f, 1.0f, 0.0f));
	mHSTitleLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mHSTitleLabel), GLVector2f(0.5f, 0.92f));

	for (int i = 0; i < 10; i++)
	{
		mHSEntryLabels[i] = make_shared<GUILabel>("");
		mHSEntryLabels[i]->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
		mHSEntryLabels[i]->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
		mHSEntryLabels[i]->SetVisible(false);
		float y = 0.82f - i * 0.075f;
		mGameDisplay->GetContainer()->AddComponent(
			static_pointer_cast<GUIComponent>(mHSEntryLabels[i]), GLVector2f(0.5f, y));
	}

	mHSBackLabel = make_shared<GUILabel>("Press any key to return");
	mHSBackLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mHSBackLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mHSBackLabel->SetColor(GLVector3f(0.7f, 0.7f, 0.7f));
	mHSBackLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mHSBackLabel), GLVector2f(0.5f, 0.05f));

	// ---- ENTER TAG LABELS ----

	mEnterTagTitleLabel = make_shared<GUILabel>("GAME OVER");
	mEnterTagTitleLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mEnterTagTitleLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mEnterTagTitleLabel->SetColor(GLVector3f(1.0f, 0.2f, 0.2f));
	mEnterTagTitleLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mEnterTagTitleLabel), GLVector2f(0.5f, 0.82f));

	mEnterTagScoreLabel = make_shared<GUILabel>("Final Score: 0");
	mEnterTagScoreLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mEnterTagScoreLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mEnterTagScoreLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mEnterTagScoreLabel), GLVector2f(0.5f, 0.70f));

	mEnterTagPromptLabel = make_shared<GUILabel>("Enter your gamer tag:");
	mEnterTagPromptLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mEnterTagPromptLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mEnterTagPromptLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mEnterTagPromptLabel), GLVector2f(0.5f, 0.57f));

	mEnterTagInputLabel = make_shared<GUILabel>("> _");
	mEnterTagInputLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mEnterTagInputLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mEnterTagInputLabel->SetColor(GLVector3f(0.3f, 1.0f, 0.3f));
	mEnterTagInputLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mEnterTagInputLabel), GLVector2f(0.5f, 0.46f));

	mEnterTagHintLabel = make_shared<GUILabel>("Press ENTER to confirm");
	mEnterTagHintLabel->SetHorizontalAlignment(GUIComponent::GUI_HALIGN_CENTER);
	mEnterTagHintLabel->SetVerticalAlignment(GUIComponent::GUI_VALIGN_MIDDLE);
	mEnterTagHintLabel->SetColor(GLVector3f(0.7f, 0.7f, 0.7f));
	mEnterTagHintLabel->SetVisible(false);
	mGameDisplay->GetContainer()->AddComponent(
		static_pointer_cast<GUIComponent>(mEnterTagHintLabel), GLVector2f(0.5f, 0.32f));
}

void Asteroids::HideAllLabels()
{
	mScoreLabel->SetVisible(false);
	mLivesLabel->SetVisible(false);
	mGameOverLabel->SetVisible(false);

	mMenuTitleLabel->SetVisible(false);
	mMenuItem1Label->SetVisible(false);
	mMenuItem2Label->SetVisible(false);
	mMenuItem3Label->SetVisible(false);
	mMenuItem4Label->SetVisible(false);

	mDiffTitleLabel->SetVisible(false);
	mDiffItem1Label->SetVisible(false);
	mDiffItem2Label->SetVisible(false);
	mDiffBackLabel->SetVisible(false);

	mInstrTitleLabel->SetVisible(false);
	mInstrLine1Label->SetVisible(false);
	mInstrLine2Label->SetVisible(false);
	mInstrLine3Label->SetVisible(false);
	mInstrLine4Label->SetVisible(false);
	mInstrLine5Label->SetVisible(false);
	mInstrLine6Label->SetVisible(false);
	mInstrLine7Label->SetVisible(false);
	mInstrBackLabel->SetVisible(false);

	mNotificationLabel->SetVisible(false);

	mHSTitleLabel->SetVisible(false);
	for (int i = 0; i < 10; i++) mHSEntryLabels[i]->SetVisible(false);
	mHSBackLabel->SetVisible(false);

	mEnterTagTitleLabel->SetVisible(false);
	mEnterTagScoreLabel->SetVisible(false);
	mEnterTagPromptLabel->SetVisible(false);
	mEnterTagInputLabel->SetVisible(false);
	mEnterTagHintLabel->SetVisible(false);
}

void Asteroids::ShowMenuState(MenuState state)
{
	mMenuState = state;
	HideAllLabels();

	switch (state)
	{
	case MENU_MAIN:
		UpdateDifficultyLabel();
		mMenuTitleLabel->SetVisible(true);
		mMenuItem1Label->SetVisible(true);
		mMenuItem2Label->SetVisible(true);
		mMenuItem3Label->SetVisible(true);
		mMenuItem4Label->SetVisible(true);
		break;

	case MENU_DIFFICULTY:
		mDiffTitleLabel->SetVisible(true);
		mDiffItem1Label->SetVisible(true);
		mDiffItem2Label->SetVisible(true);
		mDiffBackLabel->SetVisible(true);
		break;

	case MENU_INSTRUCTIONS:
		mInstrTitleLabel->SetVisible(true);
		mInstrLine1Label->SetVisible(true);
		mInstrLine2Label->SetVisible(true);
		mInstrLine3Label->SetVisible(true);
		mInstrLine4Label->SetVisible(true);
		mInstrLine5Label->SetVisible(true);
		mInstrLine6Label->SetVisible(true);
		mInstrLine7Label->SetVisible(true);
		mInstrBackLabel->SetVisible(true);
		break;

	case MENU_HIGHSCORES:
		UpdateHighScoreLabels();
		mHSTitleLabel->SetVisible(true);
		for (int i = 0; i < (int)mHighScores.size() && i < 10; i++)
			mHSEntryLabels[i]->SetVisible(true);
		if (mHighScores.empty())
			mHSEntryLabels[0]->SetVisible(true); // shows "(no entries yet)"
		mHSBackLabel->SetVisible(true);
		break;

	case MENU_ENTER_TAG:
		mEnterTagTitleLabel->SetVisible(true);
		mEnterTagScoreLabel->SetVisible(true);
		mEnterTagPromptLabel->SetVisible(true);
		mEnterTagInputLabel->SetVisible(true);
		mEnterTagHintLabel->SetVisible(true);
		break;

	case STATE_PLAYING:
		mScoreLabel->SetVisible(true);
		mLivesLabel->SetVisible(true);
		break;
	}
}

void Asteroids::UpdateDifficultyLabel()
{
	if (mDifficultyEasy)
		mMenuItem2Label->SetText("2 - Difficulty  [EASY]");
	else
		mMenuItem2Label->SetText("2 - Difficulty  [HARD]");
}

void Asteroids::UpdateHighScoreLabels()
{
	if (mHighScores.empty())
	{
		mHSEntryLabels[0]->SetText("  (no entries yet)");
		return;
	}
	for (int i = 0; i < 10; i++)
	{
		if (i < (int)mHighScores.size())
		{
			std::ostringstream ss;
			ss << (i + 1) << ".  ";
			std::string entry = ss.str() + mHighScores[i].name;
			while ((int)entry.length() < 22) entry += " ";
			entry += std::to_string(mHighScores[i].score);
			mHSEntryLabels[i]->SetText(entry);
		}
		else
		{
			mHSEntryLabels[i]->SetText("");
		}
	}
}

void Asteroids::AddHighScore(const std::string& name, int score)
{
	HighScoreEntry entry;
	entry.name = name;
	entry.score = score;
	mHighScores.push_back(entry);
	std::sort(mHighScores.begin(), mHighScores.end(),
		[](const HighScoreEntry& a, const HighScoreEntry& b) {
			return a.score > b.score;
		});
	if (mHighScores.size() > 10)
		mHighScores.resize(10);
}

void Asteroids::OnScoreChanged(int score)
{
	std::ostringstream msg_stream;
	msg_stream << "Score: " << score;
	mScoreLabel->SetText(msg_stream.str());

	// Power-ups: milestone bonuses every 250 points (Easy mode only)
	if (mDifficultyEasy && score >= mNextMilestone)
	{
		mNextMilestone += 250;

		// Power-up 1: Extra life
		mPlayer.AddLife();
		std::ostringstream lives_stream;
		lives_stream << "Lives: " << mPlayer.GetLives();
		mLivesLabel->SetText(lives_stream.str());

		// Power-up 3: Control upgrade
		mControlLevel++;
		if (mSpaceship)
			mSpaceship->SetControlLevel(mControlLevel);

		// Notification
		std::ostringstream note;
		note << "MILESTONE! +1 Life  |  Controls Lv." << mControlLevel;
		ShowNotification(note.str());
	}
}

void Asteroids::ShowNotification(const std::string& msg)
{
	mNotificationLabel->SetText(msg);
	mNotificationLabel->SetVisible(true);
	SetTimer(2500, HIDE_NOTIFICATION);
}

void Asteroids::OnPlayerKilled(int lives_left)
{
	shared_ptr<GameObject> explosion = CreateExplosion();
	explosion->SetPosition(mSpaceship->GetPosition());
	explosion->SetRotation(mSpaceship->GetRotation());
	mGameWorld->AddObject(explosion);

	std::ostringstream msg_stream;
	msg_stream << "Lives: " << lives_left;
	mLivesLabel->SetText(msg_stream.str());

	if (lives_left > 0)
	{
		SetTimer(1000, CREATE_NEW_PLAYER);
	}
	else
	{
		SetTimer(1500, SHOW_GAME_OVER);
	}
}

shared_ptr<GameObject> Asteroids::CreateExplosion()
{
	Animation *anim_ptr = AnimationManager::GetInstance().GetAnimationByName("explosion");
	shared_ptr<Sprite> explosion_sprite =
		make_shared<Sprite>(anim_ptr->GetWidth(), anim_ptr->GetHeight(), anim_ptr);
	explosion_sprite->SetLoopAnimation(false);
	shared_ptr<GameObject> explosion = make_shared<Explosion>();
	explosion->SetSprite(explosion_sprite);
	explosion->Reset();
	return explosion;
}
