#pragma once

namespace nutmeg::ai::utility {

// Every number the ball carrier's decision reads. Metres unless noted. Threat
// and utility are unitless: a goal is worth kGoalValue and everything else is
// scaled against it. Chances are probabilities in 0..1.
namespace tuning {

// Threat of a spot: closeness to goal plus how far up the pitch it is.
constexpr float kThreatScale = 24.0f;
constexpr float kThreatGoalWeight = 1.0f;
constexpr float kThreatAdvanceWeight = 0.6f;

constexpr float kGoalValue = 1.6f;
// Cost of losing the ball: kLossBase anywhere, plus up to kLossOwnGoal as the
// spot nears the carrier's own goal.
// Scales every risk term: above 1 the carrier plays safer.
constexpr float kRiskAversion = 1.8f;
constexpr float kLossBase = 0.04f;
constexpr float kLossOwnGoal = 0.12f;

// Shot chance: a rational falloff with distance, scaled by the angle the posts
// make, the finisher, pressure and bodies in the line of the shot.
constexpr float kXgBase = 0.42f;
constexpr float kXgScale = 15.0f;
constexpr float kXgAngleRef = 0.6f;
constexpr float kXgAngleCap = 1.3f;
constexpr float kXgMaxRange = 32.0f;
constexpr float kXgFinishingLow = 0.6f;
constexpr float kXgFinishingHigh = 1.3f;
constexpr float kXgPressure = 0.5f;
constexpr float kXgBlocker = 0.3f;
constexpr float kXgBlockerWidth = 1.2f;
// Below this a shot is not worth listing.
constexpr float kShotMinChance = 0.01f;
constexpr float kShotPower = 1.0f;
constexpr float kShotAimFraction = 0.8f;
// A missed shot usually goes out, so it costs less than a lost pass.
constexpr float kShotMissLoss = 0.5f;

constexpr float kPassMinDistance = 4.0f;
constexpr float kPassMaxDistance = 38.0f;
constexpr float kPassBaseChance = 1.0f;
constexpr float kPassDistanceLoss = 0.01f;
constexpr float kPassMinChance = 0.02f;
constexpr float kPassMaxChance = 0.98f;
constexpr float kPassLaneClear = 4.0f;
constexpr float kPassLanePenalty = 0.6f;
constexpr float kPassPassingLow = 0.95f;
constexpr float kPassPassingHigh = 1.05f;
constexpr float kPassReceiverMarked = 0.2f;
constexpr float kPassMarkRadius = 4.0f;
constexpr float kPassPressure = 0.3f;
constexpr float kPassOffside = 0.15f;
constexpr float kPassOffsideMargin = 0.5f;
constexpr float kPassVisionRange = 30.0f;
constexpr float kPassVisionBonus = 25.0f;

constexpr float kThroughMinAdvance = 12.0f;
constexpr float kThroughPower = 0.85f;
constexpr float kThroughChanceFactor = 0.8f;

constexpr float kCrossMinWidth = 20.0f;
constexpr float kCrossMaxGoalLineDistance = 30.0f;
constexpr float kCrossMinDistance = 14.0f;
constexpr float kCrossMaxDistance = 36.0f;
constexpr float kCrossBaseChance = 0.5f;
// A cross into the area with nobody picked out, aimed at the penalty spot.
constexpr float kCrossMixerChance = 0.3f;
constexpr float kCrossCrossingLow = 0.7f;
constexpr float kCrossCrossingHigh = 1.3f;
// A teammate counts as in the box when this close to the goal centre.
constexpr float kCrossBoxReach = 30.0f;
constexpr float kCrossBoxBonus = 0.35f;

// A switch is a pass to the far side of the pitch.
constexpr float kSwitchMinLateral = 24.0f;
constexpr float kSwitchBonus = 0.05f;

// Dribbling heads for goal; the chance falls when an opponent covers the run.
constexpr float kRunTargetDistance = 10.0f;
constexpr float kDribbleChance = 0.7f;
constexpr float kDribbleSkillSwing = 0.18f;
constexpr float kDribbleGain = 0.8f;
constexpr float kDribbleBlockRadius = 3.5f;
constexpr float kDribbleBlockedChance = 0.35f;
constexpr float kDribbleBlockedGain = 0.1f;

constexpr float kHoldUtility = 0.0f;
constexpr float kHoldPressureCost = 0.18f;

// Ticks between decisions, how long a carrier waits after first getting the
// ball, and the most a perfect passer shaves off the interval.
constexpr int kDecisionInterval = 10;
constexpr int kFirstDecisionDelay = 12;
constexpr int kDecisionVisionSaving = 6;

}

}
