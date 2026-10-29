#include <helpers/MotionBlur.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace MotionBlur;

// --- scaled ---

TEST(MotionBlur, scaledStrengthZeroCollapsesMotionOntoCurrent) {
    const CBox CURRENT  = {100, 100, 200, 150};
    const CBox PREVIOUS = {50, 50, 300, 100};

    const auto SCALED = scaled(CURRENT, PREVIOUS, 0.0);

    EXPECT_DOUBLE_EQ(SCALED.x, CURRENT.x);
    EXPECT_DOUBLE_EQ(SCALED.y, CURRENT.y);
    EXPECT_DOUBLE_EQ(SCALED.w, CURRENT.w);
    EXPECT_DOUBLE_EQ(SCALED.h, CURRENT.h);
}

TEST(MotionBlur, scaledStrengthOnePreservesPreviousExactly) {
    const CBox CURRENT  = {100, 100, 200, 150};
    const CBox PREVIOUS = {50, 64, 210.37, 149.13};

    const auto SCALED = scaled(CURRENT, PREVIOUS, 1.0);

    EXPECT_EQ(SCALED.x, PREVIOUS.x);
    EXPECT_EQ(SCALED.y, PREVIOUS.y);
    EXPECT_EQ(SCALED.w, PREVIOUS.w);
    EXPECT_EQ(SCALED.h, PREVIOUS.h);
}

TEST(MotionBlur, scaledStrengthTwoDoublesDisplacementAndSizeDelta) {
    const CBox CURRENT  = {50, 50, 100, 100};
    const CBox PREVIOUS = {100, 100, 110, 120};

    const auto SCALED = scaled(CURRENT, PREVIOUS, 2.0);

    // displacement away from the current position is doubled
    EXPECT_DOUBLE_EQ(SCALED.x, 150);
    EXPECT_DOUBLE_EQ(SCALED.y, 150);
    // the size change between the frames is doubled
    EXPECT_DOUBLE_EQ(SCALED.w, 120);
    EXPECT_DOUBLE_EQ(SCALED.h, 140);
}

TEST(MotionBlur, scaledStrengthTenKeepsCollapsedSizePositive) {
    // the window grew from 50px to 100px, extrapolating by 10 would give a width of -400
    const CBox CURRENT  = {50, 50, 100, 100};
    const CBox PREVIOUS = {50, 50, 50, 50};

    const auto SCALED = scaled(CURRENT, PREVIOUS, 10.0);

    EXPECT_GT(SCALED.w, 0);
    EXPECT_GT(SCALED.h, 0);

    const auto EXTENTS = extents(SCALED, CURRENT);
    EXPECT_TRUE(std::isfinite(EXTENTS.x) && std::isfinite(EXTENTS.y) && std::isfinite(EXTENTS.w) && std::isfinite(EXTENTS.h));
    EXPECT_GT(EXTENTS.w, 0);
    EXPECT_GT(EXTENTS.h, 0);
}

TEST(MotionBlur, scaledNonFiniteStrengthPreservesPrevious) {
    const CBox CURRENT  = {50, 50, 100, 100};
    const CBox PREVIOUS = {100, 100, 110, 120};

    for (const double STRENGTH : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        const auto SCALED = scaled(CURRENT, PREVIOUS, STRENGTH);

        EXPECT_EQ(SCALED.x, PREVIOUS.x);
        EXPECT_EQ(SCALED.y, PREVIOUS.y);
        EXPECT_EQ(SCALED.w, PREVIOUS.w);
        EXPECT_EQ(SCALED.h, PREVIOUS.h);
    }
}

// --- CTracker ---

TEST(MotionBlurTracker, defaultStrengthMatchesStockGeometry) {
    CTracker   tracker;
    const CBox PREVIOUS = {40, 50, 210.5, 149.25};
    const CBox CURRENT  = {100, 100, 200, 150};

    tracker.record(PREVIOUS, CURRENT);

    const auto STATE = tracker.state(7, {}, true);
    ASSERT_TRUE(STATE.has_value());

    EXPECT_EQ(STATE->previous.x, PREVIOUS.x);
    EXPECT_EQ(STATE->previous.y, PREVIOUS.y);
    EXPECT_EQ(STATE->previous.w, PREVIOUS.w);
    EXPECT_EQ(STATE->previous.h, PREVIOUS.h);
    EXPECT_EQ(STATE->current.x, CURRENT.x);
    EXPECT_EQ(STATE->current.y, CURRENT.y);
    EXPECT_EQ(STATE->samples, 7);
}

