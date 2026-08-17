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
#ifndef UISETUPINPUTLATCH_H
#define UISETUPINPUTLATCH_H

/*! \file UISetupInputLatch.h
    \brief Turning a polled "what is held right now" signal into one press.

    PERSDLJoyScan() reports buttons and hats by level, not by edge: while a pad
    button is held down it returns the same code on every poll. The setup
    wizard polls every 25 ms and advances to the next Saturn button as soon as
    it sees a code, so a single held press used to be written to several
    buttons in a row - the whole thirteen step sequence can be consumed in
    about a third of a second.

    The fix is to commit an input when it is released rather than when it is
    pressed. That is what this does, and it is kept out of the wizard page so
    the rule can be tested without a window or a real pad.

    The settings dialog does not need this: it binds one button at a time and
    stops polling straight afterwards, so nothing cascades. Changing the
    peripheral core's scan semantics instead would have changed that dialog's
    behaviour too.
*/

#include "../../core.h"

class UISetupInputLatch
{
public:
	UISetupInputLatch() : mPending(0), mIgnoreWhileHeld(0), mIgnoreOnce(0) {}

	//! The code being held, if any. The caller needs it to work out its opposite.
	u32 pending() const { return mPending; }

	/*! Swallow this code the next single time it turns up, however long that
	    takes.

	    For the opposite direction of an axis that was just bound. It has to
	    survive the polls that report nothing in between, because a trigger
	    reads as 0 for as long as it is held down and only reports its release
	    when the user finally lets go.
	*/
	void ignoreOnce(u32 code) { if (code != 0) mIgnoreOnce = code; }

	/*! Abandon a half-finished press, and keep ignoring it until it goes away.

	    Called when the step changes. Without the second half, skipping a
	    button while still holding an input would bind that input to the button
	    after it as soon as the user let go.
	*/
	void reset(u32 alsoIgnoreOnce = 0)
	{
		if (mPending != 0)
			mIgnoreWhileHeld = mPending;
		mPending = 0;
		ignoreOnce(alsoIgnoreOnce);
	}

	//! Forget everything, including what is being ignored. For leaving the page.
	void clear() { mPending = 0; mIgnoreWhileHeld = 0; mIgnoreOnce = 0; }

	//! True once an input has been seen and we are waiting for it to be let go.
	bool isArmed() const { return mPending != 0; }

	/*! Feed one poll result; returns the code to bind, or 0 for "not yet".

	    scanned is whatever the peripheral core reported this tick, 0 meaning
	    "nothing to report". The first non-zero code is remembered and nothing
	    is returned; the remembered code is returned once the core reports 0.

	    A second input pressed while the first is still down is ignored: the
	    code latched first is the one that gets bound, and the commit waits
	    until everything is released. Guessing which of two simultaneous inputs
	    the user meant would be worse than making them let go.
	*/
	u32 update(u32 scanned)
	{
		/* An input abandoned by skipping a button stays ignored for as long as
		   it keeps being reported, so letting go of it later does not land it
		   on the button after the one that was skipped. */
		if (mIgnoreWhileHeld != 0)
		{
			if (scanned == mIgnoreWhileHeld)
				return 0;
			mIgnoreWhileHeld = 0;
		}

		if (scanned != 0 && scanned == mIgnoreOnce)
		{
			mIgnoreOnce = 0;
			return 0;
		}

		if (mPending == 0)
		{
			/* Something else arrived first: whatever we were waiting to
			   swallow is no longer the next thing to happen. */
			if (scanned != 0)
				mIgnoreOnce = 0;
			mPending = scanned;
			return 0;
		}
		if (scanned != 0)
			return 0;

		const u32 committed = mPending;
		mPending = 0;
		return committed;
	}

private:
	u32 mPending;
	u32 mIgnoreWhileHeld;
	u32 mIgnoreOnce;
};

#endif /* UISETUPINPUTLATCH_H */
