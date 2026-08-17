/*  Copyright 2026 devMiyax

	This file is part of Yabause.

	Yabause is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Yabause is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Yabause; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*! \file test_UISetupInputLatch.cpp
    \brief Regression tests for committing a wizard binding on release.

    The bug this guards against, reported from real hardware: PERSDLJoyScan()
    reports pad buttons by level, so holding one button returned its code on
    every 25 ms poll. The wizard bound it and advanced, then bound the same
    code to the next Saturn button on the next poll, and so on - one held
    press could fill several buttons in under a second.
*/

#include <stdio.h>

#include "persdlcodes.h"
#include "qt/ui/UISetupInputLatch.h"

/* A Qt key code, well above everything this core emits. */
static const u32 Qt_Key_Like_Code = 0x01000013u;

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) \
	do { \
		tests_run++; \
		if (!(cond)) { \
			tests_failed++; \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
		} \
	} while (0)

//////////////////////////////////////////////////////////////////////////////

/* The reported bug. A button held across many polls must produce exactly one
   binding, and only once the user lets go. */
static void test_held_input_commits_once_on_release()
{
	UISetupInputLatch latch;
	int commits = 0;
	u32 committed = 0;

	for (int poll = 0; poll < 40; poll++)
	{
		const u32 out = latch.update(0x40000B);
		if (out != 0) { commits++; committed = out; }
	}
	CHECK(commits == 0, "a held input was bound before it was released");

	const u32 out = latch.update(0);
	if (out != 0) { commits++; committed = out; }

	CHECK(commits == 1, "releasing a held input did not bind it exactly once");
	CHECK(committed == 0x40000B, "the wrong code was bound on release");
}

/* Polling an idle pad must never bind anything. */
static void test_idle_never_commits()
{
	UISetupInputLatch latch;
	for (int poll = 0; poll < 10; poll++)
		CHECK(latch.update(0) == 0, "an idle poll produced a binding");
	CHECK(!latch.isArmed(), "an idle latch reported itself as armed");
}

/* A tap that is gone by the next poll still binds. */
static void test_single_poll_tap_commits()
{
	UISetupInputLatch latch;
	CHECK(latch.update(0x40000B) == 0, "a press bound immediately instead of on release");
	CHECK(latch.isArmed(), "a press did not arm the latch");
	CHECK(latch.update(0) == 0x40000B, "a quick tap was not bound");
	CHECK(!latch.isArmed(), "the latch stayed armed after committing");
}

/* Pressing a second input before releasing the first is ignored: the first one
   wins, and the commit waits for everything to be let go. */
static void test_second_input_while_held_is_ignored()
{
	UISetupInputLatch latch;
	CHECK(latch.update(0x40000B) == 0, "the first press bound immediately");
	CHECK(latch.update(0x40000C) == 0, "a second press bound while the first was held");
	CHECK(latch.update(0x40000C) == 0, "a commit happened while an input was still held");
	CHECK(latch.update(0) == 0x40000B, "the code latched first was not the one bound");
}

/* Consecutive steps: after a commit the latch takes the next input cleanly. */
static void test_latch_is_reusable()
{
	UISetupInputLatch latch;
	latch.update(0x111);
	CHECK(latch.update(0) == 0x111, "the first binding was wrong");
	latch.update(0x222);
	CHECK(latch.update(0) == 0x222, "the latch did not accept a second input");
}

/* Skipping a button while still holding an input must not carry that input
   over to the button after it - the wizard calls reset() on every step change. */
static void test_reset_discards_a_held_press()
{
	UISetupInputLatch latch;
	latch.update(0x40000B);
	CHECK(latch.isArmed(), "the press did not arm the latch");
	latch.reset();
	CHECK(!latch.isArmed(), "reset left the latch armed");
	/* Still holding it after the skip: it must stay ignored all the way to the
	   release, rather than becoming the next button's binding. */
	CHECK(latch.update(0x40000B) == 0, "a skipped press came back as a new one");
	CHECK(latch.update(0x40000B) == 0, "a skipped press came back while still held");
	CHECK(latch.update(0) == 0, "a skipped press was bound on release");

	/* Letting go and pressing again is a deliberate new press, and does bind. */
	CHECK(latch.update(0x40000B) == 0, "a fresh press bound before release");
	CHECK(latch.update(0) == 0x40000B, "a fresh press after a skip was not bound");
}

/* The reported L/R bug, as the core actually behaves.

   An axis is compared against a baseline the core re-takes as soon as it has
   reported a move, and the code carries the direction of travel. A trigger
   therefore reads: POS when pulled, 0 for as long as it is held, and NEG when
   released - two DIFFERENT codes for one physical action. Binding the pull to
   L used to leave the release to be bound to R. */