TEST(MotionBlurTracker, strengthZeroYieldsCurrentGeometry) {
    CTracker   tracker;
    const CBox PREVIOUS = {40, 50, 210.5, 149.25};
    const CBox CURRENT  = {100, 100, 200, 150};

    tracker.record(PREVIOUS, CURRENT);

    const auto STATE = tracker.state(7, {}, true, 0.0);
    ASSERT_TRUE(STATE.has_value());

    EXPECT_EQ(STATE->previous.x, CURRENT.x);
    EXPECT_EQ(STATE->previous.y, CURRENT.y);
    EXPECT_EQ(STATE->previous.w, CURRENT.w);
    EXPECT_EQ(STATE->previous.h, CURRENT.h);

    // no motion left: the extents degenerate to the current box
    const auto EXTENTS = STATE->extents();
    EXPECT_DOUBLE_EQ(EXTENTS.x, CURRENT.x);
    EXPECT_DOUBLE_EQ(EXTENTS.y, CURRENT.y);
    EXPECT_DOUBLE_EQ(EXTENTS.w, CURRENT.w);
    EXPECT_DOUBLE_EQ(EXTENTS.h, CURRENT.h);
}

TEST(MotionBlurTracker, strengthOneKeepsFractionalRecordedGeometry) {
    CTracker   tracker;
    const CBox PREVIOUS = {40, 50, 210.5, 149.25};
    const CBox CURRENT  = {100, 100, 200, 150};

    tracker.record(PREVIOUS, CURRENT);

    const auto STATE = tracker.state(7, {}, true, 1.0);
    ASSERT_TRUE(STATE.has_value());

    EXPECT_EQ(STATE->previous.w, 210.5);
    EXPECT_EQ(STATE->previous.h, 149.25);
}

TEST(MotionBlurTracker, strengthTwoExtendsExtents) {
    // pure translation of 50px
    CTracker   translationTracker;
    const CBox PREVIOUS = {100, 50, 100, 100};
    const CBox CURRENT  = {50, 50, 100, 100};

    translationTracker.record(PREVIOUS, CURRENT);

    const auto BASELINE = translationTracker.state(7, {}, true, 1.0);
    ASSERT_TRUE(BASELINE.has_value());
    // window width + one motion length
    EXPECT_DOUBLE_EQ(BASELINE->extents().w, 150);

    const auto DOUBLED = translationTracker.state(7, {}, true, 2.0);
    ASSERT_TRUE(DOUBLED.has_value());
    // window width + two motion lengths
    EXPECT_DOUBLE_EQ(DOUBLED->extents().w, 200);
    EXPECT_DOUBLE_EQ(DOUBLED->extents().x, 50);
    EXPECT_DOUBLE_EQ(DOUBLED->extents().y, 50);
    EXPECT_DOUBLE_EQ(DOUBLED->extents().h, 100);

    // the extents still cover both the scaled previous and the current box
    EXPECT_LE(DOUBLED->extents().x, DOUBLED->previous.x);
    EXPECT_GE(DOUBLED->extents().x + DOUBLED->extents().w, DOUBLED->previous.x + DOUBLED->previous.w);
    EXPECT_LE(DOUBLED->extents().x, CURRENT.x);
    EXPECT_GE(DOUBLED->extents().x + DOUBLED->extents().w, CURRENT.x + CURRENT.w);

    // pure resize of 10px in width
    CTracker   resizeTracker;
    const CBox RESIZE_PREVIOUS = {50, 50, 110, 100};
    const CBox RESIZE_CURRENT  = {50, 50, 100, 100};

    resizeTracker.record(RESIZE_PREVIOUS, RESIZE_CURRENT);

    const auto RESIZE_DOUBLED = resizeTracker.state(7, {}, true, 2.0);
    ASSERT_TRUE(RESIZE_DOUBLED.has_value());
    // width delta doubled: 100 + 2 * 10
    EXPECT_DOUBLE_EQ(RESIZE_DOUBLED->previous.w, 120);
    EXPECT_DOUBLE_EQ(RESIZE_DOUBLED->extents().w, 120);
}

TEST(MotionBlurTracker, clampsStrengthToConfiguredRange) {
    CTracker   tracker;
    const CBox PREVIOUS = {100, 100, 110, 120};
    const CBox CURRENT  = {50, 50, 100, 100};

    tracker.record(PREVIOUS, CURRENT);

    const auto CLAMPED_HIGH = tracker.state(7, {}, true, 42.0);
    const auto AT_MAX       = tracker.state(7, {}, true, 10.0);
    ASSERT_TRUE(CLAMPED_HIGH.has_value());
    ASSERT_TRUE(AT_MAX.has_value());
    EXPECT_EQ(CLAMPED_HIGH->previous.x, AT_MAX->previous.x);
    EXPECT_EQ(CLAMPED_HIGH->previous.w, AT_MAX->previous.w);

    const auto CLAMPED_LOW = tracker.state(7, {}, true, -5.0);
    const auto AT_ZERO     = tracker.state(7, {}, true, 0.0);
    ASSERT_TRUE(CLAMPED_LOW.has_value());
    ASSERT_TRUE(AT_ZERO.has_value());
    EXPECT_EQ(CLAMPED_LOW->previous.x, AT_ZERO->previous.x);
    EXPECT_EQ(CLAMPED_LOW->previous.w, AT_ZERO->previous.w);
}