static void test_axis_return_to_rest_is_not_a_second_press()
{
	UISetupInputLatch latch;
	const u32 pull = 0x410004;     /* SDL_GC_AXIS_POS_VALUE | axis 4 */
	const u32 release = 0x420004;  /* SDL_GC_AXIS_NEG_VALUE | axis 4 */

	CHECK(latch.update(pull) == 0, "the pull bound immediately");
	CHECK(latch.update(0) == pull, "the pull was not bound");

	/* What the wizard does next: arm the opposite direction, then advance a
	   step, which drops the pending press but keeps the one-shot. */
	latch.ignoreOnce(release);
	latch.reset();

	/* Held for a while - the core reports nothing at all during this. */
	CHECK(latch.update(0) == 0, "an idle poll produced a binding");
	CHECK(latch.update(0) == 0, "an idle poll produced a binding");

	CHECK(latch.update(release) == 0, "the return to rest was taken as a press");
	CHECK(latch.update(0) == 0, "the return to rest was bound to the next button");
}

/* The one-shot must not be spent by an unrelated input arriving first. */
static void test_a_different_input_clears_the_pending_one_shot()
{
	UISetupInputLatch latch;
	latch.ignoreOnce(0x420004);

	CHECK(latch.update(0x40000B) == 0, "a button bound on press");
	CHECK(latch.update(0) == 0x40000B, "a button was swallowed by an unrelated one-shot");

	/* The stale one-shot is gone, so that axis works normally now. */
	CHECK(latch.update(0x420004) == 0, "the axis bound on press");
	CHECK(latch.update(0) == 0x420004, "the axis was still being ignored");
}

/* The same trigger may be put on two Saturn buttons on purpose. */
static void test_same_axis_can_be_reused_deliberately()
{
	UISetupInputLatch latch;
	const u32 pull = 0x410004;
	const u32 release = 0x420004;

	latch.update(pull);
	CHECK(latch.update(0) == pull, "the first binding failed");
	latch.ignoreOnce(release);
	latch.reset();
	CHECK(latch.update(release) == 0, "the return to rest was taken as a press");

	/* Pulled again, deliberately this time. */
	CHECK(latch.update(pull) == 0, "the deliberate second pull bound too early");
	CHECK(latch.update(0) == pull, "the same axis could not be reused");
}

/* A button after an axis binding is unaffected by the armed one-shot. */
static void test_button_after_an_axis_commit_still_works()
{
	UISetupInputLatch latch;
	latch.update(0x410004);
	CHECK(latch.update(0) == 0x410004, "the axis was not bound");
	latch.ignoreOnce(0x420004);
	latch.reset();

	CHECK(latch.update(0x40000C) == 0, "the next button bound on press");
	CHECK(latch.update(0x40000C) == 0, "the next button bound while held");
	CHECK(latch.update(0) == 0x40000C, "the next button was not bound on release");
}

/* The arithmetic the wizard relies on to know which code is the "other half"
   of an axis. Checked directly rather than only through the latch, because a
   wrong mask would silently make the L/R fix do nothing. */
static void test_opposite_axis_code()
{
	/* Game controller, device 0 and device 2. */
	CHECK(PERSDLOppositeAxisCode(0x410004) == 0x420004, "GC positive axis had the wrong opposite");
	CHECK(PERSDLOppositeAxisCode(0x420004) == 0x410004, "GC negative axis had the wrong opposite");
	CHECK(PERSDLOppositeAxisCode(0x410004 | (2u << 18)) == (0x420004 | (2u << 18)),
	      "the device field was not preserved");

	/* Raw joystick. */
	CHECK(PERSDLOppositeAxisCode(SDL_MIN_AXIS_VALUE | 3) == (SDL_MAX_AXIS_VALUE | 3),
	      "raw minimum axis had the wrong opposite");
	CHECK(PERSDLOppositeAxisCode(SDL_MAX_AXIS_VALUE | 3) == (SDL_MIN_AXIS_VALUE | 3),
	      "raw maximum axis had the wrong opposite");

	/* Things with no opposite. */
	CHECK(PERSDLOppositeAxisCode(SDL_GC_BUTTON_VALUE | 5) == 0, "a button reported an opposite");
	CHECK(PERSDLOppositeAxisCode(SDL_GC_AXIS_ANALOG_VALUE | 4) == 0, "an analog axis reported an opposite");
	CHECK(PERSDLOppositeAxisCode(SDL_HAT_VALUE | 1) == 0, "a hat reported an opposite");
	CHECK(PERSDLOppositeAxisCode(0) == 0, "the unbound code reported an opposite");
	CHECK(PERSDLOppositeAxisCode(Qt_Key_Like_Code) == 0, "a keyboard code reported an opposite");
}

//////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	test_held_input_commits_once_on_release();
	test_idle_never_commits();
	test_single_poll_tap_commits();
	test_second_input_while_held_is_ignored();
	test_latch_is_reusable();
	test_reset_discards_a_held_press();
	test_axis_return_to_rest_is_not_a_second_press();
	test_a_different_input_clears_the_pending_one_shot();
	test_same_axis_can_be_reused_deliberately();
	test_button_after_an_axis_commit_still_works();
	test_opposite_axis_code();

	printf("%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed == 0 ? 0 : 1;
}
